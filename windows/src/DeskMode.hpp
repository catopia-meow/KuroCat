#pragma once
#include <windows.h>
#include <uiautomation.h>
#include <atomic>
#include <memory>
#include <thread>
#include <chrono>
#include <string>
#include <cwctype>
#include <algorithm>

// Read control types and caret state only. No text, window titles or keystrokes are needed.
namespace desk {
inline bool artProcess(DWORD pid) {
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return false;
    wchar_t path[32768];
    DWORD length = ARRAYSIZE(path);
    bool ok = QueryFullProcessImageNameW(process, 0, path, &length) != FALSE;
    CloseHandle(process);
    if (!ok) return false;
    std::wstring exe(path, length);
    exe.erase(0, exe.find_last_of(L"\\/") + 1);
    std::transform(exe.begin(), exe.end(), exe.begin(), [](wchar_t c){ return std::towlower(c); });
    for (auto name : {L"zbrush.exe", L"zbrushcore.exe", L"zbrushcoremini.exe", L"blender.exe",
                      L"photoshop.exe", L"krita.exe", L"clipstudiopaint.exe", L"sai.exe", L"sai2.exe",
                      L"3dcoat.exe", L"3dcoatgl64.exe", L"3dcoatdx64.exe", L"mudbox.exe",
                      L"adobe substance 3d painter.exe", L"substance painter.exe", L"sculptris.exe",
                      L"affinityphoto2.exe", L"photo.exe", L"paintdotnet.exe", L"mspaint.exe"})
        if (exe == name) return true;
    return exe.rfind(L"gimp-",0) == 0 || exe == L"gimp.exe";
}

inline bool textFocus(DWORD tid, DWORD pid, IUIAutomation* automation) {
    GUITHREADINFO info{sizeof(info)};
    if (GetGUIThreadInfo(tid,&info)) {
        if (info.hwndCaret) return true;
        wchar_t type[128]{};
        GetClassNameW(info.hwndFocus,type,ARRAYSIZE(type));
        if (!_wcsicmp(type,L"Edit") || !_wcsnicmp(type,L"RichEdit",8)) return true;
    }
    IUIAutomationElement* element = nullptr;
    if (!automation || FAILED(automation->GetFocusedElement(&element)) || !element) return false;
    CONTROLTYPEID type = 0;
    int owner = 0;
    element->get_CurrentProcessId(&owner);
    element->get_CurrentControlType(&type);
    bool edit = static_cast<DWORD>(owner) == pid && type == UIA_EditControlTypeId;
    if (static_cast<DWORD>(owner) == pid && type == UIA_DocumentControlTypeId) {
        IUIAutomationTextPattern2* pattern = nullptr;
        if (SUCCEEDED(element->GetCurrentPatternAs(UIA_TextPattern2Id,IID_PPV_ARGS(&pattern))) && pattern) {
            BOOL active = FALSE;
            IUIAutomationTextRange* range = nullptr;
            if (SUCCEEDED(pattern->GetCaretRange(&active,&range))) edit = active != FALSE;
            if (range) range->Release();
            pattern->Release();
        }
    }
    element->Release();
    return edit;
}

// Keep UI Automation calls off the animation thread: some graphics apps take time to answer.
struct Monitor {
    std::atomic<bool> enabled{true}, stop{false};
    std::atomic<int> result{-1}; // -1 pending, 0 keyboard, 1 tablet
};
inline std::shared_ptr<Monitor> start() {
    auto state = std::make_shared<Monitor>();
    std::thread([state] {
        HRESULT init = CoInitializeEx(nullptr,COINIT_MULTITHREADED);
        IUIAutomation* automation = nullptr;
        if (SUCCEEDED(init)) CoCreateInstance(CLSID_CUIAutomation,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&automation));
        DWORD cachedPid = 0;
        bool art = false;
        while (!state->stop.load()) {
            HWND window = GetForegroundWindow();
            DWORD pid = 0;
            DWORD tid = GetWindowThreadProcessId(window,&pid);
            if (!state->enabled.load()) state->result = -1;
            else if (tid && pid != GetCurrentProcessId()) {
                if (pid != cachedPid) { art = artProcess(pid); cachedPid = pid; }
                int mode = art && !textFocus(tid,pid,automation) ? 1 : 0;
                if (state->enabled && window == GetForegroundWindow()) state->result = mode;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(220));
        }
        if (automation) automation->Release();
        if (SUCCEEDED(init)) CoUninitialize();
    }).detach();
    return state;
}
}
