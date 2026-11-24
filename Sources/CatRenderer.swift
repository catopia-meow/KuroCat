import Foundation
import CoreGraphics
import AppKit

enum CatHitZone: Equatable {
    case none
    case head
    case body
    case belly
    case leftEar
    case rightEar
    case desk
    case bowl
    case control(PetAction)
    case tabletKey(Int)
    case permissionPill
}

enum CatRenderer {
    private static func patterned(_ coat: CatCoatTheme) -> Bool { coat == .shorthair || coat == .orangeWhite }
    private static func rgb(_ hex: Int, _ alpha: CGFloat = 1) -> NSColor {
        NSColor(calibratedRed: CGFloat((hex >> 16) & 255)/255,
                green: CGFloat((hex >> 8) & 255)/255, blue: CGFloat(hex & 255)/255, alpha: alpha)
    }
    private static func bodyColor(_ coat: CatCoatTheme) -> NSColor {
        coat == .shorthair ? rgb(0xC2C5C1) : (coat == .orangeWhite ? rgb(0xF2EEE4) : catBlack)
    }
    private static func limbColor(_ coat: CatCoatTheme) -> NSColor {
        coat == .orangeWhite ? rgb(0xD8AA73) : bodyColor(coat)
    }
    private static func pawColor(_ coat: CatCoatTheme) -> NSColor {
        if coat == .orcaCat { return rgb(0xF3F2EE) }
        return patterned(coat) ? rgb(0xF2EEE4) : (coat == .siamese ? siamesePoint : catBlack)
    }
    private static func earTailColor(_ coat: CatCoatTheme) -> NSColor {
        coat == .siamese ? siamesePoint : limbColor(coat)
    }
    private static func creaseColor(_ coat: CatCoatTheme) -> NSColor {
        if coat == .orcaCat { return rgb(0xA5A4AA, 0.52) }
        return patterned(coat) ? rgb(0x8B8171, 0.60) : contourMint
    }

    static let catBlack = NSColor(calibratedRed: 0.085, green: 0.085, blue: 0.095, alpha: 1.0)
    static let contourMint = NSColor(calibratedRed: 0.82, green: 0.92, blue: 0.89, alpha: 0.76)
    static let whiskerColor = NSColor(calibratedRed: 0.88, green: 0.90, blue: 0.92, alpha: 0.72)

    // Siamese Seal-Point Palette (挖煤小暹罗: 暖燕麦奶白躯干 + 浓郁可可焦糖重点色)
    static let siameseCream   = NSColor(calibratedRed: 0.945, green: 0.905, blue: 0.835, alpha: 1.0) // #F1E7D5
    static let siameseLatte   = NSColor(calibratedRed: 0.865, green: 0.795, blue: 0.695, alpha: 1.0) // #DDCBB1
    static let siameseMocha   = NSColor(calibratedRed: 0.45,  green: 0.34,  blue: 0.27,  alpha: 1.0) // #735745
    static let siamesePoint   = NSColor(calibratedRed: 0.175, green: 0.128, blue: 0.105, alpha: 1.0) // #2D211B
    static let siameseOutline = NSColor(calibratedRed: 0.26,  green: 0.19,  blue: 0.155, alpha: 0.88)
    static let siameseWhisker = NSColor(calibratedRed: 0.36,  green: 0.27,  blue: 0.22,  alpha: 0.68)

    static let permissionPillRect = CGRect(x: 92, y: 294, width: 176, height: 22)

    /// Hit test in 360x320 logical canvas coordinates for smart click-through & petting.
    static func hitTest(point p: CGPoint, physics: CatPhysics, showPermissionPill: Bool, showControls: Bool = false) -> CatHitZone {
        if showPermissionPill && permissionPillRect.insetBy(dx: -4, dy: -4).contains(p) {
            return .permissionPill
        }
        if showControls, let action = PetAction.allCases.first(where: { controlRect($0).contains(p) }) {
            return .control(action)
        }
        if bowlAt(p, physics: physics) { return .bowl }

        let cx = CatPhysics.seatAnchor.x + physics.bodyShiftX.value
        let headCenter = headPosition(seatX: cx, physics: physics)
        let dx = p.x - headCenter.x, dy = p.y - headCenter.y
        let tilt = physics.headTilt.value
        let local = CGPoint(x: dx * cos(tilt) + dy * sin(tilt), y: dy * cos(tilt) - dx * sin(tilt))
        if local.y < -17 {
            for left in [true, false] where earPath(left: left, physics: physics).contains(local) {
                return left ? .leftEar : .rightEar
            }
        }
        let bx = (p.x - cx) / 26, by = (p.y - (CatPhysics.seatAnchor.y - 22)) / 19
        if bx * bx + by * by <= 1 { return .belly }

        // Head hit ellipse (generous for cozy petting)
        let hdx = (p.x - headCenter.x) / 48.0
        let hdy = (p.y - headCenter.y) / 42.0
        if hdx * hdx + hdy * hdy <= 1.0 {
            return .head
        }

        if physics.tabletMode && p.y >= 229 {
            if physics.usingDesk, let slot = physics.tabletKeyIndex(at: p) { return .tabletKey(slot) }
            return .none
        }
        if !physics.tabletMode && !physics.sleeping && CGRect(x: 72, y: 236, width: 228, height: 62).contains(p) {
            return .desk
        }

        let bdx = (p.x - cx) / 58.0
        let bdy = (p.y - 196.0) / 56.0
        if bdx * bdx + bdy * bdy <= 1.0 {
            return .body
        }

        return .none
    }

    private static func headPosition(seatX: CGFloat, physics: CatPhysics) -> CGPoint {
        let profile = sin(physics.turn * .pi)
        let step = abs(sin(physics.turn * 2 * .pi)) * profile
        let breath = sin(physics.time * 2.1) * 1.6
        let x = seatX + physics.bodyTilt.value * 36 + physics.headYaw.value * 7.5 + profile * 21
        let y = 126 + physics.headOffsetY.value + physics.headPitch.value * 4.8 + breath * 1.1
        return CGPoint(x: x, y: y + profile * 1.5 - step * 3)
    }

    /// Main 60fps draw routine in flipped 360x320 logical coordinate space.
    static func draw(in ctx: CGContext, physics: CatPhysics, showPermissionPill: Bool) {
        ctx.saveGState()
        ctx.setAllowsAntialiasing(true)
        ctx.setShouldAntialias(true)

        let settings = CatSettings.shared
        let coat = settings.coatTheme
        let breath = sin(physics.time * 2.1) * 1.6
        let baseSeat = CatPhysics.seatAnchor.x + physics.bodyShiftX.value
        let seatX = baseSeat - sin(physics.turn * .pi) * 11
        let seatY = CatPhysics.seatAnchor.y

        if physics.tabletMode {
            drawTablet(in: ctx, physics: physics, showLegends: settings.showKeyLegends)
        } else {
            drawMouse(in: ctx, physics: physics, theme: settings.keyboardTheme)
            drawKeyboard(in: ctx, physics: physics, theme: settings.keyboardTheme, eyeTheme: settings.eyeTheme, showLegends: settings.showKeyLegends)
        }

        // 3. Cat Tail (behind body)
        drawTail(in: ctx, seatX: seatX, seatY: seatY, physics: physics, coat: coat)

        // 4. 3D-Projected Ears (behind crown silhouette so Siamese seal-point ears emerge cleanly from cream skull!)
        let headCenter = headPosition(seatX: baseSeat, physics: physics)
        drawEarsLayer(in: ctx, center: headCenter, physics: physics, coat: coat)

        // 5. Unified C1-Continuous Head + Neck + Pear Torso Silhouette + Siamese 2.5D Face Mask
        drawUnifiedSilhouette(
            in: ctx,
            seatX: seatX,
            seatY: seatY,
            headCenter: headCenter,
            breath: breath,
            physics: physics,
            coat: coat
        )

        if physics.turn == 0 {
            if physics.hindFootActive.value > 0.03 {
                drawHindFoot(in: ctx, seatX: seatX, seatY: seatY, physics: physics, coat: coat)
            }
            drawHeadFeatures(in: ctx, center: headCenter, physics: physics, eyeTheme: settings.eyeTheme, coat: coat)
            if physics.tabletMode && physics.usingDesk { drawStylus(in: ctx, physics: physics) }
            drawFrontPaws(in: ctx, seatX: seatX, seatY: seatY, breath: breath, physics: physics, coat: coat)
        } else {
            if physics.turn < 0.60 { drawHeadFeatures(in: ctx, center: headCenter, physics: physics, eyeTheme: settings.eyeTheme, coat: coat) }
            drawTurningPaws(in: ctx, seatX: seatX, seatY: seatY, physics: physics, coat: coat)
        }

        if physics.bowlVisible { drawBowl(in: ctx, physics: physics) }

        // 8. Floating Mechanical Keycap Badges (keyboard + mouse clicks)
        if !physics.floatingKeycaps.isEmpty {
            drawFloatingKeycaps(in: ctx, physics: physics)
        }

        // 9. Subtle Petting Sparkles / Sleep Z
        drawSparkles(in: ctx, physics: physics, eyeTheme: settings.eyeTheme)
        drawAngerMark(in: ctx, head: headCenter, progress: physics.angerMarkProgress)

        // 10. Optional non-intrusive Permission Pill if global monitoring not yet granted
        if showPermissionPill {
            drawPermissionPill(in: ctx)
        }

        ctx.restoreGState()
    }

    static let controlsCanvasHeight: CGFloat = 352

    static func controlRect(_ action: PetAction) -> CGRect {
        CGRect(x: 62 + CGFloat(action.rawValue) * 30, y: 322, width: 28, height: 22)
    }

    private static func controlSymbol(_ action: PetAction, mode: DeskMode) -> String {
        switch action {
        case .treat: return ""
        case .sleep: return "zzz"
        case .wake: return "sun.max"
        case .music: return "headphones"
        case .mode:
            switch mode {
            case .keyboard: return "keyboard"
            case .tablet: return "square.and.pencil"
            case .automatic: return "arrow.triangle.2.circlepath"
            }
        case .keyBubbles: return "character.bubble"
        case .mouseBubbles: return "computermouse"
        case .hide: return "chevron.down"
        }
    }

    static func drawControls(in ctx: CGContext, physics: CatPhysics, mode: DeskMode,
                             keyBubbles: Bool, mouseBubbles: Bool, hover: PetAction?, held: PetAction?) {
        let group = CGRect(x: 58, y: 318, width: 246, height: 30)
        fillRounded(in: ctx, group, radius: 9, color: rgb(0x242529, 0.92))
        NSGraphicsContext.saveGraphicsState()
        NSGraphicsContext.current = NSGraphicsContext(cgContext: ctx, flipped: true)
        for action in PetAction.allCases {
            let rect = controlRect(action)
            let active = (action == .music && physics.listening) || action == .mode ||
                (action == .keyBubbles && keyBubbles) || (action == .mouseBubbles && mouseBubbles)
            if action == held || action == hover || active {
                fillRounded(in: ctx, rect, radius: 5, color: rgb(0xF8BA59, action == held ? 0.32 : 0.15))
            }
            let enabled = physics.canPerform(action)
            let color = rgb(active ? 0xF3BD70 : 0xEDECEA, enabled ? 1 : 0.32)
            if action == .treat {
                ctx.saveGState(); ctx.translateBy(x: rect.midX, y: rect.midY); ctx.rotate(by: .pi / 4)
                ctx.setFillColor(color.cgColor)
                ctx.fill(CGRect(x: -1.25, y: -2, width: 2.5, height: 8))
                ctx.fillEllipse(in: CGRect(x: -3.5, y: 4, width: 4, height: 4))
                ctx.fillEllipse(in: CGRect(x: -0.5, y: 4, width: 4, height: 4))
                ctx.setFillColor(rgb(0xD88B52, enabled ? 1 : 0.32).cgColor)
                ctx.fillEllipse(in: CGRect(x: -4.5, y: -8, width: 9, height: 11))
                ctx.restoreGState()
            } else if let image = NSImage(systemSymbolName: controlSymbol(action, mode: mode),
                                          accessibilityDescription: action.title),
                      let symbol = image.withSymbolConfiguration(.init(paletteColors: [color])) {
                let size = symbol.size
                let scale = min(17 / max(size.width, size.height), 1)
                let width = size.width * scale, height = size.height * scale
                symbol.draw(in: CGRect(x: rect.midX - width / 2, y: rect.midY - height / 2, width: width, height: height),
                            from: .zero, operation: .sourceOver, fraction: 1, respectFlipped: true, hints: nil)
            }
        }
        NSGraphicsContext.restoreGraphicsState()
    }

    static func drawPortrait(in ctx: CGContext, coat: CatCoatTheme, eyes: EyeColorTheme) {
        let pose = CatPhysics()
        drawEarsLayer(in: ctx, center: .zero, physics: pose, coat: coat)
        ctx.saveGState()
        ctx.addEllipse(in: CGRect(x: -45, y: -37, width: 90, height: 80)); ctx.clip()
        drawUnifiedSilhouette(in: ctx, seatX: 0, seatY: CatPhysics.seatAnchor.y - 126,
                              headCenter: .zero, breath: 0, physics: pose, coat: coat)
        drawHeadFeatures(in: ctx, center: .zero, physics: pose, eyeTheme: eyes, coat: coat)
        ctx.restoreGState()
    }

    private static func fillRounded(in ctx: CGContext, _ rect: CGRect, radius: CGFloat, color: NSColor) {
        ctx.setFillColor(color.cgColor)
        ctx.addPath(CGPath(roundedRect: rect, cornerWidth: radius, cornerHeight: radius, transform: nil))
        ctx.fillPath()
    }

    private static func bowlOpacity(_ age: CGFloat) -> CGFloat {
        CatPhysics.ease(age / 0.24) * CatPhysics.ease((CatPhysics.bowlEnd - age) / 0.34)
    }

    private static func bowlAt(_ pt: CGPoint, physics: CatPhysics) -> Bool {
        guard physics.bowlVisible && bowlOpacity(physics.treatAge) >= 0.12 else { return false }
        let x = pt.x - 184, y = pt.y - 246
        if y < 0 { return x * x / 484 + y * y / 36 <= 1 }
        return y <= 12 && abs(x) <= 22 - y * 0.36
    }

    private static func drawBowl(in ctx: CGContext, physics: CatPhysics) {
        let opacity = bowlOpacity(physics.treatAge)
        ctx.saveGState(); ctx.translateBy(x: 184, y: 246); ctx.setAlpha(opacity)
        ctx.setFillColor(rgb(0x1A1A1F, 0.32).cgColor); ctx.fillEllipse(in: CGRect(x: -24, y: 7, width: 48, height: 8))
        let ceramic = CGMutablePath()
        ceramic.move(to: CGPoint(x: -22, y: 0)); ceramic.addQuadCurve(to: CGPoint(x: -13, y: 11), control: CGPoint(x: -19, y: 10))
        ceramic.addQuadCurve(to: CGPoint(x: 13, y: 11), control: CGPoint(x: 0, y: 14))
        ceramic.addQuadCurve(to: CGPoint(x: 22, y: 0), control: CGPoint(x: 19, y: 10)); ceramic.closeSubpath()
        ctx.setFillColor(rgb(0xC68453).cgColor); ctx.addPath(ceramic); ctx.fillPath()
        ctx.setFillColor(rgb(0xECD6AF).cgColor); ctx.fillEllipse(in: CGRect(x: -22, y: -6, width: 44, height: 12))
        ctx.setFillColor(rgb(0x624432).cgColor); ctx.fillEllipse(in: CGRect(x: -18, y: -4, width: 36, height: 8))
        for (i, x) in [-12, -7, -3, 2, 8, 12].enumerated() where physics.treatAge < 2.4 - CGFloat(i) * 0.22 {
            ctx.setFillColor(rgb(i % 2 == 0 ? 0xB77942 : 0xD49557).cgColor)
            ctx.fillEllipse(in: CGRect(x: x - 2, y: i % 2 - 2, width: 5, height: 4))
        }
        ctx.restoreGState()
    }

    private static func drawTablet(in ctx: CGContext, physics: CatPhysics, showLegends: Bool) {
        let base = CGRect(x: 116, y: 230, width: 211, height: 62)
        fillRounded(in: ctx, base.offsetBy(dx: 0, dy: 4), radius: 8, color: rgb(0x17181C))
        fillRounded(in: ctx, base, radius: 8, color: rgb(0x34363B))
        let writing = CGRect(x: 133, y: 238, width: 180, height: 49)
        fillRounded(in: ctx, writing, radius: 5, color: rgb(0x24262B))
        ctx.saveGState(); ctx.clip(to: writing)
        ctx.setStrokeColor(rgb(0xCEB48F, 0.78).cgColor)
        ctx.setLineWidth(1.3); ctx.setLineCap(.round); ctx.setLineJoin(.round)
        var starting = true
        for point in physics.ink {
            guard let point else { starting = true; continue }
            if starting { ctx.move(to: point) } else { ctx.addLine(to: point) }
            starting = false
        }
        ctx.strokePath(); ctx.restoreGState()
        fillRounded(in: ctx, CGRect(x: 45, y: 234, width: 65, height: 62), radius: 8, color: rgb(0x17181C))
        fillRounded(in: ctx, CGRect(x: 45, y: 230, width: 65, height: 62), radius: 8, color: rgb(0x34363B))
        NSGraphicsContext.saveGraphicsState()
        NSGraphicsContext.current = NSGraphicsContext(cgContext: ctx, flipped: true)
        for (slot, key) in physics.tabletKeys.enumerated() {
            let rect = CatPhysics.tabletKeyRect(slot)
            let down = physics.tabletHeld.contains(key)
            fillRounded(in: ctx, rect, radius: 3, color: rgb(down ? 0xDAAE62 : 0xE6DED1))
            if showLegends {
                let name = keycapLegend(key)
                let attributes: [NSAttributedString.Key: Any] = [.font: NSFont.systemFont(ofSize: 6.5, weight: .semibold), .foregroundColor: rgb(0x303137)]
                let label = NSAttributedString(string: name, attributes: attributes)
                let size = label.size()
                label.draw(at: CGPoint(x: rect.midX - size.width / 2, y: rect.midY - size.height / 2))
            }
        }
        NSGraphicsContext.restoreGraphicsState()
    }

    private static func drawStylus(in ctx: CGContext, physics: CatPhysics) {
        let tip = physics.tabletTip
        let grip = physics.penGrip
        ctx.saveGState()
        ctx.setLineCap(.round)
        ctx.setStrokeColor(rgb(0x17191D).cgColor); ctx.setLineWidth(4.2)
        ctx.move(to: CGPoint(x: tip.x, y: tip.y - 2))
        ctx.addLine(to: CGPoint(x: grip.x + 8, y: grip.y - 16)); ctx.strokePath()
        ctx.setStrokeColor(rgb(0xAAACAF).cgColor); ctx.setLineWidth(0.9)
        ctx.move(to: CGPoint(x: tip.x - 0.6, y: tip.y - 3))
        ctx.addLine(to: CGPoint(x: grip.x + 7, y: grip.y - 16)); ctx.strokePath()
        ctx.restoreGState()
    }

    // MARK: - Mini White Mouse

    private static func drawMouse(in ctx: CGContext, physics: CatPhysics, theme: KeyboardTheme) {
        let center = CGPoint(
            x: CatPhysics.mouseBaseCenter.x + physics.mouseOffset.point.x,
            y: CatPhysics.mouseBaseCenter.y + physics.mouseOffset.point.y
        )
        let pressShift = max(physics.mousePressLeft.value, physics.mousePressRight.value) * 1.6

        ctx.saveGState()
        ctx.translateBy(x: center.x, y: center.y + pressShift)
        ctx.rotate(by: 0.22) // angled naturally for the cat's right paw (facing the cat!)

        // Soft underside shadow
        ctx.saveGState()
        ctx.setFillColor(NSColor(calibratedWhite: 0.0, alpha: 0.24).cgColor)
        ctx.fillEllipse(in: CGRect(x: -13.5, y: -13.5 + 4.0, width: 27, height: 31))
        ctx.restoreGState()

        let bodyRect = CGRect(x: -13.5, y: -16.5, width: 27, height: 33)
        let bodyColor: NSColor
        let borderColor: NSColor
        let wheelColor: NSColor
        switch theme {
        case .classicMilk:
            bodyColor = NSColor(calibratedRed: 0.955, green: 0.950, blue: 0.925, alpha: 1.0)
            borderColor = NSColor(calibratedRed: 0.14, green: 0.14, blue: 0.16, alpha: 1.0)
            wheelColor = NSColor(calibratedRed: 0.96, green: 0.69, blue: 0.16, alpha: 1.0)
        case .midnightHacker:
            bodyColor = NSColor(calibratedRed: 0.20, green: 0.21, blue: 0.24, alpha: 1.0)
            borderColor = NSColor(calibratedRed: 0.08, green: 0.08, blue: 0.10, alpha: 1.0)
            wheelColor = NSColor(calibratedRed: 0.78, green: 0.95, blue: 0.25, alpha: 1.0)
        case .retroCream:
            bodyColor = NSColor(calibratedRed: 0.89, green: 0.87, blue: 0.82, alpha: 1.0)
            borderColor = NSColor(calibratedRed: 0.20, green: 0.19, blue: 0.18, alpha: 1.0)
            wheelColor = NSColor(calibratedRed: 0.88, green: 0.42, blue: 0.24, alpha: 1.0)
        }

        // Mouse body (palm-rest dome toward cat at top, click buttons & scroll wheel away from cat at bottom!)
        ctx.setFillColor(bodyColor.cgColor)
        ctx.fillEllipse(in: bodyRect)

        // Subtle click flash highlight (cat-facing: left button = inner right side, right button = outer left side!)
        if physics.mousePressLeft.value > 0.05 {
            ctx.saveGState()
            ctx.addEllipse(in: bodyRect)
            ctx.clip()
            ctx.setFillColor(NSColor(calibratedRed: 0.79, green: 0.95, blue: 0.25, alpha: 0.38 * physics.mousePressLeft.value).cgColor)
            ctx.fill(CGRect(x: 0, y: 0, width: 14, height: 17))
            ctx.restoreGState()
        }
        if physics.mousePressRight.value > 0.05 {
            ctx.saveGState()
            ctx.addEllipse(in: bodyRect)
            ctx.clip()
            ctx.setFillColor(NSColor(calibratedRed: 0.79, green: 0.95, blue: 0.25, alpha: 0.38 * physics.mousePressRight.value).cgColor)
            ctx.fill(CGRect(x: -14, y: 0, width: 14, height: 17))
            ctx.restoreGState()
        }

        // Button split seam pointing forward (bottom, away from cat!)
        ctx.setStrokeColor(borderColor.withAlphaComponent(0.32).cgColor)
        ctx.setLineWidth(1.05)
        ctx.move(to: CGPoint(x: 0, y: 16))
        ctx.addLine(to: CGPoint(x: 0, y: 1.5))
        ctx.strokePath()

        // Signature amber scroll wheel on the front (bottom) half facing away from cat
        let wheelRect = CGRect(x: -1.9, y: 2.5, width: 3.8, height: 8.5)
        let wheelPath = CGPath(roundedRect: wheelRect, cornerWidth: 1.9, cornerHeight: 1.9, transform: nil)
        ctx.setFillColor(wheelColor.cgColor)
        ctx.addPath(wheelPath)
        ctx.fillPath()

        // Outer crisp outline
        ctx.setStrokeColor(borderColor.cgColor)
        ctx.setLineWidth(1.7)
        ctx.strokeEllipse(in: bodyRect)

        ctx.restoreGState()
    }

    // MARK: - Mini Angled 65% Mechanical Keyboard (facing the cat!)

    private static func drawKeyboard(
        in ctx: CGContext,
        physics: CatPhysics,
        theme: KeyboardTheme,
        eyeTheme: EyeColorTheme,
        showLegends: Bool
    ) {
        let tl = KeyboardLayout.screenQuadPoint(su: -0.012, sv: -0.04)
        let tr = KeyboardLayout.screenQuadPoint(su: 1.012, sv: -0.04)
        let bl = KeyboardLayout.screenQuadPoint(su: -0.016, sv: 1.04)
        let br = KeyboardLayout.screenQuadPoint(su: 1.016, sv: 1.04)

        let caseDepth: CGFloat = 4.5

        // Soft ambient drop shadow beneath keyboard
        ctx.saveGState()
        let shadowPath = roundedQuadPath(
            tl: CGPoint(x: tl.x, y: tl.y + caseDepth + 2),
            tr: CGPoint(x: tr.x, y: tr.y + caseDepth + 2),
            br: CGPoint(x: br.x, y: br.y + caseDepth + 3),
            bl: CGPoint(x: bl.x, y: bl.y + caseDepth + 3),
            radius: 6
        )
        ctx.setFillColor(NSColor(calibratedWhite: 0, alpha: 0.22).cgColor)
        ctx.addPath(shadowPath)
        ctx.fillPath()
        ctx.restoreGState()

        // Dark 3D chassis lower lip
        let baseColor: NSColor
        let deckColor: NSColor
        let keyColor: NSColor
        let keyBorderColor: NSColor
        let accentColor: NSColor

        switch theme {
        case .classicMilk:
            baseColor = NSColor(calibratedRed: 0.10, green: 0.10, blue: 0.12, alpha: 1.0)
            deckColor = NSColor(calibratedRed: 0.14, green: 0.14, blue: 0.16, alpha: 1.0)
            keyColor = NSColor(calibratedRed: 0.968, green: 0.964, blue: 0.942, alpha: 1.0)
            keyBorderColor = NSColor(calibratedRed: 0.78, green: 0.77, blue: 0.74, alpha: 0.75)
            accentColor = NSColor(calibratedRed: 0.97, green: 0.70, blue: 0.16, alpha: 1.0)
        case .midnightHacker:
            baseColor = NSColor(calibratedRed: 0.06, green: 0.06, blue: 0.08, alpha: 1.0)
            deckColor = NSColor(calibratedRed: 0.10, green: 0.11, blue: 0.13, alpha: 1.0)
            keyColor = NSColor(calibratedRed: 0.21, green: 0.22, blue: 0.25, alpha: 1.0)
            keyBorderColor = NSColor(calibratedRed: 0.11, green: 0.11, blue: 0.13, alpha: 0.95)
            accentColor = NSColor(calibratedRed: 0.77, green: 0.94, blue: 0.25, alpha: 1.0)
        case .retroCream:
            baseColor = NSColor(calibratedRed: 0.25, green: 0.24, blue: 0.22, alpha: 1.0)
            deckColor = NSColor(calibratedRed: 0.38, green: 0.36, blue: 0.33, alpha: 1.0)
            keyColor = NSColor(calibratedRed: 0.90, green: 0.88, blue: 0.82, alpha: 1.0)
            keyBorderColor = NSColor(calibratedRed: 0.68, green: 0.65, blue: 0.60, alpha: 0.85)
            accentColor = NSColor(calibratedRed: 0.88, green: 0.42, blue: 0.24, alpha: 1.0)
        }

        let lowerChassis = roundedQuadPath(
            tl: CGPoint(x: tl.x, y: tl.y + caseDepth),
            tr: CGPoint(x: tr.x, y: tr.y + caseDepth),
            br: CGPoint(x: br.x, y: br.y + caseDepth),
            bl: CGPoint(x: bl.x, y: bl.y + caseDepth),
            radius: 5.5
        )
        ctx.setFillColor(baseColor.cgColor)
        ctx.addPath(lowerChassis)
        ctx.fillPath()

        let topChassis = roundedQuadPath(tl: tl, tr: tr, br: br, bl: bl, radius: 5.0)
        ctx.setFillColor(deckColor.cgColor)
        ctx.addPath(topChassis)
        ctx.fillPath()

        // Individual keycaps — tight clean mechanical spacing matching the reference video
        let padU: CGFloat = 0.0028
        let padV: CGFloat = 0.014
        let glowBaseColor = eyeTheme.glowColor

        for cap in KeyboardLayout.keycaps {
            let glow = physics.keyGlow[cap.key] ?? 0
            // 180° rotated to face the cat: su = 1 - u, sv = 1 - v
            let su0 = (1.0 - (cap.xUnits + cap.widthUnits)) + padU
            let su1 = (1.0 - cap.xUnits) - padU
            let sv0 = (1.0 - CGFloat(cap.row + 1) / 5.0) + padV
            let sv1 = (1.0 - CGFloat(cap.row) / 5.0) - padV

            let pressDy = glow * 1.9
            let kTL = offsetY(KeyboardLayout.screenQuadPoint(su: su0, sv: sv0), pressDy)
            let kTR = offsetY(KeyboardLayout.screenQuadPoint(su: su1, sv: sv0), pressDy)
            let kBR = offsetY(KeyboardLayout.screenQuadPoint(su: su1, sv: sv1), pressDy)
            let kBL = offsetY(KeyboardLayout.screenQuadPoint(su: su0, sv: sv1), pressDy)

            let kPath = roundedQuadPath(tl: kTL, tr: kTR, br: kBR, bl: kBL, radius: 1.6)

            var fill = cap.isAmberAccent ? accentColor : keyColor
            if glow > 0.02 {
                fill = blendColor(fill, glowBaseColor, t: glow * 0.55)
            }

            ctx.setFillColor(fill.cgColor)
            ctx.addPath(kPath)
            ctx.fillPath()

            ctx.setStrokeColor(keyBorderColor.cgColor)
            ctx.setLineWidth(0.65)
            ctx.addPath(kPath)
            ctx.strokePath()

            if showLegends {
                let isSpace = cap.key == .space
                let legend = isSpace ? "space" : keycapLegend(cap.key)
                if !legend.isEmpty {
                    var cx = (kTL.x + kTR.x + kBR.x + kBL.x) * 0.25
                    var cy = (kTL.y + kTR.y + kBR.y + kBL.y) * 0.25
                    if isSpace {
                        // The spacebar sits right under the cat: its legs hide the middle, and an
                        // unlabeled visible end looked like an odd blank patch beside the cat.
                        // Label the end that stays visible instead.
                        let sp = KeyboardLayout.screenQuadPoint(su: su0 + (su1 - su0) * 0.82, sv: (sv0 + sv1) * 0.5)
                        cx = sp.x
                        cy = sp.y + pressDy
                    }
                    let textColor: NSColor
                    if cap.isAmberAccent || theme != .midnightHacker {
                        textColor = NSColor(calibratedRed: 0.14, green: 0.14, blue: 0.16, alpha: 0.78)
                    } else {
                        textColor = NSColor(calibratedRed: 0.88, green: 0.90, blue: 0.92, alpha: 0.82)
                    }
                    let fontSize: CGFloat = legend.count <= 1 ? 5.2 : 4.2
                    let attrs: [NSAttributedString.Key: Any] = [
                        .font: NSFont.systemFont(ofSize: fontSize, weight: .semibold),
                        .foregroundColor: textColor
                    ]
                    let str = NSAttributedString(string: legend, attributes: attrs)
                    let sz = str.size()
                    ctx.saveGState()
                    ctx.translateBy(x: cx, y: cy)
                    // Keyboard faces the cat (180°), so legends read upright from the cat's seat.
                    ctx.rotate(by: .pi + 0.052)
                    NSGraphicsContext.saveGraphicsState()
                    NSGraphicsContext.current = NSGraphicsContext(cgContext: ctx, flipped: true)
                    str.draw(at: CGPoint(x: -sz.width * 0.5, y: -sz.height * 0.5))
                    NSGraphicsContext.restoreGraphicsState()
                    ctx.restoreGState()
                }
            }
        }

        // Signature soft chartreuse backlit glow halos on active keys!
        for (key, glow) in physics.keyGlow where glow > 0.05 {
            let center = KeyboardLayout.canvasPoint(for: key)
            drawRadialGlow(
                in: ctx,
                center: center,
                radius: 17.0 + glow * 6.0,
                color: glowBaseColor.withAlphaComponent(0.36 * glow)
            )
        }
    }

    // MARK: - Tail

    private static func drawTail(in ctx: CGContext, seatX: CGFloat, seatY: CGFloat, physics: CatPhysics, coat: CatCoatTheme) {
        let p = physics.tailPhase
        let hunt = max(0, min(1, physics.huntIntensity.value))
        let wag1 = sin(p) * (8.0 + hunt * 5.5)
        let wag2 = cos(p * 1.3) * (6.0 + hunt * 4.5)
        // Tail drops lower & swishes tautly when stalking/hunting!
        let huntDrop = hunt * 14.0

        // Signature low sweep out to the right, then playful upward curl at the tip!
        let turn = physics.turn
        let direction = cos(turn * .pi)
        let curl = CatPhysics.ease((turn - 0.30) / 0.23) * (1 - CatPhysics.ease((turn - 0.61) / 0.27))
        let tipDirection = cos(min(1, turn + curl * 0.13) * .pi)
        let root = CGPoint(x: seatX + direction * 34, y: seatY - 6)
        var c1 = CGPoint(x: seatX + direction * 64 - curl * 31, y: seatY + 2 + wag1 * 0.35 + huntDrop * 0.4 + curl * 19)
        var c2 = CGPoint(x: seatX + tipDirection * (66 + wag2 * 0.4) - curl * 26, y: seatY - 26 + wag1 * 0.7 + huntDrop + curl * 13)
        var tip = CGPoint(x: seatX + tipDirection * (91 + wag2 * 0.5) - curl * 16, y: seatY - 30 + wag1 * 0.85 + huntDrop + curl * 14)
        let impatient = physics.tailImpatience
        if impatient > 0.001 {
            let beat = physics.tailSlapTime.truncatingRemainder(dividingBy: 0.56) / 0.56
            func lift(_ phase: CGFloat) -> CGFloat {
                CatPhysics.ease(phase / 0.43) * (1 - CatPhysics.ease((phase - 0.43) / 0.19))
            }
            let baseLift = lift(beat), middleLift = lift(beat - 0.08), tipLift = lift(beat - 0.14)
            let rebound = CatPhysics.ease((beat - 0.76) / 0.08) * (1 - CatPhysics.ease((beat - 0.84) / 0.13))
            func blend(_ point: CGPoint, _ x: CGFloat, _ y: CGFloat) -> CGPoint {
                CGPoint(x: point.x + (seatX + direction * x - point.x) * impatient,
                        y: point.y + (seatY + y - point.y) * impatient)
            }
            c1 = blend(c1, 61 - 11 * baseLift, 7 - 26 * baseLift)
            c2 = blend(c2, 77 - 24 * middleLift, 15 - 69 * middleLift)
            tip = blend(tip, 99 - 21 * tipLift - 5 * rebound, 11 - 59 * tipLift - 10 * rebound)
        }

        ctx.saveGState()
        let tailColor = earTailColor(coat)
        ctx.setStrokeColor(tailColor.cgColor)
        ctx.setLineWidth(11.0)
        ctx.setLineCap(.round)
        ctx.setLineJoin(.round)
        ctx.move(to: root)
        ctx.addCurve(to: tip, control1: c1, control2: c2)
        let tailLine = ctx.path
        ctx.strokePath()
        if patterned(coat), let line = tailLine {
            ctx.saveGState()
            ctx.addPath(line.copy(strokingWithWidth: 11, lineCap: .round, lineJoin: .round, miterLimit: 10))
            ctx.clip()
            ctx.setStrokeColor(rgb(coat == .shorthair ? 0x55595A : 0xA57543, 0.50).cgColor)
            ctx.setLineWidth(3.5); ctx.setLineCap(.butt)
            for t: CGFloat in [0.50, 0.78] {
                let u = 1-t
                let p = CGPoint(x: u*u*u*root.x + 3*u*u*t*c1.x + 3*u*t*t*c2.x + t*t*t*tip.x,
                                y: u*u*u*root.y + 3*u*u*t*c1.y + 3*u*t*t*c2.y + t*t*t*tip.y)
                let dx = 3*u*u*(c1.x-root.x)+6*u*t*(c2.x-c1.x)+3*t*t*(tip.x-c2.x)
                let dy = 3*u*u*(c1.y-root.y)+6*u*t*(c2.y-c1.y)+3*t*t*(tip.y-c2.y)
                let length = max(0.001, hypot(dx,dy)), nx = -dy/length*8, ny = dx/length*8
                ctx.move(to: CGPoint(x:p.x+nx,y:p.y+ny)); ctx.addLine(to: CGPoint(x:p.x-nx,y:p.y-ny)); ctx.strokePath()
            }
            ctx.restoreGState()
        }
        ctx.restoreGState()
    }

    private static func drawAngerMark(in ctx: CGContext, head: CGPoint, progress: CGFloat) {
        guard progress < 1 else { return }
        let appear = CatPhysics.ease(progress / 0.18)
        let fade = 1 - CatPhysics.ease((progress - 0.65) / 0.35)
        let size = 0.65 + 0.43 * appear + 0.12 * sin(.pi * appear)
        let mark = CGMutablePath()
        for x: CGFloat in [-1, 1] {
            for y: CGFloat in [-1, 1] {
                mark.move(to: CGPoint(x: x * 3.6, y: y * 9.8))
                mark.addQuadCurve(to: CGPoint(x: x * 9.8, y: y * 3.6), control: CGPoint(x: x * 2, y: y * 2))
            }
        }
        ctx.saveGState()
        ctx.translateBy(x: head.x + 44 + progress * 4, y: head.y - 49 - progress * 13)
        ctx.rotate(by: (14 - progress * 10) * .pi / 180)
        ctx.scaleBy(x: size, y: size)
        ctx.setLineCap(.round)
        ctx.setStrokeColor(rgb(0xFFF1E5, 0.87 * appear * fade).cgColor)
        ctx.setLineWidth(4.7); ctx.addPath(mark); ctx.strokePath()
        ctx.setStrokeColor(rgb(0xCF373C, appear * fade).cgColor)
        ctx.setLineWidth(2.8); ctx.addPath(mark); ctx.strokePath()
        ctx.restoreGState()
    }

    // MARK: - Unified C1-Continuous Cat Silhouette (Crown -> Cheeks -> Plush Neck -> Pear Hips)

    private static func drawUnifiedSilhouette(
        in ctx: CGContext,
        seatX: CGFloat,
        seatY: CGFloat,
        headCenter: CGPoint,
        breath: CGFloat,
        physics: CatPhysics,
        coat: CatCoatTheme
    ) {
        let yaw = physics.headYaw.value
        let pitch = physics.headPitch.value
        let hTilt = physics.headTilt.value
        let bTilt = physics.bodyTilt.value

        let cH = cos(hTilt), sH = sin(hTilt)
        func H(_ lx: CGFloat, _ ly: CGFloat) -> CGPoint {
            CGPoint(
                x: headCenter.x + lx * cH - ly * sH,
                y: headCenter.y + lx * sH + ly * cH
            )
        }

        let cB = cos(bTilt), sB = sin(bTilt)
        let sy = physics.bodySquash.value
        let turn = physics.turn
        let profile = sin(turn * .pi)
        let sx = (1.0 + (1.0 - sy) * 0.55) * (1 - profile * 0.13)
        func B(_ lx: CGFloat, _ ly: CGFloat) -> CGPoint {
            let rx = lx * sx
            let ry = ly * sy
            return CGPoint(
                x: seatX + rx * cB - ry * sB,
                y: seatY + rx * sB + ry * cB
            )
        }

        let cheekShift = yaw * 3.0
        let crownDrop = pitch * 2.0
        let leftCheekX: CGFloat = -42.5 + cheekShift * 0.4 + profile * 6
        let rightCheekX: CGFloat = 42.5 + cheekShift * 0.4 - profile * 10

        // ONE single tangent-continuous closed cubic Bezier path — mathematically impossible to form any ledge or protrusion!
        let crown  = H(cheekShift * 0.2, -30 + crownDrop)
        let rCheek = H(rightCheekX, 5)
        let rHip   = B(49.0 + profile * 3, -14)
        let rBot   = B(24.0, 12)
        let lBot   = B(-24.0, 12)
        let lHip   = B(-49.0, -14 - profile * 3)
        let lCheek = H(leftCheekX, 5)

        let path = CGMutablePath()
        path.move(to: crown)
        path.addCurve(to: rCheek, control1: H(26 + cheekShift * 0.3, -30 + crownDrop), control2: H(rightCheekX + 1.5, -14))
        path.addCurve(to: rHip,   control1: H(rightCheekX - 2.5, 24), control2: B(51.5, -48 + breath * 0.5))
        path.addCurve(to: rBot,   control1: B(47.0, 5), control2: B(36.0, 12))
        path.addLine(to: lBot)
        path.addCurve(to: lHip,   control1: B(-36.0, 12), control2: B(-47.0, 5))
        path.addCurve(to: lCheek, control1: B(-51.5, -48 + breath * 0.5), control2: H(leftCheekX + 2.5, 24))
        path.addCurve(to: crown,  control1: H(leftCheekX - 1.5, -14), control2: H(-26 + cheekShift * 0.3, -30 + crownDrop))
        path.closeSubpath()

        let back = CatPhysics.ease((turn - 0.43) / 0.37)
        ctx.saveGState()
        if back > 0 {
            ctx.addPath(path); ctx.clip()
            let fur = coat == .siamese ? (siameseCream.blended(withFraction: back, of: siameseLatte) ?? siameseLatte) : bodyColor(coat)
            let shoulder = fur.blended(withFraction: 0.14, of: rgb(0xA7A2A0)) ?? fur
            let colors = [shoulder.cgColor, fur.cgColor] as CFArray
            if let gradient = CGGradient(colorsSpace: CGColorSpaceCreateDeviceRGB(), colors: colors, locations: [0, 1]) {
                ctx.drawLinearGradient(gradient, start: crown, end: B(0, 12), options: [.drawsBeforeStartLocation, .drawsAfterEndLocation])
            }
            if coat == .shorthair {
                ctx.setStrokeColor(rgb(0x62605F, 0.56 * back).cgColor)
                ctx.setLineWidth(2.7); ctx.setLineCap(.round)
                for y: CGFloat in [-64, -47, -28] {
                    ctx.move(to: B(-27, y))
                    ctx.addCurve(to: B(27, y), control1: B(-12, y - 7), control2: B(12, y - 7))
                    ctx.strokePath()
                }
            }
        } else if coat != .siamese {
            ctx.setFillColor(bodyColor(coat).cgColor)
            ctx.addPath(path)
            ctx.fillPath()
            if patterned(coat) {
                ctx.saveGState(); ctx.addPath(path); ctx.clip()
                drawCoatMarks(in: ctx, seatX: seatX, seatY: seatY, head: headCenter, physics: physics, coat: coat)
                ctx.restoreGState()
            }
        } else {
            // Siamese warm oat-milk cream gradient + soft toasted back shading + 2.5D rotating seal-point face mask!
            ctx.addPath(path)
            ctx.clip()

            let cs = CGColorSpaceCreateDeviceRGB()
            let bodyColors = [siameseCream.cgColor, siameseLatte.cgColor] as CFArray
            if let grad = CGGradient(colorsSpace: cs, colors: bodyColors, locations: [0.15, 1.0]) {
                ctx.drawLinearGradient(
                    grad,
                    start: CGPoint(x: seatX, y: headCenter.y - 34),
                    end: CGPoint(x: seatX, y: seatY + 18),
                    options: [.drawsBeforeStartLocation, .drawsAfterEndLocation]
                )
            }

            // Soft toasted ear-base blend at top crown so espresso ears melt organically into cream head
            let yawRad = faceYaw(physics)
            let pitchRad = pitch * 0.36
            ctx.saveGState()
            ctx.translateBy(x: headCenter.x, y: headCenter.y)
            ctx.rotate(by: hTilt)

            // 2.5D Rotating Seal-Point "挖煤" Face Mask (moves naturally across cheeks as head turns!)
            let maskCenter = projectHead3D(x: 0, y: 2.5, z: 27.0, yawRad: yawRad, pitchRad: pitchRad).pt
            let maskScaleX = max(0.76, cos(yawRad * 0.7))
            let maskScaleY = max(0.82, cos(pitchRad * 0.7))
            ctx.saveGState()
            ctx.translateBy(x: maskCenter.x, y: maskCenter.y)
            ctx.scaleBy(x: maskScaleX * 1.18, y: maskScaleY * 0.88)
            let maskColors = [
                siamesePoint.cgColor,
                siamesePoint.withAlphaComponent(0.96).cgColor,
                siameseMocha.withAlphaComponent(0.70).cgColor,
                siameseMocha.withAlphaComponent(0.0).cgColor
            ] as CFArray
            if let maskGrad = CGGradient(colorsSpace: cs, colors: maskColors, locations: [0.0, 0.44, 0.74, 1.0]) {
                ctx.drawRadialGradient(
                    maskGrad,
                    startCenter: .zero, startRadius: 2,
                    endCenter: .zero, endRadius: 31,
                    options: []
                )
            }
            ctx.restoreGState()
            ctx.restoreGState()
        }
        ctx.restoreGState()

        if coat == .orcaCat && turn < 0.63 {
            let rotation = turn / 0.63 * .pi / 2
            let width = cos(rotation), side = sin(rotation)
            func chest(_ x: CGFloat, _ y: CGFloat) -> CGPoint { B(x * width + 39 * side, y) }
            func chin(_ x: CGFloat, _ y: CGFloat) -> CGPoint {
                let p = projectHead3D(x: x, y: y, z: 22, yawRad: yaw * 0.5, pitchRad: pitch * 0.36).pt
                return H(p.x * width + 29 * side, p.y)
            }
            let bib = CGMutablePath()
            bib.move(to: chin(-27, 24))
            bib.addCurve(to: chin(0, 17), control1: chin(-24, 17), control2: chin(-16, 17))
            bib.addCurve(to: chin(27, 24), control1: chin(16, 17), control2: chin(24, 17))
            bib.addCurve(to: chest(22, -32), control1: chin(31, 30), control2: chest(30, -47))
            bib.addCurve(to: chest(5, -15), control1: chest(18, -24), control2: chest(10, -19))
            bib.addLine(to: chest(5, -19))
            bib.addCurve(to: chest(-2, -10), control1: chest(2, -14), control2: chest(0, -12))
            bib.addLine(to: chest(-3, -16))
            bib.addCurve(to: chest(-29, -52), control1: chest(-17, -23), control2: chest(-28, -29))
            bib.addCurve(to: chin(-27, 24), control1: chest(-31, -66), control2: chin(-31, 31))
            bib.closeSubpath()
            ctx.saveGState(); ctx.addPath(path); ctx.clip()
            ctx.setFillColor(pawColor(coat).withAlphaComponent(CatPhysics.ease((0.63 - turn) / 0.08)).cgColor)
            ctx.addPath(bib); ctx.fillPath(); ctx.restoreGState()
        }

        // Crisp subtle warm-cocoa outline around Siamese cream body so it pops on light & dark wallpapers!
        if coat == .siamese {
            ctx.saveGState()
            ctx.setStrokeColor(siameseOutline.cgColor)
            ctx.setLineWidth(1.5)
            ctx.addPath(path)
            ctx.strokePath()
            ctx.restoreGState()
        }

        if physics.turn > 0 { return }
        ctx.saveGState()
        ctx.translateBy(x: seatX, y: seatY)
        ctx.rotate(by: bTilt)
        ctx.scaleBy(x: sx, y: sy)
        ctx.setFillColor(pawColor(coat).cgColor)

        ctx.fillEllipse(in: CGRect(x: -35, y: 3, width: 17, height: 11))
        if physics.hindFootActive.value < 0.35 {
            ctx.fillEllipse(in: CGRect(x: 18, y: 3, width: 17, height: 11))
        }

        // Subtle bottom rim highlights on tucked haunches
        let rimColor = (coat == .siamese) ? siameseCream.withAlphaComponent(0.55) : creaseColor(coat).withAlphaComponent(0.58)
        ctx.setStrokeColor(rimColor.cgColor)
        ctx.setLineWidth(1.3)
        ctx.setLineCap(.round)
        ctx.addArc(center: CGPoint(x: -26.5, y: 8.5), radius: 7.5, startAngle: 0.25, endAngle: .pi - 0.25, clockwise: false)
        ctx.strokePath()
        if physics.hindFootActive.value < 0.35 {
            ctx.addArc(center: CGPoint(x: 26.5, y: 8.5), radius: 7.5, startAngle: 0.25, endAngle: .pi - 0.25, clockwise: false)
            ctx.strokePath()
        }
        ctx.restoreGState()
    }

    private static func coatMarkPoint(_ p: CGPoint, headSurface: Bool, skull: Bool, coat: CatCoatTheme, seatX: CGFloat, seatY: CGFloat,
                                      head: CGPoint, physics: CatPhysics) -> CGPoint {
        if headSurface {
            let depth: CGFloat = coat == .orangeWhite
                ? 29.6 * sqrt(max(0, 1 - pow(p.x / 46, 2) - pow(p.y / 40, 2))) : 27.5
            let projected = skull ? p : projectHead3D(x: p.x, y: p.y, z: depth,
                                          yawRad: physics.headYaw.value * 0.50,
                                          pitchRad: physics.headPitch.value * 0.36).pt
            let c = cos(physics.headTilt.value), s = sin(physics.headTilt.value)
            return CGPoint(x: head.x + projected.x*c - projected.y*s,
                           y: head.y + projected.x*s + projected.y*c)
        }
        let sy = physics.bodySquash.value, sx = 1 + (1 - sy) * 0.55
        let c = cos(physics.bodyTilt.value), s = sin(physics.bodyTilt.value)
        return CGPoint(x: seatX + p.x*sx*c - p.y*sy*s,
                       y: seatY + p.x*sx*s + p.y*sy*c)
    }
    private static func drawCoatMarks(in ctx: CGContext, seatX: CGFloat, seatY: CGFloat,
                                      head: CGPoint, physics: CatPhysics, coat: CatCoatTheme) {
        let marks = CoatPatterns.marks(for: coat)
        func markPath(_ mark: CoatMark) -> CGMutablePath {
            let path = CGMutablePath()
            for (i, point) in mark.points.enumerated() {
                let p = coatMarkPoint(point, headSurface: mark.head, skull: mark.skull, coat: coat, seatX: seatX, seatY: seatY, head: head, physics: physics)
                if i == 0 { path.move(to: p) } else { path.addLine(to: p) }
            }
            if mark.width == 0 { path.closeSubpath() }
            return path
        }
        for mark in marks {
            ctx.saveGState()
            let path = markPath(mark)
            let color = rgb(mark.rgb, mark.alpha)
            if mark.width == 0 {
                path.closeSubpath(); ctx.setFillColor(color.cgColor); ctx.addPath(path); ctx.fillPath()
            } else {
                ctx.setStrokeColor(color.cgColor); ctx.setLineWidth(mark.width)
                ctx.setLineCap(.round); ctx.setLineJoin(.round); ctx.addPath(path); ctx.strokePath()
            }
            ctx.restoreGState()
        }
    }

    // MARK: - 3rd Key Hind Foot Easter Egg

    private static func drawHindFoot(in ctx: CGContext, seatX: CGFloat, seatY: CGFloat, physics: CatPhysics, coat: CatCoatTheme) {
        let hip = CGPoint(x: seatX + 26, y: seatY + 2)
        let target = physics.hindFoot.point
        let mid = CGPoint(
            x: (hip.x + target.x) * 0.5 + 10,
            y: (hip.y + target.y) * 0.5 - 8
        )

        drawLimb(
            in: ctx,
            start: hip,
            control: mid,
            end: target,
            startWidth: 15.0,
            endWidth: 12.0,
            outlineOnLeftSide: true,
            outlineAlpha: 0.85 * physics.hindFootActive.value,
            coat: coat
        )
    }

    // MARK: - 2.5D Spherical Head Projection Helper

    /// Projects a 3D point on the cat's head ellipsoid rotated by (yawRad, pitchRad)
    /// into 2D head-local coordinates — creating true Live2D-style 2.5D head turning!
    private static func projectHead3D(
        x: CGFloat, y: CGFloat, z: CGFloat,
        yawRad: CGFloat, pitchRad: CGFloat
    ) -> (pt: CGPoint, z: CGFloat) {
        // 1. Yaw around Y axis (positive yaw = turning right toward viewer's right)
        let cy = cos(yawRad)
        let sy = sin(yawRad)
        let x1 = x * cy + z * sy
        let z1 = -x * sy + z * cy

        // 2. Pitch around X axis (positive pitch = nodding down)
        let cp = cos(pitchRad)
        let sp = sin(pitchRad)
        let y2 = y * cp + z1 * sp
        let z2 = -y * sp + z1 * cp

        return (CGPoint(x: x1, y: y2), z2)
    }

    // MARK: - 2.5D Ears Layer (drawn behind unified silhouette so Siamese seal-point ears emerge organically!)

    private static func drawEarsLayer(
        in ctx: CGContext,
        center: CGPoint,
        physics: CatPhysics,
        coat: CatCoatTheme
    ) {
        ctx.saveGState()
        ctx.translateBy(x: center.x, y: center.y)
        ctx.rotate(by: physics.headTilt.value)

        ctx.setFillColor(earTailColor(coat).cgColor)
        for left in [true, false] {
            ctx.addPath(earPath(left: left, physics: physics))
            ctx.fillPath()
        }

        ctx.restoreGState()
    }

    // MARK: - 2.5D Head Features (Luminous Eyes & Whiskers)

    private static func drawHeadFeatures(
        in ctx: CGContext,
        center: CGPoint,
        physics: CatPhysics,
        eyeTheme: EyeColorTheme,
        coat: CatCoatTheme
    ) {
        ctx.saveGState()
        ctx.translateBy(x: center.x, y: center.y)
        ctx.rotate(by: physics.headTilt.value)

        let yawRad = faceYaw(physics)
        let pitchRad = physics.headPitch.value * 0.36
        if physics.turn > 0 {
            ctx.addEllipse(in: CGRect(x: -40, y: -30, width: 73.5, height: 76)); ctx.clip()
        }
        if coat == .orcaCat && yawRad < 1.33 { drawOrcaMuzzle(in: ctx, yaw: yawRad, pitch: pitchRad) }

        // 1. 3D-Projected Eyes with authentic spherical foreshortening & parallax!
        drawEye3D(in: ctx, isLeft: true, yawRad: yawRad, pitchRad: pitchRad, physics: physics, eyeTheme: eyeTheme, coat: coat)
        drawEye3D(in: ctx, isLeft: false, yawRad: yawRad, pitchRad: pitchRad, physics: physics, eyeTheme: eyeTheme, coat: coat)

        // 2. 3D-Projected Whiskers (outer-side whiskers poke out past silhouette when turning!)
        if yawRad < 1.12 { drawWhiskers3D(in: ctx, yawRad: yawRad, pitchRad: pitchRad, physics: physics, coat: coat) }
        if physics.headphones > 0.015 { drawHeadphones(in: ctx, fit: physics.headphones, eye: eyeTheme) }

        ctx.restoreGState()
    }

    private static func faceYaw(_ physics: CatPhysics) -> CGFloat {
        physics.headYaw.value * 0.5 + physics.turn * 3.42
    }

    private static func drawOrcaMuzzle(in ctx: CGContext, yaw: CGFloat, pitch: CGFloat) {
        func p(_ x: CGFloat, _ y: CGFloat, _ z: CGFloat = 31) -> CGPoint {
            projectHead3D(x: x, y: y, z: z, yawRad: yaw, pitchRad: pitch).pt
        }
        let muzzle = CGMutablePath()
        muzzle.move(to: p(-1, 12))
        muzzle.addCurve(to: p(8, 9.2), control1: p(2, 12), control2: p(3, 9.2))
        muzzle.addCurve(to: p(19, 18.5), control1: p(14, 9.2), control2: p(19, 14))
        muzzle.addCurve(to: p(12, 27), control1: p(19.5, 24), control2: p(17, 27))
        muzzle.addCurve(to: p(0.5, 22), control1: p(7, 27.5), control2: p(3, 25.5))
        muzzle.addCurve(to: p(-10, 26.5), control1: p(-2.5, 24.8), control2: p(-6.5, 26.5))
        muzzle.addCurve(to: p(-18.5, 19), control1: p(-15.5, 27), control2: p(-18.5, 23.5))
        muzzle.addCurve(to: p(-1, 12), control1: p(-18.5, 13.5), control2: p(-10, 11))
        muzzle.closeSubpath()
        ctx.setFillColor(catBlack.cgColor); ctx.addPath(muzzle); ctx.fillPath()

        let nose = CGMutablePath()
        nose.move(to: p(-3.8, 13, 34))
        nose.addCurve(to: p(3.8, 13, 34), control1: p(-3.8, 11.6, 34), control2: p(3.8, 11.6, 34))
        nose.addCurve(to: p(0, 16.3, 35), control1: p(3.8, 14.2, 34), control2: p(1.2, 16.3, 35))
        nose.addCurve(to: p(-3.8, 13, 34), control1: p(-1.2, 16.3, 35), control2: p(-3.8, 14.2, 34))
        nose.closeSubpath()
        ctx.setFillColor(rgb(0x0D0D0F).cgColor); ctx.addPath(nose); ctx.fillPath()
    }

    private static func drawHeadphones(in ctx: CGContext, fit: CGFloat, eye: EyeColorTheme) {
        ctx.saveGState()
        ctx.setAlpha(fit)
        ctx.translateBy(x: 0, y: -(1 - fit) * 14)
        ctx.setLineCap(.round)
        let arch = CGMutablePath()
        arch.move(to: CGPoint(x: -41, y: -2))
        arch.addCurve(to: CGPoint(x: 41, y: -2), control1: CGPoint(x: -42, y: -47), control2: CGPoint(x: 42, y: -47))
        ctx.setStrokeColor(rgb(0x212329).cgColor); ctx.setLineWidth(8)
        ctx.addPath(arch); ctx.strokePath()
        ctx.setStrokeColor(rgb(0x6B707A).cgColor); ctx.setLineWidth(2)
        ctx.addPath(arch); ctx.strokePath()
        for x: CGFloat in [-47, 33] {
            let pad = CGRect(x: x, y: -9, width: 14, height: 26)
            fillRounded(in: ctx, pad, radius: 5, color: rgb(0x33363D))
            fillRounded(in: ctx, pad.insetBy(dx: 4.5, dy: 7), radius: 2, color: eye.primaryColor)
        }
        ctx.restoreGState()
    }

    private static func earPath(left isLeft: Bool, physics: CatPhysics) -> CGPath {
        let yawRad = physics.headYaw.value * 0.5 + sin(physics.turn * .pi) * 1.07
        let pitchRad = physics.headPitch.value * 0.36
        let sign: CGFloat = isLeft ? -1.0 : 1.0
        let foldAngle = sign * (physics.earAngle(left: isLeft) - physics.earTwitch.value * 0.045)

        let tipDx = sin(foldAngle) * 32.0
        let tipDy = -cos(foldAngle) * 32.0

        // Bases anchored comfortably inside the upper skull contour so they merge seamlessly at every yaw/pitch!
        let innerBase3D = projectHead3D(x: sign * 9.0, y: -16.0, z: 0.0, yawRad: yawRad, pitchRad: pitchRad).pt
        let outerBase3D = projectHead3D(x: sign * 35.0, y: -5.0, z: -2.0, yawRad: yawRad, pitchRad: pitchRad).pt
        let tip3D       = projectHead3D(x: sign * 24.5 + tipDx, y: -20.0 + tipDy, z: -3.0, yawRad: yawRad, pitchRad: pitchRad).pt

        // Soft plush rounded-tip feline ear
        let tipLeft  = CGPoint(x: tip3D.x - sign * 3.8, y: tip3D.y + 2.8)
        let tipRight = CGPoint(x: tip3D.x + sign * 3.8, y: tip3D.y + 3.2)

        let ear = CGMutablePath()
        ear.move(to: innerBase3D)
        ear.addQuadCurve(
            to: tipLeft,
            control: CGPoint(x: (innerBase3D.x + tipLeft.x) * 0.5 - sign * 1.5, y: (innerBase3D.y + tipLeft.y) * 0.5 - 2.0)
        )
        ear.addQuadCurve(to: tipRight, control: CGPoint(x: tip3D.x, y: tip3D.y - 2.2))
        ear.addQuadCurve(
            to: outerBase3D,
            control: CGPoint(x: (outerBase3D.x + tipRight.x) * 0.5 + sign * 2.5, y: (outerBase3D.y + tipRight.y) * 0.5 - 2.0)
        )
        ear.closeSubpath()

        return ear
    }

    private static func drawEye3D(
        in ctx: CGContext,
        isLeft: Bool,
        yawRad: CGFloat,
        pitchRad: CGFloat,
        physics: CatPhysics,
        eyeTheme: EyeColorTheme,
        coat: CatCoatTheme
    ) {
        let sign: CGFloat = isLeft ? -1.0 : 1.0
        let eyeColor = eyeTheme.primaryColor

        // Eye sits on the front surface of the 3D head sphere at (±17.5, -2.0, +28.0)
        let proj = projectHead3D(x: sign * 17.2, y: -2.0, z: 27.5, yawRad: yawRad, pitchRad: pitchRad)

        // Authentic spherical foreshortening: the eye turning away toward the edge narrows horizontally!
        let eyeNormalAngle = yawRad + sign * 0.34
        if physics.turn >= 0.57 || cos(eyeNormalAngle) <= 0 { return }
        let foreshortenX = max(physics.turn > 0 ? 0.05 : 0.64, min(1.06, cos(eyeNormalAngle) / cos(0.34)))
        let foreshortenY = max(0.78, min(1.03, cos(pitchRad * 0.85)))

        ctx.saveGState()
        ctx.translateBy(x: proj.pt.x, y: proj.pt.y)
        ctx.scaleBy(x: foreshortenX, y: foreshortenY)

        // Mode 1: Happy closed glowing lime-green curved line eyes (petting head!)
        if physics.isHappyClosedEyes && !physics.angry {
            ctx.setStrokeColor(eyeColor.cgColor)
            ctx.setLineWidth(2.6)
            ctx.setLineCap(.round)
            ctx.move(to: CGPoint(x: -11.5, y: 1.0))
            ctx.addQuadCurve(
                to: CGPoint(x: 11.5, y: 1.0),
                control: CGPoint(x: sign * 1.5, y: 3.4)
            )
            ctx.strokePath()
            ctx.restoreGState()
            return
        }

        // Mode 2: Blissful half-squint glowing lime slits (petting back/body!)
        if physics.isBlissfulSquint && !physics.angry {
            ctx.setFillColor(eyeColor.cgColor)
            let slit = CGMutablePath()
            slit.move(to: CGPoint(x: -11.5, y: -0.5))
            slit.addQuadCurve(to: CGPoint(x: 11.5, y: -0.5), control: CGPoint(x: 0, y: 0.6))
            slit.addQuadCurve(to: CGPoint(x: -11.5, y: -0.5), control: CGPoint(x: 0, y: 4.8))
            slit.closeSubpath()
            ctx.addPath(slit)
            ctx.fillPath()
            ctx.restoreGState()
            return
        }

        let rest = max(physics.sleeping ? physics.sleepiness : 0, max(physics.musicEyes, physics.washLift))
        if rest > 0.68 && !physics.angry {
            ctx.setStrokeColor(eyeColor.withAlphaComponent(0.60 * CatPhysics.ease((rest - 0.68) / 0.26)).cgColor)
            ctx.setLineCap(.round); ctx.setLineWidth(1.55)
            ctx.move(to: CGPoint(x: -9.5, y: 2)); ctx.addQuadCurve(to: CGPoint(x: 9.5, y: 2), control: CGPoint(x: 0, y: 8.4))
            ctx.strokePath()
        }
        let hunt = physics.angry ? 0 : max(0, min(1, physics.huntIntensity.value))

        // Subtle feline eye angle (straightens slightly when wide-eyed hunting!)
        ctx.rotate(by: sign * (0.06 * (1.0 - hunt * 0.6)))

        // Eyes grow rounder & slightly larger in full hunting focus!
        let rx: CGFloat = (coat == .orcaCat ? 10.6 : 12) + hunt * 1.1
        let ry: CGFloat = (coat == .orcaCat ? 10.0 : 10.8) + hunt * 1.8
        let eyeRect = CGRect(x: -rx, y: -ry, width: rx * 2, height: ry * 2)

        // Clip to the visible opening before painting any iris/pupil pixels.
        // Painting a lid over an antialiased oval leaves a colored fringe.
        ctx.saveGState()
        ctx.addEllipse(in: eyeRect)
        ctx.clip()

        let closure = physics.angry ? 0 : max(0, min(1, physics.effectiveEyeClosure))
        if physics.angry {
            ctx.move(to: CGPoint(x: -rx - 2, y: -3 + sign * 6.4))
            ctx.addLine(to: CGPoint(x: rx + 2, y: -3 - sign * 6.4))
            ctx.addLine(to: CGPoint(x: rx + 2, y: ry + 2)); ctx.addLine(to: CGPoint(x: -rx - 2, y: ry + 2))
            ctx.closePath(); ctx.clip()
        }
        if closure >= 1 {
            ctx.restoreGState()
            ctx.restoreGState()
            return
        }
        if closure > 0 {
            ctx.clip(to: CGRect(x: -rx - 2, y: -ry + 2 * ry * closure,
                                width: 2 * rx + 4, height: 2 * ry * (1 - closure)))
        }

        // Luminous chartreuse iris
        ctx.setFillColor(eyeColor.cgColor)
        ctx.fill(eyeRect)

        // Pupil morphs smoothly from vertical almond slit -> HUGE round black boba pupil when hunting!
        let gaze = physics.eyeGaze.point
        let px = gaze.x * (4.2 - hunt * 1.2)
        let py = gaze.y * (3.6 - hunt * 1.0)
        let pScale = physics.pupilScale.value
        let normalW: CGFloat = 5.0 * pScale
        let normalH: CGFloat = 13.5 * min(1.12, pScale)
        let huntDiameter: CGFloat = 18.8 // huge round boba pupil leaving an electric chartreuse ring!
        let pupilW = normalW + (huntDiameter - normalW) * hunt
        let pupilH = normalH + (huntDiameter - normalH) * hunt
        let pupilRect = CGRect(x: px - pupilW * 0.5, y: py - pupilH * 0.5, width: pupilW, height: pupilH)
        // Photo-sampled hazel: #DAB257 outside, a narrow #8D9180 ring by the pupil.
        // Paint inside the existing lid clip and follow the animated pupil position/size.
        if eyeTheme == .referenceHazel {
            for i in 0...24 {
                let t = CGFloat(i) / 24
                let mix = t * t * (3 - 2 * t)
                ctx.setFillColor(NSColor(calibratedRed: (218 - 77 * mix)/255,
                    green: (178 - 33 * mix)/255, blue: (87 + 41 * mix)/255, alpha: 1).cgColor)
                let rim = (1 - t) * 2.2
                ctx.fillEllipse(in: pupilRect.insetBy(dx: -rim, dy: -rim))
            }
        }
        ctx.setFillColor(catBlack.cgColor)
        ctx.fillEllipse(in: pupilRect)

        // Tiny crisp white specular sparkle dot as hunting boba pupils dilate!
        if hunt > 0.25 {
            let glintAlpha = min(1.0, (hunt - 0.25) / 0.55) * 0.92
            ctx.setFillColor(NSColor(calibratedWhite: 1.0, alpha: glintAlpha).cgColor)
            let glintR: CGFloat = 2.1 + hunt * 0.9
            ctx.fillEllipse(in: CGRect(
                x: px - pupilW * 0.22 - glintR * 0.5,
                y: py - pupilH * 0.22 - glintR * 0.5,
                width: glintR * 2,
                height: glintR * 2
            ))
        }

        ctx.restoreGState()
        ctx.restoreGState()
    }

    private static func drawWhiskers3D(
        in ctx: CGContext,
        yawRad: CGFloat,
        pitchRad: CGFloat,
        physics: CatPhysics,
        coat: CatCoatTheme
    ) {
        let wiggle = sin(physics.time * 4.5) * (max(physics.petHeadIntensity, physics.petBodyIntensity) * 1.2)
        ctx.saveGState()
        let wColor = (coat == .siamese) ? siameseWhisker : (patterned(coat) ? rgb(0x8D8170) : whiskerColor).withAlphaComponent(0.68)
        ctx.setStrokeColor(wColor.cgColor)
        ctx.setLineWidth(1.0)
        ctx.setLineCap(.round)

        // 3D cheek whiskers — when turning left, left whiskers naturally poke past silhouette
        // while right whiskers sweep inward across the cheek (exact Xiaohongshu effect!)
        for sign in [-1.0, 1.0] as [CGFloat] {
            let rows: [(CGFloat, CGFloat, CGFloat)] = [
                (10.5, 36.5, 8.8 + wiggle),
                (14.2, 37.8, 14.2 + wiggle * 0.5),
                (18.0, 35.5, 19.6 - wiggle * 0.4)
            ]
            for (y0, xEnd, yEnd) in rows {
                let p0 = projectHead3D(x: sign * 24.0, y: y0, z: 23.0, yawRad: yawRad, pitchRad: pitchRad).pt
                let p2 = projectHead3D(x: sign * xEnd, y: yEnd, z: 14.0, yawRad: yawRad, pitchRad: pitchRad).pt
                let ctrl = CGPoint(x: (p0.x + p2.x) * 0.5, y: (p0.y + p2.y) * 0.5 - 0.8)
                ctx.move(to: p0)
                ctx.addQuadCurve(to: p2, control: ctrl)
            }
        }
        ctx.strokePath()
        ctx.restoreGState()
    }

    // MARK: - Front Paws (Idle Sitting & Dynamic Reaching with Signature Outlines / Siamese Socks)

    private static func drawFrontPaws(
        in ctx: CGContext,
        seatX: CGFloat,
        seatY: CGFloat,
        breath: CGFloat,
        physics: CatPhysics,
        coat: CatCoatTheme
    ) {
        let lPos = physics.leftPaw.point
        let rPos = physics.rightPaw.point

        let lDist = hypot(lPos.x - CatPhysics.leftPawRest.x, lPos.y - CatPhysics.leftPawRest.y)
        let rDist = hypot(rPos.x - CatPhysics.rightPawRest.x, rPos.y - CatPhysics.rightPawRest.y)

        // Attach to the transformed chest, below the moving face rather than its white blaze.
        func shoulder(_ x: CGFloat) -> CGPoint {
            let sy = physics.bodySquash.value, sx = 1 + (1 - sy) * 0.55
            let c = cos(physics.bodyTilt.value), s = sin(physics.bodyTilt.value)
            let faceBottom = 126 + physics.headOffsetY.value + physics.headPitch.value * 4.8 + breath * 1.1 + (physics.usingDesk ? 52 : 33)
            return CGPoint(x: seatX + x*sx*c + 43*sy*s,
                           y: max(seatY + x*sx*s - 43*sy*c, faceBottom))
        }
        let leftShoulder = shoulder(-20)
        let rightShoulder = shoulder(20)

        // Left arm
        if physics.usingDesk && lDist < 6.0 && !physics.leftPawOnMouse && !physics.leftPawTyping {
            drawIdleFrontLeg(in: ctx, top: CGPoint(x: seatX - 18, y: seatY - 30), bottom: CGPoint(x: seatX - 16, y: seatY + 12), isLeft: true, coat: coat)
        } else {
            let mid = CGPoint(
                x: (leftShoulder.x + lPos.x) * 0.5 - (physics.leftPawOnMouse ? 8 : 4),
                y: (leftShoulder.y + lPos.y) * 0.5 - 6
            )
            drawLimb(
                in: ctx,
                start: leftShoulder,
                control: mid,
                end: lPos,
                startWidth: 19.0,
                endWidth: 13.5,
                outlineOnLeftSide: false,
                outlineAlpha: 0.78,
                coat: coat,
                chestFur: physics.feeding ? 0.14 : 1 - physics.sleepiness * 0.8
            )
        }

        // Right arm
        if physics.usingDesk && !physics.tabletMode && rDist < 6.0 && !physics.rightPawTyping {
            drawIdleFrontLeg(in: ctx, top: CGPoint(x: seatX + 18, y: seatY - 30), bottom: CGPoint(x: seatX + 16, y: seatY + 12), isLeft: false, coat: coat)
        } else {
            let mid = CGPoint(
                x: (rightShoulder.x + rPos.x) * 0.5 + 4 + 31 * physics.washLift,
                y: (rightShoulder.y + rPos.y) * 0.5 - 6 + 10 * physics.washLift
            )
            drawLimb(
                in: ctx,
                start: rightShoulder,
                control: mid,
                end: rPos,
                startWidth: 19.0,
                endWidth: 13.5,
                outlineOnLeftSide: true,
                outlineAlpha: 0.78,
                coat: coat,
                chestFur: physics.feeding ? 0.14 : 1 - physics.sleepiness * 0.8
            )
        }
    }

    private static func drawTurningPaws(in ctx: CGContext, seatX: CGFloat, seatY: CGFloat, physics: CatPhysics, coat: CatCoatTheme) {
        let turn = physics.turn
        guard turn < 0.61 else { return }
        let arc = sin(turn * .pi), step = abs(sin(turn * 2 * .pi)) * arc
        let weight = CatPhysics.ease(turn / 0.22)
        let nearVisibility = 1 - CatPhysics.ease((turn - 0.43) / 0.18)
        let farVisibility = 1 - CatPhysics.ease((turn - 0.24) / 0.23)
        let left = physics.leftPaw.point, right = physics.rightPaw.point
        let near = CGPoint(x: left.x + (seatX - 22 + arc * 35 - left.x) * weight,
                           y: left.y + (seatY + 9 - step * 8 - (1 - nearVisibility) * 6 - left.y) * weight)
        let far = CGPoint(x: right.x + (seatX + 26 + arc * 14 - right.x) * weight,
                          y: right.y + (seatY + 7 - (arc - step) * 8 - (1 - farVisibility) * 5 - right.y) * weight)
        let shoulder = CGPoint(x: seatX - 14 + arc * 48, y: seatY - 38)
        if nearVisibility > 0.02 {
            let mid = CGPoint(x: (shoulder.x + near.x) * 0.5 - 3, y: (shoulder.y + near.y) * 0.5)
            let chestFur = 0.65 * nearVisibility * (1 - CatPhysics.ease((turn - 0.28) / 0.16))
            drawLimb(in: ctx, start: shoulder, control: mid, end: near, startWidth: 16 * nearVisibility,
                     endWidth: 12 * nearVisibility, outlineOnLeftSide: false, outlineAlpha: 0.6 * nearVisibility,
                     coat: coat, chestFur: chestFur)
        }
        ctx.setFillColor(pawColor(coat).cgColor)
        ctx.fillEllipse(in: CGRect(x: near.x - 8 * nearVisibility, y: near.y - 4 * nearVisibility,
                                  width: 16 * nearVisibility, height: 9 * nearVisibility))
        if farVisibility > 0 {
            ctx.fillEllipse(in: CGRect(x: far.x - 7 * farVisibility, y: far.y - 4 * farVisibility,
                                      width: 14 * farVisibility, height: 9 * farVisibility))
        }
    }

    /// Signature tucked sitting front legs (black with mint lines OR Siamese cream-to-chocolate socks!)
    private static func drawIdleFrontLeg(in ctx: CGContext, top: CGPoint, bottom: CGPoint, isLeft: Bool, coat: CatCoatTheme) {
        let sign: CGFloat = isLeft ? -1.0 : 1.0
        ctx.saveGState()

        if coat == .siamese {
            // Creamy upper leg dipping smoothly into adorable seal-point chocolate socks at the bottom!
            let legRect = CGRect(x: bottom.x - 7.5, y: top.y, width: 15.0, height: bottom.y - top.y + 1.0)
            let legPath = CGPath(roundedRect: legRect, cornerWidth: 6.5, cornerHeight: 6.5, transform: nil)
            ctx.saveGState()
            ctx.addPath(legPath)
            ctx.clip()
            let cs = CGColorSpaceCreateDeviceRGB()
            let colors = [
                siameseCream.withAlphaComponent(0.0).cgColor,
                siameseMocha.withAlphaComponent(0.75).cgColor,
                siamesePoint.cgColor
            ] as CFArray
            if let grad = CGGradient(colorsSpace: cs, colors: colors, locations: [0.05, 0.52, 0.85]) {
                ctx.drawLinearGradient(
                    grad,
                    start: CGPoint(x: top.x, y: top.y),
                    end: CGPoint(x: bottom.x, y: bottom.y + 2),
                    options: []
                )
            }
            ctx.restoreGState()
        }

        if coat == .orcaCat {
            let leg = CGRect(x: bottom.x - 7.5, y: top.y, width: 15, height: bottom.y - top.y)
            let path = CGPath(roundedRect: leg, cornerWidth: 7.5, cornerHeight: 7.5, transform: nil)
            drawOrcaShoulder(in: ctx, path: path, start: top, end: bottom, strength: 1)
            fillRounded(in: ctx, CGRect(x: bottom.x - 7.5, y: bottom.y - 9, width: 15, height: 14), radius: 5, color: pawColor(coat))
        } else if patterned(coat) {
            ctx.setFillColor(pawColor(coat).cgColor)
            let cuff = CGRect(x: bottom.x - 7.5, y: bottom.y - 13, width: 15, height: 18)
            ctx.addPath(CGPath(roundedRect: cuff, cornerWidth: 6.5, cornerHeight: 6.5, transform: nil))
            ctx.fillPath()
        }
        let lineColor = (coat == .siamese) ? siameseOutline.withAlphaComponent(0.62) : creaseColor(coat)
        ctx.setStrokeColor(lineColor.cgColor)
        ctx.setLineWidth(1.45)
        ctx.setLineCap(.round)

        // Outer separation crease
        ctx.move(to: CGPoint(x: top.x + sign * 8.5, y: top.y))
        ctx.addQuadCurve(
            to: CGPoint(x: bottom.x + sign * 7.5, y: bottom.y - 1),
            control: CGPoint(x: top.x + sign * 9.5, y: (top.y + bottom.y) * 0.5)
        )
        // Inner separation crease
        ctx.move(to: CGPoint(x: top.x - sign * 7.0, y: top.y + 4))
        ctx.addQuadCurve(
            to: CGPoint(x: bottom.x - sign * 6.0, y: bottom.y - 1),
            control: CGPoint(x: top.x - sign * 6.5, y: (top.y + bottom.y) * 0.5)
        )
        ctx.strokePath()
        ctx.restoreGState()
    }

    /// Smooth tapered limb + plump rounded paw (matte black OR Siamese cream-to-espresso chocolate socks!)
    private static func drawLimb(
        in ctx: CGContext,
        start: CGPoint,
        control: CGPoint,
        end: CGPoint,
        startWidth: CGFloat,
        endWidth: CGFloat,
        outlineOnLeftSide: Bool,
        outlineAlpha: CGFloat,
        coat: CatCoatTheme,
        chestFur: CGFloat = 1
    ) {
        let steps = 14
        var leftEdge: [CGPoint] = []
        var rightEdge: [CGPoint] = []

        for i in 0...steps {
            let t = CGFloat(i) / CGFloat(steps)
            let p = quadPoint(start, control, end, t: t)
            let tan = quadTangent(start, control, end, t: t)
            let len = max(0.001, hypot(tan.x, tan.y))
            let nx = -tan.y / len
            let ny = tan.x / len
            let halfW = (startWidth + (endWidth - startWidth) * t) * 0.5
            leftEdge.append(CGPoint(x: p.x + nx * halfW, y: p.y + ny * halfW))
            rightEdge.append(CGPoint(x: p.x - nx * halfW, y: p.y - ny * halfW))
        }

        let limbPath = CGMutablePath()
        limbPath.move(to: leftEdge[0])
        for pt in leftEdge.dropFirst() { limbPath.addLine(to: pt) }
        for pt in rightEdge.reversed() { limbPath.addLine(to: pt) }
        let tangent = quadTangent(start, control, end, t: 0)
        let length = max(0.001, hypot(tangent.x, tangent.y))
        limbPath.addQuadCurve(to: leftEdge[0], control: CGPoint(
            x: start.x - tangent.x/length*startWidth*0.5,
            y: start.y - tangent.y/length*startWidth*0.5))
        limbPath.closeSubpath()

        let pawR = endWidth * 0.58
        let pawRect = CGRect(x: end.x - pawR, y: end.y - pawR * 0.85, width: pawR * 2, height: pawR * 1.7)

        ctx.saveGState()
        if coat == .orcaCat {
            drawOrcaShoulder(in: ctx, path: limbPath, start: start, end: end, strength: chestFur)
            let cuff = CGMutablePath()
            let first = 11
            cuff.move(to: leftEdge[first])
            for pt in leftEdge.dropFirst(first + 1) { cuff.addLine(to: pt) }
            for pt in rightEdge.dropFirst(first).reversed() { cuff.addLine(to: pt) }
            cuff.closeSubpath()
            ctx.setFillColor(pawColor(coat).cgColor); ctx.addPath(cuff); ctx.fillPath()
            ctx.fillEllipse(in: pawRect)
        } else if coat != .siamese {
            ctx.setFillColor(limbColor(coat).cgColor)
            ctx.addPath(limbPath)
            if patterned(coat) {
                ctx.saveGState(); ctx.clip()
                let colors = [pawColor(coat).cgColor, limbColor(coat).cgColor] as CFArray
                if let gradient = CGGradient(colorsSpace: CGColorSpaceCreateDeviceRGB(), colors: colors, locations: [0, 0.30]) {
                    ctx.drawLinearGradient(gradient, start: start, end: end,
                                           options: [.drawsBeforeStartLocation, .drawsAfterEndLocation])
                }
                ctx.restoreGState()
            } else { ctx.fillPath() }
            if !patterned(coat) { ctx.fillEllipse(in: pawRect) }
            if patterned(coat) {
                let cuff = CGMutablePath()
                let first = Int(CGFloat(steps) * 0.72)
                cuff.move(to: leftEdge[first])
                for pt in leftEdge.dropFirst(first + 1) { cuff.addLine(to: pt) }
                for pt in rightEdge.dropFirst(first).reversed() { cuff.addLine(to: pt) }
                cuff.closeSubpath()
                ctx.setFillColor(pawColor(coat).cgColor); ctx.addPath(cuff); ctx.fillPath()
                ctx.fillEllipse(in: pawRect)
            }
        } else {
            // Siamese chocolate-dipped socks gradient along the limb!
            // Starts transparent at shoulder (0.0) so it melts 100% seamlessly into the creamy chest!
            ctx.saveGState()
            ctx.addPath(limbPath)
            ctx.clip()
            let cs = CGColorSpaceCreateDeviceRGB()
            let colors = [
                siameseCream.withAlphaComponent(0.0).cgColor,
                siameseCream.cgColor,
                siameseMocha.cgColor,
                siamesePoint.cgColor
            ] as CFArray
            if let grad = CGGradient(colorsSpace: cs, colors: colors, locations: [0.0, 0.16, 0.48, 0.78]) {
                ctx.drawLinearGradient(grad, start: start, end: end, options: [.drawsAfterEndLocation])
            }
            ctx.restoreGState()

            // Solid rich espresso-chocolate rounded paw tip (no winding-rule overlap artifacts!)
            ctx.setFillColor(siamesePoint.cgColor)
            ctx.fillEllipse(in: pawRect)

            // Clean outer edge outline on Siamese arm (fading in below shoulder) so it reads crisply over light backgrounds
            ctx.setStrokeColor(siameseOutline.withAlphaComponent(0.72).cgColor)
            ctx.setLineWidth(1.25)
            ctx.setLineCap(.round)
            ctx.setLineJoin(.round)
            let outerEdge = outlineOnLeftSide ? rightEdge : leftEdge
            if outerEdge.count > 4 {
                ctx.move(to: outerEdge[3])
                for idx in 4..<(outerEdge.count - 1) { ctx.addLine(to: outerEdge[idx]) }
                ctx.strokePath()
            }
        }

        // Crisp separation stroke along upper-to-mid arm where it crosses the torso
        let highlightEdge = outlineOnLeftSide ? leftEdge : rightEdge
        if highlightEdge.count > 6 {
            let strokeColor = (coat == .siamese)
                ? siameseOutline.withAlphaComponent(outlineAlpha * 0.78)
                : creaseColor(coat).withAlphaComponent(outlineAlpha * 0.9)
            ctx.setStrokeColor(strokeColor.cgColor)
            ctx.setLineWidth(1.35)
            ctx.setLineCap(.round)
            ctx.setLineJoin(.round)
            let startIdx = 2
            let endIdx = Int(CGFloat(highlightEdge.count - 1) * 0.68)
            ctx.move(to: highlightEdge[startIdx])
            for idx in (startIdx + 1)...endIdx {
                ctx.addLine(to: highlightEdge[idx])
            }
            ctx.strokePath()
        }
        ctx.restoreGState()
    }

    private static func drawOrcaShoulder(in ctx: CGContext, path: CGPath, start: CGPoint, end: CGPoint, strength: CGFloat) {
        let length = max(1, hypot(end.x - start.x, end.y - start.y))
        let dx = (end.x - start.x) / length, dy = (end.y - start.y) / length
        let pad: CGFloat = 16
        let span = length + pad * 2
        let fade = min(34, length * 0.52)
        let fur = pawColor(.orcaCat)
        let shoulder = catBlack.blended(withFraction: strength, of: fur) ?? fur
        let gray = catBlack.blended(withFraction: strength, of: rgb(0x939297)) ?? catBlack
        let dark = catBlack.blended(withFraction: strength, of: rgb(0x38373B)) ?? catBlack
        let clear = shoulder.withAlphaComponent(0)
        let colors = [clear, clear, shoulder, gray, dark, catBlack, catBlack].map { $0.cgColor } as CFArray
        let stops: [CGFloat] = [0, (pad - 3) / span, (pad + fade * 0.22) / span, (pad + fade * 0.5) / span,
                               (pad + fade * 0.78) / span, (pad + fade) / span, 1]
        guard let gradient = CGGradient(colorsSpace: CGColorSpaceCreateDeviceRGB(), colors: colors, locations: stops) else { return }
        ctx.saveGState(); ctx.addPath(path); ctx.clip()
        ctx.drawLinearGradient(gradient, start: CGPoint(x: start.x - dx * pad, y: start.y - dy * pad),
                               end: CGPoint(x: end.x + dx * pad, y: end.y + dy * pad), options: [])
        ctx.restoreGState()
    }

    // MARK: - Floating Mechanical Keycap Popups

    private static func drawFloatingKeycaps(in ctx: CGContext, physics: CatPhysics) {
        for badge in physics.floatingKeycaps {
            let progress = badge.age / badge.lifetime
            // Spring pop-in scale (starts crisp and readable immediately!)
            let scale: CGFloat
            if badge.age < 0.09 {
                let t = badge.age / 0.09
                scale = 0.72 + 0.38 * sin(t * .pi * 0.65)
            } else if badge.age < 0.18 {
                let t = (badge.age - 0.09) / 0.09
                scale = 1.10 - 0.10 * t
            } else {
                scale = 1.0
            }

            let alpha: CGFloat = progress < 0.62 ? 1.0 : max(0, 1.0 - (progress - 0.62) / 0.38)

            let charCount = badge.label.count
            let fontSize: CGFloat = charCount <= 1 ? 11.5 : 10.0
            let font = NSFont.systemFont(ofSize: fontSize, weight: .bold)
            let attrs: [NSAttributedString.Key: Any] = [
                .font: font,
                .foregroundColor: NSColor(calibratedRed: 0.12, green: 0.12, blue: 0.14, alpha: 1.0)
            ]
            let attrStr = NSAttributedString(string: badge.label, attributes: attrs)
            let strSize = attrStr.size()
            let w: CGFloat = max(23, ceil(strSize.width) + 12)
            let h: CGFloat = 22

            ctx.saveGState()
            ctx.setAlpha(alpha)
            ctx.translateBy(x: badge.position.x, y: badge.position.y)
            ctx.rotate(by: badge.rotation)
            ctx.scaleBy(x: scale, y: scale)

            // 3D bottom keycap edge / shadow
            let baseRect = CGRect(x: -w * 0.5, y: -h * 0.5 + 2.5, width: w, height: h)
            let basePath = CGPath(roundedRect: baseRect, cornerWidth: 5.5, cornerHeight: 5.5, transform: nil)
            ctx.setFillColor(NSColor(calibratedRed: 0.15, green: 0.15, blue: 0.17, alpha: 0.95).cgColor)
            ctx.addPath(basePath)
            ctx.fillPath()

            // Crisp white mechanical keycap top face
            let faceRect = CGRect(x: -w * 0.5, y: -h * 0.5, width: w, height: h)
            let facePath = CGPath(roundedRect: faceRect, cornerWidth: 5.2, cornerHeight: 5.2, transform: nil)
            ctx.setFillColor(NSColor(calibratedRed: 0.975, green: 0.972, blue: 0.952, alpha: 1.0).cgColor)
            ctx.addPath(facePath)
            ctx.fillPath()

            ctx.setStrokeColor(NSColor(calibratedRed: 0.16, green: 0.16, blue: 0.18, alpha: 1.0).cgColor)
            ctx.setLineWidth(1.45)
            ctx.addPath(facePath)
            ctx.strokePath()

            // Centered bold key label
            let drawPt = CGPoint(x: -strSize.width * 0.5, y: -strSize.height * 0.5 - 0.5)
            NSGraphicsContext.saveGraphicsState()
            let nsCtx = NSGraphicsContext(cgContext: ctx, flipped: true)
            NSGraphicsContext.current = nsCtx
            attrStr.draw(at: drawPt)
            NSGraphicsContext.restoreGraphicsState()

            ctx.restoreGState()
        }
    }

    // MARK: - Subtle Petting Sparkles & Sleep Z

    private static func drawSparkles(in ctx: CGContext, physics: CatPhysics, eyeTheme: EyeColorTheme) {
        for sp in physics.sparkles {
            let p = sp.age / sp.lifetime
            let alpha = sin(min(1.0, p) * .pi)
            ctx.saveGState()
            ctx.setAlpha(alpha * 0.85)
            if sp.isSleepZ {
                let attrs: [NSAttributedString.Key: Any] = [
                    .font: NSFont.systemFont(ofSize: 11 + p * 3, weight: .bold),
                    .foregroundColor: eyeTheme.primaryColor
                ]
                NSGraphicsContext.saveGraphicsState()
                NSGraphicsContext.current = NSGraphicsContext(cgContext: ctx, flipped: true)
                NSAttributedString(string: "z", attributes: attrs).draw(at: sp.position)
                NSGraphicsContext.restoreGraphicsState()
            } else {
                // Tiny glowing chartreuse 4-point star
                let r: CGFloat = 3.5 + p * 1.5
                ctx.translateBy(x: sp.position.x, y: sp.position.y)
                ctx.setFillColor(eyeTheme.primaryColor.cgColor)
                let star = CGMutablePath()
                star.move(to: CGPoint(x: 0, y: -r))
                star.addQuadCurve(to: CGPoint(x: r, y: 0), control: CGPoint(x: 0.8, y: -0.8))
                star.addQuadCurve(to: CGPoint(x: 0, y: r), control: CGPoint(x: 0.8, y: 0.8))
                star.addQuadCurve(to: CGPoint(x: -r, y: 0), control: CGPoint(x: -0.8, y: 0.8))
                star.addQuadCurve(to: CGPoint(x: 0, y: -r), control: CGPoint(x: -0.8, y: -0.8))
                star.closeSubpath()
                ctx.addPath(star)
                ctx.fillPath()
            }
            ctx.restoreGState()
        }
    }

    // MARK: - Non-Intrusive Permission Pill

    private static func drawPermissionPill(in ctx: CGContext) {
        let r = permissionPillRect
        let path = CGPath(roundedRect: r, cornerWidth: 11, cornerHeight: 11, transform: nil)

        ctx.saveGState()
        ctx.setFillColor(NSColor(calibratedRed: 0.11, green: 0.11, blue: 0.13, alpha: 0.90).cgColor)
        ctx.addPath(path)
        ctx.fillPath()

        ctx.setStrokeColor(NSColor(calibratedRed: 0.79, green: 0.95, blue: 0.25, alpha: 0.90).cgColor)
        ctx.setLineWidth(1.2)
        ctx.addPath(path)
        ctx.strokePath()

        let text = "点击开启全局/游戏键盘监听 🔑"
        let attrs: [NSAttributedString.Key: Any] = [
            .font: NSFont.systemFont(ofSize: 10.5, weight: .semibold),
            .foregroundColor: NSColor(calibratedRed: 0.95, green: 0.97, blue: 0.90, alpha: 1.0)
        ]
        let str = NSAttributedString(string: text, attributes: attrs)
        let sz = str.size()
        NSGraphicsContext.saveGraphicsState()
        NSGraphicsContext.current = NSGraphicsContext(cgContext: ctx, flipped: true)
        str.draw(at: CGPoint(x: r.midX - sz.width * 0.5, y: r.midY - sz.height * 0.5 - 0.5))
        NSGraphicsContext.restoreGraphicsState()

        ctx.restoreGState()
    }

    // MARK: - Geometry & Color Helpers

    private static func keycapLegend(_ key: PhysicalKey) -> String {
        switch key {
        case .escape: return "ESC"
        case .tab: return "⇥"
        case .capsLock: return "⇪"
        case .leftShift, .rightShift: return "⇧"
        case .leftControl, .rightControl: return "⌃"
        case .leftOption, .rightOption: return "⌥"
        case .leftCommand, .rightCommand: return "⌘"
        case .backspace: return "⌫"
        case .enter: return "↵"
        case .space: return ""
        default: return key.badgeLabel
        }
    }

    private static func offsetY(_ pt: CGPoint, _ dy: CGFloat) -> CGPoint {
        CGPoint(x: pt.x, y: pt.y + dy)
    }

    private static func quadPoint(_ p0: CGPoint, _ p1: CGPoint, _ p2: CGPoint, t: CGFloat) -> CGPoint {
        let mt = 1 - t
        return CGPoint(
            x: mt * mt * p0.x + 2 * mt * t * p1.x + t * t * p2.x,
            y: mt * mt * p0.y + 2 * mt * t * p1.y + t * t * p2.y
        )
    }

    private static func quadTangent(_ p0: CGPoint, _ p1: CGPoint, _ p2: CGPoint, t: CGFloat) -> CGPoint {
        let mt = 1 - t
        return CGPoint(
            x: 2 * mt * (p1.x - p0.x) + 2 * t * (p2.x - p1.x),
            y: 2 * mt * (p1.y - p0.y) + 2 * t * (p2.y - p1.y)
        )
    }

    private static func roundedQuadPath(
        tl: CGPoint, tr: CGPoint, br: CGPoint, bl: CGPoint, radius: CGFloat
    ) -> CGPath {
        let p = CGMutablePath()
        let midTop = CGPoint(x: (tl.x + tr.x) * 0.5, y: (tl.y + tr.y) * 0.5)
        p.move(to: midTop)
        p.addArc(tangent1End: tr, tangent2End: br, radius: radius)
        p.addArc(tangent1End: br, tangent2End: bl, radius: radius)
        p.addArc(tangent1End: bl, tangent2End: tl, radius: radius)
        p.addArc(tangent1End: tl, tangent2End: tr, radius: radius)
        p.closeSubpath()
        return p
    }

    private static func blendColor(_ a: NSColor, _ b: NSColor, t: CGFloat) -> NSColor {
        let ca = a.usingColorSpace(.deviceRGB) ?? a
        let cb = b.usingColorSpace(.deviceRGB) ?? b
        let clamped = max(0, min(1, t))
        return NSColor(
            calibratedRed: ca.redComponent + (cb.redComponent - ca.redComponent) * clamped,
            green: ca.greenComponent + (cb.greenComponent - ca.greenComponent) * clamped,
            blue: ca.blueComponent + (cb.blueComponent - ca.blueComponent) * clamped,
            alpha: ca.alphaComponent + (cb.alphaComponent - ca.alphaComponent) * clamped
        )
    }

    private static func drawRadialGlow(in ctx: CGContext, center: CGPoint, radius: CGFloat, color: NSColor) {
        let cs = CGColorSpaceCreateDeviceRGB()
        let colors = [color.cgColor, color.withAlphaComponent(0.0).cgColor] as CFArray
        let locs: [CGFloat] = [0.0, 1.0]
        if let grad = CGGradient(colorsSpace: cs, colors: colors, locations: locs) {
            ctx.saveGState()
            ctx.drawRadialGradient(
                grad,
                startCenter: center, startRadius: 0,
                endCenter: center, endRadius: radius,
                options: []
            )
            ctx.restoreGState()
        }
    }
}
