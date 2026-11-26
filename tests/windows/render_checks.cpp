// Windows renderer checks. Builds against the real headers in windows/src and draws into
// off-screen GDI+ bitmaps, so it never opens a window, reads input or changes settings.
// Build and run: tests/windows/run.bat (CI runs it on windows-latest).
#define NOMINMAX
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <map>
#include <tuple>
#include <string>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <random>
#include <functional>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <cwctype>
#include <cwchar>
#include <cstdint>
#include <cstring>
// The leg shading helper and bar layout are private to the renderer; open them up for this test
// only. Standard headers are included above so the macro never touches them.
#define _ALLOW_KEYWORD_MACROS
#define private public
#include "../../windows/src/CatRenderer.hpp"
#undef private

#pragma comment(lib, "gdiplus.lib")

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FAIL: %s\n", msg); ++failures; } } while (0)

struct Canvas {
    int w, h;
    std::vector<BYTE> pixels;
    Gdiplus::Bitmap bitmap;
    Canvas(int width, int height)
        : w(width), h(height), pixels((size_t)width * height * 4, 0),
          bitmap(width, height, width * 4, PixelFormat32bppPARGB, pixels.data()) {}
    const BYTE* at(int x, int y) const { return &pixels[((size_t)y * w + x) * 4]; } // B, G, R, A
};

static std::vector<BYTE> renderFrame(const CatPhysics& physics, const CatSettings& settings) {
    Canvas canvas(360, 320);
    {
        Gdiplus::Graphics g(&canvas.bitmap);
        g.Clear(Gdiplus::Color(0, 0, 0, 0));
        CatRenderer::draw(g, physics, settings, false);
    }
    return canvas.pixels;
}

static void orcaLegHasCrispEdge() {
    // One straight foreleg, shoulder at the top, paw at the bottom, on a transparent canvas.
    Canvas canvas(40, 120);
    {
        Gdiplus::Graphics g(&canvas.bitmap);
        g.Clear(Gdiplus::Color(0, 0, 0, 0));
        Gdiplus::GraphicsPath leg;
        leg.AddRectangle(Gdiplus::RectF(12.5f, 10.0f, 15.0f, 100.0f));
        CatRenderer::fillOrcaForeleg(g, leg, {20.0f, 10.0f}, {20.0f, 110.0f});
    }
    int grey = 0, white = 0, black = 0;
    for (int y = 10; y < 110; ++y) {
        const BYTE* p = canvas.at(20, y);
        if (p[3] < 250) continue; // the transparent shoulder cap blends into the chest
        int luma = (p[0] + p[1] + p[2]) / 3;
        if (luma > 220) ++white;
        else if (luma < 40) ++black;
        else ++grey;
    }
    std::printf("orca leg: %d white, %d grey, %d black rows\n", white, grey, black);
    CHECK(white > 3 && black > 40, "Orca foreleg should show a white shoulder and a black leg");
    CHECK(grey <= 4, "Orca foreleg white-to-black edge should be crisp, not a long grey ramp");
}

static void legendsOnlyOnGlowingKeys() {
    CatPhysics physics;
    CatSettings withLegends;
    withLegends.coatTheme = CatCoatTheme::ClassicBlack;
    withLegends.showKeyLegends = true;
    withLegends.autoDeskMode = false;
    CatSettings withoutLegends = withLegends;
    withoutLegends.showKeyLegends = false;
    CHECK(renderFrame(physics, withLegends) == renderFrame(physics, withoutLegends),
          "Idle keys should not draw legends");
    physics.keyGlow[PhysicalKey::W] = 1.0f;
    CHECK(renderFrame(physics, withLegends) != renderFrame(physics, withoutLegends),
          "A glowing key should show its legend");
}

static void controlBarLayout() {
    const PetControl expected[] = {PetControl::Treat, PetControl::Sleep, PetControl::Music,
                                   PetControl::Mode, PetControl::Hide};
    int i = 0;
    for (const auto& button : CatRenderer::controlButtons) {
        CHECK(i < 5 && button.control == expected[i], "Unexpected control bar button");
        Pt centre{button.x + button.width * 0.5f, CatRenderer::controlTop + CatRenderer::controlHeight * 0.5f};
        CHECK(CatRenderer::controlAt(centre) == button.control, "Button centre should hit its own button");
        CHECK(button.x >= CatRenderer::controlBarLeft && button.x + button.width <= CatRenderer::controlBarRight,
              "Button should sit inside the bar");
        ++i;
    }
    CHECK(i == 5, "Control bar should have five buttons");
    CatPhysics physics;
    CHECK(CatRenderer::resolveControl(PetControl::Sleep, physics) == PetControl::Sleep, "Awake cat shows sleep");
    physics.goToSleep();
    CHECK(CatRenderer::resolveControl(PetControl::Sleep, physics) == PetControl::Wake, "Sleeping cat shows wake");
    CHECK(CatRenderer::controlEnabled(CatRenderer::resolveControl(PetControl::Sleep, physics), physics),
          "Wake should be enabled while sleeping");
}

static void fontsAreCached() {
    const Gdiplus::Font& a = CatRenderer::cachedFont(L"Segoe UI", 11.0f, Gdiplus::FontStyleBold);
    const Gdiplus::Font& b = CatRenderer::cachedFont(L"Segoe UI", 11.01f, Gdiplus::FontStyleBold);
    const Gdiplus::Font& c = CatRenderer::cachedFont(L"Segoe UI", 9.2f, Gdiplus::FontStyleBold);
    CHECK(&a == &b, "Nearly equal sizes should share one cached font");
    CHECK(&a != &c, "Different sizes should get different fonts");
}

static void everyCoatDraws() {
    for (int coat = 0; coat <= (int)CatCoatTheme::DopeyCat; ++coat) {
        if ((CatCoatTheme)coat == CatCoatTheme::DopeyCat) continue; // needs the embedded art resource
        CatPhysics physics;
        CatSettings settings;
        settings.coatTheme = (CatCoatTheme)coat;
        settings.showControls = true;
        auto pixels = renderFrame(physics, settings);
        size_t painted = 0;
        for (size_t i = 3; i < pixels.size(); i += 4) painted += pixels[i] != 0;
        CHECK(painted > 10000, "Every coat should render a visible cat");
    }
}

int main() {
    Gdiplus::GdiplusStartupInput input;
    ULONG_PTR token = 0;
    if (Gdiplus::GdiplusStartup(&token, &input, nullptr) != Gdiplus::Ok) {
        std::printf("FAIL: GDI+ did not start\n");
        return 1;
    }
    orcaLegHasCrispEdge();
    legendsOnlyOnGlowingKeys();
    controlBarLayout();
    fontsAreCached();
    everyCoatDraws();
    // Cached fonts are intentionally never freed, so shutting GDI+ down here is safe.
    Gdiplus::GdiplusShutdown(token);
    if (failures) {
        std::printf("%d check(s) failed\n", failures);
        return 1;
    }
    std::printf("PASS: Windows renderer checks\n");
    return 0;
}
