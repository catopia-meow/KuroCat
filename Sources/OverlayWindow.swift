import Foundation
import AppKit

private let captureGreen = NSColor(srgbRed: 0, green: 1, blue: 0, alpha: 1)

final class CatCanvasView: NSView {
    private enum ClickTarget: Equatable {
        case bowl, restingHead, restingBody, sulkingCat, dopeyCat
    }

    private struct Press {
        let screenPoint: CGPoint
        let windowOrigin: CGPoint
        let target: ClickTarget?
    }

    let physics = CatPhysics()
    var onRequestPermission: (() -> Void)?
    var onRightClickMenu: ((NSEvent) -> Void)?
    var onAction: ((PetAction) -> Void)?
    var onWake: (() -> Void)?
    var onBowlTaken: (() -> Void)?

    private var lastHoverCanvasPt: CGPoint?
    private var hover: PetAction?
    private var held: PetAction?
    private var press: Press?
    private var previousClick: ClickTarget?
    private(set) var isDragging = false
    private var lastDragCursor = CGPoint.zero
    private var isShowingOpenHand = false

    override var isFlipped: Bool { true }
    override var isOpaque: Bool { CatSettings.shared.greenScreenCapture }
    override var acceptsFirstResponder: Bool { false }
    override func acceptsFirstMouse(for event: NSEvent?) -> Bool { true }
    static var logicalHeight: CGFloat {
        CatSettings.shared.showControls ? CatRenderer.controlsCanvasHeight : CatPhysics.canvasSize.height
    }

    override func draw(_ dirtyRect: NSRect) {
        guard let ctx = NSGraphicsContext.current?.cgContext else { return }
        let settings = CatSettings.shared
        if settings.greenScreenCapture {
            ctx.setFillColor(captureGreen.cgColor)
            ctx.fill(bounds)
        } else {
            ctx.clear(bounds)
        }

        let scaleX = bounds.width / CatPhysics.canvasSize.width
        let scaleY = bounds.height / Self.logicalHeight
        ctx.saveGState()
        ctx.scaleBy(x: scaleX, y: scaleY)

        let showPill = InputEngine.shared.status == .needsPermission
        CatRenderer.draw(in: ctx, physics: physics, showPermissionPill: showPill, dragging: isDragging)
        if settings.showControls && !(settings.coatTheme == .dopeyCat && isDragging) {
            CatRenderer.drawControls(in: ctx, physics: physics, mode: settings.deskMode,
                                     doubleClickWake: settings.doubleClickWake,
                                     keyBubbles: settings.showKeyBadges, mouseBubbles: settings.showMouseBadges,
                                     hover: hover, held: held)
        }

        ctx.restoreGState()
    }

    func canvasPoint(fromScreenPoint screenPt: CGPoint) -> CGPoint? {
        guard let win = window else { return nil }
        let frame = win.frame
        guard frame.contains(screenPt) else { return nil }
        let localX = (screenPt.x - frame.minX) / frame.width * CatPhysics.canvasSize.width
        // Convert bottom-up screen Y to flipped top-down canvas Y
        let localY = (frame.maxY - screenPt.y) / frame.height * Self.logicalHeight
        return CGPoint(x: localX, y: localY)
    }

    var pointerOnPet: Bool {
        if press != nil || held != nil { return true }
        guard window?.isVisible == true, let pt = canvasPoint(fromScreenPoint: NSEvent.mouseLocation) else { return false }
        let settings = CatSettings.shared
        let showPill = InputEngine.shared.status == .needsPermission
        let zone = CatRenderer.hitTest(point: pt, physics: physics, showPermissionPill: showPill, showControls: settings.showControls)
        if settings.fullClickThrough && !NSEvent.modifierFlags.contains(.option) { return zone == .permissionPill }
        return zone != .none
    }

    /// Called on every 60fps tick to handle smart click-through AND petting strokes.
    func updateHoverAndClickThrough() {
        guard let win = window else { return }
        let screenPt = NSEvent.mouseLocation
        let showPill = InputEngine.shared.status == .needsPermission

        if press != nil || held != nil {
            physics.setBellyHovered(false)
            physics.overControl = held != nil
            win.ignoresMouseEvents = false
            return
        }

        guard let pt = canvasPoint(fromScreenPoint: screenPt) else {
            lastHoverCanvasPt = nil
            hover = nil
            toolTip = nil
            physics.overControl = false
            physics.setBellyHovered(false)
            if !win.ignoresMouseEvents {
                win.ignoresMouseEvents = true
            }
            resetCursorIfNeeded()
            return
        }

        let settings = CatSettings.shared
        let zone = CatRenderer.hitTest(point: pt, physics: physics, showPermissionPill: showPill, showControls: settings.showControls)
        let optionHeld = NSEvent.modifierFlags.contains(.option)
        let unlocked = !settings.fullClickThrough || optionHeld
        if case let .control(action) = zone, unlocked {
            if hover != action {
                toolTip = action == .mode ? "\(settings.deskMode.title) · 点击切换" : action.title
            }
            hover = action
        } else {
            if hover != nil { toolTip = nil }
            hover = nil
        }
        if case .tabletKey = zone {
            physics.overControl = unlocked
        } else {
            physics.overControl = hover != nil
        }
        physics.setBellyHovered(settings.coatTheme != .dopeyCat && settings.petInteraction && zone == .belly)
        if zone == .desk { physics.stopPetting() }

        let ear = zone == .leftEar || zone == .rightEar
        if settings.coatTheme != .dopeyCat && (zone == .head || zone == .body || ear) {
            let delta = lastHoverCanvasPt.map { hypot(pt.x - $0.x, pt.y - $0.y) } ?? 0
            let stroke = delta > 1.2 && delta < 80
            if ear && settings.petInteraction && (lastHoverCanvasPt == nil || stroke) {
                physics.touchEar(left: zone == .leftEar)
            } else if !ear && stroke {
                physics.registerPetStroke(canvasPoint: pt, delta: delta, isHead: zone == .head)
            }
            lastHoverCanvasPt = pt
        } else {
            lastHoverCanvasPt = nil
        }

        // Smart Click-Through logic (hold Option ⌥ to temporarily unlock Full Click-Through!)
        if !unlocked {
            // Keep permission pill clickable only if permission isn't granted yet
            win.ignoresMouseEvents = (zone != .permissionPill)
            resetCursorIfNeeded()
        } else {
            let shouldCapture = (zone != .none)
            if win.ignoresMouseEvents == shouldCapture {
                win.ignoresMouseEvents = !shouldCapture
            }
            if zone == .head || zone == .body || zone == .belly || ear {
                if !isShowingOpenHand {
                    NSCursor.openHand.set()
                    isShowingOpenHand = true
                }
            } else if zone == .permissionPill || zone == .bowl || hover != nil {
                NSCursor.pointingHand.set()
                isShowingOpenHand = false
            } else {
                resetCursorIfNeeded()
            }
        }
    }

    private func resetCursorIfNeeded() {
        if isShowingOpenHand {
            NSCursor.arrow.set()
            isShowingOpenHand = false
        }
    }

    override func mouseDown(with event: NSEvent) {
        guard let win = window else { return }
        let optionHeld = event.modifierFlags.contains(.option)
        let settings = CatSettings.shared
        let screenPt = win.convertPoint(toScreen: event.locationInWindow)
        let showPill = InputEngine.shared.status == .needsPermission
        guard let pt = canvasPoint(fromScreenPoint: screenPt) else { return }
        let zone = CatRenderer.hitTest(point: pt, physics: physics, showPermissionPill: showPill,
                                       showControls: settings.showControls)
        if zone == .permissionPill {
            previousClick = nil
            onRequestPermission?()
            return
        }
        guard !settings.fullClickThrough || optionHeld, zone != .none else { return }
        if case let .control(action) = zone {
            previousClick = nil
            held = action
            return
        }
        if case let .tabletKey(slot) = zone, !optionHeld {
            previousClick = nil
            physics.triggerKeyDown(physics.tabletKeys[slot])
            return
        }

        let target: ClickTarget?
        let catHead = zone == .head || zone == .leftEar || zone == .rightEar
        let catBody = zone == .body || zone == .belly
        if !optionHeld && zone == .bowl {
            target = .bowl
        } else if !optionHeld && settings.coatTheme == .dopeyCat && physics.usingDesk && (catHead || catBody) {
            target = .dopeyCat
        } else if !optionHeld && physics.facingAway && (catHead || catBody) {
            target = .sulkingCat
        } else if !optionHeld && physics.atRest && catHead {
            target = .restingHead
        } else if !optionHeld && physics.atRest && catBody {
            target = .restingBody
        } else {
            target = nil
        }
        press = Press(screenPoint: screenPt, windowOrigin: win.frame.origin, target: target)
        lastDragCursor = screenPt
        physics.eggRoll = 0
        physics.remainingRoll = 0
    }

    override func mouseDragged(with event: NSEvent) {
        guard let press, let win = window else { return }
        let currentScreenPt = NSEvent.mouseLocation
        let dx = currentScreenPt.x - press.screenPoint.x
        let dy = currentScreenPt.y - press.screenPoint.y
        if !isDragging {
            guard hypot(dx, dy) >= 4 else { return }
            isDragging = true
            NSCursor.closedHand.set()
        }
        if CatSettings.shared.coatTheme == .dopeyCat {
            let scale = bounds.width / CatPhysics.canvasSize.width
            physics.rollEgg(dx: (currentScreenPt.x - lastDragCursor.x) / scale,
                            dy: (lastDragCursor.y - currentScreenPt.y) / scale)
            lastDragCursor = currentScreenPt
        }
        win.setFrameOrigin(CGPoint(x: press.windowOrigin.x + dx, y: press.windowOrigin.y + dy))
    }

    override func mouseUp(with event: NSEvent) {
        if let action = held {
            held = nil
            if let point = canvasPoint(fromScreenPoint: NSEvent.mouseLocation), CatRenderer.controlRect(action).contains(point) {
                onAction?(action)
            }
            return
        }
        guard let press, let win = window else { return }
        self.press = nil
        if isDragging {
            isDragging = false
            previousClick = nil
            NSCursor.openHand.set()
            isShowingOpenHand = true
            UserDefaults.standard.set(Double(win.frame.origin.x), forKey: "KuroCat.PosX")
            let baseY = win.frame.maxY - CatPhysics.canvasSize.height * CatSettings.shared.scale
            UserDefaults.standard.set(Double(baseY), forKey: "KuroCat.PosY")
            return
        }
        let doubleClick = event.clickCount >= 2 && previousClick == press.target
        previousClick = doubleClick ? nil : press.target
        switch press.target {
        case .dopeyCat? where CatSettings.shared.coatTheme == .dopeyCat:
            if physics.listening { onAction?(.music) }
            physics.clickDopeyCat()
        case .bowl? where physics.bowlVisible && !physics.ignoring:
            if doubleClick && CatSettings.shared.coatTheme != .dopeyCat { onBowlTaken?() }
            physics.clickBowl(take: doubleClick)
        case .sulkingCat? where physics.facingAway:
            physics.pokeBack()
        case .restingHead? where physics.atRest:
            if doubleClick && CatSettings.shared.doubleClickWake { onWake?() }
            else { physics.bounceAtRest() }
        case .restingBody? where physics.atRest:
            physics.bounceAtRest()
        default: break
        }
    }

    override func rightMouseDown(with event: NSEvent) {
        previousClick = nil
        onRightClickMenu?(event)
    }
}

final class OverlayWindow: NSPanel {
    let catView = CatCanvasView()

    init() {
        let scale = CatSettings.shared.scale
        let baseSize = CGSize(width: CatPhysics.canvasSize.width * scale, height: CatPhysics.canvasSize.height * scale)
        let size = CGSize(
            width: baseSize.width,
            height: CatCanvasView.logicalHeight * scale
        )
        var origin = OverlayWindow.initialOrigin(for: baseSize)
        if CatSettings.shared.showControls {
            let baseFrame = CGRect(origin: origin, size: baseSize)
            let lowerEdge = NSScreen.screens.first { $0.frame.intersects(baseFrame) }?.visibleFrame.minY ?? origin.y
            origin.y = max(lowerEdge, baseFrame.maxY - size.height)
        }

        super.init(
            contentRect: CGRect(origin: origin, size: size),
            styleMask: [.borderless, .nonactivatingPanel],
            backing: .buffered,
            defer: false
        )

        title = "SachiCat"
        isOpaque = false
        backgroundColor = .clear
        hasShadow = false
        isMovable = false
        hidesOnDeactivate = false
        becomesKeyOnlyIfNeeded = true
        acceptsMouseMovedEvents = true
        ignoresMouseEvents = true

        applyWindowLevelAndBehavior()

        catView.frame = CGRect(origin: .zero, size: size)
        catView.autoresizingMask = [.width, .height]
        contentView = catView
        applyCaptureMode()
    }

    func applyCaptureMode() {
        let greenScreen = CatSettings.shared.greenScreenCapture
        isOpaque = greenScreen
        backgroundColor = greenScreen ? captureGreen : .clear
        catView.needsDisplay = true
    }

    func applyWindowLevelAndBehavior() {
        if CatSettings.shared.fullScreenOverlay {
            level = .statusBar
            collectionBehavior = [.canJoinAllSpaces, .fullScreenAuxiliary, .stationary, .ignoresCycle]
        } else {
            level = .floating
            collectionBehavior = [.canJoinAllSpaces, .stationary, .ignoresCycle]
        }
    }

    func applyScale(_ newScale: CGFloat) {
        CatSettings.shared.scale = newScale
        let oldFrame = frame
        let newSize = CGSize(
            width: CatPhysics.canvasSize.width * newScale,
            height: CatCanvasView.logicalHeight * newScale
        )
        // Anchor bottom-center so scaling feels grounded
        let newOrigin = CGPoint(
            x: oldFrame.midX - newSize.width * 0.5,
            y: oldFrame.minY
        )
        setFrame(CGRect(origin: newOrigin, size: newSize), display: true)
        UserDefaults.standard.set(Double(newOrigin.x), forKey: "KuroCat.PosX")
        UserDefaults.standard.set(Double(newOrigin.y + newSize.height - CatPhysics.canvasSize.height * newScale), forKey: "KuroCat.PosY")
    }

    func toggleControls() {
        CatSettings.shared.showControls.toggle()
        let height = CatCanvasView.logicalHeight * CatSettings.shared.scale
        let lowerEdge = screen?.visibleFrame.minY ?? frame.minY
        let origin = CGPoint(x: frame.minX, y: max(lowerEdge, frame.maxY - height))
        setFrame(CGRect(origin: origin, size: CGSize(width: frame.width, height: height)), display: true)
    }

    func resetPosition() {
        let origin = OverlayWindow.defaultOrigin(for: frame.size)
        setFrameOrigin(origin)
        UserDefaults.standard.set(Double(origin.x), forKey: "KuroCat.PosX")
        UserDefaults.standard.set(Double(origin.y + frame.height - CatPhysics.canvasSize.height * CatSettings.shared.scale), forKey: "KuroCat.PosY")
    }

    private static func initialOrigin(for size: CGSize) -> CGPoint {
        let d = UserDefaults.standard
        if d.object(forKey: "KuroCat.PosX") != nil && d.object(forKey: "KuroCat.PosY") != nil {
            let x = CGFloat(d.double(forKey: "KuroCat.PosX"))
            let y = CGFloat(d.double(forKey: "KuroCat.PosY"))
            let saved = CGPoint(x: x, y: y)
            // Ensure saved origin is still on a connected display
            let rect = CGRect(origin: saved, size: size)
            if NSScreen.screens.contains(where: { $0.frame.intersects(rect) }) {
                return saved
            }
        }
        return defaultOrigin(for: size)
    }

    private static func defaultOrigin(for size: CGSize) -> CGPoint {
        let screen = NSScreen.main?.visibleFrame ?? CGRect(x: 0, y: 0, width: 1440, height: 900)
        return CGPoint(
            x: screen.maxX - size.width - 28,
            y: screen.minY + 18
        )
    }
}
