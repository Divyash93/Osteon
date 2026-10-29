// Win32 + Direct3D 11 host for the Osteon editor.
#include "App.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include <d3d11.h>
#include <dwmapi.h>
#include <shellapi.h>

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

static LRESULT WINAPI wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
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

static void applyTheme(float scale) {
    ImGuiStyle& s = ImGui::GetStyle();
    s = ImGuiStyle();
    ImGui::StyleColorsDark(&s);
    s.WindowRounding = 2; s.FrameRounding = 2; s.TabRounding = 2; s.ScrollbarRounding = 2; s.GrabRounding = 2; s.PopupRounding = 2;
    s.WindowPadding = ImVec2(6, 6); s.FramePadding = ImVec2(6, 3); s.ItemSpacing = ImVec2(6, 4); s.IndentSpacing = 14;
    s.WindowBorderSize = 1; s.FrameBorderSize = 0; s.TabBorderSize = 0;
    ImVec4* c = s.Colors;
    // Neutral dark greys with a blue accent, close to Spine's palette.
    ImVec4 bg(0.165f, 0.165f, 0.165f, 1), panel(0.20f, 0.20f, 0.20f, 1), frame(0.12f, 0.12f, 0.12f, 1);
    ImVec4 accent(0.16f, 0.50f, 0.78f, 1), accentHi(0.22f, 0.58f, 0.86f, 1);
    c[ImGuiCol_WindowBg] = panel;
    c[ImGuiCol_ChildBg] = panel;
    c[ImGuiCol_PopupBg] = ImVec4(0.17f, 0.17f, 0.17f, 0.98f);
    c[ImGuiCol_MenuBarBg] = bg;
    c[ImGuiCol_Border] = ImVec4(0.09f, 0.09f, 0.09f, 1);
    c[ImGuiCol_FrameBg] = frame;
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.16f, 0.16f, 0.16f, 1);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.18f, 0.18f, 0.18f, 1);
    c[ImGuiCol_TitleBg] = bg; c[ImGuiCol_TitleBgActive] = bg; c[ImGuiCol_TitleBgCollapsed] = bg;
    c[ImGuiCol_Button] = ImVec4(0.28f, 0.28f, 0.28f, 1);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.34f, 0.34f, 0.34f, 1);
    c[ImGuiCol_ButtonActive] = accent;
    c[ImGuiCol_Header] = ImVec4(0.16f, 0.45f, 0.70f, 0.85f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1);
    c[ImGuiCol_HeaderActive] = accent;
    c[ImGuiCol_CheckMark] = accentHi;
    c[ImGuiCol_SliderGrab] = accent; c[ImGuiCol_SliderGrabActive] = accentHi;
    c[ImGuiCol_Tab] = bg;
    c[ImGuiCol_TabHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1);
    c[ImGuiCol_TabSelected] = panel;
    c[ImGuiCol_TabSelectedOverline] = accent;
    c[ImGuiCol_TabDimmed] = bg;
    c[ImGuiCol_TabDimmedSelected] = panel;
    c[ImGuiCol_DockingPreview] = ImVec4(0.16f, 0.50f, 0.78f, 0.5f);
    c[ImGuiCol_DockingEmptyBg] = bg;
    c[ImGuiCol_Separator] = ImVec4(0.10f, 0.10f, 0.10f, 1);
    c[ImGuiCol_SeparatorHovered] = accent; c[ImGuiCol_SeparatorActive] = accentHi;
    c[ImGuiCol_ResizeGrip] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TextSelectedBg] = ImVec4(0.16f, 0.50f, 0.78f, 0.5f);
    c[ImGuiCol_NavHighlight] = accent;
    s.ScaleAllSizes(scale);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int) {
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
    static std::string iniPath = joinPath(appDataDir(), "layout.ini");
    io.IniFilename = iniPath.c_str();
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    float fontSize = 15.0f * dpi;
    ImFontConfig fc;
    fc.OversampleH = 2;
    if (fileExists("C:/Windows/Fonts/segoeui.ttf")) io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/segoeui.ttf", fontSize, &fc);
    else io.Fonts->AddFontDefault();
    applyTheme(dpi);

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
    return 0;
}
