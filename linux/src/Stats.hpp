#pragma once
#include <ctime>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include "CatPhysics.hpp"

// Today's keystroke count and the lifetime petting count, kept in ~/.config/kurocat/stats.ini so
// a restart or an update does not reset them (same counts the Windows and macOS builds keep).
namespace stats {

inline std::string today() {
    std::time_t now = std::time(nullptr);
    std::tm local{};
    localtime_r(&now, &local);
    char text[16];
    std::strftime(text, sizeof(text), "%Y-%m-%d", &local);
    return text;
}

struct Snapshot {
    std::string day;
    int todayKeys = 0;
    int petCount = 0;
    bool operator!=(const Snapshot& o) const { return day != o.day || todayKeys != o.todayKeys || petCount != o.petCount; }
};

inline std::string path() { return CatSettings::configPath() + "/stats.ini"; }

/// Loads the saved counts. Keystrokes from an earlier day start again at zero.
inline Snapshot load() {
    Snapshot snapshot;
    snapshot.day = today();
    std::ifstream file(path());
    std::string line, savedDay;
    int keys = 0;
    while (std::getline(file, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        try {
            if (k == "day") savedDay = v;
            else if (k == "todayKeys") keys = std::stoi(v);
            else if (k == "petCount") snapshot.petCount = std::stoi(v);
        } catch (...) {}
    }
    if (savedDay == snapshot.day) snapshot.todayKeys = keys;
    return snapshot;
}

inline void save(const Snapshot& snapshot) {
    std::string dir = CatSettings::configPath();
    mkdir(dir.substr(0, dir.find_last_of('/')).c_str(), 0755);
    mkdir(dir.c_str(), 0755);
    std::ofstream file(path());
    file << "day=" << snapshot.day << "\ntodayKeys=" << snapshot.todayKeys << "\npetCount=" << snapshot.petCount << "\n";
}

} // namespace stats
