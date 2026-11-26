#pragma once
#define NOMINMAX
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <cmath>
#include <vector>
#include <algorithm>
#include <map>
#include <tuple>
#include <string>
#include "CatPhysics.hpp"
#include "DopeyCat.hpp"
#include "../../shared/CoatPatterns.hpp"

enum class CatHitZone {
    None,
    Head,
    Body, LeftEar, RightEar, Belly, Desk
};

enum class PetControl { None, Treat, Sleep, Wake, DoubleClickWake, Music, Mode, KeyBubbles, MouseBubbles, Hide };

class CatRenderer {
public:
    static constexpr float RadToDeg = 57.2957795f;

    /// Fonts are reused across frames instead of being created for every key, badge and "z".
    /// Sizes are rounded to 0.25px so animated sizes only ever create a handful of fonts.
    /// The fonts are deliberately never freed: GDI+ objects must not outlive GdiplusShutdown.
    static const Gdiplus::Font& cachedFont(const wchar_t* family, float size, INT style) {
        using Key = std::tuple<std::wstring, int, INT>;
        static std::map<Key, Gdiplus::Font*>* fonts = new std::map<Key, Gdiplus::Font*>();
        int quarterPx = (int)std::lround(size * 4.0f);
        Key key{family, quarterPx, style};
        auto it = fonts->find(key);
        if (it == fonts->end()) {
            it = fonts->emplace(key, new Gdiplus::Font(family, quarterPx / 4.0f, style, Gdiplus::UnitPixel)).first;
        }
        return *it->second;
    }

    static float canvasHeight(const CatSettings& settings) {
        return settings.showControls ? 342.0f : CatPhysics::CanvasHeight;
    }

    static PetControl controlAt(Pt point) {
        if (point.y < controlTop || point.y >= controlTop + controlHeight) return PetControl::None;
        for (const auto& button : controlButtons) {
            if (point.x >= button.x && point.x < button.x + button.width) return button.control;
        }
        return PetControl::None;
    }

    static bool controlEnabled(PetControl control, const CatPhysics& physics) {
        switch (control) {
            case PetControl::Treat: return !physics.ignoring() && !physics.feeding();
            case PetControl::Sleep: return !physics.ignoring() && !physics.resting();
            case PetControl::Wake: return !physics.ignoring() && physics.resting();
            case PetControl::Music: return !physics.ignoring() && !physics.resting();
            default: return true;
        }
    }

    /// The shared sleep slot acts as "wake" while the cat is resting.
    static PetControl resolveControl(PetControl control, const CatPhysics& physics) {
        return control == PetControl::Sleep && physics.resting() ? PetControl::Wake : control;
    }

    static const wchar_t* controlLabel(PetControl control) {
        return control == PetControl::Wake ? L"晚上猫熬人" : nullptr;
    }

    static void drawControls(Gdiplus::Graphics& g, const CatPhysics& physics, const CatSettings& settings,
                             PetControl hover = PetControl::None, PetControl held = PetControl::None,
                             bool showTip = false) {
        using namespace Gdiplus;
        const auto colors = keyboardColors(settings.activeKeyboardTheme());
        GraphicsPath shell;
        addRoundedRect(shell, controlBarLeft, controlTop - 3.0f, controlBarRight - controlBarLeft, controlHeight + 7.0f, 6.0f);
        SolidBrush deck(colors.base);
        g.FillPath(&deck, &shell);

        const Color gold = eyePrimary(settings.eyeTheme);
        const Color ink = settings.activeKeyboardTheme() == KeyboardTheme::MidnightHacker ? rgb(0xF0EEE8) : rgb(0x28262B);
        const ControlButton* described = nullptr;
        for (const auto& button : controlButtons) {
            const PetControl control = resolveControl(button.control, physics);
            bool enabled = controlEnabled(control, physics);
            bool active = control == PetControl::Music ? physics.listening() : controlActive(control, settings);
            bool pressing = enabled && hover == button.control && held == button.control;
            float top = controlTop + (pressing ? 1.5f : 0.0f);
            // Every key keeps the same cap colour; an "on" state is a short gold bar under the icon,
            // so the bar no longer reads as a patchwork of gold, white and grey blocks.
            Color fill = colors.cap;
            if (!enabled) fill = blendColor(fill, colors.deck, 0.31f);
            if (enabled && hover == button.control) fill = blendColor(fill, gold, 0.22f);
            if (pressing) fill = blendColor(fill, colors.base, 0.10f);

            GraphicsPath key;
            addRoundedRect(key, button.x, top, button.width, controlHeight - 1.5f, 4.0f);
            SolidBrush cap(fill);
            g.FillPath(&cap, &key);
            Pen rim(colors.border, 0.65f);
            g.DrawPath(&rim, &key);
            if (active) {
                GraphicsPath bar;
                addRoundedRect(bar, button.x + 6.0f, top + controlHeight - 5.2f, button.width - 12.0f, 2.0f, 1.0f);
                SolidBrush lit(gold);
                g.FillPath(&lit, &bar);
            }
            auto state = g.Save();
            g.TranslateTransform(button.x + button.width * 0.5f, top + (controlHeight - 1.5f) * 0.5f - (active ? 1.0f : 0.0f));
            drawControlSymbol(g, control, enabled ? ink : blendColor(ink, fill, 0.57f), settings.deskMode());
            g.Restore(state);
            if (showTip && button.control == hover) described = &button;
        }
        if (described) {
            const Font& label = cachedFont(L"Microsoft YaHei UI", 10.2f, FontStyleRegular);
            const PetControl describedControl = resolveControl(described->control, physics);
            const wchar_t* tipLabel = describedControl == PetControl::Mode
                ? modeTooltip(settings.deskMode())
                : (controlLabel(describedControl) ? controlLabel(describedControl) : described->label);
            RectF measured;
            g.MeasureString(tipLabel, -1, &label, PointF(0, 0), &measured);
            float width = measured.Width + 10.0f;
            float left = described->x + described->width * 0.5f - width * 0.5f;
            RectF bounds(left, controlTop - 26.0f, width, 20.0f);
            GraphicsPath tip;
            addRoundedRect(tip, bounds.X, bounds.Y, bounds.Width, bounds.Height, 5.0f);
            g.FillPath(&deck, &tip);
            SolidBrush text(rgb(0xFAF5E8));
            StringFormat centered;
            centered.SetAlignment(StringAlignmentCenter);
            centered.SetLineAlignment(StringAlignmentCenter);
            g.DrawString(tipLabel, -1, &label, bounds, &centered, &text);
        }
    }

    static bool bowlAt(Pt point, const CatPhysics& physics, CatCoatTheme coat = CatCoatTheme::ClassicBlack) {
        if (!physics.bowlVisible()) return false;
        if (coat == CatCoatTheme::DopeyCat) return DopeyArt::dangoAt(point,physics);
        float show = bowlOpacity(physics.treatAge());
        if (show < 0.12f) return false;
        float x = point.x - CatPhysics::SeatAnchor.x, y = point.y - (219.0f - 3.0f * show);
        if (y < 0.0f) return x * x / (22.0f * 22.0f) + y * y / 36.0f <= 1.0f;
        return y <= 12.0f && std::abs(x) <= 22.0f - y * 0.36f;
    }

    static CatHitZone hitTest(Pt p, const CatPhysics& physics, CatCoatTheme coat = CatCoatTheme::ClassicBlack,
                              bool arcadeMode = false) {
        float cx = CatPhysics::SeatAnchor.x + physics.bodyShiftX.value -
                   std::sin(physics.turnAway() * 3.14159265f) * 11.0f;
        Pt headCenter = headPosition(physics);
        const float lift = coat == CatCoatTheme::DopeyCat ? physics.jumpLift : 0;
        headCenter.y -= lift;
        const bool sideSleep = coat == CatCoatTheme::DopeyCat && physics.resting() && !physics.feeding();
        float tilt = physics.headTilt.value;
        Pt headScale{1,1};
        if (sideSleep) {
            const auto pose = DopeyArt::sleepHead(physics);
            headCenter = pose.center;
            tilt = pose.angle;
            headScale = pose.scale;
        }
        else if (coat == CatCoatTheme::DopeyCat && physics.feeding()) headCenter = {CatPhysics::SeatAnchor.x,128};
        float dx = p.x - headCenter.x, dy = p.y - headCenter.y;
        float x = (dx * std::cos(tilt) + dy * std::sin(tilt)) / headScale.x;
        float y = (dy * std::cos(tilt) - dx * std::sin(tilt)) / headScale.y;
        if (coat == CatCoatTheme::DopeyCat) {
            const float u = 33.0f - std::abs(x), v = y + 18;
            if (v < -11 && v > -44 && u > -15 && u < (v+44)*0.86f-6) return CatHitZone::Head;
            if (x*x/(52*52) + (y-6)*(y-6)/(44*44) <= 1) return CatHitZone::Head;
            if (sideSleep && (std::hypot((p.x-159)/48,(p.y-196)/32) <= 1 ||
                              std::hypot((p.x-151)/16,(p.y-161)/15) <= 1 ||
                              std::hypot((p.x-241)/19,(p.y-222)/13) <= 1)) return CatHitZone::Body;
        }
        if (coat != CatCoatTheme::DopeyCat && y < 0 && y > -64 && std::abs(x) < 68 &&
            x*x/(43*43) + y*y/(30*30) > 1.0f) {
            bool left = x < 0;
            Gdiplus::GraphicsPath path;
            buildEar(path, left, physics);
            if (path.IsVisible(x,y)) return left ? CatHitZone::LeftEar : CatHitZone::RightEar;
        }
        float bx = (p.x - cx) / 26.0f;
        float by = (p.y - (CatPhysics::SeatAnchor.y - lift - 22.0f)) / 19.0f;
        if (bx*bx + by*by <= 1.0f) return CatHitZone::Belly;
        float hdx = (p.x - headCenter.x) / 48.0f;
        float hdy = (p.y - headCenter.y) / 42.0f;
        if (hdx * hdx + hdy * hdy <= 1.0f) return CatHitZone::Head;

        if (arcadeMode) {
            const Pt pivot = CatPhysics::ArcadePivot, ball = physics.arcadeBall();
            float baseX = (p.x-pivot.x)/18.0f, baseY = (p.y-pivot.y-1.5f)/14.0f;
            if (baseX*baseX+baseY*baseY <= 1 || std::hypot(p.x-ball.x,p.y-ball.y) <= 11.0f ||
                (p.x >= 161 && p.x <= 294 && p.y >= 236 && p.y <= 298))
                return CatHitZone::Desk;
            if (p.x >= std::min(pivot.x,ball.x)-4 && p.x <= std::max(pivot.x,ball.x)+4 &&
                p.y >= ball.y && p.y <= pivot.y) return CatHitZone::Desk;
        } else {
            const Pt topLeft = KeyboardLayout::screenQuadPoint(-0.016f, -0.035f);
            const Pt topRight = KeyboardLayout::screenQuadPoint(1.016f, -0.035f);
            const Pt bottomLeft = KeyboardLayout::screenQuadPoint(-0.016f, 1.04f);
            const Pt bottomRight = KeyboardLayout::screenQuadPoint(1.016f, 1.04f);
            if (p.x >= bottomLeft.x - 1 && p.x <= bottomRight.x + 1) {
                float u = std::clamp((p.x - topLeft.x) / (topRight.x - topLeft.x), 0.0f, 1.0f);
                float top = topLeft.y + u * (topRight.y - topLeft.y);
                float bottom = bottomLeft.y + u * (bottomRight.y - bottomLeft.y) + 5.5f;
                if (p.y >= top - 1 && p.y <= bottom) return CatHitZone::Desk;
            }
        }

        float puff = physics.furPuff();
        float bdx = (p.x - cx) / (58.0f + 16.0f * puff);
        float bdy = (p.y - 196.0f + 16.0f * puff) / (56.0f + 11.0f * puff);
        if (bdx * bdx + bdy * bdy <= 1.0f) return CatHitZone::Body;

        if (!arcadeMode && p.x >= 58.0f && p.x <= 294.0f && p.y >= 232.0f && p.y <= 296.0f) return CatHitZone::Desk;
        return CatHitZone::None;
    }

    static void drawTrayPortrait(Gdiplus::Graphics& g, int size, const CatSettings& settings) {
        using namespace Gdiplus;
        CatPhysics pose;
        const Pt head{0, 0};
        GraphicsState state = g.Save();
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        g.TranslateTransform(size * 0.5f, size * 0.55f);
        g.ScaleTransform(size / 100.0f, size / 100.0f);
        if (settings.coatTheme == CatCoatTheme::DopeyCat) {
            g.ScaleTransform(0.82f,0.82f);
            DopeyArt::head(g);
            g.Restore(state);
            return;
        }
        drawEarsLayer(g, head, pose, settings.coatTheme);
        GraphicsPath face;
        face.AddEllipse(-45.0f, -37.0f, 90.0f, 80.0f);
        g.SetClip(&face, CombineModeIntersect);
        drawUnifiedSilhouette(g, 0, CatPhysics::SeatAnchor.y - 126.0f, head, 0, pose, settings.coatTheme);
        drawHeadFeatures(g, head, pose, settings.eyeTheme, settings.coatTheme);
        g.Restore(state);
    }

    static void draw(Gdiplus::Graphics& g, const CatPhysics& physics, const CatSettings& settings, bool dragging = false) {
        using namespace Gdiplus;
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

        if (settings.coatTheme == CatCoatTheme::DopeyCat && dragging) {
            DopeyArt::egg(g, CatPhysics::SeatAnchor.x, 155, physics.eggRoll);
            return;
        }
        float breath = physics.breathingOffset();
        float seatX = CatPhysics::SeatAnchor.x + physics.bodyShiftX.value;
        float seatY = CatPhysics::SeatAnchor.y;

        Pt headCenter = headPosition(physics);

        if (!settings.arcadeMode && settings.tabletMode) {
            drawTablet(g, physics);
        } else if (!settings.arcadeMode) {
            const auto theme = settings.activeKeyboardTheme();
            drawMouse(g, physics, theme);
            drawKeyboard(g, physics, theme, settings.eyeTheme, settings.showKeyLegends);
        }

        const float turn = physics.turnAway();
        const float profile = std::sin(turn * 3.14159265f);
        const float step = std::abs(std::sin(turn * 6.2831853f)) * profile;
        const float backMix = CatPhysics::ease((turn - 0.43f) / 0.37f);
        seatX -= profile * 11.0f;
        if (settings.coatTheme == CatCoatTheme::DopeyCat && physics.feeding()) {
            if (physics.bowlVisible()) DopeyArt::eating(g, physics);
            else DopeyArt::barrelReaction(g, physics);
        } else if (settings.coatTheme == CatCoatTheme::DopeyCat && physics.resting()) {
            DopeyArt::sleeping(g, physics);
            drawSparkles(g, physics, settings.eyeTheme, true);
        } else if (settings.coatTheme == CatCoatTheme::DopeyCat) {
            DopeyArt::body(g, physics, seatX, seatY - physics.jumpLift);
            const auto state = g.Save();
            g.TranslateTransform(headCenter.x,headCenter.y - physics.jumpLift);
            g.RotateTransform(physics.headTilt.value * RadToDeg);
            DopeyArt::head(g);
            if (physics.headphones() > 0.015f) {
                g.ScaleTransform(1.14f, 1.10f);
                drawHeadphones(g, physics.headphones(), 0, settings.eyeTheme);
            }
            g.Restore(state);
            if (settings.arcadeMode && physics.usingDesk()) drawArcade(g, physics, settings.activeKeyboardTheme());
            const bool stylus = settings.tabletMode && physics.usingDesk() && !physics.doingDopeyGesture();
            if (stylus) drawStylus(g, physics);
            DopeyArt::hands(g, physics, stylus);
        } else {
            drawStandardCat(g, physics, settings, seatX, seatY, headCenter, breath, turn, step, backMix);
        }

        if (settings.arcadeMode && !physics.usingDesk()) drawArcade(g, physics, settings.activeKeyboardTheme());
        if (physics.bowlVisible() && settings.coatTheme != CatCoatTheme::DopeyCat) drawTreatBowl(g, physics.treatAge());
        if (!physics.floatingKeycaps.empty()) drawFloatingKeycaps(g, physics, settings);
        if (settings.coatTheme == CatCoatTheme::DopeyCat) return;
        drawSparkles(g, physics, settings.eyeTheme);
        if (physics.facingAway()) {
            float phase = physics.backAngerPhase();
            drawAngerMark(g, headCenter, phase);
            drawAngerMark(g, headCenter, std::fmod(phase + 0.5f, 1.0f), -1.0f);
        } else {
            drawAngerMark(g, headCenter, physics.angerMarkProgress());
        }
    }

private:
    static void drawStandardCat(Gdiplus::Graphics& g, const CatPhysics& physics, const CatSettings& settings,
                                float seatX, float seatY, Pt headCenter, float breath, float turn, float step, float backMix) {
        drawTail(g, seatX, seatY, physics, settings.coatTheme);
        drawEarsLayer(g, headCenter, physics, settings.coatTheme);
        drawUnifiedSilhouette(g, seatX, seatY, headCenter, breath, physics, settings.coatTheme, backMix);
        if (turn == 0.0f) {
            if (physics.hindFootActive.value > 0.03f) {
                drawHindFoot(g, seatX, seatY, physics, settings.coatTheme);
            }
            drawHeadFeatures(g, headCenter, physics, settings.eyeTheme, settings.coatTheme);
            if (settings.arcadeMode && physics.usingDesk()) drawArcade(g, physics, settings.activeKeyboardTheme());
            bool holdingStylus = settings.tabletMode && physics.usingDesk();
            if (holdingStylus) drawStylus(g, physics);
            drawFrontPaws(g, seatX, seatY, breath, physics, settings.coatTheme, holdingStylus);
        } else {
            if (turn < 0.60f) drawHeadFeatures(g, headCenter, physics, settings.eyeTheme, settings.coatTheme);
            drawTurningPaws(g, seatX, seatY, step, turn, settings.coatTheme, physics);
        }

    }

    static Pt headPosition(const CatPhysics& physics) {
        float turn = physics.turnAway();
        float profile = std::sin(turn * 3.14159265f);
        float step = std::abs(std::sin(turn * 6.2831853f)) * profile;
        float x = CatPhysics::SeatAnchor.x + physics.bodyShiftX.value +
                  physics.bodyTilt.value * 36.0f + physics.headYaw.value * 7.5f;
        float y = 126.0f + physics.headOffsetY.value + physics.headPitch.value * 4.8f +
                  physics.breathingOffset() * 1.1f;
        return {x + profile * 21.0f, y + profile * 1.5f - step * 3.0f};
    }

    enum class Limb { LeftForeleg, RightForeleg, HindLeg };

    struct ControlButton { PetControl control; float x, width; const wchar_t* label; };
    static constexpr float controlTop = 312.0f;
    static constexpr float controlHeight = 22.0f;
    // Only the everyday actions live on the bar; double-click wake and the bubble toggles stay in
    // the right-click and tray menus. Sleep and wake share one slot that shows whichever applies.
    static constexpr ControlButton controlButtons[] = {
        {PetControl::Treat, 118, 22, L"喂零食"}, {PetControl::Sleep, 144, 22, L"白天不熬猫"},
        {PetControl::Music, 170, 22, L"听音乐"}, {PetControl::Mode, 196, 22, L"切换模式"},
        {PetControl::Hide, 222, 22, L"隐藏控件"}
    };
    static constexpr float controlBarLeft = 113.0f;
    static constexpr float controlBarRight = 249.0f;


    static bool controlActive(PetControl control, const CatSettings& settings) {
        switch (control) {
            case PetControl::DoubleClickWake: return settings.doubleClickWake;
            case PetControl::KeyBubbles: return settings.showKeyBadges;
            case PetControl::MouseBubbles: return settings.showMouseBadges;
            default: return false;
        }
    }

    static const wchar_t* modeTooltip(DeskMode mode) {
        switch (mode) {
            case DeskMode::Keyboard: return L"键鼠 · 点击切换";
            case DeskMode::Tablet: return L"数位板 · 点击切换";
            case DeskMode::Arcade: return L"街机摇杆 · 点击切换";
            default: return L"自动检测 · 点击切换";
        }
    }

    static void drawControlSymbol(Gdiplus::Graphics& g, PetControl control, Gdiplus::Color color, DeskMode mode) {
        using namespace Gdiplus;
        Pen pen(color, 1.25f);
        pen.SetStartCap(LineCapRound);
        pen.SetEndCap(LineCapRound);
        pen.SetLineJoin(LineJoinRound);
        SolidBrush ink(color);
        auto rounded = [&](float x, float y, float w, float h, float r) {
            GraphicsPath shape;
            addRoundedRect(shape, x, y, w, h, r);
            g.DrawPath(&pen, &shape);
        };
        auto bubble = [&] {
            const PointF points[] = {{-7,-6},{7,-6},{7,3},{-1,3},{-5,6},{-5,3},{-7,3},{-7,-6}};
            g.DrawLines(&pen, points, 8);
        };
        switch (control) {
            case PetControl::Treat: {
                g.DrawLine(&pen, -2.0f, 1.0f, -5.0f, 4.0f);
                g.FillEllipse(&ink, -7.4f, 2.3f, 3.5f, 3.4f);
                g.FillEllipse(&ink, -5.3f, 4.4f, 3.1f, 3.1f);
                GraphicsPath meat;
                meat.AddBezier(-3.0f, 2.0f, -5.3f, 0.0f, -2.1f, -6.2f, 1.7f, -7.0f);
                meat.AddBezier(1.7f, -7.0f, 5.7f, -8.1f, 8.2f, -3.9f, 5.8f, -0.9f);
                meat.AddBezier(5.8f, -0.9f, 3.8f, 2.1f, -1.0f, 4.3f, -3.0f, 2.0f);
                meat.CloseFigure();
                SolidBrush glaze(blendColor(color, rgb(0xC28655, color.GetA()), 0.77f));
                g.FillPath(&glaze, &meat);
                g.DrawPath(&pen, &meat);
                break;
            }
            case PetControl::Sleep: {
                const PointF large[]={{-7,-1},{-2,-1},{-7,5},{-2,5}};
                const PointF mid[]={{0,-4},{4,-4},{0.2f,0.4f},{4,0.4f}};
                const PointF smallestZ[]={{5.3f,-7},{8,-7},{5.3f,-4},{8,-4}};
                g.DrawLines(&pen, large, 4);
                g.DrawLines(&pen, mid, 4);
                g.DrawLines(&pen, smallestZ, 4);
                break;
            }
            case PetControl::Wake:
                g.DrawEllipse(&pen, -3.5f, -3.5f, 7.0f, 7.0f);
                for (int i = 0; i < 8; ++i) {
                    float angle = i * 0.7853982f;
                    float x = std::cos(angle), y = std::sin(angle);
                    g.DrawLine(&pen, x * 5.3f, y * 5.3f, x * 7.4f, y * 7.4f);
                }
                break;
            case PetControl::DoubleClickWake:
                g.DrawEllipse(&pen, -4.5f, -3.5f, 9.0f, 11.0f);
                g.DrawLine(&pen, 0.0f, -3.3f, 0.0f, 0.0f);
                g.DrawLine(&pen, -2.5f, -5.1f, -4.0f, -7.0f);
                g.DrawLine(&pen, 1.2f, -5.3f, 2.0f, -7.7f);
                break;
            case PetControl::Music:
                g.DrawArc(&pen, -6.3f, -6.6f, 12.6f, 13.2f, 180.0f, 180.0f);
                rounded(-7.4f, -0.7f, 3.8f, 6.5f, 1.4f);
                rounded(3.6f, -0.7f, 3.8f, 6.5f, 1.4f);
                break;
            case PetControl::Mode:
                if (mode == DeskMode::Keyboard) {
                    rounded(-7.5f, -5.0f, 15.0f, 10.0f, 2.1f);
                    for (int i = -1; i <= 1; ++i) g.FillEllipse(&ink, i * 4.5f - 0.7f, -2.5f, 1.4f, 1.4f);
                    g.DrawLine(&pen, -4.0f, 2.0f, 4.0f, 2.0f);
                } else if (mode == DeskMode::Tablet) {
                    rounded(-7.7f, -4.8f, 15.4f, 10.5f, 2.0f);
                    g.DrawLine(&pen, -0.4f, 2.7f, 5.9f, -6.5f);
                    g.FillEllipse(&ink, -5.5f, -2.9f, 1.8f, 1.8f);
                } else if (mode == DeskMode::Arcade) {
                    rounded(-7.5f, 1.0f, 15.0f, 5.4f, 2.0f);
                    g.DrawLine(&pen, -3.0f, 1.0f, -4.5f, -4.2f);
                    g.FillEllipse(&ink, -7.0f, -7.0f, 5.2f, 5.2f);
                    g.FillEllipse(&ink, 1.0f, -0.4f, 2.8f, 2.4f);
                    g.FillEllipse(&ink, 4.5f, -1.2f, 2.8f, 2.4f);
                } else {
                    g.DrawArc(&pen, -6.8f, -6.8f, 13.6f, 13.6f, 192.0f, 153.0f);
                    g.DrawArc(&pen, -6.8f, -6.8f, 13.6f, 13.6f, 12.0f, 153.0f);
                    const PointF top[]={{3.8f,-3.0f},{6.6f,-1.2f},{7.5f,-4.4f}};
                    const PointF bottom[]={{-3.8f,3.0f},{-6.6f,1.2f},{-7.5f,4.4f}};
                    g.DrawLines(&pen, top, 3);
                    g.DrawLines(&pen, bottom, 3);
                }
                break;
            case PetControl::KeyBubbles:
                bubble();
                rounded(-3.1f, -4.2f, 6.2f, 5.3f, 1.0f);
                g.DrawLine(&pen, -1.2f, -1.0f, 1.2f, -1.0f);
                break;
            case PetControl::MouseBubbles:
                bubble();
                rounded(-2.2f, -4.5f, 4.8f, 6.4f, 2.1f);
                g.DrawLine(&pen, 0.2f, -3.6f, 0.2f, -2.0f);
                break;
            case PetControl::Hide: {
                GraphicsPath eye;
                eye.AddBezier(-7.6f, 0.0f, -3.6f, -6.3f, 3.6f, -6.3f, 7.6f, 0.0f);
                eye.AddBezier(7.6f, 0.0f, 3.6f, 6.3f, -3.6f, 6.3f, -7.6f, 0.0f);
                g.DrawPath(&pen, &eye);
                g.DrawEllipse(&pen, -2.2f, -2.2f, 4.4f, 4.4f);
                g.DrawLine(&pen, -6.0f, 6.4f, 6.0f, -6.4f);
                break;
            }
            case PetControl::None: break;
        }
    }

    static Gdiplus::Color catBlack(BYTE a = 255)      { return Gdiplus::Color(a, 22, 22, 24); }
    static Gdiplus::Color contourMint(BYTE a = 194)   { return Gdiplus::Color(a, 209, 235, 227); }
    static Gdiplus::Color whiskerWhite(BYTE a = 174)  { return Gdiplus::Color(a, 224, 230, 235); }
    static Gdiplus::Color siameseCream(BYTE a = 255)  { return Gdiplus::Color(a, 241, 231, 213); }
    static Gdiplus::Color siameseLatte(BYTE a = 255)  { return Gdiplus::Color(a, 221, 203, 177); }
    static Gdiplus::Color siameseMocha(BYTE a = 255)  { return Gdiplus::Color(a, 115, 87, 69); }
    static Gdiplus::Color siamesePoint(BYTE a = 255)  { return Gdiplus::Color(a, 45, 33, 27); }
    static Gdiplus::Color siameseOutline(BYTE a = 224){ return Gdiplus::Color(a, 66, 48, 40); }
    static Gdiplus::Color siameseWhisker(BYTE a = 174){ return Gdiplus::Color(a, 92, 69, 56); }

    static bool patterned(CatCoatTheme c) { return c == CatCoatTheme::Shorthair || c == CatCoatTheme::OrangeWhite; }
    static bool hasWhiteSocks(CatCoatTheme c) { return c == CatCoatTheme::OrcaCat || patterned(c); }
    static Gdiplus::Color rgb(unsigned int hex, BYTE a = 255) { return Gdiplus::Color(a, (hex >> 16) & 255, (hex >> 8) & 255, hex & 255); }

    struct KeyboardColors {
        Gdiplus::Color base, deck, cap, border, accent;
    };

    static KeyboardColors keyboardColors(KeyboardTheme theme) {
        if (theme == KeyboardTheme::DopeyBlue) {
            return {rgb(0x343C69), rgb(0x596694), rgb(0xFFF0CC), rgb(0x8792B8), rgb(0x9AADE9)};
        }
        if (theme == KeyboardTheme::MidnightHacker) {
            return {rgb(0x0F0F14), rgb(0x1A1C21), rgb(0x363840), rgb(0x1C1C21, 242), rgb(0xC4F040)};
        }
        if (theme == KeyboardTheme::RetroCream) {
            return {rgb(0x403D38), rgb(0x615C54), rgb(0xE6E0D1), rgb(0xADA69A, 216), rgb(0xE06B3D)};
        }
        return {rgb(0x1A1A1F), rgb(0x242429), rgb(0xF7F6F0), rgb(0xC7C4BD, 190), rgb(0xF7B329)};
    }

    static Gdiplus::Color bodyColor(CatCoatTheme c) { return c == CatCoatTheme::Shorthair ? rgb(0xC2C5C1) : (c == CatCoatTheme::OrangeWhite ? rgb(0xF2EEE4) : catBlack()); }
    static Gdiplus::Color limbColor(CatCoatTheme c) { return c == CatCoatTheme::OrangeWhite ? rgb(0xD8AA73) : bodyColor(c); }
    static Gdiplus::Color pawColor(CatCoatTheme c) {
        if (c == CatCoatTheme::OrcaCat) return rgb(0xF3F2EE);
        return patterned(c) ? rgb(0xF2EEE4) : (c == CatCoatTheme::Siamese ? siamesePoint() : catBlack());
    }
    static Gdiplus::Color earTailColor(CatCoatTheme c) { return c == CatCoatTheme::Siamese ? siamesePoint() : limbColor(c); }
    static Gdiplus::Color creaseColor(CatCoatTheme c, BYTE a = 255) {
        if (c == CatCoatTheme::OrcaCat) return rgb(0xA5A4AA, (BYTE)(a*0.72f));
        return patterned(c) ? rgb(0x8B8171, (BYTE)(a*0.60f)) : contourMint(a);
    }
    static Gdiplus::PointF coatMarkPoint(CoatPoint p, bool headSurface, bool skull, CatCoatTheme coat, float seatX, float seatY, Pt head, const CatPhysics& physics) {
        if (headSurface) {
            float depth=coat==CatCoatTheme::OrangeWhite ? 29.6f*std::sqrt(std::max(0.0f,1-p.x*p.x/(46*46)-p.y*p.y/(40*40))) : 27.5f;
            Pt q=skull ? Pt{p.x,p.y} : projectHead3D(p.x,p.y,depth,physics.headYaw.value*0.50f,physics.headPitch.value*0.36f);
            float c = std::cos(physics.headTilt.value), s = std::sin(physics.headTilt.value);
            return {head.x + q.x*c - q.y*s, head.y + q.x*s + q.y*c};
        }
        float sy = physics.bodySquash.value, sx = 1.0f+(1.0f-sy)*0.55f;
        float c = std::cos(physics.bodyTilt.value), s = std::sin(physics.bodyTilt.value);
        return {seatX+p.x*sx*c-p.y*sy*s, seatY+p.x*sx*s+p.y*sy*c};
    }
    static void drawCoatMarks(Gdiplus::Graphics& g, float seatX, float seatY, Pt head, const CatPhysics& physics, CatCoatTheme coat) {
        using namespace Gdiplus;
        const auto& marks = coat == CatCoatTheme::Shorthair ? shorthairMarks() : orangeWhiteMarks();
        for (const auto& mark : marks) {
            std::vector<PointF> points;
            for (const auto& p : mark.points) points.push_back(coatMarkPoint(p, mark.head, mark.skull, coat, seatX, seatY, head, physics));
            GraphicsPath path;
            Color color = rgb(mark.rgb, (BYTE)(mark.alpha*255));
            if (mark.width == 0) { path.AddPolygon(points.data(), (INT)points.size()); SolidBrush brush(color); g.FillPath(&brush, &path); }
            else { path.AddLines(points.data(), (INT)points.size()); Pen pen(color, mark.width); pen.SetStartCap(LineCapRound); pen.SetEndCap(LineCapRound); pen.SetLineJoin(LineJoinRound); g.DrawPath(&pen, &path); }
        }
    }

    static Gdiplus::Color eyePrimary(EyeColorTheme t, BYTE a = 255) {
        switch (t) {
            case EyeColorTheme::NeonChartreuse: return Gdiplus::Color(a, 201, 242, 64);
            case EyeColorTheme::SiameseBlue:    return Gdiplus::Color(a, 94, 199, 250);
            case EyeColorTheme::WarmAmber:      return Gdiplus::Color(a, 250, 194, 56);
            case EyeColorTheme::IceCyan:        return Gdiplus::Color(a, 115, 235, 245);
            case EyeColorTheme::ReferenceHazel: return Gdiplus::Color(a, 218, 178, 87);
            case EyeColorTheme::SoftAmber: return Gdiplus::Color(a, 217, 173, 85);
        }
        return Gdiplus::Color(a, 201, 242, 64);
    }

    static Gdiplus::Color blendColor(Gdiplus::Color a, Gdiplus::Color b, float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        auto lerp = [t](BYTE x, BYTE y) -> BYTE { return (BYTE)std::clamp((int)(x + (y - x) * t + 0.5f), 0, 255); };
        return Gdiplus::Color(lerp(a.GetA(), b.GetA()), lerp(a.GetR(), b.GetR()), lerp(a.GetG(), b.GetG()), lerp(a.GetB(), b.GetB()));
    }

    static void addRoundedRect(Gdiplus::GraphicsPath& p, float x, float y, float w, float h, float r) {
        float d = std::min(r * 2.0f, std::min(w, h));
        if (d <= 0.5f) {
            p.AddRectangle(Gdiplus::RectF(x, y, w, h));
            return;
        }
        p.AddArc(x, y, d, d, 180.0f, 90.0f);
        p.AddArc(x + w - d, y, d, d, 270.0f, 90.0f);
        p.AddArc(x + w - d, y + h - d, d, d, 0.0f, 90.0f);
        p.AddArc(x, y + h - d, d, d, 90.0f, 90.0f);
        p.CloseFigure();
    }

    static void addQuadPoly(Gdiplus::GraphicsPath& p, Pt tl, Pt tr, Pt br, Pt bl) {
        Gdiplus::PointF pts[4] = { {tl.x, tl.y}, {tr.x, tr.y}, {br.x, br.y}, {bl.x, bl.y} };
        p.AddPolygon(pts, 4);
    }

    static void addQuadBezier(Gdiplus::GraphicsPath& p, Pt p0, Pt ctrl, Pt p2) {
        // Convert quadratic Bezier (p0, ctrl, p2) -> exact cubic Bezier for GDI+
        Gdiplus::PointF c1(p0.x + (2.0f / 3.0f) * (ctrl.x - p0.x), p0.y + (2.0f / 3.0f) * (ctrl.y - p0.y));
        Gdiplus::PointF c2(p2.x + (2.0f / 3.0f) * (ctrl.x - p2.x), p2.y + (2.0f / 3.0f) * (ctrl.y - p2.y));
        p.AddBezier(Gdiplus::PointF(p0.x, p0.y), c1, c2, Gdiplus::PointF(p2.x, p2.y));
    }

    static Pt projectHead3D(float x, float y, float z, float yawRad, float pitchRad) {
        float cy = std::cos(yawRad), sy = std::sin(yawRad);
        float x1 = x * cy + z * sy;
        float z1 = -x * sy + z * cy;
        float cp = std::cos(pitchRad), sp = std::sin(pitchRad);
        float y2 = y * cp + z1 * sp;
        return { x1, y2 };
    }

    static Pt quadPoint(Pt p0, Pt p1, Pt p2, float t) {
        float mt = 1.0f - t;
        return { mt * mt * p0.x + 2.0f * mt * t * p1.x + t * t * p2.x,
                 mt * mt * p0.y + 2.0f * mt * t * p1.y + t * t * p2.y };
    }

    static Pt quadTangent(Pt p0, Pt p1, Pt p2, float t) {
        float mt = 1.0f - t;
        return { 2.0f * mt * (p1.x - p0.x) + 2.0f * t * (p2.x - p1.x),
                 2.0f * mt * (p1.y - p0.y) + 2.0f * t * (p2.y - p1.y) };
    }

    // MARK: - Mini White Mouse (180° facing the cat)

    static void drawMouse(Gdiplus::Graphics& g, const CatPhysics& physics, KeyboardTheme theme) {
        using namespace Gdiplus;
        Pt center = { CatPhysics::MouseBaseCenter.x + physics.mouseOffset.point().x,
                      CatPhysics::MouseBaseCenter.y + physics.mouseOffset.point().y };
        float pressShift = std::max(physics.mousePressLeft.value, physics.mousePressRight.value) * 1.6f;

        GraphicsState st = g.Save();
        g.TranslateTransform(center.x, center.y + pressShift);
        g.RotateTransform(0.22f * RadToDeg);

        SolidBrush shadowBrush(Color(60, 0, 0, 0));
        g.FillEllipse(&shadowBrush, -13.5f, -9.5f, 27.0f, 31.0f);

        Color bodyCol, borderCol, wheelCol;
        if (theme == KeyboardTheme::DopeyBlue) {
            auto colors = keyboardColors(theme);
            bodyCol = colors.cap; borderCol = colors.base; wheelCol = colors.accent;
        } else if (theme == KeyboardTheme::MidnightHacker) {
            bodyCol = Color(255, 51, 54, 61); borderCol = Color(255, 20, 20, 26); wheelCol = Color(255, 199, 242, 64);
        } else if (theme == KeyboardTheme::RetroCream) {
            bodyCol = Color(255, 227, 222, 209); borderCol = Color(255, 51, 48, 46); wheelCol = Color(255, 224, 107, 61);
        } else {
            bodyCol = Color(255, 244, 242, 236); borderCol = Color(255, 36, 36, 41); wheelCol = Color(255, 245, 176, 41);
        }

        SolidBrush bodyBrush(bodyCol);
        g.FillEllipse(&bodyBrush, -13.5f, -16.5f, 27.0f, 33.0f);

        if (physics.mousePressLeft.value > 0.05f || physics.mousePressRight.value > 0.05f) {
            GraphicsPath clipPath;
            clipPath.AddEllipse(-13.5f, -16.5f, 27.0f, 33.0f);
            GraphicsState cs = g.Save();
            g.SetClip(&clipPath);
            if (physics.mousePressLeft.value > 0.05f) {
                SolidBrush hb(Color((BYTE)(96 * physics.mousePressLeft.value), 201, 242, 64));
                g.FillRectangle(&hb, 0.0f, 0.0f, 14.0f, 17.0f);
            }
            if (physics.mousePressRight.value > 0.05f) {
                SolidBrush hb(Color((BYTE)(96 * physics.mousePressRight.value), 201, 242, 64));
                g.FillRectangle(&hb, -14.0f, 0.0f, 14.0f, 17.0f);
            }
            g.Restore(cs);
        }

        // Front button split seam pointing away from cat (bottom)
        Pen seamPen(Color(82, borderCol.GetR(), borderCol.GetG(), borderCol.GetB()), 1.05f);
        g.DrawLine(&seamPen, 0.0f, 16.0f, 0.0f, 1.5f);

        // Amber scroll wheel on front (bottom) half
        GraphicsPath wheelPath;
        addRoundedRect(wheelPath, -1.9f, 2.5f, 3.8f, 8.5f, 1.9f);
        SolidBrush wheelBrush(wheelCol);
        g.FillPath(&wheelBrush, &wheelPath);

        Pen borderPen(borderCol, 1.7f);
        g.DrawEllipse(&borderPen, -13.5f, -16.5f, 27.0f, 33.0f);
        g.Restore(st);
    }

    // MARK: - Mini 65% Mechanical Keyboard (180° facing the cat)

    static std::wstring keycapLegend(PhysicalKey k) {
        switch (k) {
            case PhysicalKey::Escape: return L"ESC";
            case PhysicalKey::Tab: return L"\u21E5";
            case PhysicalKey::CapsLock: return L"\u21EA";
            case PhysicalKey::LeftShift:
            case PhysicalKey::RightShift: return L"\u21E7";
            case PhysicalKey::LeftControl:
            case PhysicalKey::RightControl: return L"Ctrl";
            case PhysicalKey::LeftOption:
            case PhysicalKey::RightOption: return L"Alt";
            case PhysicalKey::LeftCommand:
            case PhysicalKey::RightCommand: return L"Win";
            case PhysicalKey::Backspace: return L"\u232B";
            case PhysicalKey::Enter: return L"\u21B5";
            case PhysicalKey::Space: return L"";
            default: return keyBadgeLabel(k);
        }
    }

    static void drawKeyboard(Gdiplus::Graphics& g, const CatPhysics& physics, KeyboardTheme theme, EyeColorTheme eyeTheme, bool showLegends) {
        using namespace Gdiplus;
        Pt tl = KeyboardLayout::screenQuadPoint(-0.012f, -0.04f);
        Pt tr = KeyboardLayout::screenQuadPoint(1.012f, -0.04f);
        Pt bl = KeyboardLayout::screenQuadPoint(-0.016f, 1.04f);
        Pt br = KeyboardLayout::screenQuadPoint(1.016f, 1.04f);
        constexpr float depth = 4.5f;

        // Shadow
        {
            GraphicsPath sp;
            addQuadPoly(sp, {tl.x, tl.y + depth + 2}, {tr.x, tr.y + depth + 2},
                            {br.x, br.y + depth + 3}, {bl.x, bl.y + depth + 3});
            SolidBrush sb(Color(56, 0, 0, 0));
            g.FillPath(&sb, &sp);
        }

        const auto [baseCol, deckCol, keyCol, borderCol, accentCol] = keyboardColors(theme);

        {
            GraphicsPath lp;
            addQuadPoly(lp, {tl.x, tl.y + depth}, {tr.x, tr.y + depth}, {br.x, br.y + depth}, {bl.x, bl.y + depth});
            SolidBrush bb(baseCol);
            Pen bp(baseCol, 2.5f); bp.SetLineJoin(LineJoinRound);
            g.FillPath(&bb, &lp);
            g.DrawPath(&bp, &lp);
        }
        {
            GraphicsPath tp;
            addQuadPoly(tp, tl, tr, br, bl);
            SolidBrush db(deckCol);
            Pen dp(deckCol, 2.0f); dp.SetLineJoin(LineJoinRound);
            g.FillPath(&db, &tp);
            g.DrawPath(&dp, &tp);
        }

        constexpr float padU = 0.0028f;
        constexpr float padV = 0.014f;
        Color glowCol = theme == KeyboardTheme::DopeyBlue ? rgb(0x9A82E4, 215) : eyePrimary(eyeTheme, 215);
        Pen keyBorderPen(borderCol, 0.65f);
        StringFormat kbSF;
        kbSF.SetAlignment(StringAlignmentCenter);
        kbSF.SetLineAlignment(StringAlignmentCenter);

        for (const auto& cap : KeyboardLayout::keycaps()) {
            float glow = 0.0f;
            auto it = physics.keyGlow.find(cap.key);
            if (it != physics.keyGlow.end()) glow = it->second;

            // 180° rotated to face the cat
            float su0 = (1.0f - (cap.xUnits + cap.widthUnits)) + padU;
            float su1 = (1.0f - cap.xUnits) - padU;
            float sv0 = (1.0f - (cap.row + 1) / 5.0f) + padV;
            float sv1 = (1.0f - cap.row / 5.0f) - padV;

            float dy = glow * 1.9f;
            Pt kTL = KeyboardLayout::screenQuadPoint(su0, sv0); kTL.y += dy;
            Pt kTR = KeyboardLayout::screenQuadPoint(su1, sv0); kTR.y += dy;
            Pt kBR = KeyboardLayout::screenQuadPoint(su1, sv1); kBR.y += dy;
            Pt kBL = KeyboardLayout::screenQuadPoint(su0, sv1); kBL.y += dy;

            GraphicsPath kp;
            addQuadPoly(kp, kTL, kTR, kBR, kBL);
            Color fill = cap.isAmberAccent ? accentCol : keyCol;
            if (theme == KeyboardTheme::DopeyBlue && !cap.isAmberAccent && cap.widthUnits > 1.0f / 15.0f)
                fill = rgb(0xD2D9F4);
            if (glow > 0.02f) fill = blendColor(fill, glowCol, glow * 0.55f);

            SolidBrush kb(fill);
            g.FillPath(&kb, &kp);
            g.DrawPath(&keyBorderPen, &kp);

            // Legends are only a few pixels tall, so drawing all of them reads as noise. Show a
            // legend only while its key glows, and fade it in and out with the glow.
            if (showLegends && glow > 0.05f) {
                bool isSpace = (cap.key == PhysicalKey::Space);
                std::wstring legend = isSpace ? std::wstring(L"space") : keycapLegend(cap.key);
                if (!legend.empty()) {
                    float cx = (kTL.x + kTR.x + kBR.x + kBL.x) * 0.25f;
                    float cy = (kTL.y + kTR.y + kBR.y + kBL.y) * 0.25f;
                    if (isSpace) {
                        // The spacebar sits right under the cat: its legs hide the middle, and an
                        // unlabeled visible end looked like an odd blank patch beside the cat.
                        // Label the end that stays visible instead.
                        Pt sp = KeyboardLayout::screenQuadPoint(su0 + (su1 - su0) * 0.82f, (sv0 + sv1) * 0.5f);
                        cx = sp.x;
                        cy = sp.y + dy;
                    }
                    const float legendAlpha = std::min(1.0f, glow * 1.6f);
                    Color txtCol = (cap.isAmberAccent || theme != KeyboardTheme::MidnightHacker)
                        ? Color((BYTE)(198 * legendAlpha), 36, 36, 41)
                        : Color((BYTE)(210 * legendAlpha), 224, 230, 235);
                    SolidBrush tb(txtCol);
                    const Font& kf = cachedFont(L"Segoe UI", legend.size() <= 1 ? 5.2f : 3.9f, FontStyleBold);
                    GraphicsState ks = g.Save();
                    g.TranslateTransform(cx, cy);
                    // The keyboard is laid out 180° to face the cat, so the legends must be too
                    // (upside-down to the viewer, upright from the cat's seat). +3° follows the deck slope.
                    g.RotateTransform(180.0f + 3.0f);
                    g.DrawString(legend.c_str(), -1, &kf, RectF(-14.0f, -7.0f, 28.0f, 14.0f), &kbSF, &tb);
                    g.Restore(ks);
                }
            }
        }

        // Radiant backlit glow halos over pressed keys
        for (const auto& kv : physics.keyGlow) {
            if (kv.second <= 0.05f) continue;
            Pt c = KeyboardLayout::canvasPointForKey(kv.first);
            float r = 17.0f + kv.second * 6.0f;
            GraphicsPath gp;
            gp.AddEllipse(c.x - r, c.y - r, r * 2.0f, r * 2.0f);
            PathGradientBrush pgb(&gp);
            pgb.SetCenterColor(Color((BYTE)(92 * kv.second), glowCol.GetR(), glowCol.GetG(), glowCol.GetB()));
            Color surr[1] = { Color(0, glowCol.GetR(), glowCol.GetG(), glowCol.GetB()) };
            INT cnt = 1;
            pgb.SetSurroundColors(surr, &cnt);
            g.FillPath(&pgb, &gp);
        }
    }

    static Gdiplus::PointF cubicPoint(Gdiplus::PointF a, Gdiplus::PointF b, Gdiplus::PointF c, Gdiplus::PointF d, float t) {
        float u = 1.0f - t;
        return {u*u*u*a.X + 3*u*u*t*b.X + 3*u*t*t*c.X + t*t*t*d.X,
                u*u*u*a.Y + 3*u*u*t*b.Y + 3*u*t*t*c.Y + t*t*t*d.Y};
    }

    static Gdiplus::PointF cubicTangent(Gdiplus::PointF a, Gdiplus::PointF b, Gdiplus::PointF c, Gdiplus::PointF d, float t) {
        float u = 1.0f - t;
        return {u*u*(b.X-a.X) + 2*u*t*(c.X-b.X) + t*t*(d.X-c.X),
                u*u*(b.Y-a.Y) + 2*u*t*(c.Y-b.Y) + t*t*(d.Y-c.Y)};
    }

    static float furRipple(float age, float delay) {
        float phase = age - delay;
        if (phase <= 0.0f || phase >= 0.55f) return 0.0f;
        return std::sin(phase * 27.0f) * std::exp(-phase * 7.5f) * CatPhysics::ease((0.55f - phase) / 0.15f);
    }

    // MARK: - Tail

    static void drawTail(Gdiplus::Graphics& g, float seatX, float seatY, const CatPhysics& physics, CatCoatTheme coat) {
        using namespace Gdiplus;
        float p = physics.tailPhase;
        float hunt = std::clamp(physics.huntIntensity.value, 0.0f, 1.0f);
        float wag1 = std::sin(p) * (8.0f + hunt * 5.5f);
        float wag2 = std::cos(p * 1.3f) * (6.0f + hunt * 4.5f);
        float huntDrop = hunt * 14.0f;

        float turn = physics.turnAway();
        float direction = std::cos(turn * 3.14159265f);
        float curl = CatPhysics::ease((turn - 0.30f) / 0.23f) * (1.0f - CatPhysics::ease((turn - 0.61f) / 0.27f));
        float tipDirection = std::cos(std::min(1.0f, turn + curl * 0.13f) * 3.14159265f);
        PointF root(seatX + direction * 34.0f, seatY - 6.0f);
        PointF c1(seatX + direction * 64.0f - curl * 31.0f, seatY + 2.0f + wag1 * 0.35f + huntDrop * 0.4f + curl * 19.0f);
        PointF c2(seatX + tipDirection * (66.0f + wag2 * 0.4f) - curl * 26.0f, seatY - 26.0f + wag1 * 0.7f + huntDrop + curl * 13.0f);
        PointF tip(seatX + tipDirection * (91.0f + wag2 * 0.5f) - curl * 16.0f, seatY - 30.0f + wag1 * 0.85f + huntDrop + curl * 14.0f);
        float impatient = physics.tailImpatience();
        float puff = physics.furPuff();
        auto blend = [](PointF& point, PointF target, float amount) {
            point.X += (target.X - point.X) * amount;
            point.Y += (target.Y - point.Y) * amount;
        };
        if (impatient > 0.001f) {
            float beat = std::fmod(physics.tailSlapTime(), 0.56f) / 0.56f;
            auto lift = [](float phase) {
                return CatPhysics::ease(phase / 0.43f) * (1.0f - CatPhysics::ease((phase - 0.43f) / 0.19f));
            };
            // The bend travels outward. The tip lands after the base and rebounds once.
            float baseLift = lift(beat);
            float middleLift = lift(beat - 0.08f);
            float tipLift = lift(beat - 0.14f);
            float rebound = CatPhysics::ease((beat - 0.76f) / 0.08f) * (1.0f - CatPhysics::ease((beat - 0.84f) / 0.13f));
            PointF raisedBase(seatX + direction * (61.0f - 11.0f * baseLift), seatY + 7.0f - 26.0f * baseLift);
            PointF raisedMiddle(seatX + direction * (77.0f - 24.0f * middleLift), seatY + 15.0f - 69.0f * middleLift);
            PointF raisedTip(seatX + direction * (99.0f - 21.0f * tipLift - 5.0f * rebound),
                             seatY + 11.0f - 59.0f * tipLift - 10.0f * rebound);
            blend(c1, raisedBase, impatient);
            blend(c2, raisedMiddle, impatient);
            blend(tip, raisedTip, impatient);
        } else if (physics.tailKeyReach() > 0.0f) {
            float reach = physics.tailKeyReach();
            Pt key = physics.tailKeyPoint();
            blend(c1, {seatX + 118.0f, seatY - 35.0f}, reach);
            blend(c2, {std::max(key.x + 46.0f, seatX + 78.0f), key.y + 34.0f}, reach);
            blend(tip, {key.x, key.y - 14.0f * (1.0f - physics.tailKeyDip())}, reach);
        }
        blend(c1, {seatX + direction * 55.0f, seatY + 4.0f}, puff);
        blend(c2, {seatX + tipDirection * 80.0f, seatY + 5.0f}, puff);
        blend(tip, {seatX + tipDirection * 93.0f, seatY - 7.0f + wag1 * 0.30f}, puff);

        Pen tailPen(earTailColor(coat), 11.0f + 3.5f * puff);
        tailPen.SetStartCap(LineCapRound);
        tailPen.SetEndCap(LineCapRound);
        tailPen.SetLineJoin(LineJoinRound);
        g.DrawBezier(&tailPen, root, c1, c2, tip);
        if (patterned(coat)) {
            GraphicsState state = g.Save();
            GraphicsPath outline;
            outline.AddBezier(root, c1, c2, tip);
            outline.Widen(&tailPen);
            g.SetClip(&outline, CombineModeIntersect);
            Pen ring(rgb(coat == CatCoatTheme::Shorthair ? 0x55595A : 0xA57543,128),3.5f);
            for (float t : {0.50f,0.78f}) {
                auto p = cubicPoint(root, c1, c2, tip, t);
                auto tangent = cubicTangent(root, c1, c2, tip, t);
                float length = std::max(0.001f, std::hypot(tangent.X, tangent.Y));
                float span = (8.0f + 3.0f * puff) / length;
                float nx = -tangent.Y * span, ny = tangent.X * span;
                g.DrawLine(&ring, p.X + nx, p.Y + ny, p.X - nx, p.Y - ny);
            }
            g.Restore(state);
        }
    }

    static void drawAngerMark(Gdiplus::Graphics& g, Pt head, float progress, float side = 1.0f) {
        using namespace Gdiplus;
        if (progress >= 1.0f) return;
        float appear = CatPhysics::ease(progress / 0.18f);
        float fade = 1.0f - CatPhysics::ease((progress - 0.65f) / 0.35f);
        float size = 0.65f + 0.43f * appear + 0.12f * std::sin(3.14159265f * appear);
        if (side < 0.0f) size *= 0.76f;
        GraphicsState state = g.Save();
        g.TranslateTransform(head.x + side * (44.0f + progress * 4.0f), head.y - 49.0f - progress * 13.0f);
        g.RotateTransform(side * (14.0f - progress * 10.0f));
        g.ScaleTransform(size, size);
        GraphicsPath mark;
        for (float x : {-1.0f, 1.0f}) {
            for (float y : {-1.0f, 1.0f}) {
                mark.StartFigure();
                addQuadBezier(mark, {x * 3.6f, y * 9.8f}, {x * 2.0f, y * 2.0f}, {x * 9.8f, y * 3.6f});
            }
        }
        Pen edge(rgb(0xFFF1E5, static_cast<BYTE>(221.0f * appear * fade)), 4.7f);
        Pen ink(rgb(0xCF373C, static_cast<BYTE>(255.0f * appear * fade)), 2.8f);
        for (Pen* pen : {&edge, &ink}) {
            pen->SetStartCap(LineCapRound);
            pen->SetEndCap(LineCapRound);
            g.DrawPath(pen, &mark);
        }
        g.Restore(state);
    }

    // MARK: - 3D-Projected Ears

    static void drawEarsLayer(Gdiplus::Graphics& g, Pt center, const CatPhysics& physics, CatCoatTheme coat) {
        using namespace Gdiplus;
        GraphicsState st = g.Save();
        g.TranslateTransform(center.x, center.y);
        g.RotateTransform(physics.headTilt.value * RadToDeg);

        SolidBrush brush(earTailColor(coat));
        for (bool left : {true, false}) {
            GraphicsPath ear;
            buildEar(ear, left, physics);
            g.FillPath(&brush, &ear);
        }
        g.Restore(st);
    }

    static void buildEar(Gdiplus::GraphicsPath& ear, bool isLeft, const CatPhysics& physics) {
        float sign = isLeft ? -1.0f : 1.0f;
        float yawRad = physics.headYaw.value * 0.50f +
                       std::sin(physics.turnAway() * 3.14159265f) * 1.07f;
        float pitchRad = physics.headPitch.value * 0.36f;
        float foldAngle = sign * (physics.earAngle(isLeft) - physics.earTwitch.value * 0.045f);
        float tipDx = std::sin(foldAngle) * 32.0f;
        float tipDy = -std::cos(foldAngle) * 32.0f;

        Pt innerBase = projectHead3D(sign * 9.0f, -16.0f, 0.0f, yawRad, pitchRad);
        Pt outerBase = projectHead3D(sign * 35.0f, -5.0f, -2.0f, yawRad, pitchRad);
        Pt tip3D     = projectHead3D(sign * 24.5f + tipDx, -20.0f + tipDy, -3.0f, yawRad, pitchRad);

        Pt tipL = { tip3D.x - sign * 3.8f, tip3D.y + 2.8f };
        Pt tipR = { tip3D.x + sign * 3.8f, tip3D.y + 3.2f };

        addQuadBezier(ear, innerBase, { (innerBase.x + tipL.x) * 0.5f - sign * 1.5f, (innerBase.y + tipL.y) * 0.5f - 2.0f }, tipL);
        addQuadBezier(ear, tipL, { tip3D.x, tip3D.y - 2.2f }, tipR);
        addQuadBezier(ear, tipR, { (outerBase.x + tipR.x) * 0.5f + sign * 2.5f, (outerBase.y + tipR.y) * 0.5f - 2.0f }, outerBase);
        ear.CloseFigure();

    }

    static float turnYaw(const CatPhysics& physics) {
        return physics.headYaw.value * 0.50f + physics.turnAway() * 3.42f;
    }

    // MARK: - Unified C1-Continuous Cat Silhouette (+ Siamese 2.5D Face Mask)

    static void addFurStrokes(Gdiplus::GraphicsPath& strokes, Gdiplus::PointF start,
                              Gdiplus::PointF c1, Gdiplus::PointF c2, Gdiplus::PointF end,
                              float reach, int count, const CatPhysics& physics) {
        using Gdiplus::PointF;
        constexpr float lengths[] = {0.83f, 1.0f, 0.70f, 0.93f, 0.77f};
        const float spacing = 1.0f / count;
        for (int i = 0; i < count; ++i) {
            float t = (i + (i % 3 == 1 ? 0.56f : 0.45f)) * spacing;
            PointF base = cubicPoint(start, c1, c2, end, t);
            PointF tangent = cubicTangent(start, c1, c2, end, t);
            float length = std::hypot(tangent.X, tangent.Y);
            float nx = tangent.Y / length, ny = -tangent.X / length;
            float height = std::clamp((CatPhysics::SeatAnchor.y + 12.0f - base.Y) / 145.0f, 0.0f, 1.0f);
            float pulse = furRipple(physics.furPulseAge(), height * 0.09f);
            float lift = reach * lengths[i % 5] * (1.0f + 0.40f * pulse);
            float lean = (i % 2 == 0 ? 0.14f : -0.08f) * lift / length;
            strokes.StartFigure();
            strokes.AddLine(base.X - 3.0f * nx, base.Y - 3.0f * ny,
                            base.X + lift * nx + lean * tangent.X, base.Y + lift * ny + lean * tangent.Y);
        }
    }

    static void drawUnifiedSilhouette(Gdiplus::Graphics& g, float seatX, float seatY, Pt headCenter, float breath,
                                      const CatPhysics& physics, CatCoatTheme coat, float back = 0.0f) {
        using namespace Gdiplus;
        float yaw = physics.headYaw.value;
        float pitch = physics.headPitch.value;
        float hTilt = physics.headTilt.value;
        float bTilt = physics.bodyTilt.value;
        float puff = physics.furPuff();

        float cH = std::cos(hTilt), sH = std::sin(hTilt);
        auto H = [&](float lx, float ly) -> PointF {
            return PointF(headCenter.x + lx * cH - ly * sH, headCenter.y + lx * sH + ly * cH);
        };

        float cB = std::cos(bTilt), sB = std::sin(bTilt);
        float sy = physics.bodySquash.value;
        float turn = physics.turnAway();
        float profile = std::sin(turn * 3.14159265f);
        float sx = (1.0f + (1.0f - sy) * 0.55f) * (1.0f - profile * 0.13f);
        auto B = [&](float lx, float ly) -> PointF {
            float rx = lx * sx, ry = ly * sy;
            return PointF(seatX + rx * cB - ry * sB, seatY + rx * sB + ry * cB);
        };

        float cheekShift = yaw * 3.0f;
        float crownDrop = pitch * 2.0f;
        float leftCheekX = -42.5f + cheekShift * 0.4f + profile * 6.0f;
        float rightCheekX = 42.5f + cheekShift * 0.4f - profile * 10.0f;
        const float fullness = back * (0.44f + 0.56f * CatPhysics::ease(puff));
        const float hipSpread = 10.0f * fullness + 4.0f * puff * furRipple(physics.furPulseAge(), 0.0f);
        const float crownY = -30.0f + crownDrop - 6.0f * fullness;
        const float hipY = -14.0f - 8.0f * fullness;
        const float seatWidth = 24.0f + 5.0f * fullness;

        PointF crown  = H(cheekShift * 0.2f, crownY);
        PointF rCheek = H(rightCheekX + 5.0f * fullness, 5.0f);
        PointF rHip   = B(49.0f + profile * 3.0f + hipSpread, hipY);
        PointF rBot   = B(seatWidth, 12.0f);
        PointF lBot   = B(-seatWidth, 12.0f);
        PointF lHip   = B(-49.0f - hipSpread, hipY - profile * 3.0f);
        PointF lCheek = H(leftCheekX - 5.0f * fullness, 5.0f);

        GraphicsPath path, furStrokes;
        auto bodyCurve = [&](PointF start, PointF c1, PointF c2, PointF end, float tuftReach, int tuftCount) {
            path.AddBezier(start, c1, c2, end);
            if (puff > 0.0f) addFurStrokes(furStrokes, start, c1, c2, end, tuftReach * puff, tuftCount, physics);
        };
        const float crownSpan = 26.0f + 6.0f * fullness;
        const float cheekControl = 1.5f + 1.5f * fullness;
        const float shoulderControl = -2.5f + 9.0f * fullness;
        const float flankControl = 51.5f + 6.0f * fullness;
        const float rumpControl = 47.0f + 14.0f * fullness;
        const float bottomControl = 36.0f + 14.0f * fullness;
        bodyCurve(crown, H(crownSpan + cheekShift * 0.3f, crownY), H(rightCheekX + cheekControl, -14.0f - 6.0f * fullness),
                  rCheek, 4.6f, 6);
        bodyCurve(rCheek, H(rightCheekX + shoulderControl, 24.0f), B(flankControl, -48.0f + breath * 0.5f),
                  rHip, 5.5f, 8);
        bodyCurve(rHip, B(rumpControl, 5.0f - 5.0f * fullness), B(bottomControl, 12.0f),
                  rBot, 4.1f, 6);
        bodyCurve(rBot, B(9.0f, 12.0f), B(-9.0f, 12.0f), lBot, 1.4f, 6);
        bodyCurve(lBot, B(-bottomControl, 12.0f), B(-rumpControl, 5.0f - 5.0f * fullness),
                  lHip, 4.1f, 6);
        bodyCurve(lHip, B(-flankControl, -48.0f + breath * 0.5f), H(leftCheekX - shoulderControl, 24.0f),
                  lCheek, 5.5f, 8);
        bodyCurve(lCheek, H(leftCheekX - cheekControl, -14.0f - 6.0f * fullness), H(-crownSpan + cheekShift * 0.3f, crownY),
                  crown, 4.6f, 6);
        path.CloseFigure();

        if (back > 0.0f) {
            Color fur = coat == CatCoatTheme::Siamese ? blendColor(siameseCream(), siameseLatte(), back)
                                                     : bodyColor(coat);
            Color shoulder = blendColor(fur, rgb(0xA7A2A0), 0.14f);
            LinearGradientBrush backFur(crown, B(0, 18), shoulder, fur);
            backFur.SetWrapMode(WrapModeTileFlipXY);
            if (puff > 0.0f) {
                Pen outline(&backFur, 4.5f * puff);
                outline.SetStartCap(LineCapRound);
                outline.SetEndCap(LineCapRound);
                g.DrawPath(&outline, &furStrokes);
            }
            g.FillPath(&backFur, &path);
            if (coat == CatCoatTheme::Shorthair) {
                Pen stripe(rgb(0x62605F, static_cast<BYTE>(145 * back)), 2.7f);
                stripe.SetStartCap(LineCapRound);
                stripe.SetEndCap(LineCapRound);
                for (float y : {-64.0f, -47.0f, -28.0f}) {
                    g.DrawBezier(&stripe, B(-27, y), B(-12, y - 7), B(12, y - 7), B(27, y));
                }
            }
        }

        if (back == 0.0f && coat != CatCoatTheme::Siamese) {
            SolidBrush b(bodyColor(coat));
            g.FillPath(&b, &path);
            if (patterned(coat)) {
                GraphicsState cs = g.Save(); g.SetClip(&path, CombineModeIntersect);
                drawCoatMarks(g, seatX, seatY, headCenter, physics, coat); g.Restore(cs);
            }
        }
        if (coat == CatCoatTheme::OrcaCat && turn < 0.63f) {
            float rotation = turn / 0.63f * 1.57079633f;
            float width = std::cos(rotation), side = std::sin(rotation);
            // The marking wraps around the chest and stays attached to the chin at profile.
            auto chest = [&](float x, float y) { return B(x * width + 39.0f * side, y); };
            auto chin = [&](float x, float y) {
                Pt p = projectHead3D(x, y, 22.0f, yaw * 0.50f, pitch * 0.36f);
                return H(p.x * width + 29.0f * side, p.y);
            };
            GraphicsPath bib;
            bib.AddBezier(chin(-27, 24), chin(-24, 17), chin(-16, 17), chin(0, 17));
            bib.AddBezier(chin(0, 17), chin(16, 17), chin(24, 17), chin(27, 24));
            bib.AddBezier(chin(27, 24), chin(31, 30), chest(30, -47), chest(22, -32));
            bib.AddBezier(chest(22, -32), chest(18, -24), chest(10, -19), chest(5, -15));
            bib.AddLine(chest(5, -15), chest(5, -19));
            bib.AddBezier(chest(5, -19), chest(2, -14), chest(0, -12), chest(-2, -10));
            bib.AddLine(chest(-2, -10), chest(-3, -16));
            bib.AddBezier(chest(-3, -16), chest(-17, -23), chest(-28, -29), chest(-29, -52));
            bib.AddBezier(chest(-29, -52), chest(-31, -66), chin(-31, 31), chin(-27, 24));
            bib.CloseFigure();
            float coverage = CatPhysics::ease((0.63f - turn) / 0.08f);
            Color whiteFur = pawColor(coat);
            SolidBrush white(Color(static_cast<BYTE>(255 * coverage), whiteFur.GetR(), whiteFur.GetG(), whiteFur.GetB()));
            GraphicsState clip = g.Save();
            g.SetClip(&path, CombineModeIntersect);
            g.FillPath(&white, &bib);
            g.Restore(clip);
        } else if (coat == CatCoatTheme::Siamese && back == 0.0f) {
            GraphicsState cs = g.Save();
            g.SetClip(&path, CombineModeIntersect);

            LinearGradientBrush bodyGrad(
                PointF(seatX, headCenter.y - 54.0f),
                PointF(seatX, seatY + 38.0f),
                siameseCream(),
                siameseLatte()
            );
            bodyGrad.SetWrapMode(WrapModeTileFlipXY);
            Color bodyCols[4] = { siameseCream(), siameseCream(), siameseLatte(), siameseLatte() };
            REAL bodyPos[4] = { 0.0f, 0.22f, 0.80f, 1.0f };
            bodyGrad.SetInterpolationColors(bodyCols, bodyPos, 4);
            g.FillPath(&bodyGrad, &path);

            // 2.5D Rotating Seal-Point "挖煤" Face Mask!
            float yawRad = turnYaw(physics);
            float pitchRad = pitch * 0.36f;
            Pt maskC = projectHead3D(0.0f, 2.5f, 27.0f, yawRad, pitchRad);
            float maskSX = std::max(0.76f, std::cos(yawRad * 0.7f)) * 1.18f;
            float maskSY = std::max(0.82f, std::cos(pitchRad * 0.7f)) * 0.88f;

            GraphicsState ms = g.Save();
            g.TranslateTransform(headCenter.x, headCenter.y);
            g.RotateTransform(hTilt * RadToDeg);
            g.TranslateTransform(maskC.x, maskC.y);
            g.ScaleTransform(maskSX, maskSY);

            GraphicsPath maskEllipse;
            maskEllipse.AddEllipse(-31.0f, -31.0f, 62.0f, 62.0f);
            PathGradientBrush maskBrush(&maskEllipse);
            Color cols[4] = { Color(0, 115, 87, 69), Color(178, 115, 87, 69), siamesePoint(245), siamesePoint(255) };
            REAL pos[4] = { 0.0f, 0.26f, 0.56f, 1.0f };
            maskBrush.SetInterpolationColors(cols, pos, 4);
            g.FillPath(&maskBrush, &maskEllipse);

            g.Restore(ms);
            g.Restore(cs);

            // Clean subtle warm-cocoa outline so Siamese pops on white & dark wallpapers
            Pen outPen(siameseOutline(), 1.5f);
            g.DrawPath(&outPen, &path);
        }

        if (physics.turnAway() > 0.0f) return;

        // Tucked hind paws at bottom
        GraphicsState ps = g.Save();
        g.TranslateTransform(seatX, seatY);
        g.RotateTransform(bTilt * RadToDeg);
        g.ScaleTransform(sx, sy);

        SolidBrush pawBrush(pawColor(coat));
        g.FillEllipse(&pawBrush, -35.0f, 3.0f, 17.0f, 11.0f);
        if (physics.hindFootActive.value < 0.35f) {
            g.FillEllipse(&pawBrush, 18.0f, 3.0f, 17.0f, 11.0f);
        }

        Pen rimPen(coat == CatCoatTheme::Siamese ? siameseCream(140) : creaseColor(coat,148), 1.3f);
        rimPen.SetStartCap(LineCapRound); rimPen.SetEndCap(LineCapRound);
        g.DrawArc(&rimPen, -34.0f, 1.0f, 15.0f, 15.0f, 15.0f, 150.0f);
        if (physics.hindFootActive.value < 0.35f) {
            g.DrawArc(&rimPen, 19.0f, 1.0f, 15.0f, 15.0f, 15.0f, 150.0f);
        }
        g.Restore(ps);
    }

    // MARK: - Hind Foot

    static void drawHindFoot(Gdiplus::Graphics& g, float seatX, float seatY, const CatPhysics& physics, CatCoatTheme coat) {
        Pt hip = { seatX + 26.0f, seatY + 2.0f };
        Pt target = physics.hindFoot.point();
        Pt mid = { (hip.x + target.x) * 0.5f + 10.0f, (hip.y + target.y) * 0.5f - 8.0f };
        drawLimb(g, hip, mid, target, 15.0f, 12.0f, Limb::HindLeg, 0.85f * physics.hindFootActive.value, coat);
    }

    // MARK: - 2.5D Eyes & Whiskers

    static void drawHeadFeatures(Gdiplus::Graphics& g, Pt center, const CatPhysics& physics, EyeColorTheme eyeTheme, CatCoatTheme coat) {
        using namespace Gdiplus;
        GraphicsState st = g.Save();
        g.TranslateTransform(center.x, center.y);
        g.RotateTransform(physics.headTilt.value * RadToDeg);

        float yawRad = turnYaw(physics);
        float pitchRad = physics.headPitch.value * 0.36f;
        if (physics.turnAway() > 0.0f) {
            GraphicsPath skull;
            skull.AddEllipse(-40.0f, -30.0f, 73.5f, 76.0f);
            g.SetClip(&skull, CombineModeIntersect);
        }

        if (coat == CatCoatTheme::OrcaCat && yawRad < 1.33f) drawOrcaMuzzle(g, yawRad, pitchRad);
        drawEye3D(g, true, yawRad, pitchRad, physics, eyeTheme, coat);
        drawEye3D(g, false, yawRad, pitchRad, physics, eyeTheme, coat);
        if (yawRad < 1.12f) drawWhiskers3D(g, yawRad, pitchRad, physics, coat);
        if (physics.headphones() > 0.015f) drawHeadphones(g, physics.headphones(), yawRad, eyeTheme);
        g.Restore(st);
    }

    static void drawHeadphones(Gdiplus::Graphics& g, float fit, float yaw, EyeColorTheme eyes) {
        using namespace Gdiplus;
        const BYTE opacity = (BYTE)(255.0f * fit);
        const float offset = 9.0f * (1.0f - fit);
        auto color = [opacity](unsigned int hex) { return rgb(hex, opacity); };
        auto saved = g.Save();
        g.TranslateTransform(yaw * 5.5f, -offset);
        GraphicsPath band;
        band.AddBezier(-43.0f, -8.0f, -43.0f, -47.0f, 43.0f, -47.0f, 43.0f, -8.0f);
        Pen edge(color(0x171D27), 7.7f);
        Pen padding(color(0x536172), 4.8f);
        g.DrawPath(&edge, &band);
        g.DrawPath(&padding, &band);
        SolidBrush foam(color(0x19212B)), shell(color(0x394757));
        Pen rim(color(0x6C7D8C), 0.9f);
        SolidBrush trim(eyePrimary(eyes, opacity));
        for (float direction : {-1.0f, 1.0f}) {
            GraphicsPath pad, cup, accent;
            float x = direction * (41.3f + offset * 0.32f);
            addRoundedRect(pad, x - 8.0f, -10.0f, 16.0f, 24.0f, 6.0f);
            addRoundedRect(cup, x - 5.75f + direction, -8.5f, 11.5f, 20.5f, 4.7f);
            addRoundedRect(accent, x - 1.45f + direction * 2.5f, -4.8f, 2.9f, 10.0f, 1.4f);
            g.FillPath(&foam, &pad);
            g.FillPath(&shell, &cup);
            g.DrawPath(&rim, &cup);
            g.FillPath(&trim, &accent);
        }
        g.Restore(saved);
    }

    static void drawOrcaMuzzle(Gdiplus::Graphics& g, float yaw, float pitch) {
        using namespace Gdiplus;
        auto face = [&](float x, float y, float depth = 31.0f) {
            Pt p = projectHead3D(x, y, depth, yaw, pitch);
            return PointF(p.x, p.y);
        };
        PointF bridge = face(-1, 12), rightEdge = face(19, 18.5f);
        PointF rightPad = face(12, 27), lip = face(0.5f, 22);
        PointF leftPad = face(-10, 26.5f), leftEdge = face(-18.5f, 19);
        GraphicsPath muzzle;
        muzzle.AddBezier(bridge, face(2, 12), face(3, 9.2f), face(8, 9.2f));
        muzzle.AddBezier(face(8, 9.2f), face(14, 9.2f), face(19, 14), rightEdge);
        muzzle.AddBezier(rightEdge, face(19.5f, 24), face(17, 27), rightPad);
        muzzle.AddBezier(rightPad, face(7, 27.5f), face(3, 25.5f), lip);
        muzzle.AddBezier(lip, face(-2.5f, 24.8f), face(-6.5f, 26.5f), leftPad);
        muzzle.AddBezier(leftPad, face(-15.5f, 27), face(-18.5f, 23.5f), leftEdge);
        muzzle.AddBezier(leftEdge, face(-18.5f, 13.5f), face(-10, 11), bridge);
        muzzle.CloseFigure();
        SolidBrush fur(catBlack());
        g.FillPath(&fur, &muzzle);

        PointF noseL = face(-3.8f, 13.0f, 34), noseR = face(3.8f, 13.0f, 34);
        PointF noseTip = face(0, 16.3f, 35);
        GraphicsPath nose;
        nose.AddBezier(noseL, face(-3.8f, 11.6f, 34), face(3.8f, 11.6f, 34), noseR);
        nose.AddBezier(noseR, face(3.8f, 14.2f, 34), face(1.2f, 16.3f, 35), noseTip);
        nose.AddBezier(noseTip, face(-1.2f, 16.3f, 35), face(-3.8f, 14.2f, 34), noseL);
        nose.CloseFigure();
        SolidBrush noseLeather(rgb(0x0D0D0F));
        g.FillPath(&noseLeather, &nose);
        Pen groove(rgb(0x101012), 0.7f);
        g.DrawLine(&groove, noseTip, lip);
    }

    static void drawEye3D(Gdiplus::Graphics& g, bool isLeft, float yawRad, float pitchRad, const CatPhysics& physics, EyeColorTheme eyeTheme, CatCoatTheme coat) {
        using namespace Gdiplus;
        float sign = isLeft ? -1.0f : 1.0f;
        Color eyeCol = eyePrimary(eyeTheme);
        Pt proj = projectHead3D(sign * 17.2f, -2.0f, 27.5f, yawRad, pitchRad);

        float eyeNormal = yawRad + sign * 0.34f;
        if (physics.turnAway() >= 0.57f || std::cos(eyeNormal) <= 0.0f) return;
        float foreX = std::clamp(std::cos(eyeNormal) / std::cos(0.34f), physics.turnAway() > 0.0f ? 0.05f : 0.64f, 1.06f);
        float foreY = std::clamp(std::cos(pitchRad * 0.85f), 0.78f, 1.03f);

        GraphicsState st = g.Save();
        g.TranslateTransform(proj.x, proj.y);
        g.ScaleTransform(foreX, foreY);

        bool angry = physics.angry();
        float relaxed = angry ? 0.0f : std::max({physics.sleepiness, physics.washLift(), physics.musicEyes()});
        if (relaxed > 0.68f) {
            GraphicsPath lid;
            addQuadBezier(lid, {-9.5f, 2.0f}, {0.0f, 8.4f}, {9.5f, 2.0f});
            Color muted = blendColor(eyeCol, catBlack(), 0.43f);
            BYTE alpha = (BYTE)(240.0f * CatPhysics::ease((relaxed - 0.68f) / 0.26f));
            Pen line(Color(alpha, muted.GetR(), muted.GetG(), muted.GetB()), 1.55f);
            line.SetStartCap(LineCapRound);
            line.SetEndCap(LineCapRound);
            g.DrawPath(&line, &lid);
        }

        // Mode 1: Happy closed glowing curved line eyes (petting head!)
        if (!angry && physics.isHappyClosedEyes()) {
            GraphicsPath hp;
            addQuadBezier(hp, {-11.5f, 1.0f}, {sign * 1.5f, 3.4f}, {11.5f, 1.0f});
            Pen p(eyeCol, 2.6f);
            p.SetStartCap(LineCapRound); p.SetEndCap(LineCapRound);
            g.DrawPath(&p, &hp);
            g.Restore(st);
            return;
        }

        // Mode 2: Blissful half-squint glowing slits (petting body!)
        if (!angry && physics.isBlissfulSquint()) {
            GraphicsPath sp;
            addQuadBezier(sp, {-11.5f, -0.5f}, {0.0f, 0.6f}, {11.5f, -0.5f});
            addQuadBezier(sp, {11.5f, -0.5f}, {0.0f, 4.8f}, {-11.5f, -0.5f});
            sp.CloseFigure();
            SolidBrush sb(eyeCol);
            g.FillPath(&sb, &sp);
            g.Restore(st);
            return;
        }

        float hunt = angry ? 0.0f : std::clamp(physics.huntIntensity.value, 0.0f, 1.0f);
        g.RotateTransform(sign * (0.06f * (1.0f - hunt * 0.6f)) * RadToDeg);

        float rx = 12.0f + hunt * 1.1f;
        float ry = 10.8f + hunt * 1.8f;

        GraphicsPath eyeClip;
        eyeClip.AddEllipse(-rx, -ry, rx * 2.0f, ry * 2.0f);
        GraphicsState cs = g.Save();
        g.SetClip(&eyeClip, CombineModeIntersect);
        // Leave covered pixels untouched so the underlying face remains seamless.
        float closure = angry ? 0.0f : std::clamp(physics.effectiveEyeClosure(), 0.0f, 1.0f);
        if (closure >= 1.0f) {
            g.Restore(cs);
            g.Restore(st);
            return;
        }
        RectF opening(-rx - 2.0f, -ry + 2.0f * ry * closure,
                      2.0f * rx + 4.0f, 2.0f * ry * (1.0f - closure));
        if (closure > 0.0f) g.SetClip(opening, CombineModeIntersect);
        if (angry) {
            GraphicsPath frown;
            addQuadPoly(frown, {-rx - 2.0f, -2.0f + sign * 6.6f}, {rx + 2.0f, -2.0f - sign * 6.6f},
                        {rx + 2.0f, ry + 2.0f}, {-rx - 2.0f, ry + 2.0f});
            g.SetClip(&frown, CombineModeIntersect);
        }

        SolidBrush irisBrush(eyeCol);
        g.FillRectangle(&irisBrush, -rx - 1.0f, -ry - 1.0f, rx * 2.0f + 2.0f, ry * 2.0f + 2.0f);

        // Pupil morphs smoothly from vertical slit -> HUGE round black boba pupil!
        Pt gaze = physics.eyeGaze.point();
        float px = gaze.x * (4.2f - hunt * 1.2f);
        float py = gaze.y * (3.6f - hunt * 1.0f);
        float pScale = physics.pupilScale.value;
        float normalW = 5.0f * pScale;
        float normalH = 13.5f * std::min(1.12f, pScale);
        float huntDiam = 18.8f;
        float pupilW = normalW + (huntDiam - normalW) * hunt;
        float pupilH = normalH + (huntDiam - normalH) * hunt;

        // Reference iris colors, blended only in a narrow ring around the moving pupil.
        if (eyeTheme == EyeColorTheme::ReferenceHazel) {
            for (int i = 0; i <= 24; ++i) {
                float t = i / 24.0f, mix = t*t*(3-2*t), rim = (1-t)*2.2f;
                SolidBrush ring(Color(255, (BYTE)std::lround(218-77*mix),
                    (BYTE)std::lround(178-33*mix), (BYTE)std::lround(87+41*mix)));
                g.FillEllipse(&ring, px-pupilW*0.5f-rim, py-pupilH*0.5f-rim,
                    pupilW+2*rim, pupilH+2*rim);
            }
        }
        SolidBrush pupilBrush(catBlack());
        g.FillEllipse(&pupilBrush, px - pupilW * 0.5f, py - pupilH * 0.5f, pupilW, pupilH);

        // Tiny crisp white sparkle glint as boba hunting pupils dilate!
        if (hunt > 0.25f) {
            BYTE ga = (BYTE)(std::min(1.0f, (hunt - 0.25f) / 0.55f) * 235.0f);
            float gr = 2.1f + hunt * 0.9f;
            SolidBrush gb(Color(ga, 255, 255, 255));
            g.FillEllipse(&gb, px - pupilW * 0.22f - gr * 0.5f, py - pupilH * 0.22f - gr * 0.5f, gr * 2.0f, gr * 2.0f);
        }

        g.Restore(cs);
        g.Restore(st);
    }

    static void drawWhiskers3D(Gdiplus::Graphics& g, float yawRad, float pitchRad, const CatPhysics& physics, CatCoatTheme coat) {
        using namespace Gdiplus;
        float wiggle = std::sin(physics.time * 4.5f) * (std::max(physics.petHeadIntensity, physics.petBodyIntensity) * 1.2f);
        Pen pen(coat == CatCoatTheme::Siamese ? siameseWhisker() : (patterned(coat) ? rgb(0x8D8170,174) : whiskerWhite()), 1.05f);
        pen.SetStartCap(LineCapRound); pen.SetEndCap(LineCapRound);

        const float rows[3][3] = {
            { 10.5f, 36.5f, 8.8f + wiggle },
            { 14.2f, 37.8f, 14.2f + wiggle * 0.5f },
            { 18.0f, 35.5f, 19.6f - wiggle * 0.4f }
        };

        for (float sign : {-1.0f, 1.0f}) {
            for (int i = 0; i < 3; ++i) {
                Pt p0 = coat == CatCoatTheme::OrcaCat
                    ? projectHead3D(sign * (18.0f + i), rows[i][0] + 1.0f, 27.0f, yawRad, pitchRad)
                    : projectHead3D(sign * 24.0f, rows[i][0], 23.0f, yawRad, pitchRad);
                Pt p2 = projectHead3D(sign * rows[i][1], rows[i][2], 14.0f, yawRad, pitchRad);
                Pt ctrl = { (p0.x + p2.x) * 0.5f, (p0.y + p2.y) * 0.5f - 0.8f };
                GraphicsPath wp;
                addQuadBezier(wp, p0, ctrl, p2);
                g.DrawPath(&pen, &wp);
            }
        }
    }

    // MARK: - Front Paws

    static void drawArcade(Gdiplus::Graphics& g, const CatPhysics& physics, KeyboardTheme theme) {
        using namespace Gdiplus;
        const auto colors = keyboardColors(theme);
        const Color plate = blendColor(colors.deck, colors.cap, 0.16f);
        const Color accent = theme == KeyboardTheme::ClassicMilk ? rgb(0xDC527B) : colors.accent;
        auto panel = [&](float x, float y, float w, float h) {
            GraphicsPath lower, upper;
            addRoundedRect(lower, x, y+5, w, h, 7);
            addRoundedRect(upper, x, y, w, h, 7);
            SolidBrush base(colors.base), top(plate);
            g.FillPath(&base, &lower);
            g.FillPath(&top, &upper);
            Pen rim(blendColor(colors.border, colors.cap, 0.36f), 1.0f);
            g.DrawPath(&rim, &upper);
        };
        panel(161, 236, 133, 57);
        const Pt pivot = CatPhysics::ArcadePivot, ball = physics.arcadeBall();
        SolidBrush socket(colors.base), knob(accent);
        g.FillEllipse(&socket, pivot.x-18, pivot.y-9, 36.0f, 24.0f);
        SolidBrush socketTop(plate);
        g.FillEllipse(&socketTop, pivot.x-18, pivot.y-12, 36.0f, 24.0f);
        Pen ring(blendColor(colors.border,colors.cap,0.35f),1.1f);
        g.DrawEllipse(&ring, pivot.x-18,pivot.y-12,36.0f,24.0f);
        Pen stem(rgb(0xB8BCC6), 6);
        stem.SetEndCap(LineCapRound);
        g.DrawLine(&stem,pivot.x,pivot.y,ball.x,ball.y);
        SolidBrush edge(blendColor(accent, colors.base, 0.49f));
        g.FillEllipse(&edge,ball.x-10.4f,ball.y-9.0f,20.8f,20.8f);
        g.FillEllipse(&knob,ball.x-10,ball.y-10.3f,20.0f,19.8f);
        SolidBrush reflection(rgb(0xFFFFFF,115));
        g.FillEllipse(&reflection,ball.x-6,ball.y-6,7.0f,3.5f);
        for (int i=0; i<8; ++i) {
            const Pt p = CatPhysics::arcadeButtonCenter(i);
            const float press = physics.arcadeButtonPressure[i];
            const float shift = press*2.5f;
            const Color face = i % 4 < 2 ? accent : blendColor(accent,colors.cap,0.62f);
            SolidBrush rim(colors.base), lip(blendColor(face,colors.base,0.38f));
            SolidBrush cap(blendColor(face,rgb(0xFFFFFF),press*0.32f));
            g.FillEllipse(&rim,p.x-11.8f,p.y-9.1f,23.6f,18.2f);
            g.FillEllipse(&lip,p.x-10.6f,p.y-8.8f,21.2f,16.9f);
            g.FillEllipse(&cap,p.x-9.5f,p.y-8.9f+shift,19.0f,13.3f);
            Pen highlight(rgb(0xFFFFFF,115),0.8f);
            g.DrawArc(&highlight,p.x-8.2f,p.y-7.3f+shift,16.4f,10.7f,196,142);
        }
    }

    static void drawTablet(Gdiplus::Graphics& g, const CatPhysics& physics) {
        using namespace Gdiplus;
        auto panel = [&](float x, float y, float w, float h, float r, Color color) {
            GraphicsPath shape;
            addRoundedRect(shape,x,y,w,h,r);
            SolidBrush brush(color);
            g.FillPath(&brush,&shape);
        };
        constexpr float top = CatPhysics::tabletTop, height = CatPhysics::tabletHeight;
        constexpr float depth = CatPhysics::tabletDepth, left = CatPhysics::keypadLeft;
        constexpr float half = CatPhysics::keyHalf;
        panel(118,top+depth,207,height,8,rgb(0x28333D));
        panel(118,top,207,height,8,rgb(0x505F6A));
        panel(129,235,187,55,4,rgb(0x27353B));
        panel(131,237,183,51,3,rgb(0xE5E9DE));
        panel(121,239,5,2,1,physics.penDown ? rgb(0x91E6C5) : rgb(0x99ABA9));

        panel(left,top+depth,height,height,6,rgb(0x28333D));
        panel(left,top,height,height,6,rgb(0x505F6A));
        double now = nowSeconds();
        for (int i = 0; i < CatPhysics::tabletKeyCount; ++i) {
            const auto& cap = physics.tabletKeys[i];
            Pt pt = CatPhysics::tabletKeyCenter(i);
            bool lit = cap.held || now - cap.pressedAt < 0.48;
            float y = pt.y + (cap.held ? 1.6f : 0.f);
            panel(pt.x-half,pt.y-half+1.5f,2*half,2*half,2.5f,rgb(0x34414A));
            panel(pt.x-half,y-half,2*half,2*half,2.5f,lit ? rgb(0x91E6C5) : rgb(0xDFE5DC));
        }
        Pen ink(rgb(0x355A5F),1.35f);
        ink.SetStartCap(LineCapRound);
        ink.SetEndCap(LineCapRound);
        SolidBrush dot(rgb(0x355A5F));
        Pt previous{};
        for (const auto& sample : physics.ink) {
            Pt pt = sample.pos;
            if (sample.startsStroke) g.FillEllipse(&dot,pt.x-0.7f,pt.y-0.7f,1.4f,1.4f);
            else g.DrawLine(&ink,previous.x,previous.y,pt.x,pt.y);
            previous=pt;
        }
    }

    static void drawStylus(Gdiplus::Graphics& g, const CatPhysics& physics) {
        using namespace Gdiplus;
        Pt t=physics.tabletTip;
        if (!physics.penDown) t.y -= 4.f;
        Pen barrel(rgb(0x59657A),4.5f), nib(rgb(0x2C3844),1.7f), band(rgb(0x9AD6C5),3.5f);
        barrel.SetEndCap(LineCapRound);
        g.DrawLine(&barrel,t.x+2.6f,t.y-5.f,t.x+15.f,t.y-29.f);
        g.DrawLine(&band,t.x+12.f,t.y-23.f,t.x+13.f,t.y-25.f);
        g.DrawLine(&nib,t.x,t.y,t.x+2.6f,t.y-5.f);
    }

    static float bowlOpacity(float age) {
        return CatPhysics::ease(age / 0.28f) * CatPhysics::ease((CatPhysics::BowlEnd - age) / 0.43f);
    }

    static void drawTreatBowl(Gdiplus::Graphics& g, float age) {
        using namespace Gdiplus;
        float show = bowlOpacity(age);
        auto tint = [show](unsigned int hex, BYTE alpha = 255) { return rgb(hex, (BYTE)(alpha * show)); };
        GraphicsState state = g.Save();
        g.TranslateTransform(CatPhysics::SeatAnchor.x, 216.0f + 3.0f * (1.0f - show));

        SolidBrush shadow(tint(0x161618, 64));
        g.FillEllipse(&shadow, -20.0f, 7.7f, 40.0f, 7.3f);
        GraphicsPath ceramic;
        ceramic.AddBezier(-22.0f, 0.0f, -21.0f, 7.0f, -18.0f, 12.0f, 0.0f, 12.0f);
        ceramic.AddBezier(0.0f, 12.0f, 18.0f, 12.0f, 21.0f, 7.0f, 22.0f, 0.0f);
        ceramic.CloseFigure();
        LinearGradientBrush glaze(PointF(0, -1), PointF(0, 13), tint(0xC2DFDC), tint(0x6B9EA2));
        g.FillPath(&glaze, &ceramic);
        SolidBrush rim(tint(0xD9EEEA)), interior(tint(0x668C8F));
        g.FillEllipse(&rim, -22.0f, -6.0f, 44.0f, 12.0f);
        g.FillEllipse(&interior, -18.5f, -3.8f, 37.0f, 7.3f);

        const float eatenAt[] = {1.08f, 1.56f, 2.04f};
        for (int i = 0; i < 3; ++i) {
            float x = (i - 1) * 7.1f;
            float y = i == 1 ? 0.0f : -1.0f;
            float piece = CatPhysics::ease((eatenAt[i] - age) / 0.12f);
            SolidBrush treat(tint(0xD5895D, (BYTE)(255 * piece)));
            g.FillEllipse(&treat, x - 3.25f, y - 2.2f, 6.5f, 4.4f);
        }
        g.Restore(state);
    }

    static void drawTurningPaws(Gdiplus::Graphics& g, float seatX, float seatY, float step,
                                float turn, CatCoatTheme coat, const CatPhysics& physics) {
        using namespace Gdiplus;
        if (turn >= 0.61f) return;
        float weight = CatPhysics::ease(turn / 0.22f);
        float arc = std::sin(turn * 3.14159265f);
        float nearVisibility = 1.0f - CatPhysics::ease((turn - 0.43f) / 0.18f);
        float farVisibility = 1.0f - CatPhysics::ease((turn - 0.24f) / 0.23f);
        Pt nearRest = physics.leftPaw.point(), farRest = physics.rightPaw.point();
        Pt nearPaw = {nearRest.x + (seatX - 22.0f + arc * 35.0f - nearRest.x) * weight,
                      nearRest.y + (seatY + 9.0f - step * 8.0f - (1.0f - nearVisibility) * 6.0f - nearRest.y) * weight};
        Pt farPaw = {farRest.x + (seatX + 26.0f + arc * 14.0f - farRest.x) * weight,
                     farRest.y + (seatY + 7.0f - (arc - step) * 8.0f - (1.0f - farVisibility) * 5.0f - farRest.y) * weight};
        Pt shoulder = {seatX - 14.0f + arc * 48.0f, seatY - 38.0f};
        if (nearVisibility > 0.02f) {
            Pt middle = {(shoulder.x + nearPaw.x) * 0.5f - 3.0f, (shoulder.y + nearPaw.y) * 0.5f};
            float chestFur = 0.65f * nearVisibility * (1.0f - CatPhysics::ease((turn - 0.28f) / 0.16f));
            drawLimb(g, shoulder, middle, nearPaw, 16.0f * nearVisibility, 12.0f * nearVisibility,
                     Limb::LeftForeleg, 0.60f * nearVisibility, coat, chestFur);
        }
        SolidBrush fur(pawColor(coat));
        g.FillEllipse(&fur, nearPaw.x - 8.0f * nearVisibility, nearPaw.y - 4.0f * nearVisibility,
                      16.0f * nearVisibility, 9.0f * nearVisibility);
        if (farVisibility > 0.0f) {
            g.FillEllipse(&fur, farPaw.x - 7.0f * farVisibility, farPaw.y - 4.0f * farVisibility,
                          14.0f * farVisibility, 9.0f * farVisibility);
        }
    }

    static void drawFrontPaws(Gdiplus::Graphics& g, float seatX, float seatY, float breath, const CatPhysics& physics, CatCoatTheme coat, bool tabletMode) {
        Pt lPos = physics.leftPaw.point();
        Pt rPos = tabletMode ? physics.penGrip() : physics.rightPaw.point();

        float lDist = std::hypot(lPos.x - CatPhysics::LeftPawRest.x, lPos.y - CatPhysics::LeftPawRest.y);
        float rDist = std::hypot(rPos.x - CatPhysics::RightPawRest.x, rPos.y - CatPhysics::RightPawRest.y);

        auto shoulder = [&](float x) -> Pt {
            float sy=physics.bodySquash.value, sx=1+(1-sy)*0.55f;
            float c=std::cos(physics.bodyTilt.value), s=std::sin(physics.bodyTilt.value);
            float chin = 126.0f + physics.headOffsetY.value + physics.headPitch.value * 4.8f + breath * 1.1f;
            float floor = chin + (physics.usingDesk() ? 52.0f : 33.0f);
            float shoulderY = seatY + x*sx*s - 43*sy*c;
            return {seatX+x*sx*c+43*sy*s, std::max(shoulderY, floor)};
        };
        Pt leftShoulder=shoulder(-20), rightShoulder=shoulder(20);
        float chestFur = physics.feeding() ? 0.14f : 1.0f - physics.sleepiness * 0.80f;

        if (physics.usingDesk() && lDist < 6.0f && !physics.leftPawOnMouse && !physics.leftPawTyping) {
            drawIdleFrontLeg(g, { seatX - 18.0f, seatY - 30.0f }, { seatX - 16.0f, seatY + 12.0f }, true, coat);
        } else {
            Pt mid = { (leftShoulder.x + lPos.x) * 0.5f - (physics.leftPawOnMouse ? 8.0f : 4.0f),
                       (leftShoulder.y + lPos.y) * 0.5f - 6.0f };
            drawLimb(g, leftShoulder, mid, lPos, 19.0f, 13.5f, Limb::LeftForeleg, 0.78f, coat, chestFur);
        }

        if (physics.usingDesk() && !tabletMode && rDist < 6.0f && !physics.rightPawTyping) {
            drawIdleFrontLeg(g, { seatX + 18.0f, seatY - 30.0f }, { seatX + 16.0f, seatY + 12.0f }, false, coat);
        } else {
            float lift = physics.washLift();
            Pt mid = { (rightShoulder.x + rPos.x) * 0.5f + 4.0f + 31.0f * lift,
                       (rightShoulder.y + rPos.y) * 0.5f - 6.0f + 10.0f * lift };
            drawLimb(g, rightShoulder, mid, rPos, 19.0f, 13.5f, Limb::RightForeleg, 0.78f, coat, chestFur);
        }
    }

    static constexpr float orcaLegFeather = 1.5f;

    static void fillOrcaForeleg(Gdiplus::Graphics& g, const Gdiplus::GraphicsPath& leg, Pt shoulder, Pt paw, float chestFur = 1.0f) {
        using namespace Gdiplus;
        float dx = paw.x - shoulder.x, dy = paw.y - shoulder.y;
        float length = std::max(1.0f, std::hypot(dx, dy));
        float ux = dx / length, uy = dy / length;
        // Transparent padding lets the chest show through the rounded shoulder cap.
        constexpr float pad = 16.0f;
        float span = length + 2.0f * pad;
        float fade = std::min(34.0f, 0.52f * length);
        Color black = limbColor(CatCoatTheme::OrcaCat);
        Color shoulderColor = blendColor(black, pawColor(CatCoatTheme::OrcaCat), chestFur);
        Color clear(0, shoulderColor.GetR(), shoulderColor.GetG(), shoulderColor.GetB());
        LinearGradientBrush brush(PointF(shoulder.x - ux * pad, shoulder.y - uy * pad),
                                  PointF(paw.x + ux * pad, paw.y + uy * pad), clear, black);
        brush.SetWrapMode(WrapModeTileFlipXY);
        // Tuxedo fur changes from white to black at a crisp edge. A long grey ramp here read as a
        // smudge across the chest, so the white shoulder meets the black leg within a few pixels.
        float edge = pad + fade * 0.5f;
        float feather = std::min(orcaLegFeather, fade * 0.25f);
        Color colors[] = {clear, clear, shoulderColor, shoulderColor, black, black};
        REAL stops[] = {0, (pad - 3.0f) / span, (pad + fade * 0.22f) / span,
                        (edge - feather) / span, (edge + feather) / span, 1};
        brush.SetInterpolationColors(colors, stops, 6);
        g.FillPath(&brush, &leg);
    }

    static void drawIdleFrontLeg(Gdiplus::Graphics& g, Pt top, Pt bottom, bool isLeft, CatCoatTheme coat) {
        using namespace Gdiplus;
        float sign = isLeft ? -1.0f : 1.0f;

        if (coat == CatCoatTheme::Siamese) {
            GraphicsPath lp;
            addRoundedRect(lp, bottom.x - 7.5f, top.y, 15.0f, bottom.y - top.y + 1.0f, 6.5f);
            float pad = 10.0f;
            float span = std::max(1.0f, (bottom.y + 2.0f) - top.y);
            float total = span + pad * 2.0f;
            LinearGradientBrush grad(PointF(bottom.x, top.y - pad), PointF(bottom.x, bottom.y + 2.0f + pad), siameseCream(0), siamesePoint(255));
            grad.SetWrapMode(WrapModeTileFlipXY);
            Color cols[5] = { siameseCream(0), siameseCream(0), siameseMocha(190), siamesePoint(255), siamesePoint(255) };
            REAL pos[5] = { 0.0f, pad / total, (pad + span * 0.52f) / total, (pad + span * 0.85f) / total, 1.0f };
            grad.SetInterpolationColors(cols, pos, 5);
            g.FillPath(&grad, &lp);
        }

        if (coat == CatCoatTheme::OrcaCat) {
            GraphicsPath leg;
            addRoundedRect(leg, bottom.x - 7.5f, top.y, 15.0f, bottom.y - top.y, 7.5f);
            fillOrcaForeleg(g, leg, {bottom.x, top.y}, bottom);
            GraphicsPath toes;
            addRoundedRect(toes, bottom.x - 7.5f, bottom.y - 9.0f, 15.0f, 14.0f, 5.0f);
            SolidBrush white(pawColor(coat));
            g.FillPath(&white, &toes);
        } else if (patterned(coat)) {
            SolidBrush cuff(pawColor(coat));
            g.FillRectangle(&cuff, bottom.x-7.5f, bottom.y-13.0f, 15.0f, 13.0f);
            g.FillEllipse(&cuff, bottom.x-7.5f, bottom.y-5.0f, 15.0f, 10.0f);
        }
        Pen p(coat == CatCoatTheme::Siamese ? siameseOutline(158) : creaseColor(coat), 1.45f);
        p.SetStartCap(LineCapRound); p.SetEndCap(LineCapRound);

        GraphicsPath p1;
        addQuadBezier(p1, { top.x + sign * 8.5f, top.y }, { top.x + sign * 9.5f, (top.y + bottom.y) * 0.5f }, { bottom.x + sign * 7.5f, bottom.y - 1.0f });
        g.DrawPath(&p, &p1);

        GraphicsPath p2;
        addQuadBezier(p2, { top.x - sign * 7.0f, top.y + 4.0f }, { top.x - sign * 6.5f, (top.y + bottom.y) * 0.5f }, { bottom.x - sign * 6.0f, bottom.y - 1.0f });
        g.DrawPath(&p, &p2);
    }

    static void drawLimb(Gdiplus::Graphics& g, Pt start, Pt ctrl, Pt end, float startW, float endW, Limb limb,
                         float outlineAlpha, CatCoatTheme coat, float chestFur = 1.0f) {
        using namespace Gdiplus;
        bool outlineOnLeft = limb != Limb::LeftForeleg;
        constexpr int steps = 14;
        std::vector<PointF> leftEdge, rightEdge;
        leftEdge.reserve(steps + 1);
        rightEdge.reserve(steps + 1);

        for (int i = 0; i <= steps; ++i) {
            float t = (float)i / (float)steps;
            Pt p = quadPoint(start, ctrl, end, t);
            Pt tan = quadTangent(start, ctrl, end, t);
            float len = std::max(0.001f, std::hypot(tan.x, tan.y));
            float nx = -tan.y / len, ny = tan.x / len;
            float hw = (startW + (endW - startW) * t) * 0.5f;
            leftEdge.push_back(PointF(p.x + nx * hw, p.y + ny * hw));
            rightEdge.push_back(PointF(p.x - nx * hw, p.y - ny * hw));
        }

        std::vector<PointF> poly = leftEdge;
        for (int i = steps; i >= 0; --i) poly.push_back(rightEdge[i]);

        GraphicsPath limbPath;
        limbPath.AddLines(poly.data(), (INT)poly.size());
        Pt tangent=quadTangent(start,ctrl,end,0);
        float length=std::max(0.001f,std::hypot(tangent.x,tangent.y));
        addQuadBezier(limbPath, {rightEdge[0].X,rightEdge[0].Y},
            {start.x-tangent.x/length*startW*0.5f,start.y-tangent.y/length*startW*0.5f},
            {leftEdge[0].X,leftEdge[0].Y});
        limbPath.CloseFigure();

        float pawR = endW * 0.58f;
        RectF pawRect(end.x - pawR, end.y - pawR * 0.85f, pawR * 2.0f, pawR * 1.7f);

        if (coat != CatCoatTheme::Siamese) {
            SolidBrush b(limbColor(coat));
            if (patterned(coat)) {
                LinearGradientBrush gradient(PointF(start.x,start.y),PointF(end.x,end.y),pawColor(coat),limbColor(coat));
                gradient.SetWrapMode(WrapModeTileFlipXY);
                Color colors[3]={pawColor(coat),limbColor(coat),limbColor(coat)};
                REAL positions[3]={0,0.30f,1};gradient.SetInterpolationColors(colors,positions,3);
                g.FillPath(&gradient,&limbPath);
            } else if (coat == CatCoatTheme::OrcaCat && limb != Limb::HindLeg) {
                fillOrcaForeleg(g, limbPath, start, end, chestFur);
            } else g.FillPath(&b, &limbPath);
            if (!hasWhiteSocks(coat)) g.FillEllipse(&b, pawRect);
            else {
                std::vector<PointF> points;
                int first = coat == CatCoatTheme::OrcaCat ? steps - 1 : (int)(steps * 0.72f);
                for (int i=first; i<=steps; ++i) points.push_back(leftEdge[i]);
                for (int i=steps; i>=first; --i) points.push_back(rightEdge[i]);
                SolidBrush cuff(pawColor(coat));
                g.FillPolygon(&cuff, points.data(), (INT)points.size());
                g.FillEllipse(&cuff, pawRect);
                if (coat == CatCoatTheme::OrcaCat) {
                    // Shade the free edge of the white paw, leaving the wrist continuous.
                    Pt direction = quadTangent(start, ctrl, end, 1.0f);
                    float angle = std::atan2(direction.y, direction.x) * RadToDeg;
                    Pen edge(rgb(0x68666E), 1.45f);
                    edge.SetStartCap(LineCapRound);
                    edge.SetEndCap(LineCapRound);
                    RectF rim = pawRect;
                    rim.Inflate(-0.725f, -0.725f);
                    g.DrawArc(&edge, rim, angle - 100.0f, 200.0f);
                }
            }
        } else {
            // Pad gradient endpoints beyond limb geometry + use WrapModeTileFlipXY so GDI+
            // NEVER wraps t < 0 shoulder corners around to t = 1.0 dark espresso!
            float dx = end.x - start.x;
            float dy = end.y - start.y;
            float dist = std::hypot(dx, dy);
            float ux = (dist > 1.0f) ? (dx / dist) : 0.0f;
            float uy = (dist > 1.0f) ? (dy / dist) : 1.0f;
            dist = std::max(1.0f, dist);
            float padStart = startW + 18.0f;
            float padEnd = endW + 18.0f;
            float totalLen = padStart + dist + padEnd;

            PointF gStart(start.x - ux * padStart, start.y - uy * padStart);
            PointF gEnd(end.x + ux * padEnd, end.y + uy * padEnd);

            LinearGradientBrush grad(gStart, gEnd, siameseCream(0), siamesePoint(255));
            grad.SetWrapMode(WrapModeTileFlipXY);
            Color cols[6] = {
                siameseCream(0),
                siameseCream(0),
                siameseCream(255),
                siameseMocha(255),
                siamesePoint(255),
                siamesePoint(255)
            };
            REAL pos[6] = {
                0.0f,
                padStart / totalLen,
                (padStart + dist * 0.16f) / totalLen,
                (padStart + dist * 0.48f) / totalLen,
                (padStart + dist * 0.78f) / totalLen,
                1.0f
            };
            grad.SetInterpolationColors(cols, pos, 6);
            g.FillPath(&grad, &limbPath);

            SolidBrush pawBrush(siamesePoint());
            g.FillEllipse(&pawBrush, pawRect);

            const auto& outerEdge = outlineOnLeft ? rightEdge : leftEdge;
            Pen op(siameseOutline(184), 1.25f);
            op.SetStartCap(LineCapRound); op.SetEndCap(LineCapRound); op.SetLineJoin(LineJoinRound);
            g.DrawLines(&op, outerEdge.data() + 3, (INT)outerEdge.size() - 4);
        }

        const auto& hiEdge = outlineOnLeft ? leftEdge : rightEdge;
        int startIdx = 2;
        int endIdx = (int)((hiEdge.size() - 1) * 0.68f);
        if (endIdx > startIdx) {
            Color sc = (coat == CatCoatTheme::Siamese)
                ? siameseOutline((BYTE)(outlineAlpha * 199))
                : creaseColor(coat, (BYTE)(outlineAlpha * 229));
            Pen hp(sc, 1.35f);
            hp.SetStartCap(LineCapRound); hp.SetEndCap(LineCapRound); hp.SetLineJoin(LineJoinRound);
            g.DrawLines(&hp, hiEdge.data() + startIdx, endIdx - startIdx + 1);
        }
    }

    // MARK: - Floating Mechanical Keycaps

    static void drawFloatingKeycaps(Gdiplus::Graphics& g, const CatPhysics& physics, const CatSettings& settings) {
        using namespace Gdiplus;
        StringFormat sf;
        sf.SetAlignment(StringAlignmentCenter);
        sf.SetLineAlignment(StringAlignmentCenter);

        for (const auto& b : physics.floatingKeycaps) {
            if (b.mouse ? !settings.showMouseBadges : !settings.showKeyBadges) continue;
            float prog = b.age / b.lifetime;
            float scale = 1.0f;
            if (b.age < 0.09f) {
                scale = 0.72f + 0.38f * std::sin((b.age / 0.09f) * 3.14159f * 0.65f);
            } else if (b.age < 0.18f) {
                scale = 1.10f - 0.10f * ((b.age - 0.09f) / 0.09f);
            }

            float alphaF = prog < 0.62f ? 1.0f : std::max(0.0f, 1.0f - (prog - 0.62f) / 0.38f);
            BYTE a = (BYTE)(alphaF * 255.0f);
            if (a < 4) continue;

            size_t len = b.label.size();
            bool hasWide = !b.label.empty() && b.label[0] > 0x7F;
            float w = (len <= 1) ? 23.0f : (len == 2 ? (hasWide ? 34.0f : 30.0f) : 38.0f);
            float h = 22.0f;

            GraphicsState st = g.Save();
            g.TranslateTransform(b.position.x, b.position.y);
            g.RotateTransform(b.rotation * RadToDeg);
            g.ScaleTransform(scale, scale);

            GraphicsPath basePath;
            addRoundedRect(basePath, -w * 0.5f, -h * 0.5f + 2.5f, w, h, 5.5f);
            SolidBrush baseBrush(Color((BYTE)(alphaF * 242), 38, 38, 43));
            g.FillPath(&baseBrush, &basePath);

            GraphicsPath facePath;
            addRoundedRect(facePath, -w * 0.5f, -h * 0.5f, w, h, 5.2f);
            SolidBrush faceBrush(Color(a, 249, 248, 243));
            Pen facePen(Color(a, 41, 41, 46), 1.45f);
            g.FillPath(&faceBrush, &facePath);
            g.DrawPath(&facePen, &facePath);

            const Font& font = cachedFont(L"Segoe UI", len <= 1 ? 11.0f : 9.2f, FontStyleBold);
            SolidBrush txtBrush(Color(a, 31, 31, 36));
            g.DrawString(b.label.c_str(), -1, &font, RectF(-w * 0.5f, -h * 0.5f, w, h), &sf, &txtBrush);

            g.Restore(st);
        }
    }

    // MARK: - Petting Sparkles & Sleep Z

    static void drawSparkles(Gdiplus::Graphics& g, const CatPhysics& physics, EyeColorTheme eyeTheme, bool dopeySleep = false) {
        using namespace Gdiplus;
        Color baseCol = eyePrimary(eyeTheme);

        for (const auto& sp : physics.sparkles) {
            float p = std::min(1.0f, sp.age / sp.lifetime);
            BYTE a = (BYTE)(std::sin(p * 3.14159f) * 215.0f);
            if (a < 4) continue;

            Color c(a, baseCol.GetR(), baseCol.GetG(), baseCol.GetB());
            if (sp.isSleepZ) {
                if (dopeySleep) {
                    constexpr float delay = .28f;
                    for (int i = 0; i < 3; ++i) {
                        const float phase = (sp.age-i*delay)/(sp.lifetime-2*delay);
                        if (phase <= 0 || phase >= 1) continue;
                        const float opacity = CatPhysics::ease(phase/.16f) * CatPhysics::ease((1-phase)/.43f);
                        const Font& font = cachedFont(L"Segoe UI",10.f+4*i,FontStyleBold);
                        SolidBrush ink(Color(BYTE(225*opacity),135,146,203));
                        g.DrawString(L"z",1,&font,PointF(sp.position.x+11+12*i,sp.position.y-19-9*i),&ink);
                    }
                } else {
                    const Font& font = cachedFont(L"Segoe UI",11.f+p*3.f,FontStyleBold);
                    SolidBrush ink(c);
                    g.DrawString(L"z",1,&font,PointF(sp.position.x,sp.position.y),&ink);
                }
            } else {
                float r = 3.5f + p * 1.5f;
                GraphicsState st = g.Save();
                g.TranslateTransform(sp.position.x, sp.position.y);
                GraphicsPath star;
                addQuadBezier(star, {0, -r}, {0.8f, -0.8f}, {r, 0});
                addQuadBezier(star, {r, 0}, {0.8f, 0.8f}, {0, r});
                addQuadBezier(star, {0, r}, {-0.8f, 0.8f}, {-r, 0});
                addQuadBezier(star, {-r, 0}, {-0.8f, -0.8f}, {0, -r});
                star.CloseFigure();
                SolidBrush sb(c);
                g.FillPath(&sb, &star);
                g.Restore(st);
            }
        }
    }
};
