#define NOMINMAX
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shellapi.h>
#include <objidl.h>
#include <gdiplus.h>
#include <wtsapi32.h>
#include <string>
#include <vector>
#include <thread>
#include <cmath>
#include <cwchar>
#include <algorithm>
#include "KeyMap.hpp"
#include "InputEngine.hpp"
#include "CatPhysics.hpp"
#include "CatRenderer.hpp"
#include "DeskMode.hpp"
#include "AudioBeat.hpp"
#include "Stats.hpp"
#include <SachiVersion.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "wtsapi32.lib")

namespace {

// The app was called KuroCat before it became SachiCat. These names stay as they are on purpose:
// renaming them would reset every existing user's settings and launch-at-login entry, and would
// stop a new version from finding and replacing an older running copy.
constexpr const wchar_t* VERSION_PROP = L"KuroCatVersion";
constexpr const wchar_t* TRAY_OWNER_CLASS = L"KuroCatTrayOwnerClass";
constexpr const wchar_t* OVERLAY_CLASS = L"KuroCatOverlayWindowClass";
constexpr double FRAME_INTERVAL = 1.0 / 60.0;          // while typing, moving the mouse or interacting
constexpr double IDLE_FRAME_INTERVAL = 1.0 / 30.0;     // awake but nobody is using the computer
constexpr double SLEEPING_FRAME_INTERVAL = 1.0 / 15.0; // asleep: only slow breathing and floating z's
constexpr double ACTIVE_INPUT_WINDOW = 2.0;            // seconds of full frame rate after the last input
constexpr float WHEEL_ZOOM_STEP = 0.06f;               // size change per mouse-wheel notch over the cat
constexpr float MIN_SCALE = 0.5f;                      // matches the range CatSettings loads
constexpr float MAX_SCALE = 2.0f;
constexpr INT_PTR INSTANCE_VERSION = SACHICAT_INSTANCE_VERSION;
constexpr wchar_t ORCA_RELEASE_PAGE[] = L"https://github.com/catopia-meow/SachiCat/releases";
constexpr UINT WM_TRAYICON = WM_APP + 1;
constexpr UINT WM_WAKE_EXISTING = WM_APP + 2;
constexpr UINT WM_REQUEST_QUIT = WM_APP + 5; // posted by a newer / elevated instance that is taking over
constexpr UINT_PTR TIMER_RENDER = 1;
constexpr int HOTKEY_CLICKTHROUGH_ID = 101;
UINT g_msgTaskbarCreated = 0;

enum MenuCmd : UINT {
    CMD_TOGGLE_VISIBLE = 1001,
    CMD_TOGGLE_CLICKTHROUGH,
    CMD_TOGGLE_TOPMOST,
    CMD_TOGGLE_AUTOSTART,
    CMD_TOGGLE_BADGES,
    CMD_TOGGLE_MOUSE_BADGES,
    CMD_TOGGLE_KEY_LEGENDS,
    CMD_TOGGLE_FOOT,
    CMD_TOGGLE_PET,
    CMD_COAT_BLACK,
    CMD_COAT_SIAMESE,
    CMD_COAT_SHORTHAIR,
    CMD_COAT_ORANGE_WHITE,
    CMD_COAT_ORCA_CAT,
    CMD_KB_MILK,
    CMD_KB_HACKER,
    CMD_KB_RETRO,
    CMD_EYE_LIME,
    CMD_EYE_BLUE,
    CMD_EYE_AMBER,
    CMD_EYE_CYAN,
    CMD_EYE_SOFT_AMBER,
    CMD_EYE_REFERENCE_HAZEL,
    CMD_SCALE_65,
    CMD_SCALE_85,
    CMD_SCALE_100,
    CMD_SCALE_125,
    CMD_SCALE_150,
    CMD_DEMO_BURST,
    CMD_RESET_POS,
    CMD_CHECK_UPDATE,
    CMD_RELAUNCH_ADMIN,
    CMD_TOGGLE_RUN_AS_ADMIN,
    CMD_AUTO_DESK, CMD_KEYBOARD_MODE, CMD_TABLET_MODE, CMD_CLEAR_TABLET,
    CMD_QUIT,
    CMD_TOGGLE_SLEEP,
    CMD_TREAT,
    CMD_TOGGLE_CONTROLS,
    CMD_MUSIC,
    CMD_TOGGLE_DOUBLE_CLICK_WAKE,
    CMD_CAPTURE_TRANSPARENT,
    CMD_CAPTURE_GREEN,
    CMD_COAT_DOPEY_CAT,
    CMD_KB_DOPEY,
    CMD_ARCADE_MODE
};

enum class ClickTarget { None, RestingHead, RestingBody, SulkingCat, Bowl, DopeyCat };

struct AppState {
    HWND hwnd = nullptr;
    HWND menuOwnerHwnd = nullptr; // Non-NOACTIVATE helper window so TrackPopupMenu from tray NEVER fails!
    NOTIFYICONDATAW nid = {};

    CatSettings settings;
    std::shared_ptr<desk::Monitor> deskMonitor;
    InputEngine input;
    CatPhysics physics;
    AudioBeat audioBeat;

    bool isVisible = true;
    bool petPressed = false;
    bool isDragging = false;
    ClickTarget pressedTarget = ClickTarget::None;
    PetControl heldControl = PetControl::None;
    PetControl hoveredControl = PetControl::None;
    double controlHoverSince = 0.0;
    POINT dragStartCursor = {};
    POINT lastDragCursor = {};
    POINT dragStartWinOrigin = {};
    ClickTarget previousClick = ClickTarget::None;
    POINT previousClickPoint = {};
    DWORD previousClickTime = 0;
    double observedKeyActivity = 0.0;
    double observedMouseActivity = 0.0;
    bool hasLastHover = false;
    Pt lastHoverCanvas = {};

    // Reusable 32-bit premultiplied DIB backing buffer
    HDC memDC = nullptr;
    HBITMAP dibBmp = nullptr;
    HBITMAP oldBmp = nullptr;
    void* dibBits = nullptr;
    int bufW = 0;
    int bufH = 0;

    double lastTickTime = 0.0;
    bool demoActive = false;
    double demoStartTime = 0.0;
    int demoStepIdx = 0;

    int observedMealHour = 0;
    int pendingMeals = 0;

    // Keystroke and petting counts, saved a few seconds after they change and on exit.
    std::wstring statsDay;
    stats::Snapshot savedStats;
    double nextStatsSave = 0.0;

    // Robustness against games, Win+L, sleep and display changes
    double nextFrameTime = 0.0;
    double lastRenderTime = 0.0;
    double nextHousekeepingTime = 0.0;
    double pendingTopmostTime = 0.0;
    int ulwFailures = 0;
    bool isElevated = false;
    bool foregroundBlocksKeys = false;   // foreground app runs as admin -> Windows (UIPI) hides its keys from us
    double lastBlockedForegroundTime = 0.0;
    bool warnedElevatedForeground = false;
    HWINEVENTHOOK foregroundHook = nullptr;
    bool sessionNotifyRegistered = false;

    void destroyBuffer() {
        if (memDC && oldBmp) SelectObject(memDC, oldBmp);
        if (dibBmp) DeleteObject(dibBmp);
        if (memDC) DeleteDC(memDC);
        memDC = nullptr; dibBmp = nullptr; oldBmp = nullptr; dibBits = nullptr;
        bufW = 0; bufH = 0;
    }

    void ensureBuffer(int w, int h) {
        if (w == bufW && h == bufH && memDC && dibBits) return;
        destroyBuffer();

        HDC screenDC = GetDC(nullptr);
        memDC = CreateCompatibleDC(screenDC);
        ReleaseDC(nullptr, screenDC);

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = w;
        bmi.bmiHeader.biHeight = -h; // top-down 32bpp DIB
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        dibBmp = CreateDIBSection(memDC, &bmi, DIB_RGB_COLORS, &dibBits, nullptr, 0);
        if (dibBmp) {
            oldBmp = (HBITMAP)SelectObject(memDC, dibBmp);
            bufW = w;
            bufH = h;
        }
    }
};

AppState* g_app = nullptr;

// MARK: - Windows Auto-Start (HKCU\Software\Microsoft\Windows\CurrentVersion\Run)

std::wstring getCurrentExeQuotedPath() {
    wchar_t buf[MAX_PATH * 2] = {};
    DWORD len = GetModuleFileNameW(nullptr, buf, (DWORD)(sizeof(buf) / sizeof(buf[0]) - 1));
    if (len == 0) return L"";
    return L"\"" + std::wstring(buf, len) + L"\"";
}

bool isAutoStartEnabled() {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return false;
    }
    DWORD type = 0, size = 0;
    bool exists = (RegQueryValueExW(hKey, L"KuroCat", nullptr, &type, nullptr, &size) == ERROR_SUCCESS && size > 0);
    RegCloseKey(hKey);
    return exists;
}

bool setAutoStartEnabled(bool enable) {
    HKEY hKey = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) != ERROR_SUCCESS) {
        return false;
    }
    bool ok = false;
    if (enable) {
        std::wstring cmd = getCurrentExeQuotedPath();
        if (!cmd.empty()) {
            ok = (RegSetValueExW(hKey, L"KuroCat", 0, REG_SZ, (const BYTE*)cmd.c_str(), (DWORD)((cmd.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS);
        }
    } else {
        LSTATUS st = RegDeleteValueW(hKey, L"KuroCat");
        ok = (st == ERROR_SUCCESS || st == ERROR_FILE_NOT_FOUND);
    }
    RegCloseKey(hKey);
    return ok;
}

void showTrayBalloon(AppState& app, const wchar_t* title, const wchar_t* body) {
    NOTIFYICONDATAW b = app.nid;
    b.uFlags = NIF_INFO;
    b.dwInfoFlags = NIIF_INFO | NIIF_NOSOUND;
    wcsncpy_s(b.szInfoTitle, title, _TRUNCATE);
    wcsncpy_s(b.szInfo, body, _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &b);
}

// MARK: - Elevation (UIPI) helpers
//
// Many games (and launchers such as WeGame / anti-cheat clients) run as administrator. Windows'
// User Interface Privilege Isolation then refuses to deliver that window's keystrokes to normal
// programs — Raw Input, GetAsyncKeyState and low-level hooks all go silent — while mouse movement
// is still visible. That is exactly the "cat only sees the mouse in games" report. The only fix
// is for KuroCat to run at the same level, so we detect it and offer an admin relaunch.

bool isCurrentProcessElevated() {
    HANDLE tok = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) return false;
    TOKEN_ELEVATION te = {};
    DWORD sz = 0;
    bool elevated = GetTokenInformation(tok, TokenElevation, &te, sizeof(te), &sz) && te.TokenIsElevated != 0;
    CloseHandle(tok);
    return elevated;
}

bool isProcessElevatedOrProtected(DWORD pid) {
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return GetLastError() == ERROR_ACCESS_DENIED; // protected / anti-cheat processes
    bool elevated = false;
    HANDLE tok = nullptr;
    if (OpenProcessToken(h, TOKEN_QUERY, &tok)) {
        TOKEN_ELEVATION te = {};
        DWORD sz = 0;
        if (GetTokenInformation(tok, TokenElevation, &te, sizeof(te), &sz)) elevated = te.TokenIsElevated != 0;
        CloseHandle(tok);
    } else {
        elevated = (GetLastError() == ERROR_ACCESS_DENIED);
    }
    CloseHandle(h);
    return elevated;
}

bool commandLineHasFlag(const wchar_t* flag) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return false;
    bool found = false;
    for (int i = 1; i < argc; ++i) {
        if (_wcsicmp(argv[i], flag) == 0) { found = true; break; }
    }
    LocalFree(argv);
    return found;
}

/// Starts a new elevated copy of this exe (UAC prompt). Returns false if the user declined.
bool relaunchSelfElevated(const wchar_t* params) {
    wchar_t exe[MAX_PATH * 2] = {};
    if (GetModuleFileNameW(nullptr, exe, (DWORD)(sizeof(exe) / sizeof(exe[0]) - 1)) == 0) return false;
    SHELLEXECUTEINFOW sei = {};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOASYNC;
    sei.lpVerb = L"runas";
    sei.lpFile = exe;
    sei.lpParameters = params;
    sei.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&sei) != FALSE;
}

/// True when the foreground window covers its whole monitor (borderless / fullscreen games).
bool isForegroundFullscreen() {
    HWND fg = GetForegroundWindow();
    if (!fg || fg == GetDesktopWindow() || fg == GetShellWindow()) return false;
    wchar_t cls[64] = {};
    GetClassNameW(fg, cls, 63);
    if (wcscmp(cls, L"WorkerW") == 0 || wcscmp(cls, L"Progman") == 0) return false;
    RECT wr = {};
    if (!GetWindowRect(fg, &wr)) return false;
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(MonitorFromWindow(fg, MONITOR_DEFAULTTONEAREST), &mi)) return false;
    return wr.left <= mi.rcMonitor.left && wr.top <= mi.rcMonitor.top &&
           wr.right >= mi.rcMonitor.right && wr.bottom >= mi.rcMonitor.bottom;
}

void setTrayPortrait(AppState& app, DWORD action) {
    using namespace Gdiplus;
    constexpr int size = 64;
    Bitmap portrait(size, size, PixelFormat32bppPARGB);
    {
        Graphics g(&portrait);
        g.Clear(Color(0, 0, 0, 0));
        CatRenderer::drawTrayPortrait(g, size, app.settings);
    }

    NOTIFYICONDATAW icon{};
    icon.cbSize = sizeof(icon);
    icon.hWnd = app.menuOwnerHwnd ? app.menuOwnerHwnd : app.hwnd;
    icon.uID = 1;
    icon.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    icon.uCallbackMessage = WM_TRAYICON;
    wcscpy_s(icon.szTip, L"SachiCat 桌宠（点击打开菜单）");
    if (portrait.GetHICON(&icon.hIcon) != Ok) return;
    if (Shell_NotifyIconW(action, &icon)) {
        if (app.nid.hIcon) DestroyIcon(app.nid.hIcon);
        app.nid = icon;
    } else {
        DestroyIcon(icon.hIcon);
    }
}

void computeDefaultOrigin(int w, int h, int& outX, int& outY) {
    RECT work = {};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    outX = work.right - w - 24;
    outY = work.bottom - h - 12;
}

void applyWindowScaleAndPosition(AppState& app, bool anchorBottomCenter, bool persist = true) {
    int newW = (int)std::round(CatPhysics::CanvasWidth * app.settings.scale);
    int newH = (int)std::round(CatRenderer::canvasHeight(app.settings) * app.settings.scale);

    int x = app.settings.posX;
    int y = app.settings.posY;
    if (anchorBottomCenter && app.hwnd) {
        RECT r = {};
        GetWindowRect(app.hwnd, &r);
        int oldMidX = (r.left + r.right) / 2;
        int oldBotY = r.bottom;
        x = oldMidX - newW / 2;
        y = oldBotY - newH;
    } else if (x == -99999 || y == -99999) {
        computeDefaultOrigin(newW, newH, x, y);
    }

    // Clamp onto virtual desktop so saved off-monitor coords never get lost
    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (x + newW < vx + 40 || x > vx + vw - 40 || y + newH < vy + 40 || y > vy + vh - 40) {
        computeDefaultOrigin(newW, newH, x, y);
    }
    if (app.settings.showControls) {
        RECT bounds{x, y, x + newW, y + newH};
        MONITORINFO monitor{sizeof(monitor)};
        if (GetMonitorInfoW(MonitorFromRect(&bounds, MONITOR_DEFAULTTONEAREST), &monitor)) {
            y = std::min(y, (int)monitor.rcWork.bottom - newH);
        }
    }

    HWND insertAfter = app.settings.alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST;
    SetWindowPos(app.hwnd, insertAfter, x, y, newW, newH, SWP_NOACTIVATE);
    if (persist) {
        app.settings.posX = x;
        app.settings.posY = y;
        app.settings.saveToRegistry();
    }
}

/// Puts the cat back on top of other topmost windows (borderless/fullscreen games that make
/// themselves topmost after launching used to bury the cat, which looked like it "vanished").
void assertTopmost(AppState& app) {
    if (!app.hwnd || !app.isVisible || !app.settings.alwaysOnTop) return;
    SetWindowPos(app.hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOSENDCHANGING);
}

/// Full self-repair after Win+L unlock, sleep/resume, display-mode changes (games switching
/// resolution) or a rendering stall: clears stuck keys, re-registers Raw Input, rebuilds the
/// layered-window buffer, restarts the frame timer and re-places the window on a visible monitor
/// WITHOUT overwriting the user's saved position.
void recoverAfterSystemEvent(AppState& app) {
    if (!app.hwnd) return;
    double now = nowSeconds();
    app.input.resetAllState();
    app.input.registerRawInput(app.hwnd);
    app.input.lastMouseMoveTime = now; // wake the cat up so it visibly greets the user again
    app.observedMouseActivity = now;
    app.destroyBuffer();
    app.ulwFailures = 0;
    if (GetCapture() == app.hwnd) ReleaseCapture();
    app.petPressed = app.isDragging = false;
    app.lastTickTime = now;
    app.nextFrameTime = 0.0;
    app.lastRenderTime = now;
    KillTimer(app.hwnd, TIMER_RENDER);
    SetTimer(app.hwnd, TIMER_RENDER, 16, nullptr);
    if (app.isVisible) ShowWindow(app.hwnd, SW_SHOWNOACTIVATE);
    applyWindowScaleAndPosition(app, false, false);
    app.pendingTopmostTime = now + 1.5; // some games re-assert topmost shortly after a mode switch
}

bool hourlyMealDue(const SYSTEMTIME& local, int& observedHour) {
    // The local date keeps midnight and the same clock hour on different days distinct.
    int hour = (((local.wYear * 12 + local.wMonth) * 31 + local.wDay) * 24) + local.wHour;
    if (hour == observedHour) return false;
    bool crossedHour = observedHour != 0;
    observedHour = hour;
    return crossedHour && local.wMinute == 0;
}

/// Once per second: meals, watchdog and game helpers.
void syncStats(AppState& app, bool force) {
    const std::wstring day = stats::today();
    if (day != app.statsDay) {
        // Midnight passed: today's keystrokes start again. Petting is a lifetime count.
        app.statsDay = day;
        app.input.totalKeystrokesToday = 0;
    }
    stats::Snapshot current{day, app.input.totalKeystrokesToday, app.physics.totalPetSessions};
    if (current != app.savedStats && (force || nowSeconds() >= app.nextStatsSave)) {
        stats::save(current);
        app.savedStats = current;
        app.nextStatsSave = nowSeconds() + 5.0;
    }
}

void housekeeping(AppState& app) {
    double now = nowSeconds();
    if (now < app.nextHousekeepingTime) return;
    app.nextHousekeepingTime = now + 1.0;
    syncStats(app, false);
    if (!app.hwnd) return;
    SYSTEMTIME local{};
    GetLocalTime(&local);
    if (hourlyMealDue(local, app.observedMealHour)) ++app.pendingMeals;

    // Rendering stalled for >2s while visible (e.g. the layered window lost its surface): self-heal.
    if (app.isVisible && app.lastRenderTime > 0.0 && now - app.lastRenderTime > 2.0) {
        recoverAfterSystemEvent(app);
        return;
    }
    if (app.isVisible && !IsWindowVisible(app.hwnd)) ShowWindow(app.hwnd, SW_SHOWNOACTIVATE);

    bool fullscreenFg = isForegroundFullscreen();
    if (app.pendingTopmostTime > 0.0 && now >= app.pendingTopmostTime) {
        app.pendingTopmostTime = 0.0;
        assertTopmost(app);
    } else if (fullscreenFg) {
        assertTopmost(app);
    }

    // Detect admin-level games whose keystrokes Windows withholds from us.
    app.foregroundBlocksKeys = false;
    if (!app.isElevated) {
        HWND fg = GetForegroundWindow();
        DWORD pid = 0;
        if (fg) GetWindowThreadProcessId(fg, &pid);
        if (pid && pid != GetCurrentProcessId() && isProcessElevatedOrProtected(pid)) {
            app.foregroundBlocksKeys = true;
            app.lastBlockedForegroundTime = now;
            if (fullscreenFg && !app.warnedElevatedForeground) {
                app.warnedElevatedForeground = true;
                showTrayBalloon(app,
                    L"SachiCat 听不到这个游戏的键盘",
                    L"该游戏以管理员权限运行，Windows 会拦截它的键盘输入。右键小猫/托盘图标 →「以管理员身份重新启动」即可恢复。");
            }
        }
    }
}

void quitApp(AppState& app) {
    // Destroying the overlay runs the normal WM_DESTROY cleanup (tray icon, hotkey...) and quits.
    if (app.hwnd && IsWindow(app.hwnd)) DestroyWindow(app.hwnd);
    else PostQuitMessage(0);
}

void setClickThrough(HWND hwnd, bool transparent) {
    LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    bool isTrans = (ex & WS_EX_TRANSPARENT) != 0;
    if (isTrans != transparent) {
        if (transparent) ex |= WS_EX_TRANSPARENT;
        else ex &= ~WS_EX_TRANSPARENT;
        SetWindowLongPtrW(hwnd, GWL_EXSTYLE, ex);
    }
}

void setGreenScreenCapture(AppState& app, bool enabled) {
    if (enabled == app.settings.greenScreenCapture) return;
    if (enabled) {
        if (!SetLayeredWindowAttributes(app.hwnd, 0, 255, LWA_ALPHA)) {
            showTrayBalloon(app, L"SachiCat 直播采集", L"无法启用绿幕窗口。");
            return;
        }
    } else {
        // UpdateLayeredWindow can resume only after the system resets the layered style.
        const LONG_PTR ex = GetWindowLongPtrW(app.hwnd, GWL_EXSTYLE);
        SetWindowLongPtrW(app.hwnd, GWL_EXSTYLE, ex & ~WS_EX_LAYERED);
        SetWindowLongPtrW(app.hwnd, GWL_EXSTYLE, ex);
    }
    app.settings.greenScreenCapture = enabled;
    app.settings.saveToRegistry();
    app.nextFrameTime = 0.0;
}

Pt canvasPoint(POINT local, const RECT& bounds, const CatSettings& settings) {
    return {local.x * CatPhysics::CanvasWidth / std::max(1L, bounds.right - bounds.left),
            local.y * CatRenderer::canvasHeight(settings) / std::max(1L, bounds.bottom - bounds.top)};
}

PetControl controlAt(Pt point, const CatSettings& settings) {
    return settings.showControls ? CatRenderer::controlAt(point) : PetControl::None;
}

bool mouseIsUnlocked(const AppState& app) {
    return !app.settings.fullClickThrough || (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
}

CatHitZone petZoneAt(Pt point, const AppState& app) {
    if (app.physics.resting() && point.y >= 230.0f) return CatHitZone::None;
    if (app.settings.tabletMode && point.y >= 230.0f && point.y <= 296.0f &&
        point.x >= CatPhysics::keypadLeft && point.x <= 325.0f) return CatHitZone::None;
    return CatRenderer::hitTest(point, app.physics, app.settings.coatTheme, app.settings.arcadeMode);
}

bool pointerOnPet(const AppState& app, Pt point) {
    return app.petPressed || app.heldControl != PetControl::None || controlAt(point, app.settings) != PetControl::None ||
           CatRenderer::bowlAt(point, app.physics, app.settings.coatTheme) || petZoneAt(point, app) != CatHitZone::None;
}

bool isDoubleClick(AppState& app, ClickTarget target, POINT point) {
    DWORD now = static_cast<DWORD>(GetMessageTime());
    bool sameTarget = app.previousClick == target && now - app.previousClickTime <= GetDoubleClickTime() &&
        std::abs(point.x - app.previousClickPoint.x) <= GetSystemMetrics(SM_CXDOUBLECLK) / 2 &&
        std::abs(point.y - app.previousClickPoint.y) <= GetSystemMetrics(SM_CYDOUBLECLK) / 2;
    app.previousClick = sameTarget ? ClickTarget::None : target;
    app.previousClickPoint = point;
    app.previousClickTime = now;
    return sameTarget;
}

void runCommand(AppState& app, UINT cmd);

void showContextMenu(AppState& app, POINT screenPt) {
    app.previousClick = ClickTarget::None;
    HMENU menu = CreatePopupMenu();

    // Status & live stats header
    std::wstring statusStr = app.input.isRawInputRegistered
        ? L"\u25CF \u786C\u4EF6\u7EA7\u5168\u72B6\u6001\u76D1\u542C\u4E2D (Raw Input \u6E38\u620F/\u6253\u5B57)"
        : L"\u26A0 Raw Input \u672A\u542F\u52A8";
    AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, statusStr.c_str());
    if (!app.isElevated && app.lastBlockedForegroundTime > 0.0 && nowSeconds() - app.lastBlockedForegroundTime < 600.0) {
        AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, L"⚠ 刚才的游戏以管理员权限运行，键盘被 Windows 拦截了");
    }

    std::wstring stats = L"\u4ECA\u65E5\u51FB\u952E " + std::to_wstring(app.input.totalKeystrokesToday)
        + L"  \u00B7  KPM " + std::to_wstring(app.input.currentKPM())
        + L"  \u00B7  \u6478\u732B " + std::to_wstring(app.physics.totalPetSessions) + L" \u6B21";
    AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, stats.c_str());
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    AppendMenuW(menu, MF_STRING, CMD_TOGGLE_VISIBLE, app.isVisible ? L"\u9690\u85CF\u5C0F\u732B" : L"\u663E\u793A\u5C0F\u732B");
    UINT actionFlags = app.physics.ignoring() ? MF_GRAYED : 0;
    AppendMenuW(menu, MF_STRING | actionFlags, CMD_TOGGLE_SLEEP, app.physics.resting() ? L"晚上猫熬人" : L"白天不熬猫");
    AppendMenuW(menu, MF_STRING | (app.settings.doubleClickWake ? MF_CHECKED : 0), CMD_TOGGLE_DOUBLE_CLICK_WAKE, L"双击猫头唤醒");
    AppendMenuW(menu, MF_STRING | actionFlags | (app.physics.feeding() ? MF_GRAYED : 0), CMD_TREAT, L"喂零食");
    UINT musicFlags = actionFlags | (app.physics.listening() ? MF_CHECKED : 0) | (app.physics.resting() ? MF_GRAYED : 0);
    AppendMenuW(menu, MF_STRING | musicFlags, CMD_MUSIC, L"听音乐");
    AppendMenuW(menu, MF_STRING, CMD_TOGGLE_CONTROLS, app.settings.showControls ? L"隐藏控件" : L"显示控件");
    HMENU captureMenu = CreatePopupMenu();
    AppendMenuW(captureMenu, MF_STRING | (app.settings.greenScreenCapture ? 0 : MF_CHECKED),
                CMD_CAPTURE_TRANSPARENT, L"透明窗口（WGC）");
    AppendMenuW(captureMenu, MF_STRING | (app.settings.greenScreenCapture ? MF_CHECKED : 0),
                CMD_CAPTURE_GREEN, L"绿幕窗口（色度键）");
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)captureMenu, L"直播采集");
    AppendMenuW(menu, MF_STRING | (app.settings.fullClickThrough ? MF_CHECKED : 0), CMD_TOGGLE_CLICKTHROUGH, L"\u5B8C\u5168\u9F20\u6807\u7A7F\u900F (Ctrl+Alt+Shift+P / \u6309\u4F4FAlt\u53EF\u53F3\u952E)");
    AppendMenuW(menu, MF_STRING | (app.settings.alwaysOnTop ? MF_CHECKED : 0), CMD_TOGGLE_TOPMOST, L"\u7A97\u53E3\u7F6E\u9876\u663E\u793A");
    AppendMenuW(menu, MF_STRING | (isAutoStartEnabled() ? MF_CHECKED : 0), CMD_TOGGLE_AUTOSTART, L"\u5F00\u673A\u81EA\u52A8\u542F\u52A8");
    if (app.isElevated) {
        AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, L"\u2713 \u5DF2\u4EE5\u7BA1\u7406\u5458\u8EAB\u4EFD\u8FD0\u884C\uFF08\u7BA1\u7406\u5458\u6743\u9650\u6E38\u620F\u7684\u952E\u76D8\u4E5F\u80FD\u76D1\u542C\uFF09");
    } else {
        AppendMenuW(menu, MF_STRING, CMD_RELAUNCH_ADMIN, L"\u4EE5\u7BA1\u7406\u5458\u8EAB\u4EFD\u91CD\u65B0\u542F\u52A8\uFF08\u6E38\u620F\u91CC\u952E\u76D8\u6CA1\u53CD\u5E94\u65F6\u70B9\u8FD9\u91CC\uFF09");
    }
    AppendMenuW(menu, MF_STRING | (app.settings.runAsAdmin ? MF_CHECKED : 0), CMD_TOGGLE_RUN_AS_ADMIN, L"\u6BCF\u6B21\u542F\u52A8\u90FD\u4EE5\u7BA1\u7406\u5458\u8EAB\u4EFD\u8FD0\u884C");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    AppendMenuW(menu, MF_STRING | (app.settings.showKeyBadges ? MF_CHECKED : 0), CMD_TOGGLE_BADGES, L"\u663E\u793A\u6309\u952E\u6C14\u6CE1 ([S] [W] [\u21E7])");
    AppendMenuW(menu, MF_STRING | (app.settings.showMouseBadges ? MF_CHECKED : 0), CMD_TOGGLE_MOUSE_BADGES, L"\u663E\u793A\u9F20\u6807\u70B9\u51FB\u6C14\u6CE1 ([\u5DE6\u952E] [\u53F3\u952E])");
    AppendMenuW(menu, MF_STRING | (app.settings.showKeyLegends ? MF_CHECKED : 0), CMD_TOGGLE_KEY_LEGENDS, L"\u663E\u793A\u952E\u76D8\u5B57\u7B26");
    if (app.settings.coatTheme != CatCoatTheme::DopeyCat) {
        AppendMenuW(menu, MF_STRING | (app.settings.footEasterEgg ? MF_CHECKED : 0), CMD_TOGGLE_FOOT, L"高速打字彩蛋（左脚和尾巴）");
        AppendMenuW(menu, MF_STRING | (app.settings.petInteraction ? MF_CHECKED : 0), CMD_TOGGLE_PET, L"\u6478\u5934/\u6478\u80CC\u95ED\u773C\u4E92\u52A8");
    }
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    // Cat Coat Skin submenu
    HMENU coatMenu = CreatePopupMenu();
    AppendMenuW(coatMenu, MF_STRING | (app.settings.coatTheme == CatCoatTheme::ClassicBlack ? MF_CHECKED : 0), CMD_COAT_BLACK, L"\u7ECF\u5178\u5C0F\u9ED1\u732B (\u539F\u7248)");
    AppendMenuW(coatMenu, MF_STRING | (app.settings.coatTheme == CatCoatTheme::Siamese ? MF_CHECKED : 0), CMD_COAT_SIAMESE, L"\u6316\u7164\u66B9\u7F57\u732B");
    AppendMenuW(coatMenu, MF_STRING | (app.settings.coatTheme == CatCoatTheme::Shorthair ? MF_CHECKED : 0), CMD_COAT_SHORTHAIR, L"美短（银虎斑加白）");
    AppendMenuW(coatMenu, MF_STRING | (app.settings.coatTheme == CatCoatTheme::OrangeWhite ? MF_CHECKED : 0), CMD_COAT_ORANGE_WHITE, L"橘白猫");
    AppendMenuW(coatMenu, MF_STRING | (app.settings.coatTheme == CatCoatTheme::OrcaCat ? MF_CHECKED : 0), CMD_COAT_ORCA_CAT, L"虎鲸猫");
    AppendMenuW(coatMenu, MF_STRING | (app.settings.coatTheme == CatCoatTheme::DopeyCat ? MF_CHECKED : 0), CMD_COAT_DOPEY_CAT, L"呆猫");
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)coatMenu, L"\u732B\u54AA\u76AE\u80A4");

    AppendMenuW(menu, MF_STRING | (app.settings.autoDeskMode ? MF_CHECKED : 0),
                CMD_AUTO_DESK, L"自动识别绘画窗口和文字输入");
    AppendMenuW(menu, MF_STRING | (app.settings.deskMode() == DeskMode::Keyboard ? MF_CHECKED : 0),
                CMD_KEYBOARD_MODE, L"键鼠模式");
    AppendMenuW(menu, MF_STRING | (app.settings.deskMode() == DeskMode::Tablet ? MF_CHECKED : 0),
                CMD_TABLET_MODE, L"数位板模式");
    AppendMenuW(menu, MF_STRING | (app.settings.arcadeMode ? MF_CHECKED : 0),
                CMD_ARCADE_MODE, L"街机摇杆模式");
    if (app.settings.tabletMode)
        AppendMenuW(menu, MF_STRING, CMD_CLEAR_TABLET, L"清空画板");

    // Keyboard Theme submenu
    HMENU kbMenu = CreatePopupMenu();
    const auto keyboard = app.settings.activeKeyboardTheme();
    AppendMenuW(kbMenu, MF_STRING | (keyboard == KeyboardTheme::ClassicMilk ? MF_CHECKED : 0), CMD_KB_MILK, L"\u7ECF\u5178\u5976\u767D\u6696\u9EC4 (\u539F\u7248)");
    AppendMenuW(kbMenu, MF_STRING | (keyboard == KeyboardTheme::MidnightHacker ? MF_CHECKED : 0), CMD_KB_HACKER, L"\u6697\u591C\u6781\u5BA2\u9ED1");
    AppendMenuW(kbMenu, MF_STRING | (keyboard == KeyboardTheme::RetroCream ? MF_CHECKED : 0), CMD_KB_RETRO, L"\u590D\u53E4\u5976\u6CB9\u7070");
    AppendMenuW(kbMenu, MF_STRING | (keyboard == KeyboardTheme::DopeyBlue ? MF_CHECKED : 0), CMD_KB_DOPEY, L"呆猫蓝紫");
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)kbMenu, L"\u952E\u76D8\u914D\u8272");

    // Eye Color submenu
    HMENU eyeMenu = CreatePopupMenu();
    AppendMenuW(eyeMenu, MF_STRING | (app.settings.eyeTheme == EyeColorTheme::NeonChartreuse ? MF_CHECKED : 0), CMD_EYE_LIME, L"\u7ECF\u5178\u8367\u5149\u9EC4\u7EFF (\u539F\u7248)");
    AppendMenuW(eyeMenu, MF_STRING | (app.settings.eyeTheme == EyeColorTheme::SiameseBlue ? MF_CHECKED : 0), CMD_EYE_BLUE, L"\u66B9\u7F57\u5B9D\u77F3\u84DD");
    AppendMenuW(eyeMenu, MF_STRING | (app.settings.eyeTheme == EyeColorTheme::WarmAmber ? MF_CHECKED : 0), CMD_EYE_AMBER, L"\u7425\u73C0\u91D1\u77B3");
    AppendMenuW(eyeMenu, MF_STRING | (app.settings.eyeTheme == EyeColorTheme::IceCyan ? MF_CHECKED : 0), CMD_EYE_CYAN, L"\u51B0\u6676\u6D45\u84DD");
    AppendMenuW(eyeMenu, MF_STRING | (app.settings.eyeTheme == EyeColorTheme::SoftAmber ? MF_CHECKED : 0), CMD_EYE_SOFT_AMBER, L"柔和琥珀");
    AppendMenuW(eyeMenu, MF_STRING | (app.settings.eyeTheme == EyeColorTheme::ReferenceHazel ? MF_CHECKED : 0), CMD_EYE_REFERENCE_HAZEL, L"淡金青绿");
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)eyeMenu, L"\u77B3\u8272\u5207\u6362");

    // Scale submenu
    HMENU scaleMenu = CreatePopupMenu();
    auto chk = [&](float v) -> UINT { return std::fabs(app.settings.scale - v) < 0.05f ? MF_CHECKED : 0; };
    AppendMenuW(scaleMenu, MF_STRING | chk(0.65f), CMD_SCALE_65,  L"\u8FF7\u4F60 (0.65x)");
    AppendMenuW(scaleMenu, MF_STRING | chk(0.85f), CMD_SCALE_85,  L"\u504F\u5C0F (0.85x)");
    AppendMenuW(scaleMenu, MF_STRING | chk(1.00f), CMD_SCALE_100, L"\u6807\u51C6 (1.0x)");
    AppendMenuW(scaleMenu, MF_STRING | chk(1.25f), CMD_SCALE_125, L"\u504F\u5927 (1.25x)");
    AppendMenuW(scaleMenu, MF_STRING | chk(1.50f), CMD_SCALE_150, L"\u8D85\u5927 (1.5x)");
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)scaleMenu, L"\u5C3A\u5BF8\u5927\u5C0F");

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | actionFlags, CMD_DEMO_BURST, L"\u6F14\u793A\u6253\u5B57\u52A8\u753B");
    AppendMenuW(menu, MF_STRING, CMD_RESET_POS, L"\u91CD\u7F6E\u4F4D\u7F6E\u5230\u53F3\u4E0B\u89D2");
    std::wstring checkLabel = L"检查更新（前往 GitHub 发布页）· SachiCat v" SACHICAT_VERSION_W;
    AppendMenuW(menu, MF_STRING, CMD_CHECK_UPDATE, checkLabel.c_str());
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, CMD_QUIT, L"\u9000\u51FA SachiCat");

    HWND menuOwner = app.menuOwnerHwnd ? app.menuOwnerHwnd : app.hwnd;
    SetForegroundWindow(menuOwner);
    UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, screenPt.x, screenPt.y, 0, menuOwner, nullptr);
    PostMessageW(menuOwner, WM_NULL, 0, 0);
    DestroyMenu(menu);
    runCommand(app, cmd);
}

void stopListening(AppState& app) {
    app.audioBeat.stop();
    app.physics.listenToMusic(false);
}

bool startListening(AppState& app) {
    if (!app.audioBeat.start()) return false;
    app.physics.listenToMusic(true);
    return true;
}

void resumeFromInput(AppState& app, double activity) {
    if (app.settings.doubleClickWake || !app.physics.canResumeFromInput(activity)) return;
    if (app.physics.listening()) stopListening(app);
    app.physics.wakeUp();
}

bool breakIsMusic(WORD hour, bool preferred) {
    bool daytime = hour >= 6 && hour < 18;
    return daytime != preferred;
}

bool mealReady(int pendingMeals, const InputEngine& input, double now) {
    return pendingMeals > 0 && !input.activelyUsed(now) &&
           now - input.lastActivity() >= CatPhysics::MealIdleSeconds;
}

void startMeal(AppState& app) {
    if (app.physics.feeding() || app.physics.ignoring()) return;
    app.physics.offerTreat(std::max(0, app.pendingMeals - 1));
    app.pendingMeals = 0;
}

void runCommand(AppState& app, UINT cmd) {
    if (app.physics.ignoring() &&
        (cmd == CMD_TOGGLE_SLEEP || cmd == CMD_TREAT || cmd == CMD_MUSIC || cmd == CMD_DEMO_BURST)) return;

    switch (cmd) {
        case CMD_TOGGLE_SLEEP:
        case CMD_TREAT:
        case CMD_TOGGLE_VISIBLE:
        case CMD_TOGGLE_CONTROLS:
        case CMD_TOGGLE_BADGES:
        case CMD_TOGGLE_MOUSE_BADGES:
        case CMD_AUTO_DESK:
        case CMD_KEYBOARD_MODE:
        case CMD_TABLET_MODE:
        case CMD_ARCADE_MODE:
            if (app.physics.listening()) stopListening(app);
            break;
        default:
            break;
    }
    const auto previousCoat = app.settings.coatTheme;
    const auto previousEyes = app.settings.eyeTheme;
    switch (cmd) {
        case CMD_MUSIC:
            if (app.physics.resting()) break;
            if (app.physics.listening()) {
                stopListening(app);
            } else if (startListening(app)) {
                if (!app.isVisible) {
                    app.isVisible = true;
                    ShowWindow(app.hwnd, SW_SHOWNOACTIVATE);
                }
            } else {
                showTrayBalloon(app, L"SachiCat 听音乐", L"无法启动系统声音监听。");
            }
            break;
        case CMD_TOGGLE_SLEEP:
            if (app.physics.resting()) app.physics.wakeUp();
            else app.physics.goToSleep();
            break;
        case CMD_TOGGLE_DOUBLE_CLICK_WAKE:
            app.settings.doubleClickWake = !app.settings.doubleClickWake;
            app.previousClick = ClickTarget::None;
            app.observedKeyActivity = app.input.lastKeyActivityTime;
            app.observedMouseActivity = app.input.lastMouseMoveTime;
            app.settings.saveToRegistry();
            break;
        case CMD_TREAT:
            startMeal(app);
            if (!app.isVisible) {
                app.isVisible = true;
                ShowWindow(app.hwnd, SW_SHOWNOACTIVATE);
            }
            break;
        case CMD_TOGGLE_CONTROLS:
            app.settings.showControls = !app.settings.showControls;
            applyWindowScaleAndPosition(app, false);
            app.settings.saveToRegistry();
            break;
        case CMD_CAPTURE_TRANSPARENT:
        case CMD_CAPTURE_GREEN:
            setGreenScreenCapture(app, cmd == CMD_CAPTURE_GREEN);
            break;
        case CMD_TOGGLE_VISIBLE:
            app.isVisible = !app.isVisible;
            ShowWindow(app.hwnd, app.isVisible ? SW_SHOWNOACTIVATE : SW_HIDE);
            break;
        case CMD_TOGGLE_CLICKTHROUGH:
            app.settings.fullClickThrough = !app.settings.fullClickThrough;
            app.settings.saveToRegistry();
            if (app.settings.fullClickThrough) {
                showTrayBalloon(app,
                    L"SachiCat \u5DF2\u5F00\u542F\u5B8C\u5168\u9F20\u6807\u7A7F\u900F",
                    L"\u53D6\u6D88\u65B9\u5F0F\uFF1A\u6309 Ctrl+Alt+Shift+P\uFF0C\u6216\u6309\u4F4F Alt \u952E\u53F3\u952E\u5C0F\u732B\uFF0C\u6216\u70B9\u53F3\u4E0B\u89D2\u6258\u76D8\u5C0F\u732B\u56FE\u6807\uFF0C\u6216\u518D\u6B21\u53CC\u51FB SachiCat.exe");
            }
            break;
        case CMD_TOGGLE_TOPMOST:
            app.settings.alwaysOnTop = !app.settings.alwaysOnTop;
            applyWindowScaleAndPosition(app, false);
            break;
        case CMD_TOGGLE_AUTOSTART:
            setAutoStartEnabled(!isAutoStartEnabled());
            break;
        case CMD_TOGGLE_BADGES:
            app.settings.showKeyBadges = !app.settings.showKeyBadges;
            app.settings.saveToRegistry();
            break;
        case CMD_TOGGLE_MOUSE_BADGES:
            app.settings.showMouseBadges = !app.settings.showMouseBadges;
            app.settings.saveToRegistry();
            break;
        case CMD_AUTO_DESK:
        case CMD_KEYBOARD_MODE:
        case CMD_TABLET_MODE:
        case CMD_ARCADE_MODE:
            app.settings.autoDeskMode = cmd == CMD_AUTO_DESK;
            app.settings.arcadeMode = cmd == CMD_ARCADE_MODE;
            if (!app.settings.autoDeskMode) app.settings.tabletMode = cmd == CMD_TABLET_MODE;
            if (app.settings.arcadeMode && app.physics.resting()) app.physics.wakeUp();
            app.deskMonitor->result = -1;
            app.deskMonitor->enabled = app.settings.autoDeskMode;
            app.physics.penDown = false;
            app.physics.latestTabletKey = -1;
            app.physics.floatingKeycaps.clear();
            app.settings.saveToRegistry();
            break;
        case CMD_CLEAR_TABLET:
            app.physics.ink.clear();
            app.physics.penDown = false;
            break;
        case CMD_TOGGLE_KEY_LEGENDS:
            app.settings.showKeyLegends = !app.settings.showKeyLegends;
            app.settings.saveToRegistry();
            break;
        case CMD_TOGGLE_FOOT:
            app.settings.footEasterEgg = !app.settings.footEasterEgg;
            app.settings.saveToRegistry();
            break;
        case CMD_TOGGLE_PET:
            app.settings.petInteraction = !app.settings.petInteraction;
            app.settings.saveToRegistry();
            break;
        case CMD_COAT_BLACK:
            app.settings.coatTheme = CatCoatTheme::ClassicBlack;
            app.settings.eyeTheme = EyeColorTheme::NeonChartreuse;
            app.settings.saveToRegistry();
            break;
        case CMD_COAT_SIAMESE:
            app.settings.coatTheme = CatCoatTheme::Siamese;
            app.settings.eyeTheme = EyeColorTheme::SiameseBlue;
            app.settings.saveToRegistry();
            break;
        case CMD_COAT_SHORTHAIR:
        case CMD_COAT_ORANGE_WHITE:
            app.settings.coatTheme = cmd == CMD_COAT_SHORTHAIR ? CatCoatTheme::Shorthair : CatCoatTheme::OrangeWhite;
            app.settings.eyeTheme = EyeColorTheme::SoftAmber;
            app.settings.saveToRegistry();
            break;
        case CMD_COAT_DOPEY_CAT:
            if (app.physics.listening()) stopListening(app);
            app.physics.enterDopeySkin();
            app.pendingMeals = 0;
            app.settings.coatTheme = CatCoatTheme::DopeyCat;
            app.settings.saveToRegistry();
            break;
        case CMD_COAT_ORCA_CAT:
            app.settings.coatTheme = CatCoatTheme::OrcaCat;
            app.settings.eyeTheme = EyeColorTheme::ReferenceHazel;
            app.settings.saveToRegistry();
            break;
        case CMD_EYE_SOFT_AMBER: app.settings.eyeTheme = EyeColorTheme::SoftAmber; app.settings.saveToRegistry(); break;
        case CMD_EYE_REFERENCE_HAZEL: app.settings.eyeTheme = EyeColorTheme::ReferenceHazel; app.settings.saveToRegistry(); break;
        case CMD_KB_MILK:   app.settings.setActiveKeyboardTheme(KeyboardTheme::ClassicMilk); app.settings.saveToRegistry(); break;
        case CMD_KB_HACKER: app.settings.setActiveKeyboardTheme(KeyboardTheme::MidnightHacker); app.settings.saveToRegistry(); break;
        case CMD_KB_RETRO:  app.settings.setActiveKeyboardTheme(KeyboardTheme::RetroCream); app.settings.saveToRegistry(); break;
        case CMD_KB_DOPEY:  app.settings.setActiveKeyboardTheme(KeyboardTheme::DopeyBlue); app.settings.saveToRegistry(); break;
        case CMD_EYE_LIME:  app.settings.eyeTheme = EyeColorTheme::NeonChartreuse; app.settings.saveToRegistry(); break;
        case CMD_EYE_BLUE:  app.settings.eyeTheme = EyeColorTheme::SiameseBlue; app.settings.saveToRegistry(); break;
        case CMD_EYE_AMBER: app.settings.eyeTheme = EyeColorTheme::WarmAmber; app.settings.saveToRegistry(); break;
        case CMD_EYE_CYAN:  app.settings.eyeTheme = EyeColorTheme::IceCyan; app.settings.saveToRegistry(); break;
        case CMD_SCALE_65:  app.settings.scale = 0.65f; applyWindowScaleAndPosition(app, true); break;
        case CMD_SCALE_85:  app.settings.scale = 0.85f; applyWindowScaleAndPosition(app, true); break;
        case CMD_SCALE_100: app.settings.scale = 1.00f; applyWindowScaleAndPosition(app, true); break;
        case CMD_SCALE_125: app.settings.scale = 1.25f; applyWindowScaleAndPosition(app, true); break;
        case CMD_SCALE_150: app.settings.scale = 1.50f; applyWindowScaleAndPosition(app, true); break;
        case CMD_DEMO_BURST:
            app.demoActive = true;
            app.demoStartTime = nowSeconds();
            app.demoStepIdx = 0;
            break;
        case CMD_RESET_POS:
            app.settings.posX = -99999;
            app.settings.posY = -99999;
            applyWindowScaleAndPosition(app, false);
            break;
        case CMD_CHECK_UPDATE:
            if (reinterpret_cast<INT_PTR>(ShellExecuteW(app.hwnd, L"open", ORCA_RELEASE_PAGE, nullptr, nullptr, SW_SHOWNORMAL)) <= 32)
                MessageBoxW(app.hwnd, L"无法打开 GitHub 发布页。", L"SachiCat 检查更新", MB_OK | MB_ICONWARNING);
            break;
        case CMD_RELAUNCH_ADMIN:
            // The elevated copy starts with --takeover and waits for this one to exit.
            if (relaunchSelfElevated(L"--takeover --no-elevate")) quitApp(app);
            break;
        case CMD_TOGGLE_RUN_AS_ADMIN:
            app.settings.runAsAdmin = !app.settings.runAsAdmin;
            app.settings.saveToRegistry();
            if (app.settings.runAsAdmin && !app.isElevated && relaunchSelfElevated(L"--takeover --no-elevate")) {
                quitApp(app);
            }
            break;
        case CMD_QUIT:
            quitApp(app);
            break;
        default:
            break;
    }
    if (app.settings.coatTheme != previousCoat || app.settings.eyeTheme != previousEyes) {
        setTrayPortrait(app, NIM_MODIFY);
    }
}

void runControl(AppState& app, PetControl button) {
    button = CatRenderer::resolveControl(button, app.physics);
    if (!CatRenderer::controlEnabled(button, app.physics)) return;
    switch (button) {
        case PetControl::Treat: runCommand(app, CMD_TREAT); break;
        case PetControl::Sleep:
        case PetControl::Wake: runCommand(app, CMD_TOGGLE_SLEEP); break;
        case PetControl::DoubleClickWake: runCommand(app, CMD_TOGGLE_DOUBLE_CLICK_WAKE); break;
        case PetControl::Music: runCommand(app, CMD_MUSIC); break;
        case PetControl::Mode:
            switch (app.settings.deskMode()) {
                case DeskMode::Keyboard: runCommand(app, CMD_TABLET_MODE); break;
                case DeskMode::Tablet: runCommand(app, CMD_ARCADE_MODE); break;
                case DeskMode::Arcade: runCommand(app, CMD_AUTO_DESK); break;
                case DeskMode::Auto: runCommand(app, CMD_KEYBOARD_MODE); break;
            }
            break;
        case PetControl::KeyBubbles: runCommand(app, CMD_TOGGLE_BADGES); break;
        case PetControl::MouseBubbles: runCommand(app, CMD_TOGGLE_MOUSE_BADGES); break;
        case PetControl::Hide: runCommand(app, CMD_TOGGLE_CONTROLS); break;
        case PetControl::None: break;
    }
}

bool physicsLooksSane(const CatPhysics& p) {
    const float vals[] = {
        p.leftPaw.x.value, p.leftPaw.y.value, p.rightPaw.x.value, p.rightPaw.y.value,
        p.hindFoot.x.value, p.hindFoot.y.value, p.bodyShiftX.value, p.bodyTilt.value,
        p.bodySquash.value, p.headTilt.value, p.headYaw.value, p.headPitch.value,
        p.headOffsetY.value, p.eyeGaze.x.value, p.eyeGaze.y.value, p.pupilScale.value,
        p.huntIntensity.value, p.mouseOffset.x.value, p.mouseOffset.y.value, p.time
    };
    for (float v : vals) {
        if (!std::isfinite(v) || std::fabs(v) > 1.0e6f) return false;
    }
    return true;
}

void tickAndRender(AppState& app) {
    if (!app.isVisible || !app.hwnd) return;

    double now = nowSeconds();
    app.lastRenderTime = now;
    float dt = (float)(now - app.lastTickTime);
    app.lastTickTime = now;
    if (dt <= 0.0f || dt > 0.2f) dt = 1.0f / 60.0f;

    // Optional menu demo typing burst
    if (app.demoActive) {
        static const PhysicalKey demoKeys[] = {
            PhysicalKey::W, PhysicalKey::A, PhysicalKey::S, PhysicalKey::D,
            PhysicalKey::F, PhysicalKey::J, PhysicalKey::O, PhysicalKey::E,
            PhysicalKey::Space, PhysicalKey::LeftShift
        };
        int targetIdx = (int)((now - app.demoStartTime) / 0.13);
        while (app.demoStepIdx <= targetIdx && app.demoStepIdx < 10) {
            app.physics.triggerKeyDown(demoKeys[app.demoStepIdx], app.settings);
            app.demoStepIdx++;
        }
        if (app.demoStepIdx >= 10) app.demoActive = false;
    }

    RECT wr = {};
    GetWindowRect(app.hwnd, &wr);
    int w = std::max(1, (int)(wr.right - wr.left));
    int h = std::max(1, (int)(wr.bottom - wr.top));
    POINT cursor = {};
    GetCursorPos(&cursor);
    Pt cp = canvasPoint({cursor.x - wr.left, cursor.y - wr.top}, wr, app.settings);
    bool unlocked = mouseIsUnlocked(app);
    PetControl hover = unlocked && !app.petPressed ? controlAt(cp, app.settings) : PetControl::None;
    if (hover != app.hoveredControl || app.heldControl != PetControl::None) {
        app.hoveredControl = hover;
        app.controlHoverSince = now;
    }

    bool overControl = hover != PetControl::None || app.heldControl != PetControl::None;
    if (mealReady(app.pendingMeals, app.input, now)) startMeal(app);
    if (!app.settings.arcadeMode && app.physics.breakDue(dt, app.input, now, overControl)) {
        SYSTEMTIME local{};
        GetLocalTime(&local);
        static std::mt19937 random{std::random_device{}()};
        bool preferred = std::bernoulli_distribution(0.7)(random);
        if (!breakIsMusic(local.wHour, preferred) || !startListening(app)) app.physics.goToSleep();
    }

    int detected = app.deskMonitor->result.load();
    if (app.physics.usingDesk() && app.settings.autoDeskMode && detected >= 0 &&
        app.settings.tabletMode != (detected == 1)) {
        app.settings.tabletMode = detected == 1;
        app.physics.penDown = false;
        app.physics.latestTabletKey = -1;
        app.physics.floatingKeycaps.clear();
    }
    if (app.audioBeat.failed()) {
        stopListening(app);
        showTrayBalloon(app, L"SachiCat 听音乐", L"无法读取当前的系统音频输出。");
    }
    app.physics.step(dt, wr, app.input, app.settings, overControl,
                     app.audioBeat.pulse(), app.audioBeat.audible());
    if (app.isDragging && app.settings.coatTheme == CatCoatTheme::DopeyCat) app.physics.coastEgg(dt);
    if (!physicsLooksSane(app.physics)) {
        // A single non-finite value would otherwise make every later frame draw nothing (the cat
        // "vanishes" until restart). Rebuild the pose but keep the session statistics.
        int pets = app.physics.totalPetSessions;
        stopListening(app);
        app.physics = CatPhysics();
        app.physics.totalPetSessions = pets;
    }

    // Smart Click-Through + Petting Detection
    bool bellyHovered = false;
    bool cursorCaptured = app.petPressed || app.heldControl != PetControl::None;
    if (!cursorCaptured) {
        if (PtInRect(&wr, cursor)) {
            CatHitZone zone = petZoneAt(cp, app);
            bool overBowl = CatRenderer::bowlAt(cp, app.physics, app.settings.coatTheme);
            bellyHovered = app.settings.coatTheme != CatCoatTheme::DopeyCat && app.settings.petInteraction && zone == CatHitZone::Belly;
            if (zone == CatHitZone::Desk) app.physics.stopPetting();
            if (zone != CatHitZone::None) {
                float d = app.hasLastHover ? std::hypot(cp.x - app.lastHoverCanvas.x, cp.y - app.lastHoverCanvas.y) : 0.0f;
                bool stroke = d > 1.2f && d < 80.0f;
                if (app.settings.coatTheme != CatCoatTheme::DopeyCat && app.settings.petInteraction && (!app.hasLastHover || stroke)) {
                    if (zone == CatHitZone::LeftEar || zone == CatHitZone::RightEar)
                        app.physics.touchEar(zone == CatHitZone::LeftEar);
                    else if (stroke && (zone == CatHitZone::Head || zone == CatHitZone::Body))
                        app.physics.registerPetStroke(cp, d, zone == CatHitZone::Head, app.settings);
                }
                app.lastHoverCanvas = cp;
                app.hasLastHover = true;
            } else {
                app.hasLastHover = false;
            }
            bool overKey = app.settings.tabletMode && app.physics.usingDesk() && CatPhysics::tabletKeyAt(cp) >= 0;
            bool clickThrough = !unlocked || (zone == CatHitZone::None && !overBowl && !overKey && hover == PetControl::None);
            cursorCaptured = !clickThrough;
            setClickThrough(app.hwnd, clickThrough);
        } else {
            app.hasLastHover = false;
            setClickThrough(app.hwnd, true);
        }
    }
    app.physics.setBellyHovered(bellyHovered);
    if (app.input.lastKeyActivityTime > app.observedKeyActivity) {
        app.observedKeyActivity = app.input.lastKeyActivityTime;
        resumeFromInput(app, app.observedKeyActivity);
    }
    if (app.input.lastMouseMoveTime > app.observedMouseActivity) {
        app.observedMouseActivity = app.input.lastMouseMoveTime;
        if (!cursorCaptured) resumeFromInput(app, app.observedMouseActivity);
    }

    app.ensureBuffer(w, h);
    if (!app.memDC || !app.dibBits) return;

    {
        Gdiplus::Bitmap bmp(w, h, w * 4, PixelFormat32bppPARGB, static_cast<BYTE*>(app.dibBits));
        Gdiplus::Graphics g(&bmp);
        g.Clear(app.settings.greenScreenCapture ? Gdiplus::Color(255, 0, 255, 0) : Gdiplus::Color(0, 0, 0, 0));
        g.ScaleTransform((float)w / CatPhysics::CanvasWidth, (float)h / CatRenderer::canvasHeight(app.settings));
        CatRenderer::draw(g, app.physics, app.settings, app.isDragging);
        if (app.settings.showControls && !(app.isDragging && app.settings.coatTheme == CatCoatTheme::DopeyCat)) {
            CatRenderer::drawControls(g, app.physics, app.settings, hover, app.heldControl, now - app.controlHoverSince > 0.46);
        }
    }
    if (app.settings.greenScreenCapture) {
        // BitBlt capture reads the redirected client area, which UpdateLayeredWindow does not paint.
        RedrawWindow(app.hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOERASE);
        return;
    }

    POINT ptDst = { wr.left, wr.top };
    SIZE szWnd = { w, h };
    POINT ptSrc = { 0, 0 };
    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    HDC screenDC = GetDC(nullptr);
    BOOL presented = UpdateLayeredWindow(app.hwnd, screenDC, &ptDst, &szWnd, app.memDC, &ptSrc, 0, &blend, ULW_ALPHA);
    ReleaseDC(nullptr, screenDC);
    if (presented) {
        app.ulwFailures = 0;
    } else if (++app.ulwFailures > 30) {
        // Surface lost (typically after Win+L / display reconnect): rebuild the DIB next frame.
        app.ulwFailures = 0;
        app.destroyBuffer();
    }
}

/// Renders at most once per FRAME_INTERVAL; called from both the main loop and WM_TIMER (the latter
/// keeps the cat alive while a menu / message box runs its own modal loop).
/// Adaptive frame pacing, like the Linux build: full 60fps only while something is happening.
/// Redrawing the whole cat through GDI+ and UpdateLayeredWindow every 16ms kept a CPU core busy
/// even when nobody touched the computer.
double frameIntervalFor(const AppState& app, double now) {
    if (app.isDragging || app.petPressed || app.heldControl != PetControl::None ||
        app.hoveredControl != PetControl::None || app.demoActive || app.physics.listening()) {
        return FRAME_INTERVAL;
    }
    const double lastInput = std::max(app.input.lastKeyActivityTime, app.input.lastMouseMoveTime);
    if (now - lastInput < ACTIVE_INPUT_WINDOW) return FRAME_INTERVAL;
    if (app.physics.sleepiness > 0.96f) return SLEEPING_FRAME_INTERVAL;
    return IDLE_FRAME_INTERVAL;
}

void renderIfDue(AppState& app) {
    double now = nowSeconds();
    if (now < app.nextFrameTime) return;
    const double interval = frameIntervalFor(app, now);
    app.nextFrameTime = (now - app.nextFrameTime < interval) ? app.nextFrameTime + interval : now + interval;
    tickAndRender(app);
}

LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    AppState* app = g_app;
    switch (msg) {
        case WM_PAINT:
        case WM_PRINTCLIENT:
            if (app && hwnd == app->hwnd && app->settings.greenScreenCapture) {
                PAINTSTRUCT paint{};
                HDC dc = msg == WM_PAINT ? BeginPaint(hwnd, &paint) : reinterpret_cast<HDC>(wParam);
                if (app->memDC) BitBlt(dc, 0, 0, app->bufW, app->bufH, app->memDC, 0, 0, SRCCOPY);
                if (msg == WM_PAINT) EndPaint(hwnd, &paint);
                return 0;
            }
            break;

        case WM_INPUT:
            if (app) app->input.handleRawInput(lParam);
            return DefWindowProcW(hwnd, msg, wParam, lParam);

        case WM_TIMER:
            if (wParam == TIMER_RENDER && app) renderIfDue(*app);
            return 0;

        case WM_WTSSESSION_CHANGE:
            if (app && hwnd == app->hwnd) {
                switch (wParam) {
                    case WTS_SESSION_LOCK:
                    case WTS_CONSOLE_DISCONNECT:
                    case WTS_REMOTE_DISCONNECT:
                        // Win+L: the key-ups for Win and L go to the secure desktop, never to us.
                        app->input.resetAllState();
                        break;
                    case WTS_SESSION_UNLOCK:
                    case WTS_SESSION_LOGON:
                    case WTS_CONSOLE_CONNECT:
                    case WTS_REMOTE_CONNECT:
                        recoverAfterSystemEvent(*app);
                        break;
                    default:
                        break;
                }
            }
            return 0;

        case WM_POWERBROADCAST:
            if (app && hwnd == app->hwnd &&
                (wParam == PBT_APMRESUMEAUTOMATIC || wParam == PBT_APMRESUMESUSPEND)) {
                recoverAfterSystemEvent(*app);
            }
            return TRUE;

        case WM_DISPLAYCHANGE:
            // Fullscreen games often switch resolution; the cat could end up off-screen or with a
            // stale surface. Re-place it (without touching the saved position) and rebuild the buffer.
            if (app && hwnd == app->hwnd) recoverAfterSystemEvent(*app);
            return 0;

        case WM_REQUEST_QUIT:
            if (app) quitApp(*app);
            return 0;

        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCLIENT) {
                SetCursor(LoadCursorW(nullptr, IDC_HAND));
                return TRUE;
            }
            break;

        case WM_LBUTTONDBLCLK:
        case WM_LBUTTONDOWN: {
            bool altHeld = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
            if (app && (!app->settings.fullClickThrough || altHeld)) {
                RECT bounds{};
                GetClientRect(hwnd, &bounds);
                POINT local{short(LOWORD(lParam)), short(HIWORD(lParam))};
                Pt point = canvasPoint(local, bounds, app->settings);
                PetControl button = controlAt(point, app->settings);
                if (button != PetControl::None) {
                    app->previousClick = ClickTarget::None;
                    app->heldControl = button;
                    SetCapture(hwnd);
                    return 0;
                }
                const bool overBowl = CatRenderer::bowlAt(point, app->physics, app->settings.coatTheme);
                if (!overBowl && app->settings.tabletMode && app->physics.usingDesk() && !altHeld) {
                    int slot = CatPhysics::tabletKeyAt(point);
                    if (slot >= 0) {
                        app->previousClick = ClickTarget::None;
                        app->physics.triggerKeyDown(app->physics.tabletKeys[slot].key, app->settings);
                        return 0;
                    }
                }
                CatHitZone zone = CatRenderer::hitTest(point, app->physics, app->settings.coatTheme, app->settings.arcadeMode);
                if (zone == CatHitZone::None && !overBowl) return 0;
                app->pressedTarget = ClickTarget::None;
                if (!altHeld && overBowl) {
                    app->pressedTarget = ClickTarget::Bowl;
                } else if (!altHeld && app->settings.coatTheme == CatCoatTheme::DopeyCat && app->physics.usingDesk() && zone != CatHitZone::Desk) {
                    app->pressedTarget = ClickTarget::DopeyCat;
                } else if (!altHeld && app->physics.facingAway() && zone != CatHitZone::Desk) {
                    app->pressedTarget = ClickTarget::SulkingCat;
                } else if (!altHeld && app->physics.atRest()) {
                    if (zone == CatHitZone::Head || zone == CatHitZone::LeftEar || zone == CatHitZone::RightEar) {
                        app->pressedTarget = ClickTarget::RestingHead;
                    } else if (zone == CatHitZone::Body || zone == CatHitZone::Belly) {
                        app->pressedTarget = ClickTarget::RestingBody;
                    }
                }
                app->petPressed = true;
                app->dragStartCursor = local;
                ClientToScreen(hwnd, &app->dragStartCursor);
                app->lastDragCursor = app->dragStartCursor;
                app->physics.eggRoll = 0;
                app->physics.remainingRoll = 0;
                RECT wr = {};
                GetWindowRect(hwnd, &wr);
                app->dragStartWinOrigin = { wr.left, wr.top };
                SetCapture(hwnd);
            }
            return 0;
        }

        case WM_MOUSEWHEEL:
            // Scroll over the cat to resize it (same 0.5x to 2x range as the size menu).
            if (app && hwnd == app->hwnd && mouseIsUnlocked(*app)) {
                const float notches = (float)GET_WHEEL_DELTA_WPARAM(wParam) / (float)WHEEL_DELTA;
                if (notches != 0.0f) {
                    const float factor = 1.0f + std::clamp(notches * WHEEL_ZOOM_STEP, -0.25f, 0.25f);
                    app->settings.scale = std::clamp(app->settings.scale * factor, MIN_SCALE, MAX_SCALE);
                    applyWindowScaleAndPosition(*app, true);
                }
                return 0;
            }
            break;

        case WM_MOUSEMOVE:
            if (app && app->petPressed) {
                POINT cur = {};
                GetCursorPos(&cur);
                int dx = cur.x - app->dragStartCursor.x;
                int dy = cur.y - app->dragStartCursor.y;
                if (!app->isDragging && std::abs(dx) < GetSystemMetrics(SM_CXDRAG) &&
                    std::abs(dy) < GetSystemMetrics(SM_CYDRAG)) return 0;
                app->isDragging = true;
                if (app->settings.coatTheme == CatCoatTheme::DopeyCat) {
                    const float scale = app->settings.scale;
                    app->physics.rollEgg((cur.x - app->lastDragCursor.x) / scale,
                                         (cur.y - app->lastDragCursor.y) / scale);
                    app->lastDragCursor = cur;
                }
                int nx = app->dragStartWinOrigin.x + dx;
                int ny = app->dragStartWinOrigin.y + dy;
                SetWindowPos(hwnd, nullptr, nx, ny, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            }
            return 0;

        case WM_LBUTTONUP:
            if (app && app->heldControl != PetControl::None) {
                PetControl button = app->heldControl;
                app->heldControl = PetControl::None;
                ReleaseCapture();
                RECT bounds{};
                GetClientRect(hwnd, &bounds);
                Pt point = canvasPoint({short(LOWORD(lParam)), short(HIWORD(lParam))}, bounds, app->settings);
                if (mouseIsUnlocked(*app) && controlAt(point, app->settings) == button) runControl(*app, button);
                return 0;
            }
            if (app && app->petPressed) {
                bool moved = app->isDragging;
                ClickTarget target = app->pressedTarget;
                app->petPressed = app->isDragging = false;
                ReleaseCapture();
                POINT local{short(LOWORD(lParam)), short(HIWORD(lParam))};
                if (moved) {
                    app->previousClick = ClickTarget::None;
                    RECT wr = {};
                    GetWindowRect(hwnd, &wr);
                    app->settings.posX = wr.left;
                    app->settings.posY = wr.top;
                    app->settings.saveToRegistry();
                } else if (target == ClickTarget::Bowl && app->physics.bowlVisible() && !app->physics.ignoring()) {
                    const bool take = isDoubleClick(*app, target, local);
                    if (app->settings.coatTheme == CatCoatTheme::DopeyCat) {
                        app->physics.disturbDango(take);
                    } else if (take) {
                        if (app->physics.listening()) stopListening(*app);
                        app->physics.takeBowl();
                        app->demoActive = false;
                    } else {
                        app->physics.disturbBowl();
                    }
                } else if (target == ClickTarget::DopeyCat && app->settings.coatTheme == CatCoatTheme::DopeyCat) {
                    app->previousClick = ClickTarget::None;
                    if (app->physics.listening()) stopListening(*app);
                    app->physics.clickDopeyCat();
                } else if (target == ClickTarget::SulkingCat && app->physics.facingAway()) {
                    app->previousClick = ClickTarget::None;
                    app->physics.pokeBack();
                } else if ((target == ClickTarget::RestingHead || target == ClickTarget::RestingBody) &&
                           app->physics.atRest()) {
                    if (target == ClickTarget::RestingHead && app->settings.doubleClickWake &&
                        isDoubleClick(*app, target, local)) {
                        if (app->physics.listening()) stopListening(*app);
                        app->physics.wakeUp();
                    } else {
                        if (target == ClickTarget::RestingBody) app->previousClick = ClickTarget::None;
                        app->physics.bounceAtRest();
                    }
                } else {
                    app->previousClick = ClickTarget::None;
                }
            }
            return 0;

        case WM_CAPTURECHANGED:
            if (app) {
                if (app->petPressed) app->previousClick = ClickTarget::None;
                app->petPressed = app->isDragging = false;
                app->heldControl = PetControl::None;
            }
            return 0;

        case WM_RBUTTONUP:
            if (app) {
                app->previousClick = ClickTarget::None;
                POINT pt = {};
                GetCursorPos(&pt);
                showContextMenu(*app, pt);
            }
            return 0;

        case WM_TRAYICON:
            if (app && (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONUP || lParam == WM_CONTEXTMENU)) {
                POINT pt = {};
                GetCursorPos(&pt);
                showContextMenu(*app, pt);
            }
            return 0;

        case WM_HOTKEY:
            if (app && wParam == HOTKEY_CLICKTHROUGH_ID) {
                app->settings.fullClickThrough = !app->settings.fullClickThrough;
                app->settings.saveToRegistry();
                if (app->settings.fullClickThrough) {
                    showTrayBalloon(*app,
                        L"SachiCat \u5DF2\u5F00\u542F\u5B8C\u5168\u9F20\u6807\u7A7F\u900F",
                        L"\u518D\u6B21\u6309 Ctrl+Alt+Shift+P \u6216\u6309\u4F4F Alt \u53F3\u952E\u5C0F\u732B\u5373\u53EF\u53D6\u6D88");
                } else {
                    showTrayBalloon(*app,
                        L"SachiCat \u5DF2\u53D6\u6D88\u5B8C\u5168\u9F20\u6807\u7A7F\u900F",
                        L"\u73B0\u5728\u53EF\u4EE5\u6B63\u5E38\u62D6\u52A8\u3001\u6478\u5934\u548C\u53F3\u952E\u6253\u5F00\u83DC\u5355\u4E86");
                }
            }
            return 0;

        case WM_WAKE_EXISTING:
            if (app) {
                // Re-launching SachiCat.exe unlocks click-through, restores visibility & opens settings menu!
                if (app->settings.fullClickThrough) {
                    app->settings.fullClickThrough = false;
                    app->settings.saveToRegistry();
                }
                if (!app->isVisible) {
                    app->isVisible = true;
                    ShowWindow(app->hwnd, SW_SHOWNOACTIVATE);
                }
                POINT pt = {};
                GetCursorPos(&pt);
                showContextMenu(*app, pt);
            }
            return 0;

        case WM_ENDSESSION:
            // Windows is signing out or shutting down and may end the process without returning
            // to the message loop, so save the counts now.
            if (wParam && app && hwnd == app->hwnd) syncStats(*app, true);
            return 0;

        case WM_DESTROY:
            if (hwnd == (app ? app->hwnd : nullptr)) {
                KillTimer(hwnd, TIMER_RENDER);
                if (app) {
                    if (app->sessionNotifyRegistered) {
                        WTSUnRegisterSessionNotification(hwnd);
                        app->sessionNotifyRegistered = false;
                    }
                    if (app->foregroundHook) {
                        UnhookWinEvent(app->foregroundHook);
                        app->foregroundHook = nullptr;
                    }
                    Shell_NotifyIconW(NIM_DELETE, &app->nid);
                    if (app->nid.hIcon) DestroyIcon(app->nid.hIcon);
                    if (app->menuOwnerHwnd) {
                        UnregisterHotKey(app->menuOwnerHwnd, HOTKEY_CLICKTHROUGH_ID);
                        RemovePropW(app->menuOwnerHwnd, VERSION_PROP);
                        DestroyWindow(app->menuOwnerHwnd);
                        app->menuOwnerHwnd = nullptr;
                    }
                    app->destroyBuffer();
                }
                PostQuitMessage(0);
            }
            return 0;
    }
    if (g_msgTaskbarCreated != 0 && msg == g_msgTaskbarCreated && app && hwnd == app->menuOwnerHwnd) {
        setTrayPortrait(*app, NIM_ADD);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void CALLBACK onForegroundChanged(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD) {
    // A game just came to the front: many make themselves topmost right after showing, so
    // re-assert our own topmost state shortly afterwards instead of staying buried ("vanished").
    if (g_app) g_app->pendingTopmostTime = nowSeconds() + 0.4;
}

/// Asks the already-running KuroCat to exit and waits for it. Builds <= 1.3.2 do not know
/// WM_REQUEST_QUIT, but they shut down cleanly when their overlay window receives WM_CLOSE.
bool shutDownRunningInstance(HWND owner, DWORD timeoutMs) {
    DWORD pid = 0;
    if (owner) GetWindowThreadProcessId(owner, &pid);
    HANDLE hProc = pid ? OpenProcess(SYNCHRONIZE, FALSE, pid) : nullptr;
    if (owner) PostMessageW(owner, WM_REQUEST_QUIT, 0, 0);
    if (HWND overlay = FindWindowW(OVERLAY_CLASS, nullptr)) PostMessageW(overlay, WM_CLOSE, 0, 0);
    bool gone = false;
    if (hProc) {
        gone = (WaitForSingleObject(hProc, timeoutMs) == WAIT_OBJECT_0);
        CloseHandle(hProc);
    } else {
        for (DWORD waited = 0; waited <= timeoutMs; waited += 100) {
            if (!FindWindowW(TRAY_OWNER_CLASS, nullptr) && !FindWindowW(OVERLAY_CLASS, nullptr)) { gone = true; break; }
            Sleep(100);
        }
    }
    return gone;
}

} // namespace

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int) {
    // Enable crisp Per-Monitor V2 DPI awareness on Windows 10/11
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    const bool forceTakeover = commandLineHasFlag(L"--takeover");   // admin relaunch
    const bool noElevate = commandLineHasFlag(L"--no-elevate");
    const wchar_t* kMutexName = L"Local\\KuroCatDesktopPetSingleInstance";

    // Single-instance guard. Re-launching the SAME version wakes the existing cat (cancels full
    // click-through and pops the menu so users never get stuck). Launching a NEWER version (e.g. a
    // freshly downloaded exe while the old one still sits in the tray or autostarts from another
    // folder) now replaces the old process instead of silently waking it — previously the old
    // version kept running and kept reporting "new version available".
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, kMutexName);
    DWORD mutexErr = GetLastError();
    if (!hMutex && mutexErr != ERROR_ACCESS_DENIED) return 1;
    bool alreadyRunning = (hMutex && mutexErr == ERROR_ALREADY_EXISTS) || (!hMutex && mutexErr == ERROR_ACCESS_DENIED);
    if (alreadyRunning) {
        HWND owner = FindWindowW(TRAY_OWNER_CLASS, nullptr);
        if (!owner && !forceTakeover) {
            if (hMutex) CloseHandle(hMutex);
            return 0;
        }
        if (owner) {
            INT_PTR runningVersion = reinterpret_cast<INT_PTR>(GetPropW(owner, VERSION_PROP));
            bool replace = forceTakeover || runningVersion < INSTANCE_VERSION;
            if (!replace || !shutDownRunningInstance(owner, 6000)) {
                PostMessageW(owner, WM_WAKE_EXISTING, 0, 0);
                if (hMutex) CloseHandle(hMutex);
                return 0;
            }
        }
        if (!hMutex) hMutex = CreateMutexW(nullptr, FALSE, kMutexName);
        if (!hMutex) return 1;
        // A handoff may overlap another launch. Only the mutex owner can create the overlay.
        DWORD acquired = WaitForSingleObject(hMutex, forceTakeover && !owner ? 6000 : 0);
        if (acquired != WAIT_OBJECT_0 && acquired != WAIT_ABANDONED) {
            CloseHandle(hMutex);
            return 0;
        }
    }

    AppState app;
    g_app = &app;
    app.settings.loadFromRegistry();
    app.savedStats = stats::load();
    app.statsDay = app.savedStats.day;
    app.input.totalKeystrokesToday = app.savedStats.todayKeys;
    app.physics.totalPetSessions = app.savedStats.petCount;
    app.deskMonitor = desk::start();
    app.deskMonitor->enabled = app.settings.autoDeskMode;
    app.isElevated = isCurrentProcessElevated();

    // "Always run as administrator": hand over to an elevated copy (one UAC prompt). If the user
    // declines, simply carry on non-elevated.
    if (app.settings.runAsAdmin && !app.isElevated && !noElevate) {
        if (relaunchSelfElevated(L"--takeover --no-elevate")) {
            g_app = nullptr;
            ReleaseMutex(hMutex);
            CloseHandle(hMutex);
            return 0;
        }
    }

    g_msgTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

    Gdiplus::GdiplusStartupInput gdipInput;
    ULONG_PTR gdipToken = 0;
    Gdiplus::GdiplusStartup(&gdipToken, &gdipInput, nullptr);

    // Refresh auto-start exe path if user enabled auto-start and moved SachiCat.exe
    if (isAutoStartEnabled()) {
        setAutoStartEnabled(true);
    }

    app.input.onKeyDown = [&](PhysicalKey k) {
        app.observedKeyActivity = app.input.lastKeyActivityTime;
        resumeFromInput(app, app.observedKeyActivity);
        app.physics.triggerKeyDown(k, app.settings);
    };
    app.input.onMouseClick = [&](int btn, bool down) {
        app.observedMouseActivity = app.input.lastMouseMoveTime;
        bool wakeOnClick = down && !app.settings.doubleClickWake && app.physics.atRest();
        if (app.isVisible && (app.settings.showControls || wakeOnClick) && mouseIsUnlocked(app)) {
            RECT bounds{};
            POINT cursor{};
            GetWindowRect(app.hwnd, &bounds);
            GetCursorPos(&cursor);
            Pt point = canvasPoint({cursor.x - bounds.left, cursor.y - bounds.top}, bounds, app.settings);
            if (app.heldControl != PetControl::None || controlAt(point, app.settings) != PetControl::None) {
                if (!down && !app.settings.tabletMode) app.physics.triggerMouseClick(btn, false, app.settings);
                return;
            }
            if (wakeOnClick && PtInRect(&bounds, cursor) && pointerOnPet(app, point)) wakeOnClick = false;
        }
        if (wakeOnClick) resumeFromInput(app, app.input.lastMouseClickTime);
        app.physics.triggerMouseClick(btn, down, app.settings);
    };

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = wndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = OVERLAY_CLASS;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);

    // Dedicated hidden tray/menu owner window (NO WS_EX_NOACTIVATE / WS_EX_TRANSPARENT)
    WNDCLASSEXW trayWc = {};
    trayWc.cbSize = sizeof(trayWc);
    trayWc.lpfnWndProc = wndProc;
    trayWc.hInstance = hInst;
    trayWc.lpszClassName = TRAY_OWNER_CLASS;
    RegisterClassExW(&trayWc);

    app.menuOwnerHwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        trayWc.lpszClassName,
        L"KuroCatTray",
        WS_POPUP,
        0, 0, 0, 0,
        nullptr, nullptr, hInst, nullptr
    );
    if (app.menuOwnerHwnd) {
        SetPropW(app.menuOwnerHwnd, VERSION_PROP, reinterpret_cast<HANDLE>(INSTANCE_VERSION));
        // When elevated, UIPI would drop messages from the (non-elevated) taskbar and from a
        // re-launched KuroCat. Explicitly allow the few messages we rely on.
        const UINT allowed[] = { WM_TRAYICON, WM_WAKE_EXISTING, WM_REQUEST_QUIT };
        for (UINT m : allowed) ChangeWindowMessageFilterEx(app.menuOwnerHwnd, m, MSGFLT_ALLOW, nullptr);
        if (g_msgTaskbarCreated) ChangeWindowMessageFilterEx(app.menuOwnerHwnd, g_msgTaskbarCreated, MSGFLT_ALLOW, nullptr);
    }

    int initW = (int)std::round(CatPhysics::CanvasWidth * app.settings.scale);
    int initH = (int)std::round(CatRenderer::canvasHeight(app.settings) * app.settings.scale);
    int initX = 0, initY = 0;
    if (app.settings.posX != -99999 && app.settings.posY != -99999) {
        initX = app.settings.posX; initY = app.settings.posY;
    } else {
        computeDefaultOrigin(initW, initH, initX, initY);
    }

    // OBS excludes tool windows even when they are visible.
    DWORD exStyle = WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT
                  | (app.settings.alwaysOnTop ? WS_EX_TOPMOST : 0);

    app.hwnd = CreateWindowExW(
        exStyle,
        wc.lpszClassName,
        L"SachiCat",
        WS_POPUP,
        initX, initY, initW, initH,
        nullptr, nullptr, hInst, nullptr
    );
    if (!app.hwnd) {
        if (app.menuOwnerHwnd) DestroyWindow(app.menuOwnerHwnd);
        Gdiplus::GdiplusShutdown(gdipToken);
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
        return 1;
    }

    if (app.settings.greenScreenCapture && !SetLayeredWindowAttributes(app.hwnd, 0, 255, LWA_ALPHA)) {
        app.settings.greenScreenCapture = false;
    }
    applyWindowScaleAndPosition(app, false);
    app.input.registerRawInput(app.hwnd);
    setTrayPortrait(app, NIM_ADD);

    // Win+L / fast-user-switch notifications (see recoverAfterSystemEvent)
    app.sessionNotifyRegistered = WTSRegisterSessionNotification(app.hwnd, NOTIFY_FOR_THIS_SESSION) != FALSE;
    // Foreground changes (games launching) -> re-assert topmost
    app.foregroundHook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr,
                                         onForegroundChanged, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

    // Global shortcut Ctrl+Alt+Shift+P to toggle full click-through anytime!
    if (app.menuOwnerHwnd) {
        RegisterHotKey(app.menuOwnerHwnd, HOTKEY_CLICKTHROUGH_ID, MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT, 'P');
    }

    ShowWindow(app.hwnd, SW_SHOWNOACTIVATE);
    app.lastTickTime = nowSeconds();
    tickAndRender(app);
    app.nextFrameTime = nowSeconds() + FRAME_INTERVAL;
    SetTimer(app.hwnd, TIMER_RENDER, 16, nullptr);

    // Frame loop. WM_TIMER is only generated when the queue is empty, so a 1000-8000 Hz gaming mouse
    // (one WM_INPUT per report) could starve it and freeze the cat. Here frames are driven by time:
    // drain messages for at most ~10 ms, then render if a frame is due. WM_TIMER still calls
    // renderIfDue() so the cat keeps animating inside menus / message boxes (their own modal loops).
    MSG msg = {};
    int exitCode = 0;
    bool running = true;
    while (running) {
        double now = nowSeconds();
        double waitSec = app.isVisible ? (app.nextFrameTime - now) : 0.25;
        if (waitSec > 0.0) {
            MsgWaitForMultipleObjectsEx(0, nullptr, (DWORD)(waitSec * 1000.0), QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        }
        double drainUntil = nowSeconds() + 0.010;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running = false;
                exitCode = (int)msg.wParam;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (nowSeconds() > drainUntil) break;
        }
        if (!running) break;
        renderIfDue(app);
        housekeeping(app);
    }

    syncStats(app, true);
    app.deskMonitor->shutdown();
    DopeyArt::release();
    g_app = nullptr;
    Gdiplus::GdiplusShutdown(gdipToken);
    ReleaseMutex(hMutex);
    CloseHandle(hMutex);
    return exitCode;
}
