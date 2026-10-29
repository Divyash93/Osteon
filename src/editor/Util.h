// Windows helpers: UTF-8/UTF-16, files, paths, native dialogs.
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <vector>

namespace osteon {

std::wstring widen(const std::string& s);
std::string narrow(const std::wstring& s);

bool readTextFile(const std::string& path, std::string& out);
bool writeTextFile(const std::string& path, const std::string& text);
bool fileExists(const std::string& path);
bool dirExists(const std::string& path);
bool makeDirs(const std::string& path);
/** Lists files under dir (recursive) with any of the extensions (lowercase, with dot); returns paths relative to dir with '/' separators. */
std::vector<std::string> listFiles(const std::string& dir, const std::vector<std::string>& exts);

std::string normalizePath(std::string p);          // '/' separators, no trailing '/'
std::string joinPath(const std::string& a, const std::string& b);
std::string parentDir(const std::string& p);
std::string fileName(const std::string& p);
std::string stripExtension(const std::string& p);
std::string extension(const std::string& p);       // lowercase, with dot
std::string appDataDir();                          // %APPDATA%/Osteon

// Native dialogs; return empty string on cancel. Filter like L"Osteon project\0*.osteon\0All\0*.*\0".
std::string openFileDialog(HWND owner, const wchar_t* filter, const wchar_t* title);
std::string saveFileDialog(HWND owner, const wchar_t* filter, const wchar_t* defExt, const std::string& suggested, const wchar_t* title);
std::string pickFolderDialog(HWND owner, const wchar_t* title);
void openInExplorer(const std::string& path);

} // namespace osteon
