#pragma once
#include <windows.h>
#include <uiautomation.h>
#include <atomic>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
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
    std::wstring fullPath(path, length);
    std::transform(fullPath.begin(), fullPath.end(), fullPath.begin(), [](wchar_t c){ return std::towlower(c); });
    std::wstring exe = fullPath.substr(fullPath.find_last_of(L"\\/") + 1);
    // Affinity Photo 1 ships as a generic "Photo.exe"; only trust that name inside an Affinity folder.
    if (exe == L"photo.exe") return fullPath.find(L"affinity") != std::wstring::npos;
    for (auto name : {L"zbrush.exe", L"zbrushcore.exe", L"zbrushcoremini.exe", L"blender.exe",
                      L"photoshop.exe", L"krita.exe", L"clipstudiopaint.exe", L"sai.exe", L"sai2.exe",
                      L"3dcoat.exe", L"3dcoatgl64.exe", L"3dcoatdx64.exe", L"mudbox.exe",
                      L"adobe substance 3d painter.exe", L"substance painter.exe", L"sculptris.exe",
                      L"affinityphoto2.exe", L"paintdotnet.exe", L"mspaint.exe"})
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
// The worker is joined on shutdown (instead of being detached) so it never runs a COM call while
// the process is tearing down. UI Automation calls are capped with a timeout so that join stays short.
struct Monitor {
    std::atomic<bool> enabled{true}, stop{false};
    std::atomic<int> result{-1}; // -1 pending, 0 keyboard, 1 tablet
    std::mutex mutex;
    std::condition_variable wake;
    std::thread worker;

    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex);
            stop = true;
        }
        wake.notify_all();
        if (worker.joinable() && worker.get_id() != std::this_thread::get_id()) worker.join();
    }
    ~Monitor() { shutdown(); }
};

inline void limitAutomationWait(IUIAutomation* automation) {
    IUIAutomation2* bounded = nullptr;
    if (automation && SUCCEEDED(automation->QueryInterface(IID_PPV_ARGS(&bounded))) && bounded) {
        bounded->put_ConnectionTimeout(500);
        bounded->put_TransactionTimeout(500);
        bounded->Release();
    }
}

inline std::shared_ptr<Monitor> start() {
    auto state = std::make_shared<Monitor>();
    Monitor* monitor = state.get(); // outlives the worker: shutdown() joins before destruction
    monitor->worker = std::thread([monitor] {
        HRESULT init = CoInitializeEx(nullptr,COINIT_MULTITHREADED);
        IUIAutomation* automation = nullptr;
        if (SUCCEEDED(init)) CoCreateInstance(CLSID_CUIAutomation,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&automation));
        limitAutomationWait(automation);
        DWORD cachedPid = 0;
        bool art = false;
        while (!monitor->stop.load()) {
            HWND window = GetForegroundWindow();
            DWORD pid = 0;
            DWORD tid = GetWindowThreadProcessId(window,&pid);
            if (!monitor->enabled.load()) monitor->result = -1;
            else if (tid && pid != GetCurrentProcessId()) {
                if (pid != cachedPid) { art = artProcess(pid); cachedPid = pid; }
                int mode = art && !textFocus(tid,pid,automation) ? 1 : 0;
                if (monitor->enabled && window == GetForegroundWindow()) monitor->result = mode;
            }
            std::unique_lock<std::mutex> lock(monitor->mutex);
            monitor->wake.wait_for(lock, std::chrono::milliseconds(220), [monitor] { return monitor->stop.load(); });
        }
        if (automation) automation->Release();
        if (SUCCEEDED(init)) CoUninitialize();
    });
    return state;
}
}
