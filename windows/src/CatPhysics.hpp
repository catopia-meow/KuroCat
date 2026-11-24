#pragma once
#define NOMINMAX
#include <windows.h>
#include <cmath>
#include <vector>
#include <deque>
#include <unordered_map>
#include <random>
#include <algorithm>
#include "KeyMap.hpp"
#include "InputEngine.hpp"

enum class CatCoatTheme : int {
    ClassicBlack = 0,
    Siamese = 1,
    Shorthair = 2,
    OrangeWhite = 3,
    OrcaCat = 4
};

enum class KeyboardTheme : int {
    ClassicMilk = 0,
    MidnightHacker = 1,
    RetroCream = 2
};

enum class EyeColorTheme : int {
    NeonChartreuse = 0,
    SiameseBlue = 1,
    WarmAmber = 2,
    IceCyan = 3,
    SoftAmber = 4,
    ReferenceHazel = 5
};

struct CatSettings {
    bool tabletMode = true;
    bool autoDeskMode = true;
    bool showKeyBadges = true;
    bool showMouseBadges = true;
    bool showControls = true;
    bool showKeyLegends = true;
    bool footEasterEgg = true;
    bool petInteraction = true;
    bool fullClickThrough = false;
    bool alwaysOnTop = true;
    bool runAsAdmin = false; // relaunch elevated so keys typed into admin-level games are visible (UIPI)
    float scale = 1.0f;
    CatCoatTheme coatTheme = CatCoatTheme::OrcaCat;
    KeyboardTheme keyboardTheme = KeyboardTheme::ClassicMilk;
    EyeColorTheme eyeTheme = EyeColorTheme::ReferenceHazel;
    int posX = -99999;
    int posY = -99999;

    void loadFromRegistry() {
        HKEY hKey = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\KuroCat", 0, KEY_READ, &hKey) != ERROR_SUCCESS) return;
        auto readDword = [&](const wchar_t* name, DWORD defVal) -> DWORD {
            DWORD val = defVal, sz = sizeof(val);
            if (RegQueryValueExW(hKey, name, nullptr, nullptr, (LPBYTE)&val, &sz) == ERROR_SUCCESS) return val;
            return defVal;
        };
        autoDeskMode = readDword(L"AutoDeskMode", 1) != 0;
        tabletMode = readDword(L"TabletMode", 1) != 0;
        showKeyBadges    = readDword(L"ShowKeyBadges", 1) != 0;
        showMouseBadges  = readDword(L"ShowMouseBadges", 1) != 0;
        showControls     = readDword(L"ShowControls", 1) != 0;
        showKeyLegends   = readDword(L"ShowKeyLegends", 1) != 0;
        footEasterEgg    = readDword(L"FootEasterEgg", 1) != 0;
        petInteraction   = readDword(L"PetInteraction", 1) != 0;
        fullClickThrough = readDword(L"FullClickThrough", 0) != 0;
        alwaysOnTop      = readDword(L"AlwaysOnTop", 1) != 0;
        runAsAdmin       = readDword(L"RunAsAdmin", 0) != 0;
        scale            = std::clamp((float)readDword(L"ScalePct", 100) / 100.0f, 0.5f, 2.0f);
        coatTheme        = (CatCoatTheme)std::clamp((int)readDword(L"CoatTheme", (DWORD)coatTheme), 0, (int)CatCoatTheme::OrcaCat);
        keyboardTheme    = (KeyboardTheme)std::clamp((int)readDword(L"KeyboardTheme", 0), 0, 2);
        eyeTheme         = (EyeColorTheme)std::clamp((int)readDword(L"EyeTheme", (DWORD)eyeTheme), 0, 5);
        DWORD hasPos     = readDword(L"HasPos", 0);
        if (hasPos) {
            posX = (int)readDword(L"PosX", 0);
            posY = (int)readDword(L"PosY", 0);
        }
        RegCloseKey(hKey);
    }

    void saveToRegistry() const {
        HKEY hKey = nullptr;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\KuroCat", 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) != ERROR_SUCCESS) return;
        auto writeDword = [&](const wchar_t* name, DWORD val) {
            RegSetValueExW(hKey, name, 0, REG_DWORD, (const BYTE*)&val, sizeof(val));
        };
        writeDword(L"AutoDeskMode", autoDeskMode);
        if (!autoDeskMode) writeDword(L"TabletMode", tabletMode);
        writeDword(L"ShowKeyBadges", showKeyBadges ? 1 : 0);
        writeDword(L"ShowMouseBadges", showMouseBadges ? 1 : 0);
        writeDword(L"ShowControls", showControls ? 1 : 0);
        writeDword(L"ShowKeyLegends", showKeyLegends ? 1 : 0);
        writeDword(L"FootEasterEgg", footEasterEgg ? 1 : 0);
        writeDword(L"PetInteraction", petInteraction ? 1 : 0);
        writeDword(L"FullClickThrough", fullClickThrough ? 1 : 0);
        writeDword(L"AlwaysOnTop", alwaysOnTop ? 1 : 0);
        writeDword(L"RunAsAdmin", runAsAdmin ? 1 : 0);
        writeDword(L"ScalePct", (DWORD)(scale * 100.0f + 0.5f));
        writeDword(L"CoatTheme", (DWORD)coatTheme);
        writeDword(L"KeyboardTheme", (DWORD)keyboardTheme);
        writeDword(L"EyeTheme", (DWORD)eyeTheme);
        if (posX != -99999 && posY != -99999) {
            writeDword(L"HasPos", 1);
            writeDword(L"PosX", (DWORD)posX);
            writeDword(L"PosY", (DWORD)posY);
        }
        RegCloseKey(hKey);
    }
};

struct Spring1D {
    float value = 0.0f;
    float velocity = 0.0f;
    float target = 0.0f;

    explicit Spring1D(float v = 0.0f) : value(v), velocity(0.0f), target(v) {}

    void step(float dt, float stiffness = 260.0f, float damping = 22.0f) {
        float force = (target - value) * stiffness;
        float damp = velocity * damping;
        velocity += (force - damp) * dt;
        value += velocity * dt;
    }
};

struct Spring2D {
    Spring1D x;
    Spring1D y;

    explicit Spring2D(Pt p = {0.0f, 0.0f}) : x(p.x), y(p.y) {}
    Pt point() const { return { x.value, y.value }; }
    void setTarget(Pt p) { x.target = p.x; y.target = p.y; }
    void step(float dt, float stiffness = 340.0f, float damping = 25.0f) {
        x.step(dt, stiffness, damping);
        y.step(dt, stiffness, damping);
    }
};

struct FloatingKeycap {
    std::wstring label;
    Pt position;
    Pt velocity;
    float age = 0.0f;
    float lifetime = 1.25f;
    float rotation = 0.0f;
    bool mouse = false;
};

struct PurrSparkle {
    Pt position;
    Pt velocity;
    float age = 0.0f;
    float lifetime = 0.9f;
    bool isSleepZ = false;
};

class CatPhysics {
public:
    static constexpr float CanvasWidth  = 360.0f;
    static constexpr float CanvasHeight = 320.0f;
    static constexpr Pt SeatAnchor      = { 184.0f, 226.0f };
    static constexpr Pt LeftPawRest     = { 164.0f, 235.0f };
    static constexpr Pt RightPawRest    = { 204.0f, 235.0f };
    static constexpr Pt HindFootRest    = { 214.0f, 234.0f };
    static constexpr Pt MouseBaseCenter = { 78.0f, 258.0f };

    Spring2D leftPaw{LeftPawRest};
    Spring2D rightPaw{RightPawRest};
    Spring2D hindFoot{HindFootRest};
    Spring1D hindFootActive{0.0f};

    Spring1D bodyShiftX{0.0f};
    Spring1D bodyTilt{0.0f};
    Spring1D bodySquash{1.0f};
    Spring1D headTilt{0.0f};
    Spring1D headYaw{0.0f};
    Spring1D headPitch{0.0f};
    Spring1D headOffsetY{0.0f};
    Spring1D earFold{0.0f};
    Spring1D earTwitch{0.0f};

    Spring2D eyeGaze{{0.0f, 0.0f}};
    Spring1D pupilScale{1.0f};
    Spring1D huntIntensity{0.0f};
    Spring2D mouseOffset{{0.0f, 0.0f}};
    Spring1D mousePressLeft{0.0f};
    Spring1D mousePressRight{0.0f};

    bool leftPawOnMouse = false;
    bool leftPawTyping = false;
    bool rightPawTyping = false;

    std::unordered_map<PhysicalKey, float> keyGlow;
    std::vector<FloatingKeycap> floatingKeycaps;
    std::vector<PurrSparkle> sparkles;

    float time = 0.0f;
    float tailPhase = 0.0f;

    float petHeadIntensity = 0.0f;
    float petBodyIntensity = 0.0f;
    int totalPetSessions = 0;
    float sleepiness = 0.0f;

    static float ease(float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    bool resting() const { return manualSleep_ || sleepiness >= 0.4f; }
    bool feeding() const { return treatAge_ >= 0.0f; }
    bool listening() const { return listening_; }
    float headphones() const { return headsetFit_; }
    float musicEyes() const { return musicEyes_; }
    bool usingDesk() const { return !resting() && !feeding() && !listening(); }
    float breathingOffset() const { return std::sin(breathPhase_) * 1.6f; }
    float treatAge() const { return treatAge_; }
    float washLift() const {
        if (!feeding()) return 0.0f;
        return ease((treatAge_ - 2.85f) / 0.42f) * ease((5.1f - treatAge_) / 0.44f);
    }

    void goToSleep() {
        treatAge_ = -1.0f;
        manualSleep_ = true;
        clearDeskPose();
    }

    void wakeUp() {
        manualSleep_ = false;
        lastPetTime_ = nowSeconds();
        sleepZCooldown_ = 0.0f;
        sparkles.erase(std::remove_if(sparkles.begin(), sparkles.end(),
            [](const PurrSparkle& particle) { return particle.isSleepZ; }), sparkles.end());
    }

    void offerTreat() {
        if (feeding()) return;
        wakeUp();
        treatAge_ = 0.0f;
        clearDeskPose();
    }

    void listenToMusic(bool enabled) {
        listening_ = enabled;
        if (!enabled) return;
        wakeUp();
        treatAge_ = -1.0f;
        previousPulse_ = 0.0f;
        clearDeskPose();
    }

    float effectiveEyeClosure() const {
        float petClose = std::max(petHeadIntensity, petBodyIntensity * 0.62f);
        return std::min(1.0f, std::max({blinkProgress_, petClose, sleepiness * 1.12f, washLift(), musicEyes_ * 1.13f}));
    }

    bool isHappyClosedEyes() const { return petHeadIntensity > 0.45f; }
    bool isBlissfulSquint() const { return petBodyIntensity > 0.35f && !isHappyClosedEyes(); }

    // Track the virtual desktop, including monitors with negative origins.
    struct InkPoint { Pt pos; bool startsStroke; };
    std::deque<InkPoint> ink;
    Pt tabletTip{183.0f, 263.0f};
    bool penDown = false;
    static constexpr float padLeft = 134, padRight = 311, padTop = 240, padBottom = 285;

    void sampleTablet(POINT cursor, bool down) {
        int width = std::max(1, GetSystemMetrics(SM_CXVIRTUALSCREEN) - 1);
        int height = std::max(1, GetSystemMetrics(SM_CYVIRTUALSCREEN) - 1);
        float u = std::clamp(float(cursor.x - GetSystemMetrics(SM_XVIRTUALSCREEN)) / width, 0.f, 1.f);
        float v = std::clamp(float(cursor.y - GetSystemMetrics(SM_YVIRTUALSCREEN)) / height, 0.f, 1.f);
        tabletTip = {padLeft + u*(padRight-padLeft), padTop + v*(padBottom-padTop)};
        if (down || penDown) {
            bool start = !penDown || ink.empty();
            if (start || std::hypot(ink.back().pos.x-tabletTip.x, ink.back().pos.y-tabletTip.y) > 0.18f) {
                ink.push_back({tabletTip, start});
                if (ink.size() > 2048) { ink.pop_front(); ink.front().startsStroke = true; }
            }
        }
        penDown = down;
    }

    Pt penGrip() const { return {tabletTip.x + 6, tabletTip.y - (penDown ? 11.f : 15.f)}; }

    // Nine display keys fit beside the tablet. Reuse released slots for other shortcuts.
    struct TabletKey {
        PhysicalKey key;
        double pressedAt = -10.0;
        bool held = false;
    };
    static constexpr int tabletKeyCount = 9;
    TabletKey tabletKeys[tabletKeyCount] = {{PhysicalKey::LeftControl}, {PhysicalKey::LeftShift},
        {PhysicalKey::LeftOption}, {PhysicalKey::Z}, {PhysicalKey::B}, {PhysicalKey::Space},
        {PhysicalKey::X}, {PhysicalKey::C}, {PhysicalKey::V}};
    int latestTabletKey = -1;

    static constexpr float tabletTop = 230.f, tabletHeight = 62.f, tabletDepth = 4.f;
    static constexpr float keypadLeft = 45.f, keyPitch = 18.f, keyHalf = 7.75f;
    static Pt tabletKeyCenter(int slot) {
        return {keypadLeft + tabletHeight * 0.5f + keyPitch * (slot % 3 - 1),
                tabletTop + tabletHeight * 0.5f + keyPitch * (slot / 3 - 1)};
    }

    static int tabletKeyAt(Pt point) {
        for (int i = 0; i < tabletKeyCount; ++i) {
            Pt center = tabletKeyCenter(i);
            if (std::abs(point.x-center.x) <= keyHalf && std::abs(point.y-center.y) <= keyHalf)
                return i;
        }
        return -1;
    }

    void pressTabletKey(PhysicalKey key, double now) {
        int choice = -1;
        for (int i = 0; i < tabletKeyCount; ++i) {
            if (tabletKeys[i].key == key) { choice = i; break; }
            if (!tabletKeys[i].held && (choice < 0 || tabletKeys[i].pressedAt < tabletKeys[choice].pressedAt))
                choice = i;
        }
        if (choice < 0) {
            choice = 0;
            for (int i = 1; i < tabletKeyCount; ++i)
                if (tabletKeys[i].pressedAt < tabletKeys[choice].pressedAt) choice = i;
        }
        tabletKeys[choice] = {key, now, true};
        latestTabletKey = choice;
    }

    void triggerKeyDown(PhysicalKey key, const CatSettings& settings) {
        if (manualSleep_ || feeding() || listening()) return;
        double now = nowSeconds();
        if (settings.tabletMode) {
            pressTabletKey(key, now);
            leftPaw.y.velocity -= 85.f;
            earTwitch.velocity -= 1.5f;
            if (settings.showKeyBadges)
                spawnFloatingKeycap(keyBadgeLabel(key), tabletKeyCenter(latestTabletKey), now);
            return;
        }
        PhysicalKey vk = visualKeyOf(key);
        keyGlow[vk] = 1.0f;

        Pt pt = KeyboardLayout::canvasPointForKey(vk);
        hasLastTypedPoint_ = true;
        lastTypedPoint_ = pt;

        if (prefersViewerLeftPaw(key)) {
            recentLeftKey_ = vk;
            recentLeftTime_ = now;
            leftPaw.y.velocity -= 180.0f;
        } else {
            recentRightKey_ = vk;
            recentRightTime_ = now;
            rightPaw.y.velocity -= 180.0f;
        }

        bodySquash.velocity -= 1.3f;
        earTwitch.velocity += (prefersViewerLeftPaw(key) ? -3.8f : 3.8f);
        headTilt.velocity += (prefersViewerLeftPaw(key) ? -1.4f : 1.4f);
        pupilScale.target = 1.16f;

        if (settings.showKeyBadges) {
            spawnFloatingKeycap(keyBadgeLabel(key), pt, now);
        }
    }

    void triggerMouseClick(int button, bool isDown, const CatSettings& settings) {
        if (manualSleep_ || feeding() || listening()) return;
        if (settings.tabletMode) {
            POINT cursor;
            if (button == 0 && GetCursorPos(&cursor)) sampleTablet(cursor, isDown);
            return;
        }
        if (isDown) {
            double now = nowSeconds();
            if (button == 0) {
                mousePressLeft.target = 1.0f;
                leftPaw.y.velocity += 130.0f;
            } else {
                mousePressRight.target = 1.0f;
                leftPaw.y.velocity += 110.0f;
            }
            pupilScale.target = 1.12f;
            earTwitch.velocity -= 2.2f;
            if (settings.showMouseBadges) {
                std::wstring lbl = (button == 0) ? L"\u5DE6\u952E" : (button == 1 ? L"\u53F3\u952E" : L"\u4E2D\u952E");
                Pt pt = { MouseBaseCenter.x + mouseOffset.point().x, MouseBaseCenter.y + mouseOffset.point().y - 6.0f };
                spawnFloatingKeycap(lbl, pt, now, true);
            }
        } else {
            if (button == 0) mousePressLeft.target = 0.0f;
            else mousePressRight.target = 0.0f;
        }
    }

    void touchOrca(bool belly, bool left) {
        if (!usingDesk()) return;
        lastPetTime_ = nowSeconds();
        sleepiness = 0.0f;
        if (belly) {
            airplaneUntil_ = time + 0.65f;
            annoyedUntil_ = time + 2.4f;
            petHeadIntensity = petBodyIntensity = 0.0f;
            return;
        }
        if (time - lastEarTouch_ < 0.24f) return;
        earFlick_[left ? 0 : 1].velocity += 12.0f;
        if (time - lastEarTouch_ < 1.1f && time - lastShake_ > 0.8f) {
            lastShake_ = time;
            annoyedUntil_ = time + 1.6f;
        }
        lastEarTouch_ = time;
    }

    float orcaEarAngle(bool left) const {
        int side = left ? 0 : 1;
        return orcaAngle_[side].value + earFlick_[side].value;
    }

    void registerPetStroke(Pt canvasPoint, float delta, bool isHead, const CatSettings& settings) {
        if (!settings.petInteraction || !usingDesk()) return;
        double now = nowSeconds();
        lastPetTime_ = now;
        sleepiness = 0.0f;

        float energy = std::min(delta / 26.0f, 0.35f);
        if (isHead) {
            petHeadIntensity = std::min(1.0f, petHeadIntensity + energy + 0.08f);
            petBodyIntensity = std::max(petBodyIntensity, petHeadIntensity * 0.6f);
        } else {
            petBodyIntensity = std::min(1.0f, petBodyIntensity + energy + 0.07f);
        }

        petStrokeAccum_ += delta;
        if (!isCurrentlyPetting_ && petStrokeAccum_ > 45.0f) {
            isCurrentlyPetting_ = true;
            totalPetSessions++;
        }

        float dx = (canvasPoint.x - SeatAnchor.x) / 60.0f;
        headTilt.target = std::clamp(dx * 0.16f, -0.18f, 0.18f);
        headOffsetY.velocity += 18.0f;
    }

    void step(float rawDt, const RECT& windowRect, InputEngine& engine, const CatSettings& settings,
              bool overControl = false, float musicPulse = 0.0f, bool soundPlaying = false) {
        float dt = std::min(rawDt, 1.0f / 25.0f);
        time += dt;
        if (feeding()) {
            treatAge_ += dt;
            if (treatAge_ >= 5.24f) treatAge_ = -1.0f;
        }
        engine.pollMouse(dt);

        double now = nowSeconds();
        const auto& activeKeys = engine.activeKeys;
        const auto& orderedKeys = engine.pressedOrder;

        // 1. Petting decay & sparkles
        if (now - lastPetTime_ > 0.28) {
            petHeadIntensity = std::max(0.0f, petHeadIntensity - dt * 1.4f);
            petBodyIntensity = std::max(0.0f, petBodyIntensity - dt * 1.2f);
            if (petHeadIntensity < 0.05f && petBodyIntensity < 0.05f) {
                isCurrentlyPetting_ = false;
                petStrokeAccum_ = 0.0f;
            }
        } else if (petHeadIntensity > 0.55f || petBodyIntensity > 0.55f) {
            petSparkleCooldown_ -= dt;
            if (petSparkleCooldown_ <= 0.0f) {
                petSparkleCooldown_ = 0.42f;
                sparkles.push_back({
                    { SeatAnchor.x + randRange(-44.0f, 44.0f), 88.0f + randRange(-14.0f, 18.0f) },
                    { randRange(-10.0f, 10.0f), -24.0f },
                    0.0f, 0.9f, false
                });
            }
        }

        // 2. Sleepy idle detection
        double lastAny = std::max({engine.lastKeyActivityTime, engine.lastMouseMoveTime,
                                  engine.lastMouseClickTime, lastPetTime_});
        double idleSec = lastAny > 0.0 ? (now - lastAny) : 0.0;
        float targetSleep = manualSleep_ ? 1.0f : (activeKeys.empty() && !feeding() && !listening()
            ? ease((float)(idleSec - 45.0) / 6.0f) : 0.0f);
        sleepiness += (targetSleep - sleepiness) * dt * (targetSleep > sleepiness ? 3.0f : 5.0f);
        breathPhase_ += dt * (2.1f - 0.54f * sleepiness);
        headsetFit_ += ((listening() ? 1.0f : 0.0f) - headsetFit_) * std::min(1.0f, dt * 12.0f);
        float close = listening() && soundPlaying ? 1.0f : 0.0f;
        if (close == 0.0f) musicPulse = 0.0f;
        musicEyes_ += (close - musicEyes_) * std::min(1.0f, dt * 12.0f);
        musicSwayPhase_ += dt * (4.3f + 1.8f * musicPulse);
        if (musicPulse > 0.23f && previousPulse_ <= 0.23f) musicSide_ = -musicSide_;
        previousPulse_ = musicPulse;
        float sway = musicSide_ * musicPulse;
        float drift = std::sin(musicSwayPhase_) * musicEyes_;
        if (sleepiness > 0.86f && targetSleep > 0.86f) {
            sleepZCooldown_ -= dt;
            if (sleepZCooldown_ <= 0.0f) {
                sleepZCooldown_ = 2.8f;
                sparkles.push_back({ { SeatAnchor.x + 39.0f, 114.0f + headOffsetY.value },
                                     { 9.0f, -20.0f }, 0.0f, 1.4f, true });
            }
        }
        float lean = feeding() ? ease((treatAge_ - 0.25f) / 0.55f) * ease((2.73f - treatAge_) / 0.49f) : 0.0f;
        float bites = lean * (0.5f - 0.5f * std::cos(std::max(0.0f, treatAge_ - 0.8f) * 13.1f));
        float lift = washLift();
        float wipe = ease((treatAge_ - 3.31f) / 0.25f) * (0.5f - 0.5f * std::cos((treatAge_ - 3.31f) * 11.5f));

        // 2b. Fast Mouse Movement -> Hunting Mode (big round boba pupils + low crouch!)
        float mouseSpeed = std::hypot(engine.mouseVelocity.x, engine.mouseVelocity.y);
        if (usingDesk() && mouseSpeed > 460.0f && petHeadIntensity < 0.25f && petBodyIntensity < 0.25f) {
            float boost = std::min(1.0f, (mouseSpeed - 460.0f) / 1100.0f);
            huntIntensity.target = std::min(1.0f, huntIntensity.target + (0.45f + boost * 1.15f) * dt);
            sleepiness = 0.0f;
        } else {
            huntIntensity.target = std::max(0.0f, huntIntensity.target - dt * 0.65f);
        }
        huntIntensity.step(dt, 85.0f, 16.0f);
        float hunt = std::clamp(huntIntensity.value, 0.0f, 1.0f);

        // 3. Limb assignment (Left Paw, Right Paw, 3-Key Hind Foot!)
        constexpr double tapHoldWindow = 0.14;
        bool mouseRecent = (now - engine.lastMouseMoveTime < 2.2) || engine.leftMouseDown || engine.rightMouseDown;
        bool mouseActivelyUsed = (now - engine.lastMouseMoveTime < 0.75) || engine.leftMouseDown || engine.rightMouseDown;

        std::vector<PhysicalKey> leftCands, rightCands;
        for (PhysicalKey k : orderedKeys) {
            if (prefersViewerLeftPaw(k)) leftCands.push_back(k);
            else rightCands.push_back(k);
        }

        PhysicalKey leftKey = leftCands.empty() ? PhysicalKey::None : leftCands.back();
        PhysicalKey rightKey = rightCands.empty() ? PhysicalKey::None : rightCands.back();
        PhysicalKey footKey = PhysicalKey::None;

        if (leftKey == PhysicalKey::None && rightCands.size() >= 2 && !mouseActivelyUsed) {
            auto sorted = rightCands;
            std::sort(sorted.begin(), sorted.end(), [](PhysicalKey a, PhysicalKey b) {
                return KeyboardLayout::canvasPointForKey(a).x < KeyboardLayout::canvasPointForKey(b).x;
            });
            leftKey = sorted.front();
            rightKey = sorted.back();
        } else if (rightKey == PhysicalKey::None && leftCands.size() >= 2) {
            auto sorted = leftCands;
            std::sort(sorted.begin(), sorted.end(), [](PhysicalKey a, PhysicalKey b) {
                return KeyboardLayout::canvasPointForKey(a).x < KeyboardLayout::canvasPointForKey(b).x;
            });
            leftKey = sorted.front();
            rightKey = sorted.back();
        }

        if (settings.footEasterEgg) {
            if (orderedKeys.size() >= 3) {
                for (PhysicalKey k : orderedKeys) {
                    if (k != leftKey && k != rightKey) { footKey = k; break; }
                }
            } else if (mouseActivelyUsed && leftKey == PhysicalKey::None && rightCands.size() >= 2) {
                for (PhysicalKey k : rightCands) {
                    if (k != rightKey) { footKey = k; break; }
                }
            }
        }

        if (leftKey == PhysicalKey::None && recentLeftKey_ != PhysicalKey::None && now - recentLeftTime_ < tapHoldWindow) {
            leftKey = recentLeftKey_;
        }
        if (rightKey == PhysicalKey::None && recentRightKey_ != PhysicalKey::None && now - recentRightTime_ < tapHoldWindow) {
            rightKey = recentRightKey_;
        }

        // Subtle mini-mouse movement
        mouseOffset.setTarget({
            std::clamp(engine.mouseVelocity.x * 0.0045f, -5.5f, 5.5f),
            std::clamp(engine.mouseVelocity.y * 0.0045f, -4.5f, 4.5f)
        });
        mouseOffset.step(dt, 120.0f, 16.0f);
        mousePressLeft.step(dt, 400.0f, 24.0f);
        mousePressRight.step(dt, 400.0f, 24.0f);

        Pt liveMouse = { MouseBaseCenter.x + mouseOffset.point().x, MouseBaseCenter.y + mouseOffset.point().y };

        if (leftKey != PhysicalKey::None) {
            leftPaw.setTarget(KeyboardLayout::canvasPointForKey(leftKey));
            leftPawTyping = true;
            leftPawOnMouse = false;
        } else if (mouseRecent && sleepiness < 0.3f) {
            leftPaw.setTarget({ liveMouse.x + 2.0f, liveMouse.y - 3.0f });
            leftPawTyping = false;
            leftPawOnMouse = true;
        } else {
            leftPaw.setTarget(LeftPawRest);
            leftPawTyping = false;
            leftPawOnMouse = false;
        }

        if (rightKey != PhysicalKey::None) {
            rightPaw.setTarget(KeyboardLayout::canvasPointForKey(rightKey));
            rightPawTyping = true;
        } else {
            rightPaw.setTarget(RightPawRest);
            rightPawTyping = false;
        }

        if (footKey != PhysicalKey::None) {
            hindFoot.setTarget(KeyboardLayout::canvasPointForKey(footKey));
            hindFootActive.target = 1.0f;
        } else {
            hindFoot.setTarget(HindFootRest);
            hindFootActive.target = 0.0f;
        }

        if (settings.tabletMode && usingDesk()) {
            if (overControl) penDown = false;
            else sampleTablet(engine.lastCursorPos, engine.leftMouseDown);
            for (auto& cap : tabletKeys) cap.held = activeKeys.count(cap.key) != 0;
            int target = latestTabletKey;
            if (target >= 0 && !tabletKeys[target].held && now - tabletKeys[target].pressedAt > 0.32) {
                target = -1;
                for (int i = 0; i < tabletKeyCount; ++i)
                    if (tabletKeys[i].held && (target < 0 || tabletKeys[i].pressedAt > tabletKeys[target].pressedAt))
                        target = i;
            }
            Pt rest = {keypadLeft + tabletHeight - 5.f, tabletTop - 2.f};
            Pt aim = target < 0 ? rest : tabletKeyCenter(target);
            // Land on the upper rim so the paw doesn't cover the active label.
            if (target >= 0) aim.y -= keyHalf;
            leftPaw.setTarget(aim);
            rightPaw.setTarget(penGrip());
            leftPawOnMouse = rightPawTyping = false;
            leftPawTyping = target >= 0;
            hindFootActive.target = 0;
        }
        if (!usingDesk()) {
            leftPawOnMouse = leftPawTyping = rightPawTyping = false;
            hindFootActive.target = 0.0f;
            leftPaw.setTarget({SeatAnchor.x - 17.0f - 7.0f * lean, 231.0f - 7.0f * sleepiness});
            rightPaw.setTarget({SeatAnchor.x + 17.0f + 7.0f * lean + lift * (-7.0f + 17.0f * wipe),
                                231.0f - 7.0f * sleepiness - lift * (75.0f + 16.0f * wipe)});
        }
        leftPaw.step(dt, 360.0f, 24.0f);
        rightPaw.step(dt, 360.0f, 24.0f);
        hindFoot.step(dt, 320.0f, 22.0f);
        hindFootActive.step(dt, 260.0f, 22.0f);

        // 4. Body lean, squash & ear motion
        float targetShiftX = 0.0f, targetBodyTilt = 0.0f;
        if (leftPawOnMouse && !rightPawTyping) {
            targetShiftX = -10.0f;
            targetBodyTilt = -0.075f;
        } else if (leftPawOnMouse && rightPawTyping) {
            targetShiftX = -3.0f;
            targetBodyTilt = (rightPaw.point().x - 196.0f) * 0.0008f;
        } else if (leftPawTyping || rightPawTyping) {
            float avgX = ((leftPawTyping ? leftPaw.point().x : 184.0f) + (rightPawTyping ? rightPaw.point().x : 184.0f)) * 0.5f;
            targetShiftX = (avgX - 184.0f) * 0.14f;
            targetBodyTilt = (avgX - 184.0f) * 0.0011f;
        }
        bodyShiftX.target = std::clamp(targetShiftX, -16.0f, 16.0f);
        bodyTilt.target = std::clamp(targetBodyTilt, -0.12f, 0.12f) + 0.045f * sleepiness
            + 0.075f * sway + 0.041f * drift;
        bodySquash.target = 1.0f - hunt * 0.11f - 0.20f * sleepiness - 0.12f * lean;
        headOffsetY.target = (leftPawTyping || rightPawTyping) ? 3.5f
            : (sleepiness * 28.0f + hunt * 10.5f + lean * 45.0f + bites * 5.5f + lift * 4.0f + 9.0f * musicPulse);
        earFold.target = std::max(petHeadIntensity * 0.7f, petBodyIntensity * 0.45f) + sleepiness * 0.25f - hunt * 0.18f;
        earTwitch.target = 0.0f;

        bodyShiftX.step(dt, 140.0f, 18.0f);
        bodyTilt.step(dt, 140.0f, 18.0f);
        bodySquash.step(dt, 280.0f, 18.0f);
        headOffsetY.step(dt, 180.0f, 19.0f);
        if (settings.coatTheme == CatCoatTheme::OrcaCat) {
            float joy = std::max(petHeadIntensity, petBodyIntensity);
            for (int i = 0; i < 2; ++i) {
                float phase = time * 2.1f + i * 0.28f;
                float angle = 0.22f + 0.07f * std::sin(phase)
                            + bodyTilt.value * (i ? 1.0f : -1.0f);
                angle += joy * (-0.30f + 0.18f * std::sin(time * 7.0f + i));
                angle += sleepiness * (0.27f + 0.06f * std::sin(time * 1.3f + i));
                if (time < annoyedUntil_) angle = 0.83f + 0.13f * std::sin(time * 6.0f + i * 2.0f);
                if (time < airplaneUntil_) angle = 1.54f;
                orcaAngle_[i].target = angle;
                orcaAngle_[i].step(dt, 190.0f, 19.0f);
                earFlick_[i].step(dt, 420.0f, 9.0f);
            }
        }
        earFold.step(dt, 120.0f, 16.0f);
        earTwitch.step(dt, 320.0f, 16.0f);

        // 5. 2.5D Head Turning (Yaw + Pitch) & Eye Tracking (Windows screen Y is top-down!)
        bool typingRecently = !activeKeys.empty() || (now - engine.lastKeyActivityTime < 0.65);
        bool mouseMovedRecently = (now - engine.lastMouseMoveTime < 1.2);
        Pt eyeOrigin = { SeatAnchor.x + bodyShiftX.value, 124.0f + headOffsetY.value };

        float winW = std::max(1.0f, (float)(windowRect.right - windowRect.left));
        float winScale = std::max(0.2f, winW / CanvasWidth);
        float headScreenX = (float)windowRect.left + eyeOrigin.x * winScale;
        float headScreenY = (float)windowRect.top  + eyeOrigin.y * winScale;

        float curDx = ((float)engine.lastCursorPos.x - headScreenX) / 290.0f;
        float curDy = ((float)engine.lastCursorPos.y - headScreenY) / 260.0f;
        float curMag = std::hypot(curDx, curDy);
        float clampedCur = std::min(1.0f, curMag);
        float cursorNX = curMag > 0.001f ? (curDx / curMag) * clampedCur : 0.0f;
        float cursorNY = curMag > 0.001f ? (curDy / curMag) * clampedCur : 0.0f;

        float lookX = cursorNX, lookY = cursorNY;
        if (typingRecently && !mouseMovedRecently && hasLastTypedPoint_) {
            lookX = std::clamp((lastTypedPoint_.x - eyeOrigin.x) / 95.0f, -0.85f, 0.85f);
            lookY = std::clamp((lastTypedPoint_.y - eyeOrigin.y) / 125.0f, -0.3f, 0.92f);
        } else if (typingRecently && mouseMovedRecently && hasLastTypedPoint_) {
            float kdx = std::clamp((lastTypedPoint_.x - eyeOrigin.x) / 95.0f, -0.85f, 0.85f);
            lookX = cursorNX * 0.72f + kdx * 0.28f;
            lookY = cursorNY * 0.72f + 0.25f;
        }

        if (sleepiness > 0.45f) {
            headYaw.target = 0.0f;
            headPitch.target = 0.29f * sleepiness;
        } else if (listening()) {
            headYaw.target = 0.42f * sway + 0.21f * drift;
            headPitch.target = 0.12f + 0.72f * musicPulse;
        } else if (feeding()) {
            headYaw.target = 0.22f * lift;
            headPitch.target = 0.51f * lean + 0.16f * lift;
        } else {
            headYaw.target = std::clamp(lookX, -1.0f, 1.0f);
            headPitch.target = std::clamp(lookY, -1.0f, 1.0f);
        }
        float headStiff = 88.0f + hunt * 75.0f;
        float headDamp  = 14.5f + hunt * 4.5f;
        headYaw.step(dt, headStiff, headDamp);
        headPitch.step(dt, headStiff, headDamp);

        if (now - lastPetTime_ > 0.28) {
            headTilt.target = bodyTilt.target * 0.45f + headYaw.value * (0.11f + hunt * 0.05f) - headYaw.value * headPitch.value * 0.04f;
        }
        if (!usingDesk()) headTilt.target = 0.115f * sleepiness + 0.21f * lift;
        if (listening()) headTilt.target = 0.24f * sway + 0.11f * drift;
        float shakeAge = time - lastShake_;
        if (settings.coatTheme == CatCoatTheme::OrcaCat && shakeAge < 0.5f) {
            headTilt.velocity += dt * 115.0f * std::cos(shakeAge * 30.0f) * (1.0f - shakeAge * 2.0f);
        }
        headTilt.step(dt, 130.0f, 16.0f);

        eyeGaze.setTarget(listening() ? Pt{0.16f * sway, 0.1f} :
            (feeding() ? Pt{0.08f * lift, 0.65f * lean} : Pt{lookX * 0.88f, lookY * 0.85f}));
        eyeGaze.step(dt, 210.0f + hunt * 80.0f, 21.0f);

        if (hindFootActive.value > 0.3f) {
            pupilScale.target = 1.24f;
        } else if (now - engine.lastKeyActivityTime > 0.45 && now - engine.lastMouseClickTime > 0.45) {
            pupilScale.target = 1.0f;
        }
        pupilScale.step(dt, 150.0f, 18.0f);

        // 6. Blinking (no blinking during locked-on hunting!)
        if (isBlinking_) {
            blinkTimer_ += dt;
            constexpr float blinkDur = 0.14f;
            if (blinkTimer_ >= blinkDur) {
                isBlinking_ = false;
                blinkProgress_ = 0.0f;
                nextBlinkTime_ = time + randRange(2.6f, 5.6f);
            } else {
                blinkProgress_ = std::sin((blinkTimer_ / blinkDur) * 3.14159265f);
            }
        } else if (time >= nextBlinkTime_ && sleepiness < 0.5f && petHeadIntensity < 0.3f && hunt < 0.35f) {
            isBlinking_ = true;
            blinkTimer_ = 0.0f;
        }

        // 7. Tail wag speed
        float kpmBoost = std::min(2.4f, (float)engine.currentKPM() / 160.0f);
        float petBoost = std::max(petHeadIntensity, petBodyIntensity) * 1.4f;
        float tailSpeed = (1.85f + kpmBoost + petBoost + hunt * 2.8f) * (1.0f - sleepiness * 0.86f);
        tailPhase += dt * tailSpeed;

        // 8. Keycap glow decay
        std::unordered_set<PhysicalKey> visualActive;
        for (PhysicalKey k : activeKeys) visualActive.insert(visualKeyOf(k));
        for (auto it = keyGlow.begin(); it != keyGlow.end(); ) {
            if (visualActive.count(it->first)) {
                it->second = 1.0f;
                ++it;
            } else {
                it->second -= dt * 4.5f;
                if (it->second <= 0.01f) it = keyGlow.erase(it);
                else ++it;
            }
        }
        for (PhysicalKey vk : visualActive) keyGlow[vk] = 1.0f;

        // 9. Floating keycaps
        for (int i = (int)floatingKeycaps.size() - 1; i >= 0; --i) {
            auto& b = floatingKeycaps[i];
            b.age += dt;
            b.position.x += b.velocity.x * dt;
            b.position.y += b.velocity.y * dt;
            b.velocity.x *= (1.0f - dt * 2.4f);
            b.velocity.y *= (1.0f - dt * 1.6f);
            if (b.age >= b.lifetime) floatingKeycaps.erase(floatingKeycaps.begin() + i);
        }

        // 10. Sparkles / Sleep Z
        for (int i = (int)sparkles.size() - 1; i >= 0; --i) {
            auto& sp = sparkles[i];
            sp.age += dt;
            sp.position.x += sp.velocity.x * dt;
            sp.position.y += sp.velocity.y * dt;
            if (sp.age >= sp.lifetime) sparkles.erase(sparkles.begin() + i);
        }
    }

private:
    bool manualSleep_ = false;
    bool listening_ = false;
    float headsetFit_ = 0.0f;
    float musicEyes_ = 0.0f;
    float musicSwayPhase_ = 0.0f;
    float previousPulse_ = 0.0f;
    float musicSide_ = 1.0f;
    float treatAge_ = -1.0f;
    float breathPhase_ = 0.0f;

    void clearDeskPose() {
        penDown = false;
        latestTabletKey = -1;
        for (auto& key : tabletKeys) key.held = false;
        mousePressLeft.target = mousePressRight.target = 0.0f;
        huntIntensity.target = 0.0f;
        petHeadIntensity = petBodyIntensity = 0.0f;
        floatingKeycaps.clear();
    }

    Spring1D orcaAngle_[2]{Spring1D(0.22f), Spring1D(0.22f)};
    Spring1D earFlick_[2];
    float airplaneUntil_ = -1.0f, annoyedUntil_ = -1.0f;
    float lastEarTouch_ = -10.0f, lastShake_ = -10.0f;
    float nextBlinkTime_ = 3.2f;
    float blinkProgress_ = 0.0f;
    bool isBlinking_ = false;
    float blinkTimer_ = 0.0f;

    float petStrokeAccum_ = 0.0f;
    double lastPetTime_ = 0.0;
    bool isCurrentlyPetting_ = false;
    float petSparkleCooldown_ = 0.0f;
    float sleepZCooldown_ = 0.0f;

    PhysicalKey recentLeftKey_ = PhysicalKey::None;
    double recentLeftTime_ = 0.0;
    PhysicalKey recentRightKey_ = PhysicalKey::None;
    double recentRightTime_ = 0.0;
    bool hasLastTypedPoint_ = false;
    Pt lastTypedPoint_ = { 184.0f, 260.0f };

    int badgeCascade_ = 0;
    double lastBadgeTime_ = 0.0;

    static float randRange(float a, float b) {
        static thread_local std::mt19937 rng{ std::random_device{}() };
        std::uniform_real_distribution<float> dist(a, b);
        return dist(rng);
    }

    void spawnFloatingKeycap(const std::wstring& label, Pt pt, double now, bool mouse = false) {
        if (label.empty()) return;
        if (now - lastBadgeTime_ < 0.38) badgeCascade_ = (badgeCascade_ + 1) % 6;
        else badgeCascade_ = 0;
        lastBadgeTime_ = now;

        static const float spread[6] = { -10.0f, 14.0f, -22.0f, 26.0f, 2.0f, 36.0f };
        float dx = spread[badgeCascade_] + randRange(-3.0f, 3.0f);
        float startX = std::clamp(pt.x + dx * 0.45f, 55.0f, 315.0f);
        float startY = pt.y - 26.0f - (float)(badgeCascade_ % 2) * 8.0f;

        floatingKeycaps.push_back({
            label,
            { startX, startY },
            { dx * 0.65f, -36.0f - randRange(0.0f, 14.0f) },
            0.0f, 1.25f, randRange(-0.07f, 0.07f), mouse
        });
        if (floatingKeycaps.size() > 16) {
            floatingKeycaps.erase(floatingKeycaps.begin(), floatingKeycaps.begin() + (floatingKeycaps.size() - 16));
        }
    }
};
