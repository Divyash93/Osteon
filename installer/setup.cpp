// Osteon Setup: a small native wizard (no installer framework). The program, its README and the banner are embedded as
// resources. The same executable is the uninstaller when started with /uninstall (a copy is kept in the install folder).
//
// Command line:  /S                silent install, no window
//                /D=<folder>       install folder (default %LOCALAPPDATA%\Programs\Osteon)
//                /NOREG            do not write shortcuts or registry entries (for testing)
//                /uninstall        remove an installed Osteon
#include <windows.h>
#include <algorithm>
using std::min;
using std::max;
#include <shellapi.h>
#include <shlwapi.h>
#include <commctrl.h>
#include <gdiplus.h>
#include <objbase.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <string>
#include <vector>

#ifndef OSTEON_VERSION
#define OSTEON_VERSION "0.1.0"
#endif

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

namespace {

enum { ID_BACK = 1001, ID_NEXT, ID_CANCEL, ID_BROWSE, ID_PATH, ID_DESKTOP, ID_ASSOC, ID_LAUNCH, ID_PROGRESS };
enum Page { PageWelcome, PageFolder, PageInstall, PageFinish, PageCount };

struct Options {
    bool silent = false, noReg = false, uninstall = false;
    std::wstring folder;
} g_opt;

HINSTANCE g_inst;
HWND g_wnd, g_back, g_next, g_cancel, g_path, g_browse, g_desktop, g_assoc, g_launch, g_progress, g_status;
HWND g_pageCtl[PageCount][8];
int g_pageCount[PageCount];
HFONT g_font, g_fontTitle, g_fontSmall;
HBRUSH g_white;
Gdiplus::Bitmap* g_banner;
int g_page = PageWelcome;
bool g_installed = false;
float g_scale = 1;
int S(int v) { return (int)(v * g_scale + 0.5f); }

// ---- resources ----

std::vector<BYTE> resource(int id) {
    HRSRC r = FindResourceW(nullptr, MAKEINTRESOURCEW(id), RT_RCDATA);
    if (!r) return {};
    HGLOBAL g = LoadResource(nullptr, r);
    DWORD n = SizeofResource(nullptr, r);
    const BYTE* p = g ? (const BYTE*)LockResource(g) : nullptr;
    return p ? std::vector<BYTE>(p, p + n) : std::vector<BYTE>();
}

bool writeFile(const std::wstring& path, const BYTE* data, size_t n) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD w = 0;
    bool ok = WriteFile(h, data, (DWORD)n, &w, nullptr) && w == n;
    CloseHandle(h);
    return ok;
}

std::wstring defaultFolder() {
    wchar_t buf[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", buf, MAX_PATH);
    std::wstring base = n ? std::wstring(buf, n) : L"C:\\Osteon";
    return base + L"\\Programs\\Osteon";
}

std::wstring programsFolder(int csidl) {
    wchar_t buf[MAX_PATH] = {};
    SHGetFolderPathW(nullptr, csidl, nullptr, SHGFP_TYPE_CURRENT, buf);
    return buf;
}

bool createDirs(const std::wstring& path) {
    int r = SHCreateDirectoryExW(nullptr, path.c_str(), nullptr);
    return r == ERROR_SUCCESS || r == ERROR_ALREADY_EXISTS || r == ERROR_FILE_EXISTS;
}

void setReg(HKEY root, const std::wstring& key, const wchar_t* name, const std::wstring& value) {
    HKEY k;
    if (RegCreateKeyExW(root, key.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &k, nullptr) != ERROR_SUCCESS) return;
    RegSetValueExW(k, name, 0, REG_SZ, (const BYTE*)value.c_str(), (DWORD)((value.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(k);
}

void setRegDword(HKEY root, const std::wstring& key, const wchar_t* name, DWORD v) {
    HKEY k;
    if (RegCreateKeyExW(root, key.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &k, nullptr) != ERROR_SUCCESS) return;
    RegSetValueExW(k, name, 0, REG_DWORD, (const BYTE*)&v, sizeof v);
    RegCloseKey(k);
}

bool makeShortcut(const std::wstring& lnk, const std::wstring& target, const std::wstring& workDir) {
    IShellLinkW* sl = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&sl)))) return false;
    sl->SetPath(target.c_str());
    sl->SetWorkingDirectory(workDir.c_str());
    sl->SetIconLocation(target.c_str(), 0);
    sl->SetDescription(L"Osteon 2D skeletal animation editor");
    IPersistFile* pf = nullptr;
    bool ok = SUCCEEDED(sl->QueryInterface(IID_PPV_ARGS(&pf))) && SUCCEEDED(pf->Save(lnk.c_str(), TRUE));
    if (pf) pf->Release();
    sl->Release();
    return ok;
}

// ---- installing ----

void progress(int pos, const wchar_t* text) {
    if (g_progress) SendMessageW(g_progress, PBM_SETPOS, pos, 0);
    if (g_status) SetWindowTextW(g_status, text);
    MSG m;
    while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageW(&m); }
}

// Writes a file, offering Retry when the old one is in use (Osteon is running).
bool writeWithRetry(const std::wstring& path, const BYTE* data, size_t n) {
    for (;;) {
        if (writeFile(path, data, n)) return true;
        if (g_opt.silent) return false;
        int r = MessageBoxW(g_wnd, (L"Cannot write\n" + path + L"\n\nIf Osteon is running, close it and choose Retry.").c_str(), L"Osteon Setup", MB_RETRYCANCEL | MB_ICONWARNING);
        if (r != IDRETRY) return false;
    }
}

bool install(const std::wstring& folder, bool desktop, bool assoc, std::wstring& error) {
    progress(5, L"Creating the installation folder...");
    if (!createDirs(folder)) { error = L"Cannot create " + folder; return false; }
    std::wstring exe = folder + L"\\Osteon.exe";
    progress(15, L"Copying Osteon...");
    std::vector<BYTE> payload = resource(102);
    if (payload.empty()) { error = L"This installer is damaged (no program inside)."; return false; }
    if (!writeWithRetry(exe, payload.data(), payload.size())) { error = L"Cannot write " + exe; return false; }
    progress(45, L"Copying the documentation...");
    std::vector<BYTE> readme = resource(103);
    if (!readme.empty()) writeFile(folder + L"\\README.md", readme.data(), readme.size());
    progress(60, L"Writing the uninstaller...");
    wchar_t self[MAX_PATH];
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    std::wstring un = folder + L"\\Uninstall.exe";
    if (_wcsicmp(self, un.c_str()) != 0) CopyFileW(self, un.c_str(), FALSE);
    if (g_opt.noReg) { progress(100, L"Done."); return true; }

    progress(70, L"Creating shortcuts...");
    makeShortcut(programsFolder(CSIDL_PROGRAMS) + L"\\Osteon.lnk", exe, folder);
    if (desktop) makeShortcut(programsFolder(CSIDL_DESKTOPDIRECTORY) + L"\\Osteon.lnk", exe, folder);
    progress(82, L"Registering the program...");
    if (assoc) {
        setReg(HKEY_CURRENT_USER, L"Software\\Classes\\.osteon", nullptr, L"Osteon.Project");
        setReg(HKEY_CURRENT_USER, L"Software\\Classes\\Osteon.Project", nullptr, L"Osteon Project");
        setReg(HKEY_CURRENT_USER, L"Software\\Classes\\Osteon.Project\\DefaultIcon", nullptr, L"\"" + exe + L"\",0");
        setReg(HKEY_CURRENT_USER, L"Software\\Classes\\Osteon.Project\\shell\\open\\command", nullptr, L"\"" + exe + L"\" \"%1\"");
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    }
    const std::wstring key = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Osteon";
    setReg(HKEY_CURRENT_USER, key, L"DisplayName", L"Osteon");
    setReg(HKEY_CURRENT_USER, key, L"DisplayVersion", L"" OSTEON_VERSION);
    setReg(HKEY_CURRENT_USER, key, L"Publisher", L"Osteon");
    setReg(HKEY_CURRENT_USER, key, L"DisplayIcon", exe);
    setReg(HKEY_CURRENT_USER, key, L"InstallLocation", folder);
    setReg(HKEY_CURRENT_USER, key, L"UninstallString", L"\"" + un + L"\" /uninstall");
    setRegDword(HKEY_CURRENT_USER, key, L"NoModify", 1);
    setRegDword(HKEY_CURRENT_USER, key, L"NoRepair", 1);
    progress(100, L"Done.");
    return true;
}

int runUninstall() {
    wchar_t self[MAX_PATH];
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    std::wstring folder = self;
    folder = folder.substr(0, folder.find_last_of(L'\\'));
    if (!g_opt.silent && MessageBoxW(nullptr, L"Remove Osteon from this computer?\n\nYour projects and settings are kept.", L"Osteon Setup", MB_YESNO | MB_ICONQUESTION) != IDYES) return 0;
    if (!g_opt.noReg) { // shortcuts and registry entries (left alone by a /NOREG test install)
        DeleteFileW((programsFolder(CSIDL_PROGRAMS) + L"\\Osteon.lnk").c_str());
        DeleteFileW((programsFolder(CSIDL_DESKTOPDIRECTORY) + L"\\Osteon.lnk").c_str());
        SHDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\Osteon.Project");
        wchar_t val[64] = {}; DWORD n = sizeof val; HKEY k;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\.osteon", 0, KEY_READ, &k) == ERROR_SUCCESS) {
            RegQueryValueExW(k, nullptr, nullptr, nullptr, (BYTE*)val, &n);
            RegCloseKey(k);
            if (wcscmp(val, L"Osteon.Project") == 0) SHDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\.osteon");
        }
        SHDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Osteon");
    }
    DeleteFileW((folder + L"\\Osteon.exe").c_str());
    DeleteFileW((folder + L"\\README.md").c_str());
    // This program is running from the folder: remove it, and the folder, once the process has ended.
    std::wstring cmd = L"cmd.exe /c ping -n 3 127.0.0.1 >nul & del /f /q \"" + folder + L"\\Uninstall.exe\" & rmdir \"" + folder + L"\"";
    STARTUPINFOW si = {sizeof si}; si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};
    std::vector<wchar_t> buf(cmd.begin(), cmd.end()); buf.push_back(0);
    if (CreateProcessW(nullptr, buf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) { CloseHandle(pi.hThread); CloseHandle(pi.hProcess); }
    if (!g_opt.silent) MessageBoxW(nullptr, L"Osteon has been removed.", L"Osteon Setup", MB_OK | MB_ICONINFORMATION);
    return 0;
}

// ---- the wizard window ----

HWND makeCtl(const wchar_t* cls, const wchar_t* text, DWORD style, int x, int y, int w, int h, int id, HFONT font) {
    HWND c = CreateWindowExW(0, cls, text, WS_CHILD | style, S(x), S(y), S(w), S(h), g_wnd, (HMENU)(INT_PTR)id, g_inst, nullptr);
    SendMessageW(c, WM_SETFONT, (WPARAM)font, TRUE);
    return c;
}

void addToPage(int page, HWND c) { g_pageCtl[page][g_pageCount[page]++] = c; }

std::wstring getPath() {
    wchar_t buf[1024];
    GetWindowTextW(g_path, buf, 1024);
    return buf;
}

void showPage(int page) {
    g_page = page;
    for (int p = 0; p < PageCount; p++)
        for (int i = 0; i < g_pageCount[p]; i++) ShowWindow(g_pageCtl[p][i], p == page ? SW_SHOW : SW_HIDE);
    EnableWindow(g_back, page == PageFolder);
    SetWindowTextW(g_next, page == PageFolder ? L"Install" : page == PageFinish ? L"Finish" : L"Next");
    EnableWindow(g_next, page != PageInstall);
    EnableWindow(g_cancel, page != PageInstall && page != PageFinish);
    ShowWindow(g_back, page == PageFinish || page == PageInstall ? SW_HIDE : SW_SHOW);
    ShowWindow(g_cancel, page == PageFinish ? SW_HIDE : SW_SHOW);
    InvalidateRect(g_wnd, nullptr, TRUE);
}

void doInstall() {
    showPage(PageInstall);
    std::wstring folder = getPath();
    bool desktop = SendMessageW(g_desktop, BM_GETCHECK, 0, 0) == BST_CHECKED, assoc = SendMessageW(g_assoc, BM_GETCHECK, 0, 0) == BST_CHECKED;
    std::wstring error;
    if (install(folder, desktop, assoc, error)) { g_installed = true; showPage(PageFinish); }
    else {
        MessageBoxW(g_wnd, (error + L"\n\nNothing else was changed.").c_str(), L"Osteon Setup", MB_OK | MB_ICONERROR);
        showPage(PageFolder);
    }
}

void browse() {
    IFileOpenDialog* d = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&d)))) return;
    DWORD opts = 0;
    d->GetOptions(&opts);
    d->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    d->SetTitle(L"Choose the folder to install Osteon into");
    if (SUCCEEDED(d->Show(g_wnd))) {
        IShellItem* it = nullptr;
        if (SUCCEEDED(d->GetResult(&it))) {
            PWSTR p = nullptr;
            if (SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &p))) {
                std::wstring f = p;
                if (f.size() < 6 || _wcsicmp(f.substr(f.size() - 6).c_str(), L"Osteon") != 0) f += L"\\Osteon";
                SetWindowTextW(g_path, f.c_str());
                CoTaskMemFree(p);
            }
            it->Release();
        }
    }
    d->Release();
}

void paint(HDC dc) {
    RECT rc;
    GetClientRect(g_wnd, &rc);
    int bannerH = S(155), barY = rc.bottom - S(62);
    // Banner, scaled to the window width.
    if (g_banner && g_banner->GetLastStatus() == Gdiplus::Ok) {
        Gdiplus::Graphics g(dc);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.DrawImage(g_banner, 0, 0, rc.right, bannerH);
    }
    RECT body = {0, bannerH, rc.right, barY};
    FillRect(dc, &body, g_white);
    HBRUSH grey = CreateSolidBrush(RGB(240, 240, 240));
    RECT bar = {0, barY, rc.right, rc.bottom};
    FillRect(dc, &bar, grey);
    DeleteObject(grey);
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(223, 223, 223));
    HGDIOBJ old = SelectObject(dc, pen);
    MoveToEx(dc, 0, barY, nullptr); LineTo(dc, rc.right, barY);
    SelectObject(dc, old);
    DeleteObject(pen);
}

LRESULT CALLBACK wndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        paint(dc);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_CTLCOLORSTATIC: case WM_CTLCOLORBTN: case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)w;
        HWND c = (HWND)l;
        if (c == g_back || c == g_next || c == g_cancel) break;
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(20, 20, 20));
        return (LRESULT)g_white;
    }
    case WM_COMMAND:
        switch (LOWORD(w)) {
        case ID_NEXT:
            if (g_page == PageWelcome) showPage(PageFolder);
            else if (g_page == PageFolder) doInstall();
            else if (g_page == PageFinish) {
                if (SendMessageW(g_launch, BM_GETCHECK, 0, 0) == BST_CHECKED) ShellExecuteW(nullptr, L"open", (getPath() + L"\\Osteon.exe").c_str(), nullptr, getPath().c_str(), SW_SHOWNORMAL);
                DestroyWindow(h);
            }
            return 0;
        case ID_BACK: if (g_page == PageFolder) showPage(PageWelcome); return 0;
        case ID_CANCEL:
            if (MessageBoxW(h, L"Exit the Osteon Setup Wizard?", L"Osteon Setup", MB_YESNO | MB_ICONQUESTION) == IDYES) DestroyWindow(h);
            return 0;
        case ID_BROWSE: browse(); return 0;
        }
        break;
    case WM_CLOSE:
        if (g_page == PageInstall) return 0;
        if (g_page != PageFinish && MessageBoxW(h, L"Exit the Osteon Setup Wizard?", L"Osteon Setup", MB_YESNO | MB_ICONQUESTION) != IDYES) return 0;
        DestroyWindow(h);
        return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

Gdiplus::Bitmap* loadBanner() {
    std::vector<BYTE> b = resource(101);
    if (b.empty()) return nullptr;
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, b.size());
    memcpy(GlobalLock(g), b.data(), b.size());
    GlobalUnlock(g);
    IStream* s = nullptr;
    if (FAILED(CreateStreamOnHGlobal(g, TRUE, &s))) return nullptr;
    return Gdiplus::Bitmap::FromStream(s); // the stream must outlive the bitmap: it is left to the process
}

int runWizard(int show) {
    INITCOMMONCONTROLSEX icc = {sizeof icc, ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);
    WNDCLASSW wc = {};
    wc.lpfnWndProc = wndProc;
    wc.hInstance = g_inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(g_inst, MAKEINTRESOURCEW(100));
    wc.lpszClassName = L"OsteonSetup";
    RegisterClassW(&wc);
    HDC screen = GetDC(nullptr);
    UINT dpi = GetDeviceCaps(screen, LOGPIXELSX);
    ReleaseDC(nullptr, screen);
    g_scale = dpi / 96.0f;
    g_white = CreateSolidBrush(RGB(255, 255, 255));
    g_font = CreateFontW(-S(13), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fontTitle = CreateFontW(-S(24), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fontSmall = CreateFontW(-S(12), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_banner = loadBanner();

    RECT r = {0, 0, S(600), S(470)};
    DWORD style = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    AdjustWindowRect(&r, style, FALSE);
    int ww = r.right - r.left, wh = r.bottom - r.top;
    g_wnd = CreateWindowExW(0, L"OsteonSetup", L"Osteon Setup", style, (GetSystemMetrics(SM_CXSCREEN) - ww) / 2, (GetSystemMetrics(SM_CYSCREEN) - wh) / 2, ww, wh, nullptr, nullptr, g_inst, nullptr);

    // Page 0: welcome.
    addToPage(PageWelcome, makeCtl(L"STATIC", L"Welcome to the Osteon Setup Wizard", SS_LEFT, 32, 172, 540, 40, 0, g_fontTitle));
    addToPage(PageWelcome, makeCtl(L"STATIC", L"The Setup Wizard will install Osteon " OSTEON_VERSION L" on your computer. Click Next to continue or Cancel to exit the Setup Wizard.", SS_LEFT, 32, 222, 520, 60, 0, g_font));
    addToPage(PageWelcome, makeCtl(L"STATIC", L"Osteon is a free 2D skeletal animation editor.", SS_LEFT, 32, 290, 520, 22, 0, g_fontSmall));
    // Page 1: folder and options.
    addToPage(PageFolder, makeCtl(L"STATIC", L"Choose where to install Osteon", SS_LEFT, 32, 172, 540, 36, 0, g_fontTitle));
    addToPage(PageFolder, makeCtl(L"STATIC", L"Osteon will be installed into this folder. It does not need administrator rights.", SS_LEFT, 32, 214, 540, 22, 0, g_font));
    g_path = makeCtl(L"EDIT", g_opt.folder.c_str(), ES_AUTOHSCROLL | WS_BORDER | WS_TABSTOP, 32, 244, 440, 26, ID_PATH, g_font);
    g_browse = makeCtl(L"BUTTON", L"Browse...", BS_PUSHBUTTON | WS_TABSTOP, 482, 243, 86, 28, ID_BROWSE, g_font);
    g_desktop = makeCtl(L"BUTTON", L"Create a desktop shortcut", BS_AUTOCHECKBOX | WS_TABSTOP, 32, 290, 400, 24, ID_DESKTOP, g_font);
    g_assoc = makeCtl(L"BUTTON", L"Open .osteon project files with Osteon", BS_AUTOCHECKBOX | WS_TABSTOP, 32, 320, 400, 24, ID_ASSOC, g_font);
    SendMessageW(g_desktop, BM_SETCHECK, BST_CHECKED, 0);
    SendMessageW(g_assoc, BM_SETCHECK, BST_CHECKED, 0);
    for (HWND c : {g_path, g_browse, g_desktop, g_assoc}) addToPage(PageFolder, c);
    // Page 2: installing.
    addToPage(PageInstall, makeCtl(L"STATIC", L"Installing Osteon", SS_LEFT, 32, 172, 540, 36, 0, g_fontTitle));
    g_status = makeCtl(L"STATIC", L"", SS_LEFT, 32, 230, 540, 22, 0, g_font);
    g_progress = makeCtl(PROGRESS_CLASSW, L"", PBS_SMOOTH, 32, 262, 536, 20, ID_PROGRESS, g_font);
    SendMessageW(g_progress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    addToPage(PageInstall, g_status);
    addToPage(PageInstall, g_progress);
    // Page 3: finished.
    addToPage(PageFinish, makeCtl(L"STATIC", L"Osteon has been installed", SS_LEFT, 32, 172, 540, 36, 0, g_fontTitle));
    addToPage(PageFinish, makeCtl(L"STATIC", L"Click Finish to close the Setup Wizard. You can start Osteon from the Start menu at any time.", SS_LEFT, 32, 222, 520, 44, 0, g_font));
    g_launch = makeCtl(L"BUTTON", L"Start Osteon now", BS_AUTOCHECKBOX | WS_TABSTOP, 32, 290, 300, 24, ID_LAUNCH, g_font);
    SendMessageW(g_launch, BM_SETCHECK, BST_CHECKED, 0);
    addToPage(PageFinish, g_launch);
    // The button bar.
    g_back = makeCtl(L"BUTTON", L"Back", BS_PUSHBUTTON | WS_VISIBLE | WS_TABSTOP, 296, 484 - 62 + 8, 84, 28, ID_BACK, g_font);
    g_next = makeCtl(L"BUTTON", L"Next", BS_DEFPUSHBUTTON | WS_VISIBLE | WS_TABSTOP, 390, 484 - 62 + 8, 84, 28, ID_NEXT, g_font);
    g_cancel = makeCtl(L"BUTTON", L"Cancel", BS_PUSHBUTTON | WS_VISIBLE | WS_TABSTOP, 484, 484 - 62 + 8, 84, 28, ID_CANCEL, g_font);
    // The bar sits at the bottom of the client area (470 px tall); the buttons are vertically centred in its 62 px.
    for (HWND b : {g_back, g_next, g_cancel}) SetWindowPos(b, nullptr, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOMOVE);
    RECT cr; GetClientRect(g_wnd, &cr);
    int by = cr.bottom - S(62) + S(17);
    SetWindowPos(g_back, nullptr, S(296), by, S(84), S(28), SWP_NOZORDER);
    SetWindowPos(g_next, nullptr, S(390), by, S(84), S(28), SWP_NOZORDER);
    SetWindowPos(g_cancel, nullptr, S(484), by, S(84), S(28), SWP_NOZORDER);
    showPage(PageWelcome);
    ShowWindow(g_wnd, show);
    UpdateWindow(g_wnd);
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(g_wnd, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    return 0;
}

void parseArgs() {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; i < argc; i++) {
        std::wstring a = argv[i];
        if (_wcsicmp(a.c_str(), L"/S") == 0) g_opt.silent = true;
        else if (_wcsicmp(a.c_str(), L"/NOREG") == 0) g_opt.noReg = true;
        else if (_wcsicmp(a.c_str(), L"/uninstall") == 0) g_opt.uninstall = true;
        else if (a.size() > 3 && _wcsnicmp(a.c_str(), L"/D=", 3) == 0) g_opt.folder = a.substr(3);
    }
    LocalFree(argv);
    if (g_opt.folder.empty()) g_opt.folder = defaultFolder();
}

} // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show) {
    g_inst = inst;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    Gdiplus::GdiplusStartupInput gi;
    ULONG_PTR token = 0;
    Gdiplus::GdiplusStartup(&token, &gi, nullptr);
    parseArgs();
    int result = 0;
    if (g_opt.uninstall) result = runUninstall();
    else if (g_opt.silent) {
        std::wstring error;
        if (!install(g_opt.folder, true, true, error)) result = 1;
    } else result = runWizard(show);
    Gdiplus::GdiplusShutdown(token);
    return result;
}
