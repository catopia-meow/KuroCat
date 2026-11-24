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
                self.showInfoAlert(title: "系统音频无法使用", message: "请在系统设置中允许 KuroCat 录制屏幕与系统音频，再点一次听音乐。")
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
            }
        }
        engine.start()

        setupStatusItem()
        if isAutoStartEnabled() {
            // Keep LaunchAgent path updated if user moved KuroCat.app into /Applications
            _ = setAutoStartEnabled(true)
        }
        startRenderLoop()

        // Auto-check GitHub Releases for updates shortly after launch
        DispatchQueue.main.asyncAfter(deadline: .now() + 2.5) { [weak self] in
            self?.checkForUpdates(manual: false)
        }
    }

    func applicationWillTerminate(_ notification: Notification) {
        // Quitting during launch because a newer copy is running: leave its stats alone.
        if isHandingOverToNewerCopy { return }
        renderTimer?.invalidate()
        audio.stop()
        InputEngine.shared.flushStats()
        InputEngine.shared.stop()
    }

    // MARK: - One KuroCat at a time (newest copy wins)

    /// Users often download a new KuroCat.app while an older copy is still running (or is started
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
            if otherVersion.compare(currentAppVersion, options: .numeric) == .orderedDescending {
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

    // MARK: - 60 FPS Render Loop

    private func startRenderLoop() {
        lastFrameTime = CACurrentMediaTime()
        let timer = Timer(timeInterval: 1.0 / 60.0, repeats: true) { [weak self] _ in
            guard let self, self.isPetVisible else { return }
            let now = CACurrentMediaTime()
            let dt = CGFloat(now - self.lastFrameTime)
            self.lastFrameTime = now

            let view = self.overlayWindow.catView
            let physics = view.physics
            let engine = InputEngine.shared
            if now >= self.nextClockCheck {
                self.nextClockCheck = now + 1
                let last = max(engine.lastKeyActivityTime, max(engine.lastMouseMoveTime, engine.lastMouseClickTime))
                let working = !engine.activeKeys.isEmpty || engine.leftMouseDown || engine.rightMouseDown || now - last < 2
                physics.observeClock(Date(), working: working)
            }
            if now >= self.nextDeskCheck && CatSettings.shared.deskMode == .automatic {
                self.nextDeskCheck = now + 0.5
                self.deskDetection.refresh { [weak self] tablet in self?.automaticTablet = tablet }
            }
            self.updateDeskMode()
            if physics.listening {
                (physics.soundPlaying, physics.soundPulse) = self.audio.reading()
            }
            view.physics.step(dt: dt, windowFrameInScreen: self.overlayWindow.frame)
            view.updateHoverAndClickThrough()
            if engine.lastKeyActivityTime > self.observedKeyActivity {
                self.observedKeyActivity = engine.lastKeyActivityTime
                physics.resumeFromInput(self.observedKeyActivity)
            }
            if engine.lastMouseMoveTime > self.observedMouseActivity {
                self.observedMouseActivity = engine.lastMouseMoveTime
                if self.overlayWindow.ignoresMouseEvents {
                    physics.resumeFromInput(self.observedMouseActivity)
                }
            }
            view.needsDisplay = true
        }
        RunLoop.main.add(timer, forMode: .common)
        renderTimer = timer
    }

    // MARK: - Menu Bar Icon & Menu

    private func setupStatusItem() {
        statusItem = NSStatusBar.system.statusItem(withLength: NSStatusItem.squareLength)
        if let button = statusItem.button {
            button.image = makeMenuBarCatIcon()
            button.toolTip = "KuroCat 黑猫键盘桌宠"
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
        let actions: [PetAction] = [.treat, rest, .doubleClickWake, .music, .mode, .hide]
        for action in actions {
            let title: String
            switch action {
            case .music: title = physics.listening ? "停止听音乐" : action.title
            case .mode: title = "切换模式 · \(settings.deskMode.title)"
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

        let footItem = NSMenuItem(title: "高速打字彩蛋（左脚和尾巴）", action: #selector(toggleFootEgg), keyEquivalent: "")
        footItem.target = self
        footItem.state = settings.footEasterEgg ? .on : .off
        menu.addItem(footItem)

        let petItem = NSMenuItem(title: "摸头/摸背闭眼互动", action: #selector(togglePetInteraction), keyEquivalent: "")
        petItem.target = self
        petItem.state = settings.petInteraction ? .on : .off
        menu.addItem(petItem)

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
            item.state = settings.keyboardTheme == theme ? .on : .off
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

        let relaunchItem = NSMenuItem(title: "重启 KuroCat (授权后刷新)", action: #selector(relaunchApp), keyEquivalent: "")
        relaunchItem.target = self
        menu.addItem(relaunchItem)

        let checkItem = NSMenuItem(title: "检查更新 · macOS 第 \(currentAppVersion) 版", action: #selector(manualCheckForUpdates), keyEquivalent: "")
        checkItem.target = self
        menu.addItem(checkItem)

        menu.addItem(.separator())

        let quitItem = NSMenuItem(title: "退出 KuroCat", action: #selector(quitApp), keyEquivalent: "q")
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
            updateDeskMode()
        case .keyBubbles: CatSettings.shared.showKeyBadges.toggle()
        case .mouseBubbles: CatSettings.shared.showMouseBadges.toggle()
        case .hide: overlayWindow.toggleControls()
        case .music: break
        }
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
                showInfoAlert(title: "听音乐需要系统音频权限", message: "请在系统设置中允许 KuroCat 录制屏幕与系统音频，再点一次听音乐。")
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
        case .orcaCat:
            CatSettings.shared.eyeTheme = .referenceHazel
        }
        statusItem.button?.image = makeMenuBarCatIcon()
    }

    @objc private func selectKeyboardTheme(_ sender: NSMenuItem) {
        guard let raw = sender.representedObject as? String,
              let theme = KeyboardTheme(rawValue: raw) else { return }
        CatSettings.shared.keyboardTheme = theme
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

    private struct Release: Decodable {
        let tag_name: String
        let prerelease: Bool
        let draft: Bool
    }

    @objc private func manualCheckForUpdates() {
        checkForUpdates(manual: true)
    }

    private func checkForUpdates(manual: Bool) {
        let endpoint = URL(string: "https://api.github.com/repos/catopia-meow/KuroCat/releases?per_page=100")!
        let prefix = "orca-cat-macos-"
        let version = Int(currentAppVersion)!
        URLSession.shared.dataTask(with: endpoint) { [weak self] data, response, _ in
            let releases: [Release]?
            if let data, (response as? HTTPURLResponse)?.statusCode == 200 {
                releases = try? JSONDecoder().decode([Release].self, from: data)
            } else {
                releases = nil
            }
            let latest = releases?.compactMap { release -> Int? in
                guard !release.draft, !release.prerelease, release.tag_name.hasPrefix(prefix) else { return nil }
                return Int(release.tag_name.dropFirst(prefix.count))
            }.max()
            DispatchQueue.main.async {
                guard let self else { return }
                if let latest, latest > version {
                    let page = URL(string: "https://github.com/catopia-meow/KuroCat/releases/tag/\(prefix)\(latest)")!
                    NSWorkspace.shared.open(page)
                } else if manual {
                    self.showInfoAlert(title: releases == nil ? "检查更新失败" : "已是最新版本",
                                       message: releases == nil ? "暂时无法读取 GitHub 发布信息。" : "当前为 macOS 第 \(version) 版。")
                }
            }
        }.resume()
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
