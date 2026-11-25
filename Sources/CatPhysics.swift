import Foundation
import CoreGraphics
import AppKit

// MARK: - Themes & Settings

enum KeyboardTheme: String, CaseIterable {
    case classicMilk = "经典奶白暖黄 (原版)"
    case midnightHacker = "暗夜极客黑"
    case retroCream = "复古奶油灰"
    case dopeyBlue = "呆猫蓝紫"
}

enum CatCoatTheme: String, CaseIterable {
    case classicBlack = "经典小黑猫 (原版)"
    case siamese = "挖煤暹罗猫"
    case shorthair = "美短（银虎斑加白）"
    case orangeWhite = "橘白猫"
    case orcaCat = "虎鲸猫"
    case dopeyCat = "呆猫"
}

enum EyeColorTheme: String, CaseIterable {
    case neonChartreuse = "经典荧光黄绿 (原版)"
    case siameseBlue = "暹罗宝石蓝 (暹罗标配)"
    case warmAmber = "琥珀金瞳"
    case iceCyan = "冰晶浅蓝"
    case softAmber = "柔和琥珀"
    case referenceHazel = "淡金青绿"

    static func saved(_ raw: String) -> Self {
        raw == "淡金青绿（参考图）" ? .referenceHazel : (Self(rawValue: raw) ?? .neonChartreuse)
    }

    var primaryColor: NSColor {
        switch self {
        case .neonChartreuse: return NSColor(calibratedRed: 0.79, green: 0.95, blue: 0.25, alpha: 1.0) // #C9F240
        case .siameseBlue:    return NSColor(calibratedRed: 0.37, green: 0.78, blue: 0.98, alpha: 1.0) // #5EC7FA
        case .warmAmber:      return NSColor(calibratedRed: 0.98, green: 0.76, blue: 0.22, alpha: 1.0)
        case .iceCyan:        return NSColor(calibratedRed: 0.45, green: 0.92, blue: 0.96, alpha: 1.0)
        case .referenceHazel: return NSColor(calibratedRed: 218.0/255, green: 178.0/255, blue: 87.0/255, alpha: 1)
        case .softAmber: return NSColor(calibratedRed: 217.0/255, green: 173.0/255, blue: 85.0/255, alpha: 1)
        }
    }

    var glowColor: NSColor {
        switch self {
        case .neonChartreuse: return NSColor(calibratedRed: 0.77, green: 0.95, blue: 0.24, alpha: 0.85)
        case .siameseBlue:    return NSColor(calibratedRed: 0.38, green: 0.80, blue: 0.99, alpha: 0.85)
        case .warmAmber:      return NSColor(calibratedRed: 0.98, green: 0.74, blue: 0.20, alpha: 0.85)
        case .iceCyan:        return NSColor(calibratedRed: 0.42, green: 0.90, blue: 0.98, alpha: 0.85)
        case .referenceHazel: return NSColor(calibratedRed: 218.0/255, green: 178.0/255, blue: 87.0/255, alpha: 0.85)
        case .softAmber: return NSColor(calibratedRed: 217.0/255, green: 173.0/255, blue: 85.0/255, alpha: 0.85)
        }
    }
}

enum DeskMode: Int, CaseIterable {
    case keyboard, tablet, automatic, arcade
    var title: String { ["键鼠", "数位板", "自动识别", "街机摇杆"][rawValue] }
    var next: Self {
        switch self {
        case .keyboard: return .tablet
        case .tablet: return .arcade
        case .arcade: return .automatic
        case .automatic: return .keyboard
        }
    }
}

enum PetAction: Int, CaseIterable {
    case treat, sleep, wake, doubleClickWake, music, mode, keyBubbles, mouseBubbles, hide

    var title: String {
        ["喂零食", "白天不熬猫", "晚上猫熬人", "双击猫头唤醒", "听音乐", "切换模式", "按键气泡", "鼠标气泡", "隐藏控件"][rawValue]
    }
}

final class CatSettings {
    static let shared = CatSettings()
    var showControls: Bool {
        didSet { UserDefaults.standard.set(showControls, forKey: "KuroCat.ControlsVisible") }
    }
    var greenScreenCapture: Bool {
        didSet { UserDefaults.standard.set(greenScreenCapture, forKey: "KuroCat.GreenScreenCapture") }
    }
    var doubleClickWake: Bool {
        didSet { UserDefaults.standard.set(doubleClickWake, forKey: "KuroCat.DoubleClickWake") }
    }
    var deskMode: DeskMode {
        didSet { UserDefaults.standard.set(deskMode.rawValue, forKey: "KuroCat.DeskMode") }
    }

    var showKeyBadges: Bool {
        didSet { UserDefaults.standard.set(showKeyBadges, forKey: "KuroCat.ShowKeyBadges") }
    }
    var showMouseBadges: Bool {
        didSet { UserDefaults.standard.set(showMouseBadges, forKey: "KuroCat.ShowMouseBadges") }
    }
    var showKeyLegends: Bool {
        didSet { UserDefaults.standard.set(showKeyLegends, forKey: "KuroCat.ShowKeyLegends") }
    }
    var footEasterEgg: Bool {
        didSet { UserDefaults.standard.set(footEasterEgg, forKey: "KuroCat.FootEasterEgg") }
    }
    var petInteraction: Bool {
        didSet { UserDefaults.standard.set(petInteraction, forKey: "KuroCat.PetInteraction") }
    }
    var fullClickThrough: Bool {
        didSet { UserDefaults.standard.set(fullClickThrough, forKey: "KuroCat.FullClickThrough") }
    }
    var fullScreenOverlay: Bool {
        didSet { UserDefaults.standard.set(fullScreenOverlay, forKey: "KuroCat.FullScreenOverlay") }
    }
    var scale: CGFloat {
        didSet { UserDefaults.standard.set(Double(scale), forKey: "KuroCat.Scale") }
    }
    var coatTheme: CatCoatTheme {
        didSet { UserDefaults.standard.set(coatTheme.rawValue, forKey: "KuroCat.CoatTheme") }
    }
    var keyboardTheme: KeyboardTheme {
        didSet { UserDefaults.standard.set(keyboardTheme.rawValue, forKey: "KuroCat.KeyboardTheme") }
    }
    private var dopeyKeyboardTheme: KeyboardTheme {
        didSet { UserDefaults.standard.set(dopeyKeyboardTheme.rawValue, forKey: "KuroCat.DopeyKeyboardTheme") }
    }
    var activeKeyboardTheme: KeyboardTheme {
        get { coatTheme == .dopeyCat ? dopeyKeyboardTheme : keyboardTheme }
        set {
            if coatTheme == .dopeyCat { dopeyKeyboardTheme = newValue }
            else { keyboardTheme = newValue }
        }
    }
    var eyeTheme: EyeColorTheme {
        didSet { UserDefaults.standard.set(eyeTheme.rawValue, forKey: "KuroCat.EyeTheme") }
    }

    private init() {
        let d = UserDefaults.standard
        showControls = d.bool(forKey: "KuroCat.ControlsVisible")
        greenScreenCapture = d.bool(forKey: "KuroCat.GreenScreenCapture")
        doubleClickWake = d.object(forKey: "KuroCat.DoubleClickWake") as? Bool ?? true
        deskMode = (d.object(forKey: "KuroCat.DeskMode") as? Int).flatMap(DeskMode.init(rawValue:)) ?? .automatic
        showKeyBadges = d.object(forKey: "KuroCat.ShowKeyBadges") as? Bool ?? true
        showMouseBadges = d.bool(forKey: "KuroCat.ShowMouseBadges")
        showKeyLegends = d.object(forKey: "KuroCat.ShowKeyLegends") as? Bool ?? true
        footEasterEgg = d.object(forKey: "KuroCat.FootEasterEgg") as? Bool ?? true
        petInteraction = d.object(forKey: "KuroCat.PetInteraction") as? Bool ?? true
        fullClickThrough = d.object(forKey: "KuroCat.FullClickThrough") as? Bool ?? false
        fullScreenOverlay = d.object(forKey: "KuroCat.FullScreenOverlay") as? Bool ?? true
        let savedScale = d.double(forKey: "KuroCat.Scale")
        scale = savedScale > 0.3 ? CGFloat(savedScale) : 1.0
        coatTheme = CatCoatTheme(rawValue: d.string(forKey: "KuroCat.CoatTheme") ?? "") ?? .orcaCat
        keyboardTheme = KeyboardTheme(rawValue: d.string(forKey: "KuroCat.KeyboardTheme") ?? "") ?? .classicMilk
        dopeyKeyboardTheme = d.string(forKey: "KuroCat.DopeyKeyboardTheme").flatMap(KeyboardTheme.init(rawValue:)) ?? .dopeyBlue
        let savedEye = d.string(forKey: "KuroCat.EyeTheme") ?? ""
        eyeTheme = savedEye.isEmpty ? .referenceHazel : EyeColorTheme.saved(savedEye)
    }
}

// MARK: - Spring Primitives

struct Spring1D {
    var value: CGFloat
    var velocity: CGFloat = 0
    var target: CGFloat

    init(_ initial: CGFloat) {
        self.value = initial
        self.target = initial
    }

    mutating func step(dt: CGFloat, stiffness: CGFloat = 260, damping: CGFloat = 22) {
        let force = (target - value) * stiffness
        let damp = velocity * damping
        velocity += (force - damp) * dt
        value += velocity * dt
    }
}

struct Spring2D {
    var x: Spring1D
    var y: Spring1D

    init(_ pt: CGPoint) {
        self.x = Spring1D(pt.x)
        self.y = Spring1D(pt.y)
    }

    var point: CGPoint { CGPoint(x: x.value, y: y.value) }
    var target: CGPoint {
        get { CGPoint(x: x.target, y: y.target) }
        set { x.target = newValue.x; y.target = newValue.y }
    }

    mutating func step(dt: CGFloat, stiffness: CGFloat = 340, damping: CGFloat = 25) {
        x.step(dt: dt, stiffness: stiffness, damping: damping)
        y.step(dt: dt, stiffness: stiffness, damping: damping)
    }
}

// MARK: - Particles

struct FloatingKeycap {
    let id: Int
    let label: String
    var position: CGPoint
    var velocity: CGPoint
    var age: CGFloat = 0
    let lifetime: CGFloat = 1.25
    var rotation: CGFloat
}

struct PurrSparkle {
    var position: CGPoint
    var velocity: CGPoint
    var age: CGFloat = 0
    let lifetime: CGFloat = 0.9
    let isSleepZ: Bool
}

struct ScriptedFrameInput {
    var now: CFTimeInterval
    var activeKeys: Set<PhysicalKey>
    var orderedKeys: [PhysicalKey]
    var mouseScreenPos: CGPoint
    var mouseVelocity: CGVector
    var leftMouseDown: Bool
    var rightMouseDown: Bool
    var lastMouseMoveTime: CFTimeInterval
    var lastMouseClickTime: CFTimeInterval
    var lastKeyActivityTime: CFTimeInterval
}

// MARK: - Cat Physics & Procedural Animation State

final class CatPhysics {
    // Logical canvas dimensions
    static let canvasSize = CGSize(width: 360, height: 320)

    // Rest positions
    static let seatAnchor = CGPoint(x: 184, y: 226)
    static let leftPawRest = CGPoint(x: 164, y: 235)
    static let rightPawRest = CGPoint(x: 204, y: 235)
    static let hindFootRest = CGPoint(x: 214, y: 234)
    static let mouseBaseCenter = CGPoint(x: 78, y: 258)

    // Springs
    var leftPaw = Spring2D(leftPawRest)
    var rightPaw = Spring2D(rightPawRest)
    var hindFoot = Spring2D(hindFootRest)
    var hindFootActive = Spring1D(0)

    var bodyShiftX = Spring1D(0)
    var bodyTilt = Spring1D(0)       // radians
    var bodySquash = Spring1D(1.0)
    var headTilt = Spring1D(0)       // radians (roll)
    var punchReach = Spring1D(0)    // Negative brings the viewer's left fist forward.
    private(set) var jumpLift: CGFloat = 0
    static let eggRadius: CGFloat = 59
    var eggRoll: CGFloat = 0
    var remainingRoll: CGFloat = 0

    func rollEgg(dx: CGFloat, dy: CGFloat) {
        let direction = abs(dx) >= abs(dy) ? dx : dy
        let distance = hypot(dx, dy)
        let arc = (direction < 0 ? -distance : distance) / Self.eggRadius
        if arc * remainingRoll < 0 { remainingRoll = 0 }
        remainingRoll = min(0.85, max(-0.85, remainingRoll + arc * 0.55))
        eggRoll = (eggRoll + arc * 0.45).remainder(dividingBy: 2 * .pi)
    }

    func coastEgg(dt: CGFloat) {
        let next = remainingRoll * exp(-dt * 7.5)
        eggRoll = (eggRoll + remainingRoll - next).remainder(dividingBy: 2 * .pi)
        remainingRoll = next
    }
    var headYaw = Spring1D(0)        // -1 (turn left) ... +1 (turn right)
    var headPitch = Spring1D(0)      // -1 (look up)   ... +1 (look down)
    var headOffsetY = Spring1D(0)
    var earFold = Spring1D(0)        // 0 = alert, 1 = relaxed petting ears
    var earTwitch = Spring1D(0)

    var eyeGaze = Spring2D(.zero)    // normalized [-1..1, -1..1]
    var pupilScale = Spring1D(1.0)
    var huntIntensity = Spring1D(0)  // 0 = normal slit pupils, 1 = full hunting crouch + big round boba pupils!
    var mouseOffset = Spring2D(.zero)
    var mousePressLeft = Spring1D(0)
    var mousePressRight = Spring1D(0)
    static let arcadePivot = CGPoint(x: 110, y: 266)
    static func arcadeButtonCenter(_ index: Int) -> CGPoint {
        let column = index % 4
        return CGPoint(x: 181 + 30.5 * CGFloat(column),
                       y: (index < 4 ? 250 : 276) + (column == 0 ? 4 : (column == 3 ? 3 : 0)))
    }
    private(set) var arcadeStick = Spring2D(.zero)
    private(set) var arcadeButton = -1
    private(set) var arcadePressure: CGFloat = 0
    private(set) var arcadeButtonPressure = Array(repeating: CGFloat(0), count: 8)
    private var arcadeKeyButtons: [PhysicalKey: Int] = [:]
    private var arcadeTapUntil = Array(repeating: CGFloat(0), count: 8)
    private var arcadeNextButton = 0
    private var arcadeLastButton = 1
    var arcadeBall: CGPoint {
        CGPoint(x: Self.arcadePivot.x + arcadeStick.point.x, y: Self.arcadePivot.y - 23 + arcadeStick.point.y)
    }

    private func updateArcade(dt: CGFloat, playing: Bool, keys: Set<PhysicalKey>, mouseVelocity: CGVector) {
        if !playing {
            arcadeStick.target = .zero
            arcadeButton = -1
            arcadePressure = 0
            arcadeKeyButtons.removeAll()
            arcadeNextButton = 0
            for i in arcadeTapUntil.indices {
                arcadeTapUntil[i] = 0
                arcadeButtonPressure[i] = 0
            }
            arcadeStick.step(dt: dt, stiffness: 200, damping: 20)
            return
        }

        let speed = hypot(mouseVelocity.dx, mouseVelocity.dy)
        let gain: CGFloat = overControl || speed < 18 ? 0 : 1 / max(420, speed)
        arcadeStick.target = CGPoint(x: mouseVelocity.dx * 10 * gain, y: -mouseVelocity.dy * 7 * gain)
        arcadeStick.step(dt: dt, stiffness: 200, damping: 20)

        arcadeKeyButtons = arcadeKeyButtons.filter { keys.contains($0.key) }
        let held = Set(arcadeKeyButtons.values)
        var pressed = Array(repeating: false, count: 8)
        var latest: Int?
        arcadePressure = 0
        for i in pressed.indices {
            pressed[i] = held.contains(i) || time < arcadeTapUntil[i]
            let target: CGFloat = pressed[i] ? 1 : 0
            arcadeButtonPressure[i] += max(-dt*15, min(dt*22, target-arcadeButtonPressure[i]))
            arcadePressure = max(arcadePressure, arcadeButtonPressure[i])
            if pressed[i] {
                if let previous = latest {
                    if arcadeTapUntil[i] > arcadeTapUntil[previous] { latest = i }
                } else { latest = i }
            }
        }
        if arcadeButton < 0 || !pressed[arcadeButton] { arcadeButton = latest ?? -1 }
        if arcadeButton >= 0 { arcadeLastButton = arcadeButton }
        let ball = arcadeBall, button = Self.arcadeButtonCenter(arcadeLastButton)
        leftPaw.target = CGPoint(x: ball.x, y: ball.y-7)
        rightPaw.target = CGPoint(x: button.x, y: button.y-12+9*arcadeButtonPressure[arcadeLastButton])
        leftPawTyping = false
        rightPawTyping = false
        leftPawOnMouse = false
        hindFootActive.target = 0
    }

    // Paw mode flags (for crisp outline & posture rendering)
    private(set) var leftPawOnMouse = false
    private(set) var leftPawTyping = false
    private(set) var rightPawTyping = false

    // Keycap glow & press depths
    var keyGlow: [PhysicalKey: CGFloat] = [:]
    var floatingKeycaps: [FloatingKeycap] = []
    var sparkles: [PurrSparkle] = []
    private var nextBadgeID = 1
    private var badgeCascadeIndex = 0
    private var lastBadgeTime: CFTimeInterval = 0

    // Time & blink
    private(set) var time: CGFloat = 0
    private(set) var tailPhase: CGFloat = 0
    private var nextBlinkTime: CGFloat = 3.2
    private var blinkProgress: CGFloat = 0 // 0 = open, 1 = closed
    private var isBlinking = false
    private var blinkTimer: CGFloat = 0

    // Petting state
    private(set) var petHeadIntensity: CGFloat = 0  // 0..1 -> happy closed lime lines
    private(set) var petBodyIntensity: CGFloat = 0  // 0..1 -> blissful half-squint
    private(set) var totalPetSessions: Int = UserDefaults.standard.integer(forKey: "KuroCat.PetCount")
    private var petStrokeAccumulator: CGFloat = 0
    private var lastPetTime: CFTimeInterval = 0
    private var isCurrentlyPetting = false
    private var petCursorCanvasPos: CGPoint = .zero
    private var petSparkleCooldown: CGFloat = 0

    // Sleepy idle state
    private(set) var sleepiness: CGFloat = 0 // 0 = awake, 1 = napping
    private var sleepZCooldown: CGFloat = 0

    // Recent key memory so fast single taps linger visibly (~140ms)
    private var recentLeftKey: (key: PhysicalKey, time: CFTimeInterval)?
    private var recentRightKey: (key: PhysicalKey, time: CFTimeInterval)?
    private var lastTypedPoint: CGPoint?

    private(set) var sleeping = false
    private(set) var listening = false
    private(set) var treatAge: CGFloat = -1
    static let bowlEnd: CGFloat = 2.86
    static let mealIdleSeconds: CFTimeInterval = 10
    private var mealSeconds: CGFloat = 10
    static let idleSeconds: CFTimeInterval = 45
    private var workTime: CFTimeInterval = 0
    private var restStarted: CFTimeInterval = 0
    private var typingSince: CFTimeInterval?
    private(set) var tailImpatience: CGFloat = 0
    private(set) var tailSlapTime: CGFloat = 0
    private var typingTaps: [CFTimeInterval] = []
    private var typingBeat = 0
    private var recentFootKey: (key: PhysicalKey, time: CFTimeInterval)?
    private var recentTailKey: (key: PhysicalKey, time: CFTimeInterval)?
    private(set) var tailKeyPoint = CGPoint.zero
    private(set) var tailKeyReach: CGFloat = 0
    private(set) var tailKeyDip: CGFloat = 0
    private var bellyHovered = false
    private var routineStarted = CACurrentMediaTime()
    private var lastHour: Date?
    private var pendingMeals = 0
    private var airplaneUntil: CGFloat = 0
    private var angryUntil: CGFloat = 0
    private var scoldUntil: CGFloat = 0
    private var turnStarts: CFTimeInterval = 0
    private var ignoringUntil: CFTimeInterval = 0
    private(set) var turn: CGFloat = 0
    private var turnProgress: CGFloat = 0
    private var turnScoldTime: CGFloat = -2
    private var furPuffTarget: CGFloat = 0
    private var backPuffUntil: CFTimeInterval = 0
    private(set) var furPuff: CGFloat = 0
    private var furPulseStarted: CGFloat = -2
    private var leftEar = Spring1D(0)
    private var rightEar = Spring1D(0)
    private var lastEarTouch: CGFloat = -10
    private var lastEarShake: CGFloat = -10
    private(set) var musicEyes: CGFloat = 0
    private(set) var headphones: CGFloat = 0
    var soundPlaying = false
    var soundPulse: CGFloat = 0
    var onAutoMusic: (() -> Bool)?
    var onAutoWake: (() -> Void)?

    var tabletMode = false
    var overControl = false
    private(set) var penDown = false
    private(set) var ink: [CGPoint?] = []
    private(set) var tabletTip = CGPoint(x: 194, y: 268)
    private(set) var tabletKeys: [PhysicalKey] = [.leftControl, .leftShift, .leftOption, .z, .b, .space, .x, .c, .v]
    private(set) var tabletHeld: Set<PhysicalKey> = []
    private var tabletKey: PhysicalKey?
    private var tabletKeyTime: CFTimeInterval = 0

    var feeding: Bool { treatAge >= 0 }
    var bowlVisible: Bool { feeding && treatAge < Self.bowlEnd }
    var dangoLowering: CGFloat {
        let remaining = scoldUntil - time
        return Self.ease((1.2 - remaining) / 0.16) * Self.ease(remaining / 0.18)
    }
    var guardingDango: Bool { bowlVisible && time < scoldUntil }
    var angry: Bool { (bellyHovered && usingDesk) || time < angryUntil }
    var angerMarkProgress: CGFloat {
        let turnAge = time - turnScoldTime
        if turnAge < 1.48 { return turnAge / 1.48 }
        if tailImpatience > 0.10 { return tailSlapTime.truncatingRemainder(dividingBy: 1.68) / 0.90 }
        return 1
    }
    var ignoring: Bool { CACurrentMediaTime() < ignoringUntil }
    var facingAway: Bool { turn == 1 && ignoring }
    var furPulseAge: CGFloat { time - furPulseStarted }
    var backAngerPhase: CGFloat { (time - turnScoldTime).truncatingRemainder(dividingBy: 1.65) / 1.65 }
    var atRest: Bool { (sleeping || listening) && !feeding && !ignoring }
    var usingDesk: Bool { !sleeping && !feeding && !listening && !ignoring && turn == 0 }
    var washLift: CGFloat {
        feeding ? Self.ease((treatAge - 2.85) / 0.42) * Self.ease((5.1 - treatAge) / 0.44) : 0
    }
    var penGrip: CGPoint { CGPoint(x: tabletTip.x + 6, y: tabletTip.y - (penDown ? 11 : 15)) }

    static func ease(_ x: CGFloat) -> CGFloat {
        let t = min(1, max(0, x))
        return t * t * (3 - 2 * t)
    }

    func canPerform(_ action: PetAction) -> Bool {
        switch action {
        case .treat: return !ignoring && !feeding
        case .sleep: return !ignoring && !sleeping
        case .wake: return !ignoring && sleeping
        case .music: return !ignoring && !sleeping
        default: return true
        }
    }

    func sleep() {
        sleeping = true
        restStarted = CACurrentMediaTime()
        treatAge = -1
        clearDesk()
    }

    private var dopeyIdleTime: CGFloat = 0
    private var reachAge: CGFloat = 0
    private var missedReach: CGFloat = 0
    private var boxUntil: CGFloat = 0
    private static let dopeyGestureDuration: CGFloat = 3.4
    private var clickedGesture = -1
    private var clickedGestureUntil: CGFloat = 0
    var doingDopeyGesture: Bool { time < clickedGestureUntil || time < boxUntil || reachAge > 0 || dopeyIdleTime >= 4 }

    func clickDopeyCat() {
        boxUntil = 0
        clickedGesture = (clickedGesture + 1) % Self.dopeyGestures.count
        clickedGestureUntil = time + Self.dopeyGestureDuration
        dopeyIdleTime = 0
    }

    func enterDopeySkin() {
        wake()
        treatAge = -1
        ignoringUntil = 0
        backPuffUntil = 0
        turnProgress = 0
        turn = 0
        furPuff = 0
        furPuffTarget = 0
        pendingMeals = 0
        dopeyIdleTime = 0
        clickedGesture = -1
        clickedGestureUntil = 0
        punchReach = Spring1D(0)
        reachAge = 0
        missedReach = 0
        boxUntil = 0
        jumpLift = 0
        clearDesk()
    }

    private struct GestureFrame {
        let time: CGFloat
        let left, right: CGPoint
        let rollDegrees, headDrop, reach: CGFloat

        init(_ time: CGFloat, _ lx: CGFloat, _ ly: CGFloat, _ rx: CGFloat, _ ry: CGFloat,
             _ rollDegrees: CGFloat, _ headDrop: CGFloat, _ reach: CGFloat = 0) {
            self.time = time
            left = CGPoint(x: lx, y: ly)
            right = CGPoint(x: rx, y: ry)
            self.rollDegrees = rollDegrees
            self.headDrop = headDrop
            self.reach = reach
        }
    }

    private static let dopeyGestures: [[GestureFrame]] = [
        [ // wave
            .init(0, -15, 0, 15, 0, 0, 0),
            .init(0.15, -18, 5, 18, 4, -4, 5),
            .init(0.37, -49, -45, 31, 5, 9, -4),
            .init(0.55, -58, -50, 33, 7, 12, -5),
            .init(0.72, -43, -33, 29, 4, 4, 1),
            .init(0.9, -65, -49, 35, 8, 13, -3),
            .init(1.13, -44, -32, 30, 4, 4, 1),
            .init(1.34, -65, -49, 35, 8, 13, -3),
            .init(1.72, -61, -46, 33, 7, 10, -2),
            .init(1.94, -43, -34, 30, 4, 4, 1),
            .init(2.16, -65, -48, 35, 8, 13, -3),
            .init(2.47, -58, -45, 32, 6, 9, -2),
            .init(2.84, -30, -10, 22, 4, 3, 2),
            .init(3.18, -15, 0, 15, 0, 0, 0),
        ],
        [ // clap
            .init(0, -15, 0, 15, 0, 0, 0),
            .init(0.24, -45, -1, 45, -1, 0, -2),
            .init(0.33, -13, -6, 13, -6, 0, 3),
            .init(0.44, -13, -6, 13, -6, 0, 3),
            .init(0.61, -45, -1, 45, -1, 0, -2),
            .init(0.64, -45, -1, 45, -1, 0, -2),
            .init(0.95, -15, 0, 15, 0, 0, 0),
        ],
        [ // think
            .init(0, -15, 0, 15, 0, 0, 0),
            .init(0.16, -20, 3, 21, 2, 3, 3),
            .init(0.4, -4, -24, 18, -7, -4, -2),
            .init(0.72, -4, -24, 18, -7, -9, 1),
            .init(1.04, -5, -23, 17, -8, -12, 4),
            .init(1.74, -5, -23, 17, -8, -12, 4),
            .init(2.02, -7, -26, 17, -8, -8, -2),
            .init(2.43, -7, -26, 17, -8, -8, -2),
            .init(2.84, -12, -8, 16, -3, -3, 1),
            .init(3.17, -15, 0, 15, 0, 0, 0),
        ],
        [ // attention
            .init(0, -15, 0, 15, 0, 0, 0),
            .init(0.13, -18, 4, 18, 4, 0, 5),
            .init(0.28, -39, -9, 39, -9, 0, -7),
            .init(0.42, -38, -8, 38, -8, 0, -6),
            .init(0.57, -32, 18, 32, 18, 0, -1),
            .init(0.74, -32, 15, 32, 15, 0, -3),
            .init(2.28, -32, 15, 32, 15, 0, -3),
            .init(2.54, -30, 13, 30, 13, 2, 1),
            .init(2.98, -15, 0, 15, 0, 0, 0),
        ],
        [ // boxing
            .init(0, -15, 0, 15, 0, 0, 0, 0),
            .init(0.24, -18, -12, 18, -12, 0, 1, 0),
            .init(0.32, -32, -26, 15, -10, 9, -4, -1),
            .init(0.4, -32, -26, 15, -10, 9, -4, -1),
            .init(0.49, -18, -12, 18, -12, 0, 1, 0),
            .init(0.61, -15, -10, 32, -26, -9, -4, 1),
            .init(0.69, -15, -10, 32, -26, -9, -4, 1),
            .init(0.82, -18, -12, 18, -12, 0, 1, 0),
            .init(1.05, -15, 0, 15, 0, 0, 0, 0),
        ],
    ]

    private func poseReach(_ cursor: CGPoint) -> (tilt: CGFloat, drop: CGFloat) {
        let beat = reachAge.truncatingRemainder(dividingBy: 1.08)
        let flight = max(0, min(1, (beat - 0.15) / 0.66))
        let arc = 4 * flight * (1 - flight)
        let dip = beat < 0.15 ? sin(beat * .pi / 0.15) : sin(max(0, min(1, (beat - 0.81) / 0.18)) * .pi)
        jumpLift = 36 * arc - 4 * dip
        let stretch = Self.ease(arc)
        let x = Self.seatAnchor.x + bodyShiftX.value
        let center = x + max(-48, min(48, cursor.x - x)) * stretch
        let spread = 23 - 12 * stretch + 32 * sin(stretch * .pi)
        let y = 183 - jumpLift + (max(cursor.y, 61) - 147) * stretch
        leftPaw.target = CGPoint(x: center - spread, y: y)
        rightPaw.target = CGPoint(x: center + spread, y: y)
        return ((cursor.x - x) * 0.0013 * arc, 3 * dip - 3 * arc)
    }

    private func poseDopeyHands(_ gesture: Int, at elapsed: CGFloat) -> (tilt: CGFloat, drop: CGFloat) {
        var t = elapsed
        if (gesture == 1 || gesture == 4) && elapsed >= 0.24 {
            let period: CGFloat = gesture == 1 ? 0.40 : 0.58
            let repeats: CGFloat = gesture == 1 ? 7 : 5
            let age = elapsed - 0.24
            t = age < period * repeats ? 0.24 + age.truncatingRemainder(dividingBy: period) : elapsed - period * (repeats - 1)
        }
        let frames = Self.dopeyGestures[gesture]
        var next = 1
        while next + 1 < frames.count && t > frames[next].time { next += 1 }
        let a = frames[next-1], b = frames[next]
        let mix = Self.ease((t-a.time)/(b.time-a.time))
        func blend(_ start: CGFloat, _ end: CGFloat) -> CGFloat { start + (end-start)*mix }
        let x = Self.seatAnchor.x + bodyShiftX.value, y = 184 + sin(time * 2.1) * 1.6
        leftPaw.target = CGPoint(x: x + blend(a.left.x,b.left.x), y: y + blend(a.left.y,b.left.y))
        rightPaw.target = CGPoint(x: x + blend(a.right.x,b.right.x), y: y + blend(a.right.y,b.right.y))
        punchReach.target = blend(a.reach,b.reach)
        return (blend(a.rollDegrees,b.rollDegrees) * .pi / 180, blend(a.headDrop,b.headDrop))
    }

    func wake() {
        sleeping = false
        workTime = 0
        routineStarted = CACurrentMediaTime()
        sparkles.removeAll { $0.isSleepZ }
    }

    func listen(_ enabled: Bool) {
        listening = enabled
        if enabled {
            treatAge = -1
            restStarted = CACurrentMediaTime()
            routineStarted = restStarted
            workTime = 0
            clearDesk()
        }
    }

    func resumeFromInput(_ time: CFTimeInterval) {
        guard !CatSettings.shared.doubleClickWake, atRest, time > restStarted else { return }
        if listening { onAutoWake?() }
        listen(false)
        wake()
    }

    func sleepAfterMusicError() {
        listen(false)
        sleep()
    }

    func bounceAtRest() {
        bodySquash.velocity = sleeping ? -2.2 : -4.7
        headOffsetY.velocity = sleeping ? 550 : 230
        earTwitch.velocity += 2.4
    }

    func pokeBack() {
        let now = CACurrentMediaTime()
        let continuing = now < backPuffUntil
        if !continuing { furPuffTarget = furPuff }
        if continuing || furPuffTarget > 0 {
            furPuffTarget = min(1, furPuffTarget + 0.2)
            furPulseStarted = time
        }
        backPuffUntil = now + 2
        bodySquash.velocity = -2.8
        headOffsetY.velocity = 310
    }

    func offerTreat() {
        guard canPerform(.treat) else { return }
        mealSeconds = 10 + 5 * CGFloat(max(0, pendingMeals - 1))
        pendingMeals = 0
        treatAge = 0
        scoldUntil = 0
        sparkles.removeAll { $0.isSleepZ }
        clearDesk()
    }

    private func flattenEars() {
        airplaneUntil = time + 0.65
        angryUntil = time + 2.4
        petHeadIntensity = 0
        petBodyIntensity = 0
    }

    func setBellyHovered(_ hovered: Bool) {
        if hovered && usingDesk && !bellyHovered {
            stopPetting()
            lastPetTime = CACurrentMediaTime()
        }
        bellyHovered = hovered
    }

    func stopPetting() {
        petHeadIntensity = 0
        petBodyIntensity = 0
    }

    func touchEar(left: Bool) {
        guard usingDesk, time - lastEarTouch >= 0.24 else { return }
        lastPetTime = CACurrentMediaTime()
        if left { leftEar.velocity += 12 } else { rightEar.velocity += 12 }
        if time - lastEarTouch < 1.1 && time - lastEarShake > 0.8 {
            lastEarShake = time
            angryUntil = time + 1.6
        }
        lastEarTouch = time
    }

    func earAngle(left: Bool) -> CGFloat {
        0.06 + earFold.value * 0.34 + (left ? leftEar.value : rightEar.value)
    }

    func clickBowl(take: Bool) {
        guard bowlVisible, !ignoring else { return }
        if CatSettings.shared.coatTheme == .dopeyCat {
            scoldUntil = take ? time : time + 1.2
            if take { treatAge = Self.bowlEnd }
            return
        }
        flattenEars()
        if take {
            turnScoldTime = time
            backPuffUntil = 0
            airplaneUntil = time + 0.91
            treatAge = -1
            turnStarts = CACurrentMediaTime() + 0.58
            ignoringUntil = turnStarts + 180
            sparkles.removeAll()
            clearDesk()
        } else {
            scoldUntil = time + 0.8
        }
    }

    func clearTablet() {
        ink.removeAll()
        penDown = false
    }

    static func tabletKeyRect(_ i: Int) -> CGRect {
        CGRect(x: 54 + CGFloat(i % 3) * 20, y: 236 + CGFloat(i / 3) * 19, width: 17, height: 16)
    }

    func tabletKeyIndex(at point: CGPoint) -> Int? {
        tabletKeys.indices.first { Self.tabletKeyRect($0).contains(point) }
    }

    private func pressTabletKey(_ key: PhysicalKey, now: CFTimeInterval) {
        if !tabletKeys.contains(key) {
            let index = tabletKeys.firstIndex { !tabletHeld.contains($0) } ?? 0
            tabletKeys[index] = key
        }
        tabletKey = key
        tabletKeyTime = now
        if CatSettings.shared.showKeyBadges, let index = tabletKeys.firstIndex(of: key) {
            let rect = Self.tabletKeyRect(index)
            spawnFloatingKeycap(label: key.badgeLabel, near: CGPoint(x: rect.midX, y: rect.midY), now: now)
        }
    }

    private func updateTablet(cursor: CGPoint, drawing: Bool, keys: Set<PhysicalKey>, now: CFTimeInterval) {
        let screen = NSScreen.screens.reduce(CGRect.null) { $0.union($1.frame) }
        if !screen.isNull && !overControl {
            let x = min(1, max(0, (cursor.x - screen.minX) / screen.width))
            let y = min(1, max(0, (screen.maxY - cursor.y) / screen.height))
            tabletTip = CGPoint(x: 135 + 176 * x, y: 240 + 45 * y)
        }
        if drawing {
            if !penDown { ink.append(nil) }
            if let previous = ink.last ?? nil {
                if hypot(previous.x - tabletTip.x, previous.y - tabletTip.y) > 0.2 { ink.append(tabletTip) }
            } else {
                ink.append(tabletTip)
            }
            if ink.count > 2048 { ink.removeFirst(ink.count - 2048) }
        }
        penDown = drawing
        tabletHeld = keys
        let key = tabletKey.flatMap { keys.contains($0) || now - tabletKeyTime < 0.32 ? $0 : nil }
        if let key, let index = tabletKeys.firstIndex(of: key) {
            let rect = Self.tabletKeyRect(index)
            leftPaw.target = CGPoint(x: rect.midX, y: rect.minY)
        } else {
            leftPaw.target = CGPoint(x: 105, y: 234)
        }
        rightPaw.target = penGrip
        leftPawOnMouse = false
        leftPawTyping = key != nil
        rightPawTyping = false
        hindFootActive.target = 0
    }

    private func clearDesk() {
        mouseOffset = Spring2D(.zero)
        mousePressLeft = Spring1D(0)
        mousePressRight = Spring1D(0)
        penDown = false
        tabletHeld.removeAll()
        tabletKey = nil
        keyGlow.removeAll()
        floatingKeycaps.removeAll()
        recentLeftKey = nil
        recentRightKey = nil
        recentFootKey = nil
        recentTailKey = nil
        typingTaps.removeAll(keepingCapacity: true)
        lastTypedPoint = nil
        huntIntensity.target = 0
        petHeadIntensity = 0
        petBodyIntensity = 0
    }

    func observeClock(_ date: Date) {
        guard let hour = Calendar.current.dateInterval(of: .hour, for: date)?.start,
              lastHour != hour else { return }
        if lastHour != nil && Calendar.current.component(.minute, from: date) == 0 {
            pendingMeals += 1
        }
        lastHour = hour
    }

    static func choosesMusic(hour: Int, roll: Double) -> Bool {
        (6..<18).contains(hour) ? roll >= 0.7 : roll < 0.7
    }

    private func updateRoutine(now: CFTimeInterval, dt: CGFloat, rawDt: CGFloat,
                               lastInput: CFTimeInterval, working: Bool, automatic: Bool) {
        if ignoringUntil > 0 && now >= ignoringUntil {
            ignoringUntil = 0
            backPuffUntil = 0
            routineStarted = now
        }
        let target: CGFloat = ignoringUntil > 0 && now >= turnStarts ? 1 : 0
        turnProgress += min(dt / 0.84, max(-dt / 0.84, target - turnProgress))
        turn = Self.ease(turnProgress)
        let puffTarget = now < backPuffUntil ? furPuffTarget : 0
        furPuff += min(dt * 4.5, max(-dt, puffTarget - furPuff))
        if feeding && time >= scoldUntil {
            let dopeyMeal = CatSettings.shared.coatTheme == .dopeyCat
            let eating = bowlVisible
            let end: CGFloat = dopeyMeal ? (eating ? Self.bowlEnd : Self.bowlEnd + 2.85) : 5.24
            treatAge += eating ? dt * Self.bowlEnd / mealSeconds * (dopeyMeal ? 2.2 : 1) : dt
            if treatAge >= end {
                treatAge = -1
                routineStarted = now
            }
        }
        if pendingMeals > 0 && !working && now - lastInput >= Self.mealIdleSeconds {
            offerTreat()
        }
        guard automatic, usingDesk else { return }
        if working && !overControl { workTime += Double(min(rawDt, 0.10)) }
        let idle = now - max(routineStarted, max(lastInput, lastPetTime))
        let idleBreak = workTime < 25 * 60 && !working && idle >= Self.idleSeconds
        if workTime >= 25 * 60 || idleBreak {
            let hour = Calendar.current.component(.hour, from: Date())
            if !Self.choosesMusic(hour: hour, roll: Double.random(in: 0..<1)) || onAutoMusic?() != true {
                sleep()
            }
        }
    }

    // Computed eye closure (0 = wide open, 0.55 = half squint, 1 = fully closed)
    var effectiveEyeClosure: CGFloat {
        let petClose = max(petHeadIntensity * 1.0, petBodyIntensity * 0.62)
        let restClose = max(sleepiness * 1.12, max(washLift, musicEyes * 1.13))
        return min(1.0, max(blinkProgress, max(petClose, restClose)))
    }

    /// True when eyes should draw the signature glowing lime horizontal closed-eye lines
    var isHappyClosedEyes: Bool {
        petHeadIntensity > 0.45
    }

    var isBlissfulSquint: Bool {
        petBodyIntensity > 0.35 && !isHappyClosedEyes
    }

    // MARK: - Input Hooks

    func triggerKeyDown(_ key: PhysicalKey, at timestamp: CFTimeInterval? = nil) {
        guard usingDesk else { return }
        if CatSettings.shared.deskMode == .arcade {
            clickedGestureUntil = 0
            let order = [0, 4, 1, 5, 2, 6, 3, 7]
            var button = 0
            for _ in order {
                button = order[arcadeNextButton]
                arcadeNextButton = (arcadeNextButton + 1) % order.count
                if !arcadeKeyButtons.contains(where: { $0.key != key && $0.value == button }) { break }
            }
            arcadeButton = button
            arcadeLastButton = button
            arcadeKeyButtons[key] = button
            arcadeTapUntil[button] = time + 0.15
            return
        }
        clickedGestureUntil = 0
        let now = timestamp ?? CACurrentMediaTime()
        if tabletMode {
            pressTabletKey(key, now: now)
            return
        }
        let vk = key.visualKey
        keyGlow[vk] = 1.0

        let pt = KeyboardLayout.canvasPoint(for: vk)
        lastTypedPoint = pt

        var footTap = false
        var tailTap = false
        if CatSettings.shared.footEasterEgg && CatSettings.shared.coatTheme != .dopeyCat {
            let previousSpeed = typingTaps.count
            typingTaps.removeAll { now - $0 >= 1 }
            if typingTaps.isEmpty { typingBeat = 0 }
            typingTaps.append(now)
            let speed = typingTaps.count
            let beat = typingBeat
            typingBeat = (beat + 1) % 12
            tailTap = speed >= 11 && (previousSpeed < 11 || beat % 4 == 3) && tailImpatience <= 0.001
            footTap = !tailTap && speed >= 7 && (previousSpeed < 7 || beat % (speed >= 11 ? 4 : 3) == 2)
        }
        if tailTap {
            recentTailKey = (vk, now)
            tailKeyPoint = pt
        } else if footTap {
            recentFootKey = (vk, now)
            hindFoot.y.velocity -= 135
        } else if key.prefersLeftPaw {
            recentLeftKey = (vk, now)
            leftPaw.y.velocity -= 180
        } else {
            recentRightKey = (vk, now)
            rightPaw.y.velocity -= 180
        }

        // Expressive micro-reactions
        bodySquash.velocity -= 1.3
        earTwitch.velocity += (key.prefersLeftPaw ? -3.8 : 3.8)
        headTilt.velocity += (key.prefersLeftPaw ? -1.4 : 1.4)
        pupilScale.target = 1.16

        // Wake up immediately
        sleepiness = 0

        // Spawn floating keycap badge
        if CatSettings.shared.showKeyBadges {
            spawnFloatingKeycap(label: key.badgeLabel, near: pt, now: now)
        }
    }

    func triggerMouseClick(button: Int, isDown: Bool) {
        guard usingDesk, !tabletMode, !overControl, CatSettings.shared.deskMode != .arcade else { return }
        sleepiness = 0
        if isDown {
            let now = CACurrentMediaTime()
            if button == 0 {
                mousePressLeft.target = 1.0
                leftPaw.y.velocity += 130
            } else {
                mousePressRight.target = 1.0
                leftPaw.y.velocity += 110
            }
            pupilScale.target = 1.12
            earTwitch.velocity -= 2.2
            if CatSettings.shared.showMouseBadges {
                let label = button == 0 ? "左键" : (button == 1 ? "右键" : "中键")
                let pt = CGPoint(
                    x: CatPhysics.mouseBaseCenter.x + mouseOffset.point.x,
                    y: CatPhysics.mouseBaseCenter.y + mouseOffset.point.y - 6
                )
                spawnFloatingKeycap(label: label, near: pt, now: now)
            }
        } else {
            if button == 0 { mousePressLeft.target = 0 }
            else { mousePressRight.target = 0 }
        }
    }

    /// Called by the overlay window when cursor strokes across the cat's head or body.
    func registerPetStroke(canvasPoint: CGPoint, delta: CGFloat, isHead: Bool, at timestamp: CFTimeInterval? = nil) {
        guard usingDesk else { return }
        guard CatSettings.shared.petInteraction else { return }
        let now = timestamp ?? CACurrentMediaTime()
        lastPetTime = now
        petCursorCanvasPos = canvasPoint
        sleepiness = 0

        let energy = min(delta / 26.0, 0.35)
        if isHead {
            petHeadIntensity = min(1.0, petHeadIntensity + energy + 0.08)
            petBodyIntensity = max(petBodyIntensity, petHeadIntensity * 0.6)
        } else {
            petBodyIntensity = min(1.0, petBodyIntensity + energy + 0.07)
        }

        petStrokeAccumulator += delta
        if !isCurrentlyPetting && petStrokeAccumulator > 45 {
            isCurrentlyPetting = true
            totalPetSessions += 1
            UserDefaults.standard.set(totalPetSessions, forKey: "KuroCat.PetCount")
        }

        // Gentle nuzzle toward cursor
        let dx = (canvasPoint.x - CatPhysics.seatAnchor.x) / 60.0
        headTilt.target = max(-0.18, min(0.18, dx * 0.16))
        headOffsetY.velocity += 18
    }

    private func spawnFloatingKeycap(label: String, near pt: CGPoint, now: CFTimeInterval) {
        if now - lastBadgeTime < 0.38 {
            badgeCascadeIndex = (badgeCascadeIndex + 1) % 6
        } else {
            badgeCascadeIndex = 0
        }
        lastBadgeTime = now

        // Spread fast bursts gracefully so they stay readable like "F O J F E A"
        let spreadOffsets: [CGFloat] = [-10, 14, -22, 26, 2, 36]
        let dx = spreadOffsets[badgeCascadeIndex] + CGFloat.random(in: -3...3)
        let startX = max(55, min(315, pt.x + dx * 0.45))
        let startY = pt.y - 26 - CGFloat(badgeCascadeIndex % 2) * 8

        let badge = FloatingKeycap(
            id: nextBadgeID,
            label: label,
            position: CGPoint(x: startX, y: startY),
            velocity: CGPoint(x: dx * 0.65, y: -36 - CGFloat.random(in: 0...14)),
            rotation: CGFloat.random(in: -0.07...0.07)
        )
        nextBadgeID += 1
        floatingKeycaps.append(badge)
        if floatingKeycaps.count > 16 {
            floatingKeycaps.removeFirst(floatingKeycaps.count - 16)
        }
    }

    // MARK: - 60 FPS Simulation Step

    func step(dt rawDt: CGFloat, windowFrameInScreen: CGRect, scripted: ScriptedFrameInput? = nil) {
        let dt = min(rawDt, 1.0 / 25.0)
        time += dt

        let engine = InputEngine.shared
        if scripted == nil {
            engine.pollMouse(dt: dt)
        }

        let now = scripted?.now ?? CACurrentMediaTime()
        let activeKeys = scripted?.activeKeys ?? engine.activeKeys
        let orderedKeys = scripted?.orderedKeys ?? engine.pressedOrder
        let curMouseVel = scripted?.mouseVelocity ?? engine.mouseVelocity
        let curMouseLoc = scripted?.mouseScreenPos ?? engine.lastMouseLocation
        let curLeftMouse = scripted?.leftMouseDown ?? engine.leftMouseDown
        let curRightMouse = scripted?.rightMouseDown ?? engine.rightMouseDown
        let curLastMouseMove = scripted?.lastMouseMoveTime ?? engine.lastMouseMoveTime
        let curLastMouseClick = scripted?.lastMouseClickTime ?? engine.lastMouseClickTime
        let curLastKeyTime = scripted?.lastKeyActivityTime ?? engine.lastKeyActivityTime

        // 1. Decay petting intensity gracefully when hand stops moving
        if now - lastPetTime > 0.28 {
            petHeadIntensity = max(0, petHeadIntensity - dt * 1.4)
            petBodyIntensity = max(0, petBodyIntensity - dt * 1.2)
            if petHeadIntensity < 0.05 && petBodyIntensity < 0.05 {
                isCurrentlyPetting = false
                petStrokeAccumulator = 0
            }
        } else if (petHeadIntensity > 0.55 || petBodyIntensity > 0.55) {
            petSparkleCooldown -= dt
            if petSparkleCooldown <= 0 {
                petSparkleCooldown = 0.42
                let sx = CatPhysics.seatAnchor.x + CGFloat.random(in: -44...44)
                let sy = 88 + CGFloat.random(in: -14...18)
                sparkles.append(PurrSparkle(
                    position: CGPoint(x: sx, y: sy),
                    velocity: CGPoint(x: CGFloat.random(in: -10...10), y: -24),
                    isSleepZ: false
                ))
            }
        }

        let lastKeyRelease = scripted == nil ? engine.lastKeyReleaseTime : 0
        let lastInput = max(max(curLastKeyTime, lastKeyRelease), max(curLastMouseMove, curLastMouseClick))
        let otherMouseHeld = scripted == nil && engine.otherMouseDown
        let working = !activeKeys.isEmpty || curLeftMouse || curRightMouse || otherMouseHeld || (lastInput > 0 && now - lastInput < 2)
        let dopey = CatSettings.shared.coatTheme == .dopeyCat
        let arcadeMode = CatSettings.shared.deskMode == .arcade
        updateRoutine(now: now, dt: dt, rawDt: rawDt, lastInput: lastInput, working: working, automatic: scripted == nil && !arcadeMode)
        let desk = usingDesk
        let arcade = arcadeMode && desk
        let music = listening && !feeding
        let targetSleep: CGFloat = sleeping && !feeding && !ignoring ? 1 : 0
        sleepiness += (targetSleep - sleepiness) * dt * (targetSleep > sleepiness ? 3 : 5)
        if targetSleep > 0 && sleepiness > 0.86 {
            sleepZCooldown -= dt
            if sleepZCooldown <= 0 {
                sleepZCooldown = 2.8
                sparkles.append(PurrSparkle(position: CGPoint(x: 223, y: 114 + headOffsetY.value),
                                            velocity: CGPoint(x: 9, y: -20), isSleepZ: true))
            }
        }
        headphones += ((music ? 1 : 0) - headphones) * min(1, dt * 12)
        musicEyes += ((music && soundPlaying ? 1 : 0) - musicEyes) * min(1, dt * 12)
        let pulse = music && soundPlaying ? soundPulse : 0
        let sway = sin(time * 5) * (0.72 * pulse + 0.28 * musicEyes)
        let lean = feeding && time >= scoldUntil ? Self.ease((treatAge - 0.25) / 0.55) * Self.ease((2.73 - treatAge) / 0.49) : 0
        let bite = lean * (0.5 - 0.5 * cos(time * 6.55))
        let lift = washLift
        let wipe = Self.ease((treatAge - 3.31) / 0.25) * (0.5 - 0.5 * cos((treatAge - 3.31) * 11.5))
        let mouseSpeed = hypot(curMouseVel.dx, curMouseVel.dy)
        if desk && !arcade && mouseSpeed > 460 && petHeadIntensity < 0.25 && petBodyIntensity < 0.25 {
            // Build up smoothly & responsively with fast mouse waving (laser-pointer effect!)
            let boost = min(1.0, (mouseSpeed - 460) / 1100.0)
            huntIntensity.target = min(1.0, huntIntensity.target + (0.45 + boost * 1.15) * dt)
            sleepiness = 0
        } else {
            // Calm down gently once cursor stops darting
            huntIntensity.target = max(0.0, huntIntensity.target - dt * 0.65)
        }
        huntIntensity.step(dt: dt, stiffness: 85, damping: 16)
        let hunt = max(0, min(1, huntIntensity.value))

        // 3. Determine limb targets
        let tapHoldWindow: CFTimeInterval = 0.14
        let mouseRecent = (now - curLastMouseMove < 2.2) || curLeftMouse || curRightMouse
        let mouseActivelyUsed = (now - curLastMouseMove < 0.75) || curLeftMouse || curRightMouse
        let typing = !dopey && desk && !arcade && curLastKeyTime > 0 && now - curLastKeyTime <= 2
        if typing {
            if typingSince == nil { typingSince = curLastKeyTime }
        } else {
            typingSince = nil
        }
        let impatient = typingSince.map { now - $0 > 60 } == true
        let extraTyping = !dopey && desk && !tabletMode && !arcade && CatSettings.shared.footEasterEgg
        let footKey: PhysicalKey? = extraTyping ? recentFootKey.flatMap { now - $0.time < 0.20 ? $0.key : nil } : nil
        let tailKey: (key: PhysicalKey, time: CFTimeInterval)? = extraTyping && !impatient && tailImpatience <= 0.001
            ? recentTailKey.flatMap { now - $0.time < 0.24 ? $0 : nil } : nil
        if let tailKey {
            let age = CGFloat(now - tailKey.time)
            tailKeyReach = Self.ease(age / 0.075) * Self.ease((0.24 - age) / 0.095)
            tailKeyDip = Self.ease((age - 0.05) / 0.042)
        } else {
            tailKeyReach = 0
            tailKeyDip = 0
        }
        func assignedToExtra(_ key: PhysicalKey) -> Bool {
            key.visualKey == footKey || key.visualKey == tailKey?.key
        }

        // Partition active keys
        var leftCandidates: [PhysicalKey] = []
        var rightCandidates: [PhysicalKey] = []
        for k in (desk ? orderedKeys : []) where !assignedToExtra(k) {
            if k.prefersLeftPaw { leftCandidates.append(k) }
            else { rightCandidates.append(k) }
        }

        var leftKey: PhysicalKey? = leftCandidates.last
        var rightKey: PhysicalKey? = rightCandidates.last

        // If 2+ keys are pressed on the same side, let the free paw help —
        // UNLESS viewer-left paw (cat's right hand) is actively driving the mouse!
        if leftKey == nil && rightCandidates.count >= 2 && !mouseActivelyUsed {
            let sorted = rightCandidates.sorted {
                KeyboardLayout.canvasPoint(for: $0).x < KeyboardLayout.canvasPoint(for: $1).x
            }
            leftKey = sorted.first
            rightKey = sorted.last
        } else if rightKey == nil && leftCandidates.count >= 2 {
            let sorted = leftCandidates.sorted {
                KeyboardLayout.canvasPoint(for: $0).x < KeyboardLayout.canvasPoint(for: $1).x
            }
            leftKey = sorted.first
            rightKey = sorted.last
        }

        // Keep quick taps visible for a tiny snappy window
        if leftKey == nil, let rec = recentLeftKey, now - rec.time < tapHoldWindow, !assignedToExtra(rec.key) {
            leftKey = rec.key
        }
        if rightKey == nil, let rec = recentRightKey, now - rec.time < tapHoldWindow, !assignedToExtra(rec.key) {
            rightKey = rec.key
        }

        // Subtle mini-mouse movement driven by global cursor velocity
        let targetMouseDx = max(-5.5, min(5.5, curMouseVel.dx * 0.0045))
        let targetMouseDy = max(-4.5, min(4.5, -curMouseVel.dy * 0.0045))
        mouseOffset.target = desk ? CGPoint(x: targetMouseDx, y: targetMouseDy) : .zero
        mouseOffset.step(dt: dt, stiffness: 120, damping: 16)
        mousePressLeft.step(dt: dt, stiffness: 400, damping: 24)
        mousePressRight.step(dt: dt, stiffness: 400, damping: 24)

        let liveMouseCenter = CGPoint(
            x: CatPhysics.mouseBaseCenter.x + mouseOffset.point.x,
            y: CatPhysics.mouseBaseCenter.y + mouseOffset.point.y
        )

        // Left paw target
        if let lk = leftKey {
            leftPaw.target = KeyboardLayout.canvasPoint(for: lk)
            leftPawTyping = true
            leftPawOnMouse = false
        } else if mouseRecent && sleepiness < 0.3 {
            leftPaw.target = CGPoint(x: liveMouseCenter.x + 2, y: liveMouseCenter.y - 3)
            leftPawTyping = false
            leftPawOnMouse = true
        } else {
            leftPaw.target = dopey ? CGPoint(x: Self.seatAnchor.x + bodyShiftX.value - 15, y: 184 + sin(time * 2.1) * 1.6) : Self.leftPawRest
            leftPawTyping = false
            leftPawOnMouse = false
        }

        // Right paw target
        if let rk = rightKey {
            rightPaw.target = KeyboardLayout.canvasPoint(for: rk)
            rightPawTyping = true
        } else {
            rightPaw.target = dopey ? CGPoint(x: Self.seatAnchor.x + bodyShiftX.value + 15, y: 184 + sin(time * 2.1) * 1.6) : Self.rightPawRest
            rightPawTyping = false
        }

        // Hind foot target
        if let fk = footKey {
            hindFoot.target = KeyboardLayout.canvasPoint(for: fk)
            hindFootActive.target = 1.0
        } else {
            hindFoot.target = CatPhysics.hindFootRest
            hindFootActive.target = 0.0
        }

        if desk && tabletMode {
            updateTablet(cursor: curMouseLoc, drawing: curLeftMouse && !overControl, keys: activeKeys, now: now)
        } else if !desk {
            leftPawTyping = false
            leftPawOnMouse = false
            rightPawTyping = false
            hindFootActive.target = 0
            if dopey && music {
                leftPaw.target = CGPoint(x: Self.seatAnchor.x - 15, y: 184)
                rightPaw.target = CGPoint(x: Self.seatAnchor.x + 15, y: 184)
            } else {
                leftPaw.target = CGPoint(x: 167 - 7 * lean, y: 231 - 7 * sleepiness)
                rightPaw.target = CGPoint(x: 201 + 7 * lean + lift * (-7 + 17 * wipe),
                                          y: 231 - 7 * sleepiness - lift * (75 + 16 * wipe))
            }
        }

        if !dopey || !desk { clickedGestureUntil = 0 }
        let clicked = time < clickedGestureUntil
        let scale = max(0.2, windowFrameInScreen.width / Self.canvasSize.width)
        let cursorCanvas = CGPoint(x: (curMouseLoc.x - windowFrameInScreen.minX) / scale,
                                   y: (windowFrameInScreen.maxY - curMouseLoc.y) / scale)
        let lastKey = max(curLastKeyTime, lastKeyRelease)
        let canReach = dopey && desk && CatSettings.shared.petInteraction && activeKeys.isEmpty &&
            (lastKey == 0 || now - lastKey >= 0.65) && !curLeftMouse && !curRightMouse && !overControl
        if !canReach { boxUntil = 0 }
        // Use the standing pose for the hover band throughout the jump.
        if canReach && !clicked && time >= boxUntil && abs(cursorCanvas.x - Self.seatAnchor.x) <= 110 &&
            (-68...92).contains(cursorCanvas.y) {
            reachAge += dt
            func touches(_ paw: CGPoint) -> Bool { hypot(paw.x - cursorCanvas.x, paw.y - cursorCanvas.y) <= 15.2 }
            missedReach = touches(leftPaw.point) || touches(rightPaw.point) ? 0 : missedReach + rawDt
            if missedReach >= 5 {
                boxUntil = time + Self.dopeyGestureDuration
                reachAge = 0
                missedReach = 0
            }
        } else {
            reachAge = 0
            missedReach = 0
        }
        dopeyIdleTime = dopey && desk && !arcade && !clicked && reachAge == 0 && time >= boxUntil &&
            !working && !overControl ? dopeyIdleTime + dt : 0
        var gestureHead: (tilt: CGFloat, drop: CGFloat) = (0, 0)
        punchReach.target = 0
        if reachAge == 0 { jumpLift *= max(0, 1 - dt * 16) }
        if doingDopeyGesture {
            leftPawOnMouse = false
            leftPawTyping = false
            rightPawTyping = false
            let elapsed = dopeyIdleTime - 4
            if reachAge > 0 { gestureHead = poseReach(cursorCanvas) }
            else if time < boxUntil { gestureHead = poseDopeyHands(4, at: Self.dopeyGestureDuration - (boxUntil - time)) }
            else if clicked { gestureHead = poseDopeyHands(clickedGesture, at: Self.dopeyGestureDuration - (clickedGestureUntil - time)) }
            else { gestureHead = poseDopeyHands(Int(elapsed / 6) % Self.dopeyGestures.count, at: elapsed.truncatingRemainder(dividingBy: 6)) }
        }
        updateArcade(dt: dt, playing: arcade && !doingDopeyGesture, keys: activeKeys, mouseVelocity: curMouseVel)
        leftPaw.step(dt: dt, stiffness: 360, damping: 24)
        if dopey { punchReach.step(dt: dt, stiffness: 360, damping: 24) }
        rightPaw.step(dt: dt, stiffness: 360, damping: 24)
        hindFoot.step(dt: dt, stiffness: 320, damping: 22)
        hindFootActive.step(dt: dt, stiffness: 260, damping: 22)

        // 4. Expressive body lean, squash, head & ears
        var targetShiftX: CGFloat = 0
        var targetBodyTilt: CGFloat = 0
        if leftPawOnMouse && !rightPawTyping {
            targetShiftX = -10
            targetBodyTilt = -0.075
        } else if leftPawOnMouse && rightPawTyping {
            targetShiftX = -3
            targetBodyTilt = (rightPaw.point.x - 196) * 0.0008
        } else if leftPawTyping || rightPawTyping {
            let avgX = ((leftPawTyping ? leftPaw.point.x : 184) + (rightPawTyping ? rightPaw.point.x : 184)) * 0.5
            targetShiftX = (avgX - 184) * 0.14
            targetBodyTilt = (avgX - 184) * 0.0011
        }
        if arcade && !doingDopeyGesture {
            targetShiftX = arcadeStick.point.x * 0.30
            targetBodyTilt = arcadeStick.point.x * 0.006
        }
        bodyShiftX.target = max(-16, min(16, targetShiftX))
        targetBodyTilt += gestureHead.tilt * 0.55
        bodyTilt.target = max(-0.12, min(0.12, targetBodyTilt)) + (dopey ? 0 : 0.075 * sway) + 0.045 * sleepiness
        // Coiled low hunting crouch when chasing a fast-moving mouse!
        bodySquash.target = 1.0 - hunt * 0.11 - 0.20 * sleepiness - 0.12 * lean

        headOffsetY.target = (leftPawTyping || rightPawTyping) ? 3.5 : (sleepiness * 28 + hunt * 10.5 + lean * 45 + bite * 5.5 + lift * 4 + pulse * 9 + gestureHead.drop)
        earFold.target = max(petHeadIntensity * 0.7, petBodyIntensity * 0.45) + sleepiness * 0.25 - hunt * 0.18
        earTwitch.target = 0

        bodyShiftX.step(dt: dt, stiffness: 140, damping: 18)
        bodyTilt.step(dt: dt, stiffness: 140, damping: 18)
        bodySquash.step(dt: dt, stiffness: 280, damping: 18)
        headOffsetY.step(dt: dt, stiffness: 180, damping: 19)
        earFold.step(dt: dt, stiffness: 120, damping: 16)
        earTwitch.step(dt: dt, stiffness: 320, damping: 16)
        let earTarget: CGFloat = time < airplaneUntil || (bellyHovered && desk) ? 1.40 : (angry ? 0.58 : 0)
        let backEars = Self.ease((turn - 0.65) / 0.35)
        let earAngle = earTarget + (0.58 - earTarget) * backEars
        leftEar.target = earAngle
        rightEar.target = earAngle
        leftEar.step(dt: dt, stiffness: 190, damping: 19)
        rightEar.step(dt: dt, stiffness: 190, damping: 19)

        // 5. 2.5D Head Turning (Yaw + Pitch) + Eye Tracking!
        // Head smoothly turns in pseudo-3D toward the mouse cursor (or subtly nods down toward keys while typing).
        let typingRecently = !activeKeys.isEmpty || (now - curLastKeyTime < 0.65)
        let mouseMovedRecently = (now - curLastMouseMove < 1.2)
        let eyeOriginCanvas = CGPoint(x: CatPhysics.seatAnchor.x + bodyShiftX.value, y: 124 + headOffsetY.value)

        // Compute normalized vector from cat head to real macOS screen cursor
        let headScreenX = windowFrameInScreen.minX + eyeOriginCanvas.x * scale
        let headScreenY = windowFrameInScreen.maxY - eyeOriginCanvas.y * scale
        let cursor = curMouseLoc
        let curDx = (cursor.x - headScreenX) / 290.0
        let curDy = -(cursor.y - headScreenY) / 260.0
        let curMag = hypot(curDx, curDy)
        let clampedCur = min(1.0, curMag)
        let cursorNX = curMag > 0.001 ? (curDx / curMag) * clampedCur : 0
        let cursorNY = curMag > 0.001 ? (curDy / curMag) * clampedCur : 0

        let lookTargetX: CGFloat
        let lookTargetY: CGFloat
        if arcade && !doingDopeyGesture {
            lookTargetX = arcadeStick.point.x * 0.026
            lookTargetY = -0.12 + 0.055 * arcadePressure
        } else if typingRecently && !mouseMovedRecently, let keyPt = lastTypedPoint {
            let kdx = max(-0.85, min(0.85, (keyPt.x - eyeOriginCanvas.x) / 95.0))
            let kdy = max(-0.3, min(0.92, (keyPt.y - eyeOriginCanvas.y) / 125.0))
            lookTargetX = kdx
            lookTargetY = kdy
        } else if typingRecently && mouseMovedRecently, let keyPt = lastTypedPoint {
            // Blend mouse & typing naturally when gaming with mouse + keyboard!
            let kdx = max(-0.85, min(0.85, (keyPt.x - eyeOriginCanvas.x) / 95.0))
            lookTargetX = cursorNX * 0.72 + kdx * 0.28
            lookTargetY = cursorNY * 0.72 + 0.25
        } else {
            lookTargetX = cursorNX
            lookTargetY = cursorNY
        }

        // Head turns smoothly in 2.5D (yaw & pitch) — locks on faster during hunting mode!
        if feeding {
            headYaw.target = 0.22 * lift
            headPitch.target = 0.51 * lean + 0.16 * lift
        } else if music {
            headYaw.target = 0.48 * sway
            headPitch.target = 0.12 + 0.72 * pulse
        } else if sleeping || ignoring {
            headYaw.target = 0
            headPitch.target = 0.45 * sleepiness
        } else {
            headYaw.target = max(-1.0, min(1.0, lookTargetX))
            headPitch.target = max(-1.0, min(1.0, lookTargetY))
        }
        let headStiff: CGFloat = 88 + hunt * 75
        let headDamp: CGFloat = 14.5 + hunt * 4.5
        headYaw.step(dt: dt, stiffness: headStiff, damping: headDamp)
        headPitch.step(dt: dt, stiffness: headStiff, damping: headDamp)

        // Subtle natural head roll coupled to head turning (curious tilt / locked-on hunt!)
        if now - lastPetTime > 0.28 {
            headTilt.target = bodyTilt.target * 0.45 + headYaw.value * (0.11 + hunt * 0.05) - headYaw.value * headPitch.value * 0.04
        }
        if doingDopeyGesture { headTilt.target = gestureHead.tilt }
        if !desk { headTilt.target = 0.115 * sleepiness + 0.21 * lift + 0.24 * sway }
        if arcade && !doingDopeyGesture { headTilt.target = arcadeStick.point.x * 0.008 + 0.022 * arcadePressure }
        let shakeAge = time - lastEarShake
        if desk && shakeAge < 0.5 { headTilt.velocity += dt * 115 * cos(shakeAge * 30) * (1 - 2 * shakeAge) }
        headTilt.step(dt: dt, stiffness: 130, damping: 16)

        // Eyes lead slightly ahead of the 2.5D head turn
        eyeGaze.target = CGPoint(x: lookTargetX * 0.88, y: lookTargetY * 0.85)
        eyeGaze.step(dt: dt, stiffness: 210 + hunt * 80, damping: 21)

        // Pupil dilation
        if hindFootActive.value > 0.3 {
            pupilScale.target = 1.24
        } else if now - curLastKeyTime > 0.45 && now - curLastMouseClick > 0.45 {
            pupilScale.target = 1.0
        }
        pupilScale.step(dt: dt, stiffness: 150, damping: 18)

        // 6. Blinking (cats lock eyes without blinking during hunting mode!)
        if isBlinking {
            blinkTimer += dt
            let blinkDuration: CGFloat = 0.14
            if blinkTimer >= blinkDuration {
                isBlinking = false
                blinkProgress = 0
                nextBlinkTime = time + CGFloat.random(in: 2.6...5.6)
            } else {
                let p = blinkTimer / blinkDuration
                blinkProgress = sin(p * .pi)
            }
        } else if time >= nextBlinkTime && sleepiness < 0.5 && petHeadIntensity < 0.3 && hunt < 0.35 {
            isBlinking = true
            blinkTimer = 0
        }

        // 7. Tail wag speed reacts organically to energy / petting / hunting tension!
        tailImpatience = dopey ? 0 : tailImpatience + ((impatient ? 1 : 0) - tailImpatience) * min(1, dt * 6)
        tailSlapTime = impatient || tailImpatience > 0.01 ? tailSlapTime + dt : 0
        let kpmBoost = desk ? min(2.4, CGFloat(engine.currentKPM) / 160.0) : 0
        let petBoost = max(petHeadIntensity, petBodyIntensity) * 1.4
        let huntBoost = hunt * 2.8
        let tailSpeed: CGFloat = 1.85 + kpmBoost + petBoost + huntBoost - sleepiness * 1.1
        if !dopey { tailPhase += dt * tailSpeed }

        // 8. Update per-keycap glows
        let visualActive: Set<PhysicalKey> = desk && !tabletMode && !arcade ? Set(activeKeys.map { $0.visualKey }) : []
        for (k, v) in keyGlow {
            if visualActive.contains(k) {
                keyGlow[k] = 1.0
            } else {
                let next = v - dt * 4.5
                if next <= 0.01 { keyGlow.removeValue(forKey: k) }
                else { keyGlow[k] = next }
            }
        }
        for vk in visualActive {
            keyGlow[vk] = 1.0
        }

        // 9. Update floating keycaps
        for i in floatingKeycaps.indices.reversed() {
            floatingKeycaps[i].age += dt
            floatingKeycaps[i].position.x += floatingKeycaps[i].velocity.x * dt
            floatingKeycaps[i].position.y += floatingKeycaps[i].velocity.y * dt
            floatingKeycaps[i].velocity.x *= (1.0 - dt * 2.4)
            floatingKeycaps[i].velocity.y *= (1.0 - dt * 1.6)
            if floatingKeycaps[i].age >= floatingKeycaps[i].lifetime {
                floatingKeycaps.remove(at: i)
            }
        }

        // 10. Update purr / sleep particles
        for i in sparkles.indices.reversed() {
            sparkles[i].age += dt
            sparkles[i].position.x += sparkles[i].velocity.x * dt
            sparkles[i].position.y += sparkles[i].velocity.y * dt
            if sparkles[i].age >= sparkles[i].lifetime {
                sparkles.remove(at: i)
            }
        }
    }
}
