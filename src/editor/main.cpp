// Win32 + Direct3D 11 host for the Osteon editor.
#include "App.h"
#include "Icons.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include <d3d11.h>
#include <dwmapi.h>
#include <shellapi.h>
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
        return 0;
    case WM_GETMINMAXINFO:
        ((MINMAXINFO*)lParam)->ptMinTrackSize = {800, 500};
        return 0;
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
        panel = {0.196f, 0.196f, 0.196f, 1}; tree = {0.160f, 0.160f, 0.160f, 1}; strip = {0.130f, 0.130f, 0.130f, 1}; menu = {0.150f, 0.150f, 0.150f, 1};
        frame = {0.118f, 0.118f, 0.118f, 1}; border = {0, 0, 0, 0.45f}; raised = {0.262f, 0.262f, 0.262f, 1}; raisedHi = {0.330f, 0.330f, 0.330f, 1};
        text = {0.820f, 0.820f, 0.820f, 1}; textDim = {0.500f, 0.500f, 0.500f, 1}; popup = {0.170f, 0.170f, 0.170f, 0.98f};
        headerSel = accent; headerHover = {0.270f, 0.270f, 0.270f, 1}; tabHover = {0.240f, 0.240f, 0.240f, 1};
        grab = {0.320f, 0.320f, 0.320f, 1}; grabHover = {0.420f, 0.420f, 0.420f, 1}; frameHover = {0.150f, 0.150f, 0.150f, 1}; frameActive = {0.180f, 0.180f, 0.180f, 1};
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
    c[ImGuiCol_ButtonActive] = accent;
    c[ImGuiCol_Header] = headerSel;
    c[ImGuiCol_HeaderHovered] = headerHover;
    c[ImGuiCol_HeaderActive] = light ? ImVec4(0.540f, 0.690f, 0.940f, 1) : accentHi;
    c[ImGuiCol_CheckMark] = light ? ImVec4(0.071f, 0.302f, 0.678f, 1) : ImVec4(1, 1, 1, 1);
    c[ImGuiCol_SliderGrab] = accent; c[ImGuiCol_SliderGrabActive] = accentHi;
    c[ImGuiCol_Tab] = strip;
    c[ImGuiCol_TabHovered] = tabHover;
    c[ImGuiCol_TabSelected] = panel;
    c[ImGuiCol_TabSelectedOverline] = accent;
    c[ImGuiCol_TabDimmed] = strip;
    c[ImGuiCol_TabDimmedSelected] = panel;
    c[ImGuiCol_DockingPreview] = ImVec4(accent.x, accent.y, accent.z, 0.5f);
    c[ImGuiCol_DockingEmptyBg] = strip;
    c[ImGuiCol_Separator] = border;
    c[ImGuiCol_SeparatorHovered] = accent; c[ImGuiCol_SeparatorActive] = accentHi;
    c[ImGuiCol_TableBorderStrong] = border; c[ImGuiCol_TableBorderLight] = border;
    c[ImGuiCol_TableHeaderBg] = strip;
    c[ImGuiCol_ResizeGrip] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TextSelectedBg] = ImVec4(accent.x, accent.y, accent.z, 0.45f);
    c[ImGuiCol_NavHighlight] = accent;
    s.TreeLinesFlags = ImGuiTreeNodeFlags_DrawLinesToNodes; // Spine-style hierarchy lines in the tree
    s.TreeLinesSize = 1.0f;
    s.ScaleAllSizes(scale);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int) {
    // Single instance: a second launch forwards its file (if any) to the running editor and exits.
    HANDLE instanceMutex = CreateMutexW(nullptr, TRUE, L"Local\\OsteonEditorSingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (HWND existing = FindWindowW(L"OsteonEditor", nullptr)) {
            int argc0 = 0;
            LPWSTR* argv0 = CommandLineToArgvW(GetCommandLineW(), &argc0);
            if (argc0 > 1) {
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
    if (!createDevice(hwnd)) {
        MessageBoxW(nullptr, L"Direct3D 11 is not available on this system.", L"Osteon", MB_ICONERROR);
        return 1;
    }
    ShowWindow(hwnd, SW_SHOWMAXIMIZED);
    UpdateWindow(hwnd);

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
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argc > 1) editor.pendingOpen = narrow(argv[1]);
    LocalFree(argv);

    LARGE_INTEGER freq, last, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&last);
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
    }

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
