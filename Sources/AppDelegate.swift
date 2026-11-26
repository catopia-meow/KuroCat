import Foundation
import AppKit
import QuartzCore

final class AppDelegate: NSObject, NSApplicationDelegate, NSMenuDelegate {
    private var statusItem: NSStatusItem!
    private var overlayWindow: OverlayWindow!
    private var renderTimer: Timer?
    private var lastFrameTime: CFTimeInterval = CACurrentMediaTime()
    private var observedKeyActivity: CFTimeInterval = 0
    private var observedMouseActivity: CFTimeInterval = 0
    private var isPetVisible = true
    private let audio = SystemAudio()
    private let deskDetection = DeskDetection()
    private var nextDeskCheck: CFTimeInterval = 0
    private var nextClockCheck: CFTimeInterval = 0
    private var renderRunning = true
    private var automaticTablet = false
    private var showAudioFailure = false
    private var isHandingOverToNewerCopy = false

    private let currentAppVersion = Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as! String

    func applicationDidFinishLaunching(_ notification: Notification) {
        NSApp.setActivationPolicy(.accessory)
        if resolveDuplicateRunningCopies() { return }

        overlayWindow = OverlayWindow()
        overlayWindow.orderFrontRegardless()

        let catView = overlayWindow.catView
        catView.onRequestPermission = { [weak self] in
            self?.openInputMonitoringSettings()
        }
        catView.onRightClickMenu = { [weak self] event in
            guard let self else { return }
            let menu = self.buildMenu()
            NSMenu.popUpContextMenu(menu, with: event, for: self.overlayWindow.catView)
        }
        catView.onAction = { [weak self] action in self?.perform(action) }
        catView.onWake = { [weak self] in
            guard let self else { return }
            self.stopMusic()
            self.overlayWindow.catView.physics.wake()
        }
        catView.onBowlTaken = { [weak self] in self?.stopMusic() }
        catView.physics.onAutoMusic = { [weak self] in self?.startMusic(manual: false) == true }
        catView.physics.onAutoWake = { [weak self] in self?.audio.stop() }
        audio.onFailure = { [weak self] in
            guard let self else { return }
            if self.showAudioFailure {
                self.stopMusic()
                self.showInfoAlert(title: "系统音频无法使用", message: "请在系统设置中允许 SachiCat 录制屏幕与系统音频，再点一次听音乐。")
            } else {
                self.overlayWindow.catView.physics.sleepAfterMusicError()
            }
        }
        updateDeskMode()

        // Wire dual-layer InputEngine -> CatPhysics
        let engine = InputEngine.shared
        engine.onKeyDown = { [weak self] key in
            DispatchQueue.main.async {
                guard let self else { return }
                let physics = self.overlayWindow.catView.physics
                self.observedKeyActivity = InputEngine.shared.lastKeyActivityTime
                physics.resumeFromInput(self.observedKeyActivity)
                physics.triggerKeyDown(key)
                self.wakeRenderLoopToActive()
            }
        }
        engine.onMouseClick = { [weak self] button, isDown in
            DispatchQueue.main.async {
                guard let self else { return }
                let view = self.overlayWindow.catView
                self.observedMouseActivity = InputEngine.shared.lastMouseMoveTime
                if isDown && !view.pointerOnPet {
                    view.physics.resumeFromInput(InputEngine.shared.lastMouseClickTime)
                }
                view.physics.triggerMouseClick(button: button, isDown: isDown)
                self.wakeRenderLoopToActive()
            }
        }
        engine.start()

        setupStatusItem()
        if isAutoStartEnabled() {
            // Keep LaunchAgent path updated if user moved SachiCat.app into /Applications
            _ = setAutoStartEnabled(true)
        }
        startRenderLoop()

    }

    func applicationWillTerminate(_ notification: Notification) {
        // Quitting during launch because a newer copy is running: leave its stats alone.
        if isHandingOverToNewerCopy { return }
        renderTimer?.invalidate()
        audio.stop()
        InputEngine.shared.flushStats()
        InputEngine.shared.stop()
    }

    // MARK: - One SachiCat at a time (newest copy wins)

    /// Users often download a new SachiCat.app while an older copy is still running (or is started
    /// by the login item from another folder). That old process kept running and kept announcing
    /// "new version available" even though they had already updated. Keep only the newest copy:
    /// quit older ones, or hand over to an already-running newer one. Returns true if THIS process quits.
    private func resolveDuplicateRunningCopies() -> Bool {
        guard let bundleID = Bundle.main.bundleIdentifier else { return false }
        let myPID = ProcessInfo.processInfo.processIdentifier
        let myURL = Bundle.main.bundleURL.standardizedFileURL
        let others = NSRunningApplication.runningApplications(withBundleIdentifier: bundleID)
            .filter { $0.processIdentifier != myPID }
        if others.isEmpty { return false }

        for other in others {
            guard let otherURL = other.bundleURL?.standardizedFileURL, otherURL != myURL,
                  let otherVersion = Self.shortVersion(ofAppAt: otherURL) else { continue }
            // Earlier fork releases used a single integer (for example 6).
            if otherVersion.contains("."),
               otherVersion.compare(currentAppVersion, options: .numeric) == .orderedDescending {
                isHandingOverToNewerCopy = true
                _ = other.activate(options: [])
                NSApp.terminate(nil)
                return true
            }
        }
        for other in others {
            // Same path (replaced in place by an update) or an older/equal copy elsewhere.
            if !other.terminate() { _ = other.forceTerminate() }
            DispatchQueue.main.asyncAfter(deadline: .now() + 3.0) {
                if !other.isTerminated { _ = other.forceTerminate() }
            }
        }
        return false
    }

    /// Reads CFBundleShortVersionString straight from disk (Bundle(url:) may return cached info).
    private static func shortVersion(ofAppAt url: URL) -> String? {
        let plistURL = url.appendingPathComponent("Contents/Info.plist")
        guard let dict = NSDictionary(contentsOf: plistURL) as? [String: Any] else { return nil }
        return dict["CFBundleShortVersionString"] as? String
    }

    // MARK: - Adaptive Render Loop

    /// The timer's own interval follows the frame cadence (60 / 30 / 15 fps), so an idle or sleeping
    /// cat does not wake the CPU 60 times a second just to skip frames. A tolerance lets macOS
    /// coalesce our wake-ups with other timers. While the display or the Mac sleeps the loop stops.
    private func startRenderLoop() {
        lastFrameTime = CACurrentMediaTime()
        let center = NSWorkspace.shared.notificationCenter
        for name in [NSWorkspace.screensDidSleepNotification, NSWorkspace.willSleepNotification] {
            center.addObserver(self, selector: #selector(pauseRenderLoop), name: name, object: nil)
        }
        for name in [NSWorkspace.screensDidWakeNotification, NSWorkspace.didWakeNotification] {
            center.addObserver(self, selector: #selector(resumeRenderLoop), name: name, object: nil)
        }
        scheduleRenderTimer(interval: Self.activeFrameInterval)
    }

    private func scheduleRenderTimer(interval: CFTimeInterval) {
        if renderTimer?.isValid == true && renderTimer?.timeInterval == interval { return }
        renderTimer?.invalidate()
        let timer = Timer(timeInterval: interval, repeats: true) { [weak self] _ in self?.renderTick() }
        timer.tolerance = interval * 0.1
        RunLoop.main.add(timer, forMode: .common)
        renderTimer = timer
    }

    @objc private func pauseRenderLoop() {
        renderRunning = false
        renderTimer?.invalidate()
        renderTimer = nil
    }

    @objc private func resumeRenderLoop() {
        guard !renderRunning else { return }
        renderRunning = true
        // Skip the time spent asleep instead of simulating it in one giant step.
        lastFrameTime = CACurrentMediaTime()
        scheduleRenderTimer(interval: Self.activeFrameInterval)
    }

    /// Input arrived: go back to full frame rate right away instead of at the next slow tick.
    private func wakeRenderLoopToActive() {
        guard renderRunning, renderTimer?.timeInterval != Self.activeFrameInterval else { return }
        scheduleRenderTimer(interval: Self.activeFrameInterval)
    }

    private func renderTick() {
        guard isPetVisible, renderRunning else { return }
        let now = CACurrentMediaTime()
        let view = overlayWindow.catView
        let physics = view.physics
        let engine = InputEngine.shared
        let dt = CGFloat(min(now - lastFrameTime, 0.25))
        lastFrameTime = now
        if now >= nextClockCheck {
            nextClockCheck = now + 1
            physics.observeClock(Date())
        }
        if now >= nextDeskCheck && CatSettings.shared.deskMode == .automatic {
            nextDeskCheck = now + 0.5
            deskDetection.refresh { [weak self] tablet in self?.automaticTablet = tablet }
        }
        updateDeskMode()
        if physics.listening {
            (physics.soundPlaying, physics.soundPulse) = audio.reading()
        }
        physics.step(dt: dt, windowFrameInScreen: overlayWindow.frame)
        if view.isDragging && CatSettings.shared.coatTheme == .dopeyCat { physics.coastEgg(dt: dt) }
        view.updateHoverAndClickThrough()
        if engine.lastKeyActivityTime > observedKeyActivity {
            observedKeyActivity = engine.lastKeyActivityTime
            physics.resumeFromInput(observedKeyActivity)
        }
        if engine.lastMouseMoveTime > observedMouseActivity {
            observedMouseActivity = engine.lastMouseMoveTime
            if overlayWindow.ignoresMouseEvents {
                physics.resumeFromInput(observedMouseActivity)
            }
        }
        view.needsDisplay = true
        // Same cadence rules as Windows and Linux.
        scheduleRenderTimer(interval: frameInterval(now: now, physics: physics, engine: engine))
    }

    private static let activeFrameInterval: CFTimeInterval = 1.0 / 60.0   // typing, moving the mouse, interacting
    private static let idleFrameInterval: CFTimeInterval = 1.0 / 30.0     // awake but nobody is using the computer
    private static let sleepingFrameInterval: CFTimeInterval = 1.0 / 15.0 // asleep: slow breathing and z's only
    private static let activeInputWindow: CFTimeInterval = 2.0

    private func frameInterval(now: CFTimeInterval, physics: CatPhysics, engine: InputEngine) -> CFTimeInterval {
        let view = overlayWindow.catView
        if view.isDragging || physics.overControl || physics.listening { return Self.activeFrameInterval }
        if now - max(engine.lastKeyActivityTime, engine.lastMouseMoveTime) < Self.activeInputWindow {
            return Self.activeFrameInterval
        }
        return physics.sleepiness > 0.96 ? Self.sleepingFrameInterval : Self.idleFrameInterval
    }

    // MARK: - Menu Bar Icon & Menu

    private func setupStatusItem() {
        statusItem = NSStatusBar.system.statusItem(withLength: NSStatusItem.squareLength)
        if let button = statusItem.button {
            button.image = makeMenuBarCatIcon()
            button.toolTip = "SachiCat 黑猫键盘桌宠"
        }
        let menu = NSMenu()
        menu.delegate = self
        statusItem.menu = menu
    }

    func menuNeedsUpdate(_ menu: NSMenu) {
        menu.autoenablesItems = false
        menu.removeAllItems()
        for item in buildMenu().items {
            item.menu?.removeItem(item)
            menu.addItem(item)
        }
    }

    private func buildMenu() -> NSMenu {
        let menu = NSMenu()
        menu.autoenablesItems = false
        let engine = InputEngine.shared
        let settings = CatSettings.shared
        let physics = overlayWindow.catView.physics

        // Status header
        let statusPrefix = engine.status == .needsPermission ? "⚠️" : "🟢"
        let statusItem = NSMenuItem(title: "\(statusPrefix) \(engine.status.rawValue)", action: engine.status == .needsPermission ? #selector(openInputMonitoringSettings) : nil, keyEquivalent: "")
        statusItem.target = self
        statusItem.isEnabled = statusItem.action != nil
        menu.addItem(statusItem)

        if engine.status == .needsPermission {
            let grantItem = NSMenuItem(title: "🔑 前往系统设置开启「输入监控」...", action: #selector(openInputMonitoringSettings), keyEquivalent: "")
            grantItem.target = self
            menu.addItem(grantItem)
        }

        // Live stats
        let statsTitle = "⌨️ 今日击键 \(engine.totalKeystrokesToday)  ·  KPM \(engine.currentKPM)  ·  摸猫 \(physics.totalPetSessions) 次"
        let statsItem = NSMenuItem(title: statsTitle, action: nil, keyEquivalent: "")
        statsItem.isEnabled = false
        menu.addItem(statsItem)

        menu.addItem(.separator())

        // Visibility & Interactive modes
        let visItem = NSMenuItem(title: isPetVisible ? "隐藏小黑猫" : "显示小黑猫", action: #selector(toggleVisibility), keyEquivalent: "")
        visItem.target = self
        menu.addItem(visItem)

        let clickThroughItem = NSMenuItem(title: "完全鼠标穿透 (按住 ⌥Option 可临时右键/拖动)", action: #selector(toggleClickThrough), keyEquivalent: "")
        clickThroughItem.target = self
        clickThroughItem.state = settings.fullClickThrough ? .on : .off
        menu.addItem(clickThroughItem)

        let fullscreenItem = NSMenuItem(title: "全屏游戏/应用置顶显示", action: #selector(toggleFullScreenOverlay), keyEquivalent: "")
        fullscreenItem.target = self
        fullscreenItem.state = settings.fullScreenOverlay ? .on : .off
        menu.addItem(fullscreenItem)

        let autoStartItem = NSMenuItem(title: "开机自动启动", action: #selector(toggleAutoStart), keyEquivalent: "")
        autoStartItem.target = self
        autoStartItem.state = isAutoStartEnabled() ? .on : .off
        menu.addItem(autoStartItem)

        menu.addItem(.separator())

        // Features
        let rest: PetAction = physics.sleeping ? .wake : .sleep
        let actions: [PetAction] = [.treat, rest, .doubleClickWake, .music, .hide]
        for action in actions {
            let title: String
            switch action {
            case .music: title = physics.listening ? "停止听音乐" : action.title
            case .hide: title = settings.showControls ? "隐藏控件" : "显示控件"
            default: title = action.title
            }
            let item = NSMenuItem(title: title, action: #selector(selectPetAction(_:)), keyEquivalent: "")
            item.target = self
            item.representedObject = action.rawValue
            item.isEnabled = physics.canPerform(action)
            if action == .doubleClickWake { item.state = settings.doubleClickWake ? .on : .off }
            menu.addItem(item)
        }
        // Desk modes, listed the same way as the Windows menu.
        let deskModes: [(String, DeskMode)] = [("自动识别绘画窗口和文字输入", .automatic), ("键鼠模式", .keyboard),
                                               ("数位板模式", .tablet), ("街机摇杆模式", .arcade)]
        for (title, mode) in deskModes {
            let item = NSMenuItem(title: title, action: #selector(selectDeskMode(_:)), keyEquivalent: "")
            item.target = self
            item.tag = mode.rawValue
            item.state = settings.deskMode == mode ? .on : .off
            menu.addItem(item)
        }
        if physics.tabletMode {
            let clearItem = NSMenuItem(title: "清空画板", action: #selector(clearTablet), keyEquivalent: "")
            clearItem.target = self
            menu.addItem(clearItem)
        }
        let captureItem = NSMenuItem(title: "直播采集", action: nil, keyEquivalent: "")
        let captureMenu = NSMenu()
        for (title, selection) in [("透明窗口", 0), ("绿幕窗口（色度键）", 1)] {
            let item = NSMenuItem(title: title, action: #selector(selectCaptureMode(_:)), keyEquivalent: "")
            item.target = self
            item.tag = selection
            item.state = settings.greenScreenCapture == (selection == 1) ? .on : .off
            captureMenu.addItem(item)
        }
        captureItem.submenu = captureMenu
        menu.addItem(captureItem)
        menu.addItem(.separator())

        let badgesItem = NSMenuItem(title: "显示按键气泡 ([S] [W] [⇧])", action: #selector(toggleKeyBadges), keyEquivalent: "")
        badgesItem.target = self
        badgesItem.state = settings.showKeyBadges ? .on : .off
        menu.addItem(badgesItem)

        let mouseBadgesItem = NSMenuItem(title: "显示鼠标点击气泡 ([左键] [右键])", action: #selector(toggleMouseBadges), keyEquivalent: "")
        mouseBadgesItem.target = self
        mouseBadgesItem.state = settings.showMouseBadges ? .on : .off
        menu.addItem(mouseBadgesItem)

        let legendsItem = NSMenuItem(title: "显示键盘字符", action: #selector(toggleKeyLegends), keyEquivalent: "")
        legendsItem.target = self
        legendsItem.state = settings.showKeyLegends ? .on : .off
        menu.addItem(legendsItem)

        if settings.coatTheme != .dopeyCat {
            let footItem = NSMenuItem(title: "高速打字彩蛋（左脚和尾巴）", action: #selector(toggleFootEgg), keyEquivalent: "")
            footItem.target = self
            footItem.state = settings.footEasterEgg ? .on : .off
            menu.addItem(footItem)

            let petItem = NSMenuItem(title: "摸头/摸背闭眼互动", action: #selector(togglePetInteraction), keyEquivalent: "")
            petItem.target = self
            petItem.state = settings.petInteraction ? .on : .off
            menu.addItem(petItem)

        }
        menu.addItem(.separator())

        // Scale submenu
        let scaleMenu = NSMenu()
        let scales: [(String, CGFloat)] = [
            ("迷你 (0.65x)", 0.65),
            ("偏小 (0.85x)", 0.85),
            ("标准 (1.0x)", 1.0),
            ("偏大 (1.25x)", 1.25),
            ("超大 (1.5x)", 1.5)
        ]
        for (title, val) in scales {
            let item = NSMenuItem(title: title, action: #selector(selectScale(_:)), keyEquivalent: "")
            item.target = self
            item.representedObject = NSNumber(value: Double(val))
            item.state = abs(settings.scale - val) < 0.05 ? .on : .off
            scaleMenu.addItem(item)
        }
        let scaleParent = NSMenuItem(title: "尺寸大小", action: nil, keyEquivalent: "")
        scaleParent.submenu = scaleMenu
        menu.addItem(scaleParent)

        // Cat coat submenu: original skins plus approved shorthair and orange-white.
        let coatMenu = NSMenu()
        for coat in CatCoatTheme.allCases {
            let item = NSMenuItem(title: coat.rawValue, action: #selector(selectCoatTheme(_:)), keyEquivalent: "")
            item.target = self
            item.representedObject = coat.rawValue
            item.state = settings.coatTheme == coat ? .on : .off
            coatMenu.addItem(item)
        }
        let coatParent = NSMenuItem(title: "猫咪皮肤 🐱", action: nil, keyEquivalent: "")
        coatParent.submenu = coatMenu
        menu.addItem(coatParent)

        // Keyboard Theme submenu
        let kbMenu = NSMenu()
        for theme in KeyboardTheme.allCases {
            let item = NSMenuItem(title: theme.rawValue, action: #selector(selectKeyboardTheme(_:)), keyEquivalent: "")
            item.target = self
            item.representedObject = theme.rawValue
            item.state = settings.activeKeyboardTheme == theme ? .on : .off
            kbMenu.addItem(item)
        }
        let kbParent = NSMenuItem(title: "键盘配色", action: nil, keyEquivalent: "")
        kbParent.submenu = kbMenu
        menu.addItem(kbParent)

        // Eye Color submenu
        let eyeMenu = NSMenu()
        for eye in EyeColorTheme.allCases {
            let item = NSMenuItem(title: eye.rawValue, action: #selector(selectEyeTheme(_:)), keyEquivalent: "")
            item.target = self
            item.representedObject = eye.rawValue
            item.state = settings.eyeTheme == eye ? .on : .off
            eyeMenu.addItem(item)
        }
        let eyeParent = NSMenuItem(title: "瞳色切换", action: nil, keyEquivalent: "")
        eyeParent.submenu = eyeMenu
        menu.addItem(eyeParent)

        menu.addItem(.separator())

        let demoItem = NSMenuItem(title: "🎬 演示打字 + 摸猫动画", action: #selector(runDemoBurst), keyEquivalent: "")
        demoItem.target = self
        demoItem.isEnabled = physics.usingDesk
        menu.addItem(demoItem)

        let resetPosItem = NSMenuItem(title: "重置位置到右下角", action: #selector(resetPosition), keyEquivalent: "")
        resetPosItem.target = self
        menu.addItem(resetPosItem)

        let relaunchItem = NSMenuItem(title: "重启 SachiCat (授权后刷新)", action: #selector(relaunchApp), keyEquivalent: "")
        relaunchItem.target = self
        menu.addItem(relaunchItem)

        let checkItem = NSMenuItem(title: "检查更新 · SachiCat v\(currentAppVersion)", action: #selector(openReleasePage), keyEquivalent: "")
        checkItem.target = self
        menu.addItem(checkItem)

        menu.addItem(.separator())

        let quitItem = NSMenuItem(title: "退出 SachiCat", action: #selector(quitApp), keyEquivalent: "q")
        quitItem.target = self
        menu.addItem(quitItem)

        return menu
    }

    // MARK: - Actions

    @objc private func selectPetAction(_ sender: NSMenuItem) {
        guard let raw = sender.representedObject as? Int, let action = PetAction(rawValue: raw) else { return }
        perform(action)
    }

    private func perform(_ action: PetAction) {
        let physics = overlayWindow.catView.physics
        guard physics.canPerform(action) else { return }
        if action == .music {
            if physics.listening { stopMusic() } else { _ = startMusic(manual: true) }
            return
        }
        if physics.listening && action != .doubleClickWake { stopMusic() }
        switch action {
        case .treat: physics.offerTreat()
        case .sleep: physics.sleep()
        case .wake: physics.wake()
        case .doubleClickWake:
            CatSettings.shared.doubleClickWake.toggle()
            observedKeyActivity = InputEngine.shared.lastKeyActivityTime
            observedMouseActivity = InputEngine.shared.lastMouseMoveTime
        case .mode:
            CatSettings.shared.deskMode = CatSettings.shared.deskMode.next
            if CatSettings.shared.deskMode == .arcade && physics.sleeping { physics.wake() }
            updateDeskMode()
        case .keyBubbles: CatSettings.shared.showKeyBadges.toggle()
        case .mouseBubbles: CatSettings.shared.showMouseBadges.toggle()
        case .hide: overlayWindow.toggleControls()
        case .music: break
        }
    }

    @objc private func selectDeskMode(_ sender: NSMenuItem) {
        guard let mode = DeskMode(rawValue: sender.tag) else { return }
        let physics = overlayWindow.catView.physics
        if physics.listening { stopMusic() }
        if mode == .arcade && physics.sleeping { physics.wake() }
        CatSettings.shared.deskMode = mode
        updateDeskMode()
    }

    @objc private func clearTablet() {
        overlayWindow.catView.physics.clearTablet()
    }

    private func updateDeskMode() {
        let physics = overlayWindow.catView.physics
        let mode = CatSettings.shared.deskMode
        let tablet = mode == .tablet || (mode == .automatic && automaticTablet)
        if physics.usingDesk && physics.tabletMode != tablet {
            physics.tabletMode = tablet
            physics.clearTablet()
        }
    }

    private func startMusic(manual: Bool) -> Bool {
        showAudioFailure = manual
        guard audio.start(requestPermission: manual) else {
            if manual {
                showInfoAlert(title: "听音乐需要系统音频权限", message: "请在系统设置中允许 SachiCat 录制屏幕与系统音频，再点一次听音乐。")
            }
            return false
        }
        overlayWindow.catView.physics.listen(true)
        return true
    }

    private func stopMusic() {
        audio.stop()
        overlayWindow.catView.physics.listen(false)
    }

    @objc private func openInputMonitoringSettings() {
        InputEngine.shared.requestPermissionsAndOpenSettings()
    }

    @objc private func toggleVisibility() {
        isPetVisible.toggle()
        if isPetVisible {
            overlayWindow.orderFrontRegardless()
        } else {
            if overlayWindow.catView.physics.listening { stopMusic() }
            overlayWindow.orderOut(nil)
        }
    }

    @objc private func toggleClickThrough() {
        CatSettings.shared.fullClickThrough.toggle()
    }

    @objc private func selectCaptureMode(_ sender: NSMenuItem) {
        CatSettings.shared.greenScreenCapture = sender.tag == 1
        overlayWindow.applyCaptureMode()
    }

    @objc private func toggleFullScreenOverlay() {
        CatSettings.shared.fullScreenOverlay.toggle()
        overlayWindow.applyWindowLevelAndBehavior()
    }

    // MARK: - Launch at Login (macOS LaunchAgent — works reliably for signed & ad-hoc builds)

    private var launchAgentURL: URL {
        let dir = FileManager.default.homeDirectoryForCurrentUser
            .appendingPathComponent("Library/LaunchAgents", isDirectory: true)
        return dir.appendingPathComponent("com.catopia.kurocat.macos.plist")
    }

    private func isAutoStartEnabled() -> Bool {
        FileManager.default.fileExists(atPath: launchAgentURL.path)
    }

    @discardableResult
    private func setAutoStartEnabled(_ enable: Bool) -> Bool {
        let fm = FileManager.default
        let url = launchAgentURL
        if enable {
            do {
                try fm.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
                let appPath = Bundle.main.bundlePath
                let plist: [String: Any] = [
                    "Label": "com.catopia.kurocat.macos",
                    "ProgramArguments": ["/usr/bin/open", "-a", appPath],
                    "RunAtLoad": true,
                    "KeepAlive": false
                ]
                let data = try PropertyListSerialization.data(fromPropertyList: plist, format: .xml, options: 0)
                try data.write(to: url, options: .atomic)
                return true
            } catch {
                return false
            }
        } else {
            do {
                if fm.fileExists(atPath: url.path) {
                    try fm.removeItem(at: url)
                }
                return true
            } catch {
                return false
            }
        }
    }

    @objc private func toggleAutoStart() {
        _ = setAutoStartEnabled(!isAutoStartEnabled())
    }

    @objc private func toggleKeyBadges() {
        perform(.keyBubbles)
    }

    @objc private func toggleMouseBadges() {
        perform(.mouseBubbles)
    }

    @objc private func toggleKeyLegends() {
        CatSettings.shared.showKeyLegends.toggle()
    }

    @objc private func toggleFootEgg() {
        CatSettings.shared.footEasterEgg.toggle()
    }

    @objc private func togglePetInteraction() {
        CatSettings.shared.petInteraction.toggle()
    }

    @objc private func selectScale(_ sender: NSMenuItem) {
        guard let num = sender.representedObject as? NSNumber else { return }
        overlayWindow.applyScale(CGFloat(num.doubleValue))
    }

    @objc private func selectCoatTheme(_ sender: NSMenuItem) {
        guard let raw = sender.representedObject as? String,
              let coat = CatCoatTheme(rawValue: raw) else { return }
        CatSettings.shared.coatTheme = coat
        // Auto-pair signature eye color (still freely overridable via 瞳色切换!)
        switch coat {
        case .classicBlack:
            CatSettings.shared.eyeTheme = .neonChartreuse
        case .siamese:
            CatSettings.shared.eyeTheme = .siameseBlue
        case .shorthair, .orangeWhite:
            CatSettings.shared.eyeTheme = .softAmber
        case .dopeyCat:
            if overlayWindow.catView.physics.listening { stopMusic() }
            overlayWindow.catView.physics.enterDopeySkin()
        case .orcaCat:
            CatSettings.shared.eyeTheme = .referenceHazel
        }
        statusItem.button?.image = makeMenuBarCatIcon()
    }

    @objc private func selectKeyboardTheme(_ sender: NSMenuItem) {
        guard let raw = sender.representedObject as? String,
              let theme = KeyboardTheme(rawValue: raw) else { return }
        CatSettings.shared.activeKeyboardTheme = theme
    }

    @objc private func selectEyeTheme(_ sender: NSMenuItem) {
        guard let raw = sender.representedObject as? String,
              let theme = EyeColorTheme(rawValue: raw) else { return }
        CatSettings.shared.eyeTheme = theme
        statusItem.button?.image = makeMenuBarCatIcon()
    }

    @objc private func resetPosition() {
        overlayWindow.resetPosition()
    }

    @objc private func runDemoBurst() {
        let keys: [PhysicalKey] = [.w, .a, .s, .d, .f, .j, .o, .e, .space, .leftShift]
        for (idx, k) in keys.enumerated() {
            DispatchQueue.main.asyncAfter(deadline: .now() + Double(idx) * 0.13) { [weak self] in
                self?.overlayWindow.catView.physics.triggerKeyDown(k)
            }
        }
    }

    @objc private func relaunchApp() {
        let bundleURL = Bundle.main.bundleURL
        let config = NSWorkspace.OpenConfiguration()
        config.createsNewApplicationInstance = true
        NSWorkspace.shared.openApplication(at: bundleURL, configuration: config) { _, _ in
            DispatchQueue.main.async {
                NSApp.terminate(nil)
            }
        }
    }

    @objc private func quitApp() {
        NSApp.terminate(nil)
    }

    // MARK: - macOS release updates

    @objc private func openReleasePage() {
        if !NSWorkspace.shared.open(URL(string: "https://github.com/catopia-meow/SachiCat/releases")!) {
            showInfoAlert(title: "检查更新", message: "无法打开 GitHub 发布页。")
        }
    }

    private func showInfoAlert(title: String, message: String) {
        NSApp.activate(ignoringOtherApps: true)
        let alert = NSAlert()
        alert.alertStyle = .informational
        alert.messageText = title
        alert.informativeText = message
        alert.addButton(withTitle: "好")
        alert.runModal()
    }

    private func makeMenuBarCatIcon() -> NSImage {
        let coat = CatSettings.shared.coatTheme
        let eyes = CatSettings.shared.eyeTheme
        return NSImage(size: NSSize(width: 18, height: 18), flipped: true) { _ in
            guard let ctx = NSGraphicsContext.current?.cgContext else { return false }
            ctx.saveGState()
            ctx.translateBy(x: 9, y: 10)
            ctx.scaleBy(x: 0.175, y: 0.175)
            CatRenderer.drawPortrait(in: ctx, coat: coat, eyes: eyes)
            ctx.restoreGState()
            return true
        }
    }
}
