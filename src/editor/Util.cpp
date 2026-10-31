#include "Util.h"
#include <commdlg.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace osteon {

std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}

std::string narrow(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}

bool readTextFile(const std::string& path, std::string& out) {
    std::ifstream f(fs::path(widen(path)), std::ios::binary);
    if (!f) return false;
    std::stringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    if (out.size() >= 3 && (uint8_t)out[0] == 0xEF && (uint8_t)out[1] == 0xBB && (uint8_t)out[2] == 0xBF) out = out.substr(3);
    return true;
}

bool writeTextFile(const std::string& path, const std::string& text) {
    // Write to a temp file then replace, so a failed save never corrupts the project.
    fs::path target(widen(path));
    fs::path tmp = target;
    tmp += L".tmp";
    {
        std::ofstream f(tmp, std::ios::binary);
        if (!f) return false;
        f << text;
        if (!f) return false;
    }
    std::error_code ec;
    fs::rename(tmp, target, ec);
    if (ec) { fs::remove(tmp, ec); return false; }
    return true;
}

bool fileExists(const std::string& p) { std::error_code ec; return fs::is_regular_file(fs::path(widen(p)), ec); }
bool dirExists(const std::string& p) { std::error_code ec; return fs::is_directory(fs::path(widen(p)), ec); }
bool makeDirs(const std::string& p) { std::error_code ec; fs::create_directories(fs::path(widen(p)), ec); return dirExists(p); }

std::vector<std::string> listFiles(const std::string& dir, const std::vector<std::string>& exts) {
    std::vector<std::string> out;
    std::error_code ec;
    fs::path base(widen(dir));
    for (auto it = fs::recursive_directory_iterator(base, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (!it->is_regular_file()) continue;
        std::string e = narrow(it->path().extension().wstring());
        std::transform(e.begin(), e.end(), e.begin(), ::tolower);
        if (std::find(exts.begin(), exts.end(), e) == exts.end()) continue;
        out.push_back(normalizePath(narrow(fs::relative(it->path(), base, ec).wstring())));
    }
    std::sort(out.begin(), out.end());
    return out;
}

std::string normalizePath(std::string p) {
    std::replace(p.begin(), p.end(), '\\', '/');
    while (p.size() > 1 && p.back() == '/') p.pop_back();
    return p;
}

std::string joinPath(const std::string& a, const std::string& b) {
    if (a.empty()) return normalizePath(b);
    if (b.size() > 1 && (b[1] == ':' || b[0] == '/')) return normalizePath(b); // absolute
    return normalizePath(normalizePath(a) + "/" + b);
}

std::string parentDir(const std::string& p) {
    std::string n = normalizePath(p);
    size_t i = n.find_last_of('/');
    return i == std::string::npos ? std::string() : n.substr(0, i);
}

std::string fileName(const std::string& p) {
    std::string n = normalizePath(p);
    size_t i = n.find_last_of('/');
    return i == std::string::npos ? n : n.substr(i + 1);
}

std::string stripExtension(const std::string& p) {
    size_t slash = p.find_last_of("/\\"), dot = p.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return p;
    return p.substr(0, dot);
}

std::string extension(const std::string& p) {
    std::string base = fileName(p);
    size_t dot = base.find_last_of('.');
    if (dot == std::string::npos) return {};
    std::string e = base.substr(dot);
    std::transform(e.begin(), e.end(), e.begin(), ::tolower);
    return e;
}

std::string appDataDir() {
    PWSTR p = nullptr;
    std::string out;
    if (const wchar_t* over = _wgetenv(L"OSTEON_DATA_DIR")) out = narrow(over); // testing: a build kept apart from the real settings
    else if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &p))) out = joinPath(narrow(p), "Osteon");
    CoTaskMemFree(p);
    makeDirs(out);
    return out;
}

std::string openFileDialog(HWND owner, const wchar_t* filter, const wchar_t* title) {
    wchar_t buf[MAX_PATH * 4] = {};
    OPENFILENAMEW ofn = {sizeof ofn};
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH * 4;
    ofn.lpstrTitle = title;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    return GetOpenFileNameW(&ofn) ? normalizePath(narrow(buf)) : std::string();
}

std::string saveFileDialog(HWND owner, const wchar_t* filter, const wchar_t* defExt, const std::string& suggested, const wchar_t* title) {
    wchar_t buf[MAX_PATH * 4] = {};
    std::wstring s = widen(suggested);
    std::replace(s.begin(), s.end(), L'/', L'\\');
    wcsncpy_s(buf, s.c_str(), _TRUNCATE);
    OPENFILENAMEW ofn = {sizeof ofn};
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH * 4;
    ofn.lpstrDefExt = defExt;
    ofn.lpstrTitle = title;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    return GetSaveFileNameW(&ofn) ? normalizePath(narrow(buf)) : std::string();
}

std::string pickFolderDialog(HWND owner, const wchar_t* title) {
    IFileOpenDialog* dlg = nullptr;
    std::string out;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dlg)))) {
        DWORD opts = 0;
        dlg->GetOptions(&opts);
        dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
        dlg->SetTitle(title);
        if (SUCCEEDED(dlg->Show(owner))) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dlg->GetResult(&item))) {
                PWSTR p = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &p))) out = normalizePath(narrow(p));
                CoTaskMemFree(p);
                item->Release();
            }
        }
        dlg->Release();
    }
    return out;
}

void openUrl(const std::string& url) { ShellExecuteW(nullptr, L"open", widen(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL); }

void openInExplorer(const std::string& path) {
    std::wstring w = widen(path);
    std::replace(w.begin(), w.end(), L'/', L'\\');
    ShellExecuteW(nullptr, L"open", w.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

} // namespace osteon
