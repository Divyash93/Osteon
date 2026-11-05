// Win32 + Direct3D 11 host for the Osteon editor.
#include <functional>
#include "App.h"
#include "Icons.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include <d3d11.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <dbghelp.h>
#include <csignal>
#include <cstdio>
#include <exception>
#include <cmath>
#include <vector>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

using namespace osteon;

static ID3D11Device* g_device = nullptr;
static ID3D11DeviceContext* g_ctx = nullptr;
static IDXGISwapChain* g_swap = nullptr;
static ID3D11RenderTargetView* g_rtv = nullptr;
static UINT g_resizeW = 0, g_resizeH = 0;
static Editor* g_editor = nullptr;

static void createRenderTarget() {
    ID3D11Texture2D* back = nullptr;
    g_swap->GetBuffer(0, IID_PPV_ARGS(&back));
    g_device->CreateRenderTargetView(back, nullptr, &g_rtv);
    back->Release();
}

static void cleanupRenderTarget() {
    if (g_rtv) { g_rtv->Release(); g_rtv = nullptr; }
}

static bool createDevice(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL got;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2, D3D11_SDK_VERSION, &sd, &g_swap,
                                               &g_device, &got, &g_ctx);
    if (hr == DXGI_ERROR_UNSUPPORTED)
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 2, D3D11_SDK_VERSION, &sd, &g_swap,
                                           &g_device, &got, &g_ctx);
    if (FAILED(hr)) return false;
    createRenderTarget();
    return true;
}

static void cleanupDevice() {
    cleanupRenderTarget();
    if (g_swap) g_swap->Release();
    if (g_ctx) g_ctx->Release();
    if (g_device) g_device->Release();
}

// Pen taps and synthetic clicks deliver the move and the button press together. ImGui widgets
// need one frame of hover before a press, so such presses are replayed on the next frame.
struct DeferredMsg { HWND hwnd; UINT msg; WPARAM wParam; LPARAM lParam; };
static std::vector<DeferredMsg> g_deferred;
static bool g_replaying = false;
static bool g_splashMode = false; // the window is card-sized while the splash shows
static std::function<void()> g_step; // one frame: also run while Windows owns the loop (moving / sizing the window)
static bool g_inStep = false, g_sizeMove = false;

static bool isMouseButtonMsg(UINT msg) {
    return (msg >= WM_LBUTTONDOWN && msg <= WM_MBUTTONDBLCLK) || msg == WM_XBUTTONDOWN || msg == WM_XBUTTONUP || msg == WM_XBUTTONDBLCLK;
}

static LRESULT WINAPI wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (!g_replaying && isMouseButtonMsg(msg) && ImGui::GetCurrentContext()) {
        ImVec2 last = ImGui::GetIO().MousePos;
        float x = (float)(short)LOWORD(lParam), y = (float)(short)HIWORD(lParam);
        bool jumped = std::fabs(last.x - x) > 0.5f || std::fabs(last.y - y) > 0.5f;
        if (!g_deferred.empty() || jumped) {
            if (jumped) ImGui_ImplWin32_WndProcHandler(hwnd, WM_MOUSEMOVE, 0, lParam);
            g_deferred.push_back({hwnd, msg, wParam, lParam});
            return 0;
        }
    }
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) return true;
    switch (msg) {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED) return 0;
        g_resizeW = LOWORD(lParam);
        g_resizeH = HIWORD(lParam);
        if (g_sizeMove && g_step) g_step(); // repaint at the new size right away instead of stretching the old frame
        return 0;
    case WM_ENTERSIZEMOVE: g_sizeMove = true; SetTimer(hwnd, 1, 8, nullptr); return 0;
    case WM_EXITSIZEMOVE: g_sizeMove = false; KillTimer(hwnd, 1); return 0;
    case WM_TIMER:
        if (wParam == 1 && g_step) g_step();
        return 0;
    case WM_ERASEBKGND: return 1;
    case WM_GETMINMAXINFO:
        ((MINMAXINFO*)lParam)->ptMinTrackSize = g_splashMode ? POINT{200, 120} : POINT{800, 500};
        return 0;
    case WM_NCCALCSIZE: // no native caption or frame: the client area is the whole window (the header draws its own)
        if (wParam) {
            NCCALCSIZE_PARAMS* p = (NCCALCSIZE_PARAMS*)lParam;
            if (IsZoomed(hwnd)) { // a maximized window overhangs the screen by its frame: pull the edges back in
                UINT dpi = GetDpiForWindow(hwnd);
                int fx = GetSystemMetricsForDpi(SM_CXFRAME, dpi) + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
                int fy = GetSystemMetricsForDpi(SM_CYFRAME, dpi) + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
                p->rgrc[0].left += fx; p->rgrc[0].right -= fx; p->rgrc[0].top += fy; p->rgrc[0].bottom -= fy;
            }
            return 0;
        }
        break;
    case WM_NCHITTEST: {
        POINT pt = {(short)LOWORD(lParam), (short)HIWORD(lParam)};
        ScreenToClient(hwnd, &pt);
        RECT rc;
        GetClientRect(hwnd, &rc);
        if (!IsZoomed(hwnd) && !g_splashMode) { // resize borders
            int b = (int)(6 * GetDpiForWindow(hwnd) / 96.0f);
            bool l = pt.x < b, r = pt.x >= rc.right - b, t = pt.y < b, bt = pt.y >= rc.bottom - b;
            if (t && l) return HTTOPLEFT;
            if (t && r) return HTTOPRIGHT;
            if (bt && l) return HTBOTTOMLEFT;
            if (bt && r) return HTBOTTOMRIGHT;
            if (l) return HTLEFT;
            if (r) return HTRIGHT;
            if (t) return HTTOP;
            if (bt) return HTBOTTOM;
        }
        if (g_editor && g_editor->headerHeight > 0 && pt.y < (int)g_editor->headerHeight && !g_editor->headerInteractiveAt(pt.x, pt.y)) return HTCAPTION;
        return HTCLIENT;
    }
    case WM_APP + 1: // a second launch asked to be brought forward; Windows only allows self-activation
        if (GetForegroundWindow() != hwnd) {
            bool maximized = IsZoomed(hwnd);
            ShowWindow(hwnd, SW_MINIMIZE);
            ShowWindow(hwnd, maximized ? SW_SHOWMAXIMIZED : SW_RESTORE);
            SetForegroundWindow(hwnd);
        }
        return 0;
    case WM_COPYDATA: { // file path forwarded by a second launch (single instance)
        const COPYDATASTRUCT* cds = (const COPYDATASTRUCT*)lParam;
        if (g_editor && cds && cds->dwData == 0x05EE && cds->cbData > 0)
            g_editor->pendingOpen = std::string((const char*)cds->lpData, cds->cbData);
        return TRUE;
    }
    case WM_DROPFILES: {
        HDROP drop = (HDROP)wParam;
        wchar_t buf[MAX_PATH * 4];
        if (DragQueryFileW(drop, 0, buf, MAX_PATH * 4) && g_editor) g_editor->pendingOpen = narrow(buf);
        DragFinish(drop);
        return 0;
    }
    case WM_CLOSE:
        if (g_editor && !g_editor->confirmDiscard()) return 0;
        DestroyWindow(hwnd);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) return 0; // Alt alone must not open the system menu
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// Dark: Photoshop-style neutral greys. Light: light greys with dark text. Both use Osteon's dark blue accent.
void osteon::applyUiTheme(float scale, bool light) {
    ImGuiStyle& s = ImGui::GetStyle();
    s = ImGuiStyle();
    ImGui::StyleColorsDark(&s);
    // Modern flat look: soft rounded corners and no outlines; surfaces are told apart by tone, not by borders.
    s.WindowRounding = 6; s.ChildRounding = 6; s.FrameRounding = 5; s.TabRounding = 6; s.ScrollbarRounding = 8; s.GrabRounding = 5; s.PopupRounding = 8;
    s.WindowPadding = ImVec2(8, 8); s.FramePadding = ImVec2(8, 4); s.ItemSpacing = ImVec2(7, 5); s.IndentSpacing = 14;
    s.ScrollbarSize = 11; s.GrabMinSize = 10; s.SeparatorTextBorderSize = 1;
    s.WindowBorderSize = 0; s.ChildBorderSize = 0; s.FrameBorderSize = 0; s.TabBorderSize = 0; s.PopupBorderSize = 1;
    ImVec4* c = s.Colors;
    ImVec4 panel, tree, strip, menu, frame, border, raised, raisedHi, text, textDim, popup, headerSel, headerHover, tabHover, grab, grabHover, frameHover, frameActive;
    const ImVec4 accent(0.071f, 0.302f, 0.678f, 1), accentHi(0.122f, 0.400f, 0.851f, 1); // the icon's blue, darker
    if (!light) {
        // The palette lives in Theme.h: #262626 app, #323232 panels / buttons, #292929 fields, #48484b borders, #77777d button outlines.
        using namespace theme;
        panel = vec(hex(kPanel)); tree = panel; strip = vec(hex(kAppBg)); menu = panel;
        frame = vec(hex(kField)); border = vec(hex(kBorder)); raised = panel; raisedHi = vec(hex(kHover));
        text = vec(hex(kText)); textDim = vec(hex(kTextSecondary)); popup = panel;
        headerSel = accent; headerHover = vec(hex(kRowHover)); tabHover = vec(hex(kTabHover));
        grab = vec(hex(kBorder)); grabHover = vec(hex(kStroke)); frameHover = vec(hex(kFieldHover)); frameActive = vec(hex(kPressed));
        s.WindowBorderSize = 1; s.FrameBorderSize = 1; s.DisabledAlpha = 0.40f; // disabled text lands near #77777d
    } else {
        // Light: white panels, grey fields, buttons and canvases (the opposite tones of the Dark theme's depth).
        panel = {0.955f, 0.955f, 0.955f, 1}; tree = {1.000f, 1.000f, 1.000f, 1}; strip = {0.860f, 0.860f, 0.860f, 1}; menu = {0.900f, 0.900f, 0.900f, 1};
        frame = {0.860f, 0.860f, 0.860f, 1}; border = {0, 0, 0, 0.22f}; raised = {0.870f, 0.870f, 0.870f, 1}; raisedHi = {0.790f, 0.790f, 0.790f, 1};
        text = {0.090f, 0.090f, 0.090f, 1}; textDim = {0.440f, 0.440f, 0.440f, 1}; popup = {0.985f, 0.985f, 0.985f, 0.99f};
        headerSel = {0.680f, 0.790f, 0.960f, 1}; headerHover = {0.860f, 0.880f, 0.920f, 1}; tabHover = {0.900f, 0.900f, 0.900f, 1};
        grab = {0.700f, 0.700f, 0.700f, 1}; grabHover = {0.560f, 0.560f, 0.560f, 1}; frameHover = {0.820f, 0.820f, 0.820f, 1}; frameActive = {0.780f, 0.780f, 0.780f, 1};
    }
    c[ImGuiCol_Text] = text;
    c[ImGuiCol_TextDisabled] = textDim;
    c[ImGuiCol_WindowBg] = panel;
    c[ImGuiCol_ChildBg] = tree;
    c[ImGuiCol_PopupBg] = popup;
    c[ImGuiCol_MenuBarBg] = menu;
    c[ImGuiCol_Border] = border;
    c[ImGuiCol_ButtonBorder] = light ? border : theme::vec(theme::hex(theme::kStroke));
    c[ImGuiCol_FrameBg] = frame;
    c[ImGuiCol_FrameBgHovered] = frameHover;
    c[ImGuiCol_FrameBgActive] = frameActive;
    c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab] = grab;
    c[ImGuiCol_ScrollbarGrabHovered] = grabHover;
    c[ImGuiCol_ScrollbarGrabActive] = accent;
    c[ImGuiCol_TitleBg] = strip; c[ImGuiCol_TitleBgActive] = strip; c[ImGuiCol_TitleBgCollapsed] = strip;
    c[ImGuiCol_Button] = raised;
    c[ImGuiCol_ButtonHovered] = raisedHi;
    c[ImGuiCol_ButtonActive] = light ? accent : theme::vec(theme::hex(theme::kPressed));
    c[ImGuiCol_Header] = headerSel;
    c[ImGuiCol_HeaderHovered] = headerHover;
    c[ImGuiCol_HeaderActive] = light ? ImVec4(0.540f, 0.690f, 0.940f, 1) : accentHi;
    c[ImGuiCol_CheckMark] = light ? ImVec4(0.071f, 0.302f, 0.678f, 1) : theme::vec(theme::hex(theme::kText));
    c[ImGuiCol_SliderGrab] = accent; c[ImGuiCol_SliderGrabActive] = accentHi;
    c[ImGuiCol_Tab] = strip;
    c[ImGuiCol_TabHovered] = tabHover;
    c[ImGuiCol_TabSelected] = panel;
    c[ImGuiCol_TabSelectedOverline] = accent;
    c[ImGuiCol_TabDimmed] = strip;
    c[ImGuiCol_TabDimmedSelected] = panel;
    c[ImGuiCol_DockingPreview] = ImVec4(accent.x, accent.y, accent.z, 0.5f);
    c[ImGuiCol_DockingEmptyBg] = strip;
    c[ImGuiCol_Separator] = light ? border : theme::vec(theme::hex(theme::kDivider));
    c[ImGuiCol_SeparatorHovered] = accent; c[ImGuiCol_SeparatorActive] = accentHi;
    c[ImGuiCol_TableBorderStrong] = border; c[ImGuiCol_TableBorderLight] = border;
    if (!light) c[ImGuiCol_PopupBg] = popup; // opaque, like the panels
    c[ImGuiCol_TableHeaderBg] = strip;
    c[ImGuiCol_ResizeGrip] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TextSelectedBg] = ImVec4(accent.x, accent.y, accent.z, 0.45f);
    c[ImGuiCol_NavHighlight] = accent;
    s.TreeLinesFlags = ImGuiTreeNodeFlags_DrawLinesToNodes; // Spine-style hierarchy lines in the tree
    s.TreeLinesSize = 1.0f;
    s.ScaleAllSizes(scale);
}


// ---- Crash handling -------------------------------------------------------------------------
// On an unhandled exception Osteon writes crash.log and crash.dmp (in %APPDATA%/Osteon), saves a recovery copy of the
// open project, and starts itself again; the new instance offers to restore the copy. The log lists where the crash
// happened as offsets into Osteon.exe, which together with the build's .pdb name the function.
static char g_logPath[1024], g_dmpPath[1024], g_exePath[1024];
static DWORD g_startTick = 0;
static bool g_afterCrash = false;

static void tryEmergencySave() { __try { if (g_editor) g_editor->autosaveNow(); } __except (EXCEPTION_EXECUTE_HANDLER) {} }

static void writeCrashReport(EXCEPTION_POINTERS* ep) {
    char buf[8192];
    int n = 0;
    SYSTEMTIME t;
    GetLocalTime(&t);
    HMODULE self = GetModuleHandleW(nullptr);
    auto* dos = (IMAGE_DOS_HEADER*)self;
    auto* nt = (IMAGE_NT_HEADERS*)((char*)self + dos->e_lfanew);
    DWORD64 base = (DWORD64)self, size = nt->OptionalHeader.SizeOfImage;
    n += snprintf(buf + n, sizeof buf - n, "Osteon crash report\r\nlocal time: %04d-%02d-%02d %02d:%02d:%02d\r\nbuild timestamp: 0x%08lx\r\n",
                  t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, (unsigned long)nt->FileHeader.TimeDateStamp);
    if (ep && ep->ExceptionRecord) {
        const EXCEPTION_RECORD* er = ep->ExceptionRecord;
        DWORD64 addr = (DWORD64)er->ExceptionAddress;
        n += snprintf(buf + n, sizeof buf - n, "exception: 0x%08lx at 0x%llx (Osteon.exe+0x%llx)\r\n", (unsigned long)er->ExceptionCode,
                      (unsigned long long)addr, (unsigned long long)(addr >= base && addr < base + size ? addr - base : 0));
        if (er->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && er->NumberParameters >= 2)
            n += snprintf(buf + n, sizeof buf - n, "access violation: %s address 0x%llx\r\n", er->ExceptionInformation[0] == 0 ? "reading" : er->ExceptionInformation[0] == 1 ? "writing" : "executing",
                          (unsigned long long)er->ExceptionInformation[1]);
        // Values on the stack that point into Osteon.exe (likely return addresses), innermost first.
        if (ep->ContextRecord) {
            n += snprintf(buf + n, sizeof buf - n, "stack (Osteon.exe offsets):\r\n");
            __try {
                const DWORD64* sp = (const DWORD64*)ep->ContextRecord->Rsp;
                int shown = 0;
                for (int i = 0; i < 4096 && shown < 40 && n < (int)sizeof buf - 64; i++) {
                    DWORD64 v = sp[i];
                    if (v >= base && v < base + size) { n += snprintf(buf + n, sizeof buf - n, "  +0x%llx\r\n", (unsigned long long)(v - base)); shown++; }
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
    }
    HANDLE f = CreateFileA(g_logPath, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f != INVALID_HANDLE_VALUE) {
        n += snprintf(buf + n, sizeof buf - n, "----\r\n");
        DWORD w;
        WriteFile(f, buf, (DWORD)n, &w, nullptr);
        CloseHandle(f);
    }
    HANDLE d = CreateFileA(g_dmpPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (d != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mei = {GetCurrentThreadId(), ep, FALSE};
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), d, MiniDumpNormal, ep ? &mei : nullptr, nullptr, nullptr);
        CloseHandle(d);
    }
}

static LONG WINAPI crashFilter(EXCEPTION_POINTERS* ep) {
    static volatile LONG once = 0;
    if (InterlockedExchange(&once, 1)) return EXCEPTION_EXECUTE_HANDLER; // a second fault while handling the first
    writeCrashReport(ep);
    tryEmergencySave();
    // Start again so the recovery copy is offered straight away; not when this already was a restart that died at once.
    if (!(g_afterCrash && GetTickCount() - g_startTick < 15000)) {
        wchar_t cmd[1200];
        wchar_t exe[1024];
        MultiByteToWideChar(CP_UTF8, 0, g_exePath, -1, exe, 1024);
        swprintf(cmd, 1200, L"\"%s\" --after-crash %lu", exe, GetCurrentProcessId());
        STARTUPINFOW si = {sizeof si};
        PROCESS_INFORMATION pi = {};
        if (CreateProcessW(exe, cmd, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) { CloseHandle(pi.hThread); CloseHandle(pi.hProcess); }
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

static void raiseCrash() { RaiseException(0xE06D7363 + 1, EXCEPTION_NONCONTINUABLE, 0, nullptr); } // terminate() / abort() end up here
static void onSignalAbort(int) { raiseCrash(); }

static void installCrashHandler() {
    std::string dir = appDataDir();
    snprintf(g_logPath, sizeof g_logPath, "%s", joinPath(dir, "crash.log").c_str());
    snprintf(g_dmpPath, sizeof g_dmpPath, "%s", joinPath(dir, "crash.dmp").c_str());
    snprintf(g_exePath, sizeof g_exePath, "%s", narrow([]{ wchar_t p[1024]; GetModuleFileNameW(nullptr, p, 1024); return std::wstring(p); }()).c_str());
    g_startTick = GetTickCount();
    SetUnhandledExceptionFilter(crashFilter);
    std::set_terminate(raiseCrash);
    signal(SIGABRT, onSignalAbort);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int) {
    // After a crash Osteon starts itself again: wait for the old process to end, so the single-instance check passes.
    {
        int ac = 0;
        LPWSTR* av = CommandLineToArgvW(GetCommandLineW(), &ac);
        for (int i = 1; i + 1 < ac; i++)
            if (wcscmp(av[i], L"--after-crash") == 0) {
                g_afterCrash = true;
                if (HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, (DWORD)_wtoi(av[i + 1]))) { WaitForSingleObject(h, 8000); CloseHandle(h); }
            }
        LocalFree(av);
    }
    // Single instance: a second launch forwards its file (if any) to the running editor and exits.
    HANDLE instanceMutex = CreateMutexW(nullptr, TRUE, _wgetenv(L"OSTEON_ALLOW_MULTIPLE") ? L"Local\\OsteonEditorSingleInstanceDev" : L"Local\\OsteonEditorSingleInstance"); // lets a test build run beside the editor
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (HWND existing = FindWindowW(L"OsteonEditor", nullptr)) {
            int argc0 = 0;
            LPWSTR* argv0 = CommandLineToArgvW(GetCommandLineW(), &argc0);
            if (argc0 > 1 && argv0[1][0] != L'-') {
                std::string path = narrow(argv0[1]);
                COPYDATASTRUCT cds = {0x05EE, (DWORD)path.size(), (void*)path.data()};
                SendMessageW(existing, WM_COPYDATA, 0, (LPARAM)&cds);
            }
            LocalFree(argv0);
            DWORD pid = 0;
            GetWindowThreadProcessId(existing, &pid);
            AllowSetForegroundWindow(pid);
            PostMessageW(existing, WM_APP + 1, 0, 0);
        }
        return 0;
    }
    installCrashHandler();
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    ImGui_ImplWin32_EnableDpiAwareness();
    float dpi = ImGui_ImplWin32_GetDpiScaleForMonitor(MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY));

    WNDCLASSEXW wc = {sizeof(wc), CS_CLASSDC, wndProc, 0, 0, inst, LoadIconW(inst, L"IDI_APP"), LoadCursor(nullptr, IDC_ARROW),
                      nullptr, nullptr, L"OsteonEditor", nullptr};
    RegisterClassExW(&wc);
    HWND hwnd = CreateWindowExW(WS_EX_ACCEPTFILES, wc.lpszClassName, L"Osteon", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                (int)(1600 * dpi), (int)(950 * dpi), nullptr, nullptr, inst, nullptr);
    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &dark, sizeof dark);
    MARGINS shadow = {0, 0, 0, 1}; // keeps the window shadow although there is no native frame
    DwmExtendFrameIntoClientArea(hwnd, &shadow);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    if (!createDevice(hwnd)) {
        MessageBoxW(nullptr, L"Direct3D 11 is not available on this system.", L"Osteon", MB_ICONERROR);
        return 1;
    }
    // The window stays hidden until the editor knows whether to show the splash (a card-sized window) or the editor itself.

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;
    io.IniFilename = nullptr; // layouts are saved per mode by the editor (layout_setup.ini / layout_animate.ini)
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    float fontSize = 15.0f * dpi;
    ImFontConfig fc;
    fc.OversampleH = 2;
    if (fileExists("C:/Windows/Fonts/segoeui.ttf")) io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segoeui.ttf", fontSize, &fc);
    else io.Fonts->AddFontDefault();
    {
        // Fallback fonts for the scripts Segoe UI lacks (Devanagari, Chinese, Japanese). Glyphs are rasterised on demand, so merging is cheap.
        // The order decides which font draws the shared Han characters: Japanese text first looks in Yu Gothic.
        const Lang stored = storedLanguage();
        std::vector<std::pair<const char*, int>> fallbacks = {{"C:/Windows/Fonts/Nirmala.ttc", 0}};
        if (stored == Lang::Japanese) { fallbacks.push_back({"C:/Windows/Fonts/YuGothM.ttc", 0}); fallbacks.push_back({"C:/Windows/Fonts/msyh.ttc", 0}); }
        else { fallbacks.push_back({"C:/Windows/Fonts/msyh.ttc", 0}); fallbacks.push_back({"C:/Windows/Fonts/YuGothM.ttc", 0}); }
        for (auto& [path, index] : fallbacks) {
            if (!fileExists(path)) continue;
            ImFontConfig mc;
            mc.OversampleH = 2;
            mc.MergeMode = true;
            mc.FontNo = index;
            io.Fonts->AddFontFromFileTTF(path, fontSize, &mc);
        }
    }
    {
        std::string themeText;
        if (readTextFile(joinPath(appDataDir(), "ui_theme.txt"), themeText)) gLightUi = themeText.rfind("light", 0) == 0;
    }
    applyUiTheme(dpi, gLightUi);

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_device, g_ctx);

    Editor editor;
    g_editor = &editor;
    editor.hwnd = hwnd;
    editor.uiScale = dpi;
    if (!editor.gfx.init(g_device, g_ctx)) {
        MessageBoxW(nullptr, L"Failed to initialise the renderer (shader compilation).", L"Osteon", MB_ICONERROR);
        return 1;
    }
    editor.init();
    if (g_afterCrash) editor.splash.enabled = false; // a restart after a crash goes straight to the restore question
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argc > 1 && argv[1][0] != L'-') editor.pendingOpen = narrow(argv[1]);
    LocalFree(argv);
    editor.showHome = editor.homeOnStart && !g_afterCrash && editor.pendingOpen.empty(); // the Home window follows the splash
    if (editor.splash.active()) {
        // Only the splash card is on screen at first: the window is card-sized and centred, then it becomes the full editor window.
        g_splashMode = true;
        editor.splash.fillWindow = true;
        int w = (int)(640 * dpi), h = (int)(640 * dpi * 931.0f / 1756.0f);
        RECT wa;
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
        SetWindowPos(hwnd, nullptr, wa.left + ((wa.right - wa.left) - w) / 2, wa.top + ((wa.bottom - wa.top) - h) / 2, w, h, SWP_NOZORDER | SWP_FRAMECHANGED);
        ShowWindow(hwnd, SW_SHOWNORMAL);
    } else {
        ShowWindow(hwnd, SW_SHOWMAXIMIZED);
    }
    UpdateWindow(hwnd);

    LARGE_INTEGER freq, last, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&last);
    g_step = [&]() {
    if (g_inStep) return;
    g_inStep = true;
    if (g_splashMode && !editor.splash.active()) { g_splashMode = false; ShowWindow(hwnd, SW_SHOWMAXIMIZED); } // splash over: the full window for Home / the editor
    if (g_resizeW && g_resizeH) {
        cleanupRenderTarget();
        g_swap->ResizeBuffers(0, g_resizeW, g_resizeH, DXGI_FORMAT_UNKNOWN, 0);
        g_resizeW = g_resizeH = 0;
        createRenderTarget();
    }
    QueryPerformanceCounter(&now);
    float dt = (float)(now.QuadPart - last.QuadPart) / freq.QuadPart;
    last = now;

    editor.beforeNewFrame();
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    editor.frame(std::min(dt, 0.1f));
    {   // --crash-test: a deliberate crash 25 s in, to try the recovery path (developer use)
        static const bool crashTest = wcsstr(GetCommandLineW(), L"--crash-test") != nullptr;
        if (crashTest && GetTickCount() - g_startTick > 25000) *(volatile int*)nullptr = 1;
    }
    ImGui::Render();
    const float clear[4] = {0.12f, 0.12f, 0.12f, 1};
    g_ctx->OMSetRenderTargets(1, &g_rtv, nullptr);
    g_ctx->ClearRenderTargetView(g_rtv, clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    g_swap->Present(1, 0);
    std::wstring title = L"Osteon - " + widen(editor.doc.projectPath.empty() ? "Untitled" : fileName(editor.doc.projectPath)) +
                         (editor.doc.dirty ? L" *" : L"");
    static std::wstring lastTitle;
    if (title != lastTitle) { SetWindowTextW(hwnd, title.c_str()); lastTitle = title; }
    g_inStep = false;
    };
    bool running = true;
    while (running) {
        if (!g_deferred.empty()) {
            std::vector<DeferredMsg> replay;
            replay.swap(g_deferred);
            g_replaying = true;
            for (auto& m : replay) wndProc(m.hwnd, m.msg, m.wParam, m.lParam);
            g_replaying = false;
        }
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) running = false;
        }
        if (!running) break;
        if (IsIconic(hwnd)) { Sleep(30); continue; }
        g_step();
    }

    g_step = nullptr;
    editor.shutdown();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    cleanupDevice();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    CoUninitialize();
    if (instanceMutex) CloseHandle(instanceMutex);
    return 0;
}
