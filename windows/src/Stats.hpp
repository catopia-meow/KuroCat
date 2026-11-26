#pragma once
#include <windows.h>
#include <cstdio>
#include <string>

// Today's keystroke count and the lifetime petting count, kept in the registry so a restart or an
// update does not reset them. Uses the same names as the macOS build's UserDefaults keys
// (StatsDay, TodayKeys, PetCount) under the app's original HKCU\Software\KuroCat key.
namespace stats {

inline std::wstring today() {
    SYSTEMTIME local{};
    GetLocalTime(&local);
    wchar_t text[16];
    std::swprintf(text, 16, L"%04u-%02u-%02u", local.wYear, local.wMonth, local.wDay);
    return text;
}

struct Snapshot {
    std::wstring day;
    int todayKeys = 0;
    int petCount = 0;
    bool operator==(const Snapshot& other) const {
        return day == other.day && todayKeys == other.todayKeys && petCount == other.petCount;
    }
    bool operator!=(const Snapshot& other) const { return !(*this == other); }
};

/// Loads the saved counts. Keystrokes from an earlier day start again at zero.
inline Snapshot load() {
    Snapshot snapshot;
    snapshot.day = today();
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\KuroCat", 0, KEY_READ, &key) != ERROR_SUCCESS) return snapshot;
    DWORD value = 0, size = sizeof(value);
    if (RegQueryValueExW(key, L"PetCount", nullptr, nullptr, reinterpret_cast<BYTE*>(&value), &size) == ERROR_SUCCESS)
        snapshot.petCount = static_cast<int>(value);
    wchar_t savedDay[32] = {};
    DWORD type = 0, daySize = sizeof(savedDay) - sizeof(wchar_t);
    if (RegQueryValueExW(key, L"StatsDay", nullptr, &type, reinterpret_cast<BYTE*>(savedDay), &daySize) == ERROR_SUCCESS &&
        type == REG_SZ && snapshot.day == savedDay) {
        size = sizeof(value);
        if (RegQueryValueExW(key, L"TodayKeys", nullptr, nullptr, reinterpret_cast<BYTE*>(&value), &size) == ERROR_SUCCESS)
            snapshot.todayKeys = static_cast<int>(value);
    }
    RegCloseKey(key);
    return snapshot;
}

inline void save(const Snapshot& snapshot) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\KuroCat", 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS) return;
    DWORD keys = static_cast<DWORD>(snapshot.todayKeys), pets = static_cast<DWORD>(snapshot.petCount);
    RegSetValueExW(key, L"StatsDay", 0, REG_SZ, reinterpret_cast<const BYTE*>(snapshot.day.c_str()),
                   static_cast<DWORD>((snapshot.day.size() + 1) * sizeof(wchar_t)));
    RegSetValueExW(key, L"TodayKeys", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&keys), sizeof(keys));
    RegSetValueExW(key, L"PetCount", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&pets), sizeof(pets));
    RegCloseKey(key);
}

} // namespace stats
