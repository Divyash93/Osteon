#include "I18n.h"
#include "Util.h"
#include "imgui.h"
#include <cstring>
#include <sstream>
#include <unordered_map>

namespace osteon {

namespace {

Lang gLang = Lang::English;

const LangInfo kInfo[] = {
    {"en", "English"}, {"es", "Español"}, {"fr", "Français"}, {"de", "Deutsch"},
    {"pt-BR", "Português (Brasil)"}, {"hi", "हिन्दी"}, {"zh-CN", "简体中文"}, {"ja", "日本語"},
};

using Table = std::unordered_map<std::string, const char*>;
Table& table(Lang l) {
    static Table tables[(int)Lang::Count];
    static bool built = false;
    if (!built) {
        built = true;
        for (size_t i = 0; i < kTrRowCount; i++)
            for (int k = 0; k < (int)Lang::Count - 1; k++)
                if (kTrRows[i].t[k][0]) tables[k + 1][kTrRows[i].en] = kTrRows[i].t[k];
    }
    return tables[(int)l];
}

// Node based map: pointers into it stay valid while more entries are added.
std::unordered_map<std::string, std::string>& cache() {
    static std::unordered_map<std::string, std::string> c;
    return c;
}

} // namespace

const LangInfo& langInfo(Lang l) { return kInfo[(int)l]; }

Lang langFromCode(const std::string& code) {
    for (int i = 0; i < (int)Lang::Count; i++) if (code == kInfo[i].code) return (Lang)i;
    return Lang::English;
}

void setLanguage(Lang l) { gLang = l; }
Lang language() { return gLang; }

const char* tr(const char* english) {
    if (gLang == Lang::English || !english || !*english) return english;
    const char* hash = std::strstr(english, "##");
    std::string key = hash ? std::string(english, hash) : std::string(english);
    Table& t = table(gLang);
    auto it = t.find(key);
    if (it == t.end()) return english;
    std::string ck = std::to_string((int)gLang) + "|" + english;
    auto& c = cache();
    auto found = c.find(ck);
    if (found == c.end()) found = c.emplace(ck, std::string(it->second) + (hash ? hash : "")).first;
    return found->second.c_str();
}

const char* trWindow(const char* english) {
    if (!english) return english;
    std::string ck = "w" + std::to_string((int)gLang) + "|" + english;
    auto& c = cache();
    auto found = c.find(ck);
    if (found == c.end()) {
        const char* shown = english;
        if (gLang != Lang::English) {
            auto& t = table(gLang);
            auto it = t.find(english);
            if (it != t.end()) shown = it->second;
        }
        found = c.emplace(ck, std::string(shown) + "###" + english).first;
    }
    return found->second.c_str();
}

std::string migrateLayoutIni(const std::string& ini) {
    std::istringstream in(ini);
    std::string line, out;
    out.reserve(ini.size() + 256);
    while (std::getline(in, line)) {
        // "[Window][Name]" becomes "[Window][Name###Name]"; names that already carry ### stay as they are.
        const char* pre = "[Window][";
        if (line.rfind(pre, 0) == 0 && line.find("###") == std::string::npos) {
            size_t end = line.find_last_of(']');
            if (end != std::string::npos && end > 9) {
                std::string name = line.substr(9, end - 9);
                if (!name.empty() && name[0] != '#') line = std::string(pre) + name + "###" + name + "]";
            }
        }
        out += line;
        out += '\n';
    }
    return out;
}

void loadLayoutIni(const std::string& ini) {
    std::string migrated = migrateLayoutIni(ini);
    ImGui::LoadIniSettingsFromMemory(migrated.c_str(), migrated.size());
}

Lang storedLanguage() {
    std::string text;
    if (!readTextFile(joinPath(appDataDir(), "settings.txt"), text)) return Lang::English;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("language ", 0) == 0) {
            std::string code = line.substr(9);
            while (!code.empty() && (code.back() == '\r' || code.back() == ' ')) code.pop_back();
            return langFromCode(code);
        }
    }
    return Lang::English;
}

int translatedCount(Lang l) {
    if (l == Lang::English) return (int)kTrRowCount;
    int n = 0;
    for (size_t i = 0; i < kTrRowCount; i++) if (kTrRows[i].t[(int)l - 1][0]) n++;
    return n;
}

int totalLabelCount() { return (int)kTrRowCount; }

} // namespace osteon
