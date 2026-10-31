#include "Layouts.h"
#include "Util.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sstream>

namespace osteon {

namespace fs = std::filesystem;

static const char* kExt = ".layout";

std::string cleanLayoutName(const std::string& in) {
    size_t a = in.find_first_not_of(" \t"), b = in.find_last_not_of(" \t");
    if (a == std::string::npos) return {};
    std::string n = in.substr(a, b - a + 1);
    if (n.size() > 48) return {};
    for (unsigned char c : n) if (c < 32 || std::string("<>:\"/\\|?*").find((char)c) != std::string::npos) return {};
    if (n.back() == '.') return {};
    std::string upper = n;
    for (char& c : upper) c = (char)std::toupper((unsigned char)c);
    static const char* reserved[] = {"CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
                                     "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"};
    for (const char* r : reserved) if (upper == r) return {};
    return n;
}

std::string layoutsDir() { return joinPath(appDataDir(), "layouts"); }

static std::string fileFor(const std::string& dir, const std::string& name) { return joinPath(dir, name + kExt); }

std::vector<std::string> listLayouts(const std::string& dir) {
    std::vector<std::string> out;
    for (auto& f : listFiles(dir, {kExt})) {
        std::string n = stripExtension(f);
        if (!cleanLayoutName(n).empty()) out.push_back(n);
    }
    std::sort(out.begin(), out.end(), [](const std::string& x, const std::string& y) {
        return std::lexicographical_compare(x.begin(), x.end(), y.begin(), y.end(), [](unsigned char p, unsigned char q) { return std::tolower(p) < std::tolower(q); });
    });
    return out;
}

bool layoutExists(const std::string& dir, const std::string& name) {
    std::string n = cleanLayoutName(name);
    if (n.empty()) return false;
    for (auto& e : listLayouts(dir)) {
        if (e.size() != n.size()) continue;
        bool same = true;
        for (size_t i = 0; i < n.size(); i++) same &= std::tolower((unsigned char)e[i]) == std::tolower((unsigned char)n[i]);
        if (same) return true;
    }
    return false;
}

std::string serializeLayout(const SavedLayout& l) {
    std::ostringstream o;
    o << "osteon-layout 1\npanels";
    for (bool b : l.panels) o << ' ' << (b ? 1 : 0);
    o << "\n[ini]\n" << l.ini;
    return o.str();
}

bool parseLayout(const std::string& text, SavedLayout& out) {
    std::istringstream in(text);
    std::string line;
    if (!std::getline(in, line)) return false;
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
    if (line != "osteon-layout 1") return false;
    if (!std::getline(in, line)) return false;
    std::istringstream ps(line);
    std::string word;
    if (!(ps >> word) || word != "panels") return false;
    SavedLayout l;
    for (int i = 0; i < kLayoutPanels; i++) {
        int v;
        if (!(ps >> v) || (v != 0 && v != 1)) return false;
        l.panels[i] = v == 1;
    }
    if (!std::getline(in, line)) return false;
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
    if (line != "[ini]") return false;
    std::ostringstream rest;
    rest << in.rdbuf();
    l.ini = rest.str();
    out = std::move(l);
    return true;
}

bool saveLayout(const std::string& dir, const std::string& name, const SavedLayout& layout) {
    std::string n = cleanLayoutName(name);
    if (n.empty() || !makeDirs(dir)) return false;
    // Saving under a name that exists in another case replaces that layout instead of creating a twin.
    for (auto& e : listLayouts(dir))
        if (e.size() == n.size() && std::equal(e.begin(), e.end(), n.begin(), [](char a, char b) { return std::tolower((unsigned char)a) == std::tolower((unsigned char)b); })) { n = e; break; }
    return writeTextFile(fileFor(dir, n), serializeLayout(layout));
}

bool loadLayout(const std::string& dir, const std::string& name, SavedLayout& out) {
    std::string n = cleanLayoutName(name), text;
    return !n.empty() && readTextFile(fileFor(dir, n), text) && parseLayout(text, out);
}

bool renameLayout(const std::string& dir, const std::string& from, const std::string& to) {
    std::string f = cleanLayoutName(from), t = cleanLayoutName(to);
    if (f.empty() || t.empty() || !layoutExists(dir, f)) return false;
    bool onlyCase = f.size() == t.size() && std::equal(f.begin(), f.end(), t.begin(), [](char a, char b) { return std::tolower((unsigned char)a) == std::tolower((unsigned char)b); });
    if (!onlyCase && layoutExists(dir, t)) return false; // another layout already has that name
    std::error_code ec;
    fs::path src = fs::path(widen(fileFor(dir, f))), dst = fs::path(widen(fileFor(dir, t)));
    if (src == dst) return true;
    fs::rename(src, dst, ec);
    return !ec;
}

bool deleteLayout(const std::string& dir, const std::string& name) {
    std::string n = cleanLayoutName(name);
    if (n.empty()) return false;
    std::error_code ec;
    return fs::remove(fs::path(widen(fileFor(dir, n))), ec) && !ec;
}

} // namespace osteon
