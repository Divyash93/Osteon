#include "Audio.h"
#include <windows.h>
#include <mmsystem.h>
#include <algorithm>
#include <deque>

namespace osteon {

static std::deque<std::string> gAliases;
static int gNext = 0;

static std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n > 0 ? n - 1 : 0, L'\0');
    if (n > 1) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    return w;
}

bool playAudioFile(const std::string& path, float volume) {
    // Keep a handful of sounds open; the oldest is closed to make room.
    while (gAliases.size() >= 8) {
        mciSendStringW((L"close " + widen(gAliases.front())).c_str(), nullptr, 0, nullptr);
        gAliases.pop_front();
    }
    std::string alias = "osteonsnd" + std::to_string(gNext++);
    std::wstring wa = widen(alias);
    std::wstring open = L"open \"" + widen(path) + L"\" type mpegvideo alias " + wa;
    if (mciSendStringW(open.c_str(), nullptr, 0, nullptr) != 0) {
        // mpegvideo does not decode every format (plain wav is the usual one): let MCI choose by extension.
        open = L"open \"" + widen(path) + L"\" alias " + wa;
        if (mciSendStringW(open.c_str(), nullptr, 0, nullptr) != 0) return false;
    }
    gAliases.push_back(alias);
    int vol = (int)(std::min(std::max(volume, 0.0f), 1.0f) * 1000);
    mciSendStringW((L"setaudio " + wa + L" volume to " + std::to_wstring(vol)).c_str(), nullptr, 0, nullptr);
    return mciSendStringW((L"play " + wa).c_str(), nullptr, 0, nullptr) == 0;
}

void stopAllAudio() {
    for (auto& a : gAliases) mciSendStringW((L"close " + widen(a)).c_str(), nullptr, 0, nullptr);
    gAliases.clear();
}

} // namespace osteon
