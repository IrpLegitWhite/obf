#define WIN32_LEAN_AND_MEAN

#include "../include/AppWindow.h"
#include "../include/gui/FileDialog.h"
#include "../include/gui/LogBuffer.h"
#include "../include/core/Logger.h"
#include "../include/core/File.h"
#include "../include/plugin/PluginLoader.h"
#include "../include/plugin/IPlugin.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "TextEditor.h"

#include <windows.h>
#include <windowsx.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <tchar.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <atomic>
#include <algorithm>

#pragma comment(lib, "dwmapi.lib")

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif

// ============================================================
// D3D11
// ============================================================
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
static HWND                    g_hwnd = nullptr;

// ★ Флаг для корректной остановки
static std::atomic<bool> g_shouldExit{ false };

static void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer) {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }
}
static void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}
static bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT flags = 0;
    D3D_FEATURE_LEVEL level;
    const D3D_FEATURE_LEVEL levels[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        flags, levels, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &level, &g_pd3dDeviceContext);
    if (hr == DXGI_ERROR_UNSUPPORTED)
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
            flags, levels, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &level, &g_pd3dDeviceContext);
    if (FAILED(hr)) return false;
    CreateRenderTarget();
    return true;
}
static void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release();        g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release();        g_pd3dDevice = nullptr; }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    switch (msg) {
    case WM_NCCALCSIZE:
        if (wParam == TRUE) return 0;
        break;

    case WM_SIZE:
        if (g_pd3dDevice && wParam != SIZE_MINIMIZED) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam),
                DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;

    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
        break;

    case WM_CLOSE:
        // ★ Пользователь закрывает окно — ставим флаг и завершаем цикл
        g_shouldExit.store(true);
        ::DestroyWindow(hWnd);
        return 0;

    case WM_NCHITTEST: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        RECT rc; GetWindowRect(hWnd, &rc);
        const int B = 6;
        bool left = pt.x < rc.left + B, right = pt.x > rc.right - B;
        bool top = pt.y < rc.top + B, bottom = pt.y > rc.bottom - B;
        if (top && left)     return HTTOPLEFT;
        if (top && right)    return HTTOPRIGHT;
        if (bottom && left)  return HTBOTTOMLEFT;
        if (bottom && right) return HTBOTTOMRIGHT;
        if (left)   return HTLEFT;
        if (right)  return HTRIGHT;
        if (top)    return HTTOP;
        if (bottom) return HTBOTTOM;
        break;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static int countLines(const std::string& s) {
    if (s.empty()) return 0;
    int n = 1;
    for (char c : s) if (c == '\n') ++n;
    return n;
}

namespace obf {

    using namespace obf::core;
    using namespace obf::gui;

    // ============================================================
    // Цвета
    // ============================================================
    static ImVec4 g_accent = ImVec4(0.49f, 0.36f, 1.00f, 1.00f);
    static ImVec4 g_accentPink = ImVec4(1.00f, 0.36f, 0.62f, 1.00f);
    static ImVec4 g_textMain = ImVec4(0.90f, 0.90f, 0.94f, 1.00f);
    static ImVec4 g_textDim = ImVec4(0.55f, 0.55f, 0.65f, 1.00f);
    static ImVec4 g_danger = ImVec4(0.85f, 0.30f, 0.35f, 1.00f);
    static ImVec4 g_warning = ImVec4(1.00f, 0.75f, 0.35f, 1.00f);
    static ImVec4 g_success = ImVec4(0.35f, 0.85f, 0.50f, 1.00f);

    // ============================================================
    // AppWindow
    // ============================================================
    AppWindow::AppWindow() {}
    AppWindow::~AppWindow() { shutdown(); }

    bool AppWindow::init(void* hInstance) {
        (void)hInstance;
        return true;
    }

    // ------------------------------------------------------------
    // Определение языка
    // ------------------------------------------------------------
    std::string AppWindow::detectLanguage(const std::string& path) {
        auto endsWith = [&](const char* ext) {
            size_t n = std::strlen(ext);
            return path.size() >= n &&
                path.compare(path.size() - n, n, ext) == 0;
            };

        if (endsWith(".cpp") || endsWith(".cc") || endsWith(".cxx") ||
            endsWith(".hpp") || endsWith(".hxx") || endsWith(".h"))  return "cpp";
        if (endsWith(".c"))       return "c";
        if (endsWith(".cs"))      return "csharp";
        if (endsWith(".py"))      return "python";
        if (endsWith(".js"))      return "js";
        if (endsWith(".ts"))      return "typescript";
        if (endsWith(".java"))    return "java";
        if (endsWith(".rs"))      return "rust";
        if (endsWith(".go"))      return "go";
        if (endsWith(".php"))     return "php";
        if (endsWith(".rb"))      return "ruby";
        if (endsWith(".lua"))     return "lua";
        if (endsWith(".pl") || endsWith(".pm")) return "perl";
        if (endsWith(".kt") || endsWith(".kts")) return "kotlin";
        if (endsWith(".swift"))   return "swift";
        if (endsWith(".html") || endsWith(".htm")) return "html";
        if (endsWith(".css"))     return "css";
        if (endsWith(".def") || endsWith(".rc") || endsWith(".idl"))  return "dll";
        return "cpp";
    }

    // ------------------------------------------------------------
    // Тема
    // ------------------------------------------------------------
    void AppWindow::applyTheme() {
        ImGuiStyle& s = ImGui::GetStyle();
        ImVec4* c = s.Colors;

        s.WindowRounding = 8.0f;
        s.ChildRounding = 6.0f;
        s.FrameRounding = 6.0f;
        s.PopupRounding = 6.0f;
        s.ScrollbarRounding = 10.0f;
        s.GrabRounding = 6.0f;
        s.TabRounding = 6.0f;

        s.WindowBorderSize = 1.0f;
        s.ChildBorderSize = 1.0f;
        s.FrameBorderSize = 0.0f;
        s.PopupBorderSize = 1.0f;

        s.WindowPadding = ImVec2(10, 10);
        s.FramePadding = ImVec2(8, 4);
        s.ItemSpacing = ImVec2(6, 5);
        s.ItemInnerSpacing = ImVec2(5, 5);
        s.ScrollbarSize = 11.0f;
        s.GrabMinSize = 9.0f;
        s.IndentSpacing = 16.0f;

        s.WindowTitleAlign = ImVec2(0.0f, 0.5f);
        s.ButtonTextAlign = ImVec2(0.5f, 0.5f);
        s.SelectableTextAlign = ImVec2(0.0f, 0.5f);

        if (m_theme == AppTheme::Dark) {
            g_accent = ImVec4(0.49f, 0.36f, 1.00f, 1.00f);
            g_accentPink = ImVec4(1.00f, 0.36f, 0.62f, 1.00f);
            g_textMain = ImVec4(0.90f, 0.90f, 0.94f, 1.00f);
            g_textDim = ImVec4(0.55f, 0.55f, 0.65f, 1.00f);

            c[ImGuiCol_Text] = g_textMain;
            c[ImGuiCol_TextDisabled] = g_textDim;
            c[ImGuiCol_WindowBg] = ImVec4(0.086f, 0.086f, 0.133f, 0.96f);
            c[ImGuiCol_ChildBg] = ImVec4(0.06f, 0.06f, 0.10f, 1.00f);
            c[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.10f, 0.15f, 0.98f);
            c[ImGuiCol_Border] = ImVec4(0.24f, 0.24f, 0.33f, 0.70f);
            c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
            c[ImGuiCol_FrameBg] = ImVec4(0.13f, 0.13f, 0.20f, 1.00f);
            c[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.20f, 0.30f, 1.00f);
            c[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.20f, 0.45f, 1.00f);
            c[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.08f, 0.13f, 1.00f);
            c[ImGuiCol_TitleBgActive] = ImVec4(0.13f, 0.10f, 0.24f, 1.00f);
            c[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.10f, 0.15f, 1.00f);
            c[ImGuiCol_ScrollbarBg] = ImVec4(0.06f, 0.06f, 0.10f, 1.00f);
            c[ImGuiCol_ScrollbarGrab] = ImVec4(0.24f, 0.24f, 0.33f, 1.00f);
            c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.35f, 0.30f, 0.55f, 1.00f);
            c[ImGuiCol_ScrollbarGrabActive] = g_accent;
            c[ImGuiCol_CheckMark] = g_accent;
            c[ImGuiCol_SliderGrab] = g_accent;
            c[ImGuiCol_SliderGrabActive] = ImVec4(0.60f, 0.48f, 1.00f, 1.00f);
            c[ImGuiCol_Button] = ImVec4(0.20f, 0.18f, 0.32f, 1.00f);
            c[ImGuiCol_ButtonHovered] = ImVec4(0.32f, 0.26f, 0.55f, 1.00f);
            c[ImGuiCol_ButtonActive] = ImVec4(0.42f, 0.32f, 0.75f, 1.00f);
            c[ImGuiCol_Header] = ImVec4(0.24f, 0.20f, 0.42f, 1.00f);
            c[ImGuiCol_HeaderHovered] = ImVec4(0.32f, 0.26f, 0.55f, 1.00f);
            c[ImGuiCol_HeaderActive] = ImVec4(0.42f, 0.32f, 0.75f, 1.00f);
            c[ImGuiCol_Separator] = ImVec4(0.24f, 0.24f, 0.33f, 0.70f);
            c[ImGuiCol_SeparatorHovered] = ImVec4(0.49f, 0.36f, 1.00f, 0.80f);
            c[ImGuiCol_SeparatorActive] = g_accent;
            c[ImGuiCol_ResizeGrip] = ImVec4(0.24f, 0.24f, 0.33f, 0.50f);
            c[ImGuiCol_ResizeGripHovered] = ImVec4(0.49f, 0.36f, 1.00f, 0.80f);
            c[ImGuiCol_ResizeGripActive] = g_accent;
            c[ImGuiCol_Tab] = ImVec4(0.13f, 0.13f, 0.20f, 1.00f);
            c[ImGuiCol_TabHovered] = ImVec4(0.32f, 0.26f, 0.55f, 1.00f);
            c[ImGuiCol_TabActive] = ImVec4(0.24f, 0.20f, 0.42f, 1.00f);
            c[ImGuiCol_NavHighlight] = g_accent;
        }
        else {
            g_accent = ImVec4(0.38f, 0.26f, 0.85f, 1.00f);
            g_accentPink = ImVec4(0.90f, 0.30f, 0.55f, 1.00f);
            g_textMain = ImVec4(0.10f, 0.10f, 0.15f, 1.00f);
            g_textDim = ImVec4(0.45f, 0.45f, 0.52f, 1.00f);

            c[ImGuiCol_Text] = g_textMain;
            c[ImGuiCol_TextDisabled] = g_textDim;
            c[ImGuiCol_WindowBg] = ImVec4(0.97f, 0.97f, 0.99f, 0.98f);
            c[ImGuiCol_ChildBg] = ImVec4(0.94f, 0.94f, 0.97f, 1.00f);
            c[ImGuiCol_PopupBg] = ImVec4(0.99f, 0.99f, 1.00f, 0.98f);
            c[ImGuiCol_Border] = ImVec4(0.80f, 0.80f, 0.86f, 0.90f);
            c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
            c[ImGuiCol_FrameBg] = ImVec4(0.91f, 0.91f, 0.95f, 1.00f);
            c[ImGuiCol_FrameBgHovered] = ImVec4(0.85f, 0.85f, 0.92f, 1.00f);
            c[ImGuiCol_FrameBgActive] = ImVec4(0.78f, 0.75f, 0.92f, 1.00f);
            c[ImGuiCol_TitleBg] = ImVec4(0.90f, 0.90f, 0.94f, 1.00f);
            c[ImGuiCol_TitleBgActive] = ImVec4(0.82f, 0.80f, 0.94f, 1.00f);
            c[ImGuiCol_MenuBarBg] = ImVec4(0.93f, 0.93f, 0.96f, 1.00f);
            c[ImGuiCol_ScrollbarBg] = ImVec4(0.94f, 0.94f, 0.97f, 1.00f);
            c[ImGuiCol_ScrollbarGrab] = ImVec4(0.78f, 0.78f, 0.85f, 1.00f);
            c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.60f, 0.55f, 0.85f, 1.00f);
            c[ImGuiCol_ScrollbarGrabActive] = g_accent;
            c[ImGuiCol_CheckMark] = g_accent;
            c[ImGuiCol_SliderGrab] = g_accent;
            c[ImGuiCol_SliderGrabActive] = ImVec4(0.60f, 0.48f, 1.00f, 1.00f);
            c[ImGuiCol_Button] = ImVec4(0.87f, 0.86f, 0.94f, 1.00f);
            c[ImGuiCol_ButtonHovered] = ImVec4(0.75f, 0.71f, 0.95f, 1.00f);
            c[ImGuiCol_ButtonActive] = ImVec4(0.65f, 0.58f, 0.95f, 1.00f);
            c[ImGuiCol_Header] = ImVec4(0.85f, 0.83f, 0.95f, 1.00f);
            c[ImGuiCol_HeaderHovered] = ImVec4(0.75f, 0.71f, 0.95f, 1.00f);
            c[ImGuiCol_HeaderActive] = ImVec4(0.65f, 0.58f, 0.95f, 1.00f);
            c[ImGuiCol_Separator] = ImVec4(0.80f, 0.80f, 0.86f, 0.90f);
            c[ImGuiCol_SeparatorHovered] = ImVec4(0.49f, 0.36f, 1.00f, 0.70f);
            c[ImGuiCol_SeparatorActive] = g_accent;
            c[ImGuiCol_ResizeGrip] = ImVec4(0.75f, 0.75f, 0.82f, 0.60f);
            c[ImGuiCol_ResizeGripHovered] = ImVec4(0.49f, 0.36f, 1.00f, 0.70f);
            c[ImGuiCol_ResizeGripActive] = g_accent;
            c[ImGuiCol_Tab] = ImVec4(0.90f, 0.90f, 0.94f, 1.00f);
            c[ImGuiCol_TabHovered] = ImVec4(0.75f, 0.71f, 0.95f, 1.00f);
            c[ImGuiCol_TabActive] = ImVec4(0.82f, 0.80f, 0.94f, 1.00f);
            c[ImGuiCol_NavHighlight] = g_accent;
        }
    }

    // ------------------------------------------------------------
    // Шрифты
    // ------------------------------------------------------------
    void AppWindow::applyFonts() {
        ImGuiIO& io = ImGui::GetIO();

        ImFontConfig textCfg;
        textCfg.OversampleH = 2;
        textCfg.OversampleV = 2;
        textCfg.PixelSnapH = true;

        ImFontGlyphRangesBuilder b;
        b.AddRanges(io.Fonts->GetGlyphRangesCyrillic());
        b.AddRanges(io.Fonts->GetGlyphRangesDefault());
        ImVector<ImWchar> ranges;
        b.BuildRanges(&ranges);

        const char* textPaths[] = {
            "assets/fonts/Roboto-Regular.ttf",
            "../assets/fonts/Roboto-Regular.ttf",
            "../../assets/fonts/Roboto-Regular.ttf",
            "../../../assets/fonts/Roboto-Regular.ttf",
            "D:/Minecraft/obf/assets/fonts/Roboto-Regular.ttf",
            "assets/fonts/segoe-ui.ttf",
            "../assets/fonts/segoe-ui.ttf",
            "D:/Minecraft/obf/assets/fonts/segoe-ui.ttf",
            "C:/Windows/Fonts/segoeui.ttf",
        };

        for (const char* p : textPaths) {
            FILE* f = std::fopen(p, "rb");
            if (!f) continue;
            std::fclose(f);
            m_fontText = io.Fonts->AddFontFromFileTTF(p, 15.0f, &textCfg, ranges.Data);
            if (m_fontText) break;
        }
        if (!m_fontText) {
            ImFontConfig def; def.SizePixels = 15.0f;
            m_fontText = io.Fonts->AddFontDefault(&def);
        }

        const char* monoPaths[] = {
            "C:/Windows/Fonts/consola.ttf",
            "C:/Windows/Fonts/consolab.ttf",
        };
        for (const char* p : monoPaths) {
            FILE* f = std::fopen(p, "rb");
            if (!f) continue;
            std::fclose(f);
            m_fontMono = io.Fonts->AddFontFromFileTTF(p, 14.0f, &textCfg, ranges.Data);
            if (m_fontMono) break;
        }

        static const ImWchar iconRanges[] = {
            0x2014, 0x2014,
            0x25A0, 0x25A2,
            0x263C, 0x263C,
            0x263E, 0x263E,
            0x2715, 0x2716,
            0x2795, 0x2795,
            0x2796, 0x2796,
            0,
        };
        ImFontConfig iconCfg;
        iconCfg.OversampleH = 2;
        iconCfg.OversampleV = 2;
        iconCfg.PixelSnapH = true;

        const char* iconPaths[] = {
            "assets/fonts/seguisym.ttf",
            "../assets/fonts/seguisym.ttf",
            "D:/Minecraft/obf/assets/fonts/seguisym.ttf",
            "C:/Windows/Fonts/seguiemj.ttf",
            "C:/Windows/Fonts/seguisym.ttf",
        };

        for (const char* p : iconPaths) {
            FILE* f = std::fopen(p, "rb");
            if (!f) continue;
            std::fclose(f);
            m_fontIcon = io.Fonts->AddFontFromFileTTF(p, 18.0f, &iconCfg, iconRanges);
            if (m_fontIcon) break;
        }

        io.FontDefault = m_fontText;
    }

    void AppWindow::toggleTheme() {
        m_theme = (m_theme == AppTheme::Dark) ? AppTheme::Light : AppTheme::Dark;
        applyTheme();
        Logger::info(m_theme == AppTheme::Dark ? "Theme: dark" : "Theme: light");
    }

    // ------------------------------------------------------------
    // Визуальные помощники
    // ------------------------------------------------------------
    void AppWindow::drawSectionHeader(const char* text) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, g_textDim);
        ImGui::TextUnformatted(text);
        ImGui::PopStyleColor();

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        float w = ImGui::GetContentRegionAvail().x;
        ImU32 accent = ImGui::ColorConvertFloat4ToU32(ImVec4(
            g_accent.x, g_accent.y, g_accent.z, 0.35f));
        dl->AddRectFilledMultiColor(
            ImVec2(p.x, p.y),
            ImVec2(p.x + w, p.y + 1),
            accent,
            IM_COL32(124, 92, 255, 0),
            IM_COL32(124, 92, 255, 0),
            accent);
        ImGui::Dummy(ImVec2(0, 2));
    }

    bool AppWindow::titleBarIconButton(const char* id, const char* glyph,
        const ImVec4& hoverColor) {
        const float sz = 36.0f;
        ImGui::PushID(id);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, hoverColor);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

        if (m_fontIcon) ImGui::PushFont(m_fontIcon);
        bool clicked = ImGui::Button(glyph, ImVec2(sz, sz));
        if (m_fontIcon) ImGui::PopFont();

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);
        ImGui::PopID();
        return clicked;
    }

    static bool PillButton(const char* label, ImVec2 size,
        const ImVec4& base, const ImVec4& hovered,
        const ImVec4& active, bool enabled = true) {
        ImGui::PushStyleColor(ImGuiCol_Button, base);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hovered);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, active);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, size.y * 0.5f);
        ImGui::BeginDisabled(!enabled);
        bool clicked = ImGui::Button(label, size);
        ImGui::EndDisabled();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
        return clicked;
    }

    // ------------------------------------------------------------
    // Title bar
    // ------------------------------------------------------------
    void AppWindow::drawTitleBar() {
        const float barHeight = 40.0f;
        ImGuiIO& io = ImGui::GetIO();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, barHeight));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 2));

        ImVec4 barBg = (m_theme == AppTheme::Dark)
            ? ImVec4(0.086f, 0.086f, 0.133f, 1.0f)
            : ImVec4(0.95f, 0.95f, 0.98f, 1.0f);

        ImGui::PushStyleColor(ImGuiCol_WindowBg, barBg);
        ImGui::Begin("##TitleBar", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);

        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 wp = ImGui::GetWindowPos();
            ImVec2 ws = ImGui::GetWindowSize();
            dl->AddRectFilledMultiColor(
                ImVec2(wp.x, wp.y + ws.y - 2),
                ImVec2(wp.x + ws.x, wp.y + ws.y),
                ImGui::ColorConvertFloat4ToU32(g_accent),
                ImGui::ColorConvertFloat4ToU32(g_accentPink),
                ImGui::ColorConvertFloat4ToU32(g_accentPink),
                ImGui::ColorConvertFloat4ToU32(g_accent));
        }

        const float btnSize = 36.0f;
        const float spacing = 6.0f;
        float totalW = btnSize * 4 + spacing * 3;
        float startX = io.DisplaySize.x - totalW - 10.0f;

        ImGui::SetCursorPosX(startX);
        ImGui::SetCursorPosY((barHeight - btnSize) * 0.5f);

        ImVec4 themeHover = (m_theme == AppTheme::Dark)
            ? ImVec4(0.30f, 0.24f, 0.52f, 1.0f)
            : ImVec4(0.75f, 0.71f, 0.95f, 1.0f);

        const char* themeGlyph = (m_theme == AppTheme::Dark) ? "L" : "D";
        if (titleBarIconButton("theme", themeGlyph, themeHover)) {
            toggleTheme();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle theme");

        ImGui::SameLine(0, spacing);
        if (titleBarIconButton("min", "\xE2\x80\x94", ImVec4(0.85f, 0.75f, 0.30f, 1.0f)))
            ::ShowWindow(g_hwnd, SW_MINIMIZE);

        ImGui::SameLine(0, spacing);
        const char* maxGlyph = m_maximized ? "\xE2\x9D\x92" : "\xE2\x96\xA2";
        if (titleBarIconButton("max", maxGlyph, ImVec4(0.30f, 0.60f, 0.85f, 1.0f))) {
            if (!m_maximized) {
                RECT rc; GetWindowRect(g_hwnd, &rc);
                m_restoreX = rc.left; m_restoreY = rc.top;
                m_restoreW = rc.right - rc.left; m_restoreH = rc.bottom - rc.top;
                MONITORINFO mi = { sizeof(mi) };
                GetMonitorInfo(MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTONEAREST), &mi);
                SetWindowPos(g_hwnd, nullptr,
                    mi.rcMonitor.left, mi.rcMonitor.top,
                    mi.rcMonitor.right - mi.rcMonitor.left,
                    mi.rcMonitor.bottom - mi.rcMonitor.top,
                    SWP_FRAMECHANGED);
                m_maximized = true;
            }
            else {
                SetWindowPos(g_hwnd, nullptr, m_restoreX, m_restoreY,
                    m_restoreW, m_restoreH, SWP_FRAMECHANGED);
                m_maximized = false;
            }
        }

        ImGui::SameLine(0, spacing);
        if (titleBarIconButton("close", "\xE2\x9C\x95", ImVec4(0.85f, 0.20f, 0.25f, 1.0f))) {
            // ★ Правильное закрытие
            g_shouldExit.store(true);
            ::PostMessageW(g_hwnd, WM_CLOSE, 0, 0);
        }

        {
            const char* title = "C++ Obfuscator";
            float tw = ImGui::CalcTextSize(title).x;
            ImGui::SetCursorPosX((io.DisplaySize.x - tw) * 0.5f);
            ImGui::SetCursorPosY((barHeight - ImGui::GetTextLineHeight()) * 0.5f);
            ImGui::TextColored(g_textMain, "%s", title);
        }

        ImGui::SetCursorPos(ImVec2(0, 0));
        ImGui::InvisibleButton("##drag", ImVec2(startX, barHeight));
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            ImVec2 d = io.MouseDelta;
            RECT rc; GetWindowRect(g_hwnd, &rc);
            SetWindowPos(g_hwnd, nullptr, rc.left + (int)d.x, rc.top + (int)d.y,
                0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }

        ImGui::End();
    }

    // ------------------------------------------------------------
    // Верхняя панель
    // ------------------------------------------------------------
    void AppWindow::drawTopPanel() {
        const float TITLE_OFFSET = 42.0f;
        ImGuiIO& io = ImGui::GetIO();

        ImGui::SetNextWindowPos(ImVec2(10, TITLE_OFFSET + 6), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x - 20, 160), ImGuiCond_Always);

        ImGui::Begin("##TopPanel", nullptr, ImGuiWindowFlags_NoCollapse);

        ImGui::TextColored(g_textDim, "Файл:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(io.DisplaySize.x - 620);
        char buf[512] = {};
        std::strncpy(buf, m_inputPath.c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = 0;
        if (ImGui::InputText("##input_path", buf, sizeof(buf))) {
            m_inputPath = buf;
        }

        ImGui::SameLine();
        if (PillButton("Обзор", ImVec2(110, 26),
            ImVec4(0.20f, 0.18f, 0.32f, 1.0f),
            ImVec4(0.32f, 0.26f, 0.55f, 1.0f),
            ImVec4(0.42f, 0.32f, 0.75f, 1.0f))) {
            openFileDialog();
        }

        ImGui::SameLine();
        bool canRun = !m_running && !m_inputPath.empty();
        ImVec4 runBase = m_running
            ? ImVec4(0.35f, 0.30f, 0.20f, 1.0f)
            : ImVec4(0.30f, 0.22f, 0.65f, 1.0f);
        if (PillButton(m_running ? "Работа..." : "Запуск", ImVec2(140, 26),
            runBase,
            ImVec4(0.49f, 0.36f, 1.0f, 1.0f),
            ImVec4(0.60f, 0.48f, 1.0f, 1.0f),
            canRun)) {
            LogBuffer::instance().clear();
            runObfuscatorAsync();
        }

        ImGui::SameLine();
        if (PillButton("Сохранить", ImVec2(130, 26),
            ImVec4(0.20f, 0.30f, 0.25f, 1.0f),
            ImVec4(0.25f, 0.55f, 0.40f, 1.0f),
            ImVec4(0.35f, 0.70f, 0.50f, 1.0f),
            m_hasResult)) {
            saveFileDialog();
        }

        ImGui::SameLine();
        if (PillButton("Копировать", ImVec2(150, 26),
            ImVec4(0.20f, 0.18f, 0.32f, 1.0f),
            ImVec4(0.32f, 0.26f, 0.55f, 1.0f),
            ImVec4(0.42f, 0.32f, 0.75f, 1.0f),
            m_hasResult)) {
            ImGui::SetClipboardText(m_resultText.c_str());
            Logger::info("Скопировано в буфер обмена");
        }

        ImGui::Spacing();

        if (m_language.empty() && !m_inputPath.empty()) {
            m_language = detectLanguage(m_inputPath);
        }

        ImGui::TextColored(g_textDim, "Язык:");
        ImGui::SameLine();
        ImGui::TextColored(g_accent, "%s", m_language.empty() ? "(не выбран)" : m_language.c_str());

        ImGui::SameLine(0, 20);
        ImGui::TextColored(g_textDim, "Фильтр:");
        ImGui::SameLine();

        static const char* langItems[] = {
            "auto", "cpp", "c", "csharp", "dll", "python", "js", "typescript",
            "java", "rust", "go", "php", "ruby", "lua", "perl",
            "kotlin", "swift", "html", "css"
        };
        static int langIdx = 0;

        if (langIdx == 0 && !m_language.empty()) {
            for (int k = 1; k < IM_ARRAYSIZE(langItems); ++k) {
                if (m_language == langItems[k]) { langIdx = k; break; }
            }
        }

        ImGui::SetNextItemWidth(150);
        ImGui::Combo("##lang_filter", &langIdx, langItems, IM_ARRAYSIZE(langItems));

        ImGui::SameLine();
        if (ImGui::SmallButton("auto")) langIdx = 0;

        std::string filterLang = (langIdx == 0) ? m_language : langItems[langIdx];
        if (filterLang.empty()) filterLang = "cpp";

        ImGui::SameLine(0, 20);
        ImGui::TextColored(g_textDim, "Плагины (%s):", filterLang.c_str());

        auto& loader = plugin::PluginLoader::instance();
        const auto& plugins = loader.plugins();

        if (plugins.empty()) {
            ImGui::Spacing();
            ImGui::TextColored(g_warning, "Нет загруженных плагинов");
            ImGui::End();
            return;
        }
        if (m_pluginEnabled.size() != plugins.size()) {
            m_pluginEnabled.resize(plugins.size(), 1);
        }

        ImGui::Spacing();

        int visibleCount = 0;
        int col = 0;
        for (size_t i = 0; i < plugins.size(); ++i) {
            const auto& p = plugins[i];

            bool langOk = plugin::PluginLoader::pluginSupportsLanguage(p, filterLang);
            if (!langOk) continue;

            ImGui::PushID((int)i);

            if (col > 0 && col % 5 != 0) {
                ImGui::SameLine();
            }

            bool enabled = (m_pluginEnabled[i] != 0);
            if (ImGui::Checkbox(p.name.c_str(), &enabled)) {
                m_pluginEnabled[i] = enabled ? 1 : 0;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s\nlanguages: %s",
                    p.description.c_str(), p.languages.c_str());
            }

            ImGui::PopID();
            ++col;
            ++visibleCount;
        }

        if (visibleCount == 0) {
            ImGui::TextColored(g_warning, "Нет плагинов для языка '%s'", filterLang.c_str());
        }
        else {
            ImGui::SameLine(0, 20);
            ImGui::TextColored(g_textDim, "(%d)", visibleCount);
        }

        ImGui::Spacing();
        if (ImGui::SmallButton("Включить все")) {
            for (size_t i = 0; i < plugins.size(); ++i) {
                if (plugin::PluginLoader::pluginSupportsLanguage(plugins[i], filterLang)) {
                    if (i < m_pluginEnabled.size()) m_pluginEnabled[i] = 1;
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Выключить все")) {
            for (size_t i = 0; i < plugins.size(); ++i) {
                if (plugin::PluginLoader::pluginSupportsLanguage(plugins[i], filterLang)) {
                    if (i < m_pluginEnabled.size()) m_pluginEnabled[i] = 0;
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Только видимые вкл")) {
            for (size_t i = 0; i < m_pluginEnabled.size(); ++i) m_pluginEnabled[i] = 0;
            for (size_t i = 0; i < plugins.size(); ++i) {
                if (plugin::PluginLoader::pluginSupportsLanguage(plugins[i], filterLang)) {
                    if (i < m_pluginEnabled.size()) m_pluginEnabled[i] = 1;
                }
            }
        }

        ImGui::End();
    }

    // ------------------------------------------------------------
    // Редактор
    // ------------------------------------------------------------
    void AppWindow::drawEditorPanel() {
        const float TITLE_OFFSET = 42.0f;
        const float TOP_PANEL_H = 166.0f;
        const float LOG_PANEL_H = 160.0f;
        ImGuiIO& io = ImGui::GetIO();

        float y = TITLE_OFFSET + TOP_PANEL_H + 6.0f;
        float h = io.DisplaySize.y - y - LOG_PANEL_H - 6.0f;
        if (h < 50.0f) h = 50.0f;

        ImGui::SetNextWindowPos(ImVec2(10, y), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x - 20, h), ImGuiCond_Always);

        ImGui::Begin("##Editor", nullptr, ImGuiWindowFlags_NoCollapse);

        ImGui::TextColored(g_textDim, "Результат:");
        ImGui::SameLine();
        ImGui::TextColored(g_textDim, "(%d строк)", countLines(m_resultText));
        ImGui::SameLine(0, 30);
        ImGui::TextColored(g_textDim, "Язык: %s", m_language.c_str());
        ImGui::Separator();

        if (m_hasResult && !m_resultText.empty()) {
            static size_t lastHash = 0;
            static AppTheme lastTheme = AppTheme::Dark;
            size_t hh = std::hash<std::string>{}(m_resultText);
            if (hh != lastHash || lastTheme != m_theme) {
                m_editorRight.SetText(m_resultText);

                if (m_theme == AppTheme::Dark)
                    m_editorRight.SetPalette(TextEditor::GetDarkPalette());
                else
                    m_editorRight.SetPalette(TextEditor::GetLightPalette());

                if (m_language == "cpp" || m_language == "c" || m_language == "dll")
                    m_editorRight.SetLanguage(TextEditor::Language::Cpp());
                else if (m_language == "csharp")
                    m_editorRight.SetLanguage(TextEditor::Language::Cs());
                else if (m_language == "python")
                    m_editorRight.SetLanguage(TextEditor::Language::Python());
                else if (m_language == "js" || m_language == "typescript")
                    m_editorRight.SetLanguage(TextEditor::Language::Json());
                else if (m_language == "lua")
                    m_editorRight.SetLanguage(TextEditor::Language::Lua());
                else if (m_language == "sql")
                    m_editorRight.SetLanguage(TextEditor::Language::Sql());
                else
                    m_editorRight.SetLanguage(TextEditor::Language::Cpp());

                lastHash = hh;
                lastTheme = m_theme;
            }

            ImVec2 avail = ImGui::GetContentRegionAvail();
            if (avail.x > 10.0f && avail.y > 10.0f) {
                m_editorRight.Render("##result", avail, false);
            }
        }
        else if (m_running) {
            ImGui::TextColored(g_textDim, "Обработка...");
        }
        else {
            ImGui::TextColored(g_textDim, "Здесь появится результат");
        }

        ImGui::End();
    }

    // ------------------------------------------------------------
    // Лог
    // ------------------------------------------------------------
    void AppWindow::drawLogPanel() {
        ImGuiIO& io = ImGui::GetIO();
        const float LOG_PANEL_H = 160.0f;

        ImGui::SetNextWindowPos(ImVec2(10, io.DisplaySize.y - LOG_PANEL_H - 6),
            ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x - 20, LOG_PANEL_H),
            ImGuiCond_Always);

        ImGui::Begin("##Log", nullptr, ImGuiWindowFlags_NoCollapse);

        if (PillButton("Очистить", ImVec2(110, 22),
            ImVec4(0.20f, 0.18f, 0.32f, 1.0f),
            ImVec4(0.32f, 0.26f, 0.55f, 1.0f),
            ImVec4(0.42f, 0.32f, 0.75f, 1.0f))) {
            LogBuffer::instance().clear();
            m_logLines.clear();
        }
        ImGui::SameLine();
        ImGui::Checkbox("Автопрокрутка", &m_autoScroll);

        ImGui::SameLine(0, 20);
        ImGui::TextColored(g_textDim, "%d строк", (int)m_logLines.size());

        ImGui::Dummy(ImVec2(0, 2));

        {
            auto snap = LogBuffer::instance().snapshot();
            if (snap.size() != m_logLines.size()) {
                m_logLines.clear();
                for (const auto& s : snap) m_logLines.push_back(s);
                m_logDirty = true;
            }
        }

        ImVec4 childBg = (m_theme == AppTheme::Dark)
            ? ImVec4(0.05f, 0.05f, 0.09f, 1.0f)
            : ImVec4(0.96f, 0.96f, 0.98f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, childBg);
        ImGui::BeginChild("##log_scroll", ImVec2(0, 0), true,
            ImGuiWindowFlags_HorizontalScrollbar);

        for (const auto& line : m_logLines) {
            ImVec4 col = g_textMain;
            if (line.find("[ERROR]") != std::string::npos) col = g_danger;
            else if (line.find("[WARN]") != std::string::npos) col = g_warning;
            else if (line.find("[DEBUG]") != std::string::npos) col = g_textDim;
            else if (line.find("[INFO]") != std::string::npos) col = g_success;

            ImGui::PushStyleColor(ImGuiCol_Text, col);
            ImGui::TextUnformatted(line.c_str());
            ImGui::PopStyleColor();
        }

        if (m_autoScroll && m_logDirty) {
            ImGui::SetScrollHereY(1.0f);
            m_logDirty = false;
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::End();
    }

    // ------------------------------------------------------------
    // Actions
    // ------------------------------------------------------------
    void AppWindow::openFileDialog() {
        std::string f = FileDialog::openFile(L"Выберите файл",
            L"All source files\0"
            L"*.cpp;*.h;*.hpp;*.cc;*.cxx;*.c;*.cs;*.py;*.js;*.ts;*.java;*.rs;*.go;*.php;*.rb;*.lua;*.pl;*.pm;*.kt;*.kts;*.swift;*.html;*.htm;*.css;*.def;*.rc;*.idl\0"
            L"All\0*.*\0");

        if (!f.empty()) {
            m_inputPath = f;
            try {
                m_sourceText = File::read(m_inputPath);
                m_language = detectLanguage(m_inputPath);
                Logger::info("Загружен: " + m_inputPath);
                Logger::info("Язык: " + m_language);
            }
            catch (const std::exception& ex) {
                Logger::error(std::string("Ошибка: ") + ex.what());
            }
        }
    }

    void AppWindow::saveFileDialog() {
        std::string f = FileDialog::saveFile(L"Сохранить результат");
        if (!f.empty()) {
            try {
                File::write(f, m_resultText);
                Logger::info("Сохранено: " + f);
            }
            catch (const std::exception& ex) {
                Logger::error(std::string("Ошибка: ") + ex.what());
            }
        }
    }

    void AppWindow::runObfuscatorAsync() {
        if (m_running) return;
        if (m_inputPath.empty()) { Logger::error("Файл не выбран"); return; }
        if (m_worker.joinable()) m_worker.join();

        m_running = true;
        m_hasResult = false;
        m_resultText.clear();

        m_worker = std::thread([this]() {
            try {
                std::string code = File::read(m_inputPath);
                Logger::info("Файл загружен: " + std::to_string(code.size()) + " байт");
                Logger::info("Язык: " + m_language);

                auto& loader = plugin::PluginLoader::instance();
                const auto& plugins = loader.plugins();
                Logger::info("Плагинов загружено: " + std::to_string(plugins.size()));

                if (plugins.empty()) {
                    Logger::error("НЕТ ЗАГРУЖЕННЫХ ПЛАГИНОВ!");
                    m_resultText = code;
                    m_hasResult = true;
                    m_running = false;
                    return;
                }

                int appliedCount = 0;
                std::string current = code;

                for (size_t i = 0; i < plugins.size(); ++i) {
                    // ★ Проверка на выход
                    if (g_shouldExit.load()) {
                        Logger::info("Прерывание — закрытие приложения");
                        m_running = false;
                        return;
                    }

                    const auto& p = plugins[i];

                    Logger::info("── [" + std::to_string(i) + "] " + p.name);
                    Logger::info("      languages = '" + p.languages + "'");

                    bool enabled = (i < m_pluginEnabled.size()) && (m_pluginEnabled[i] != 0);
                    Logger::info("      enabled   = " + std::string(enabled ? "yes" : "no"));
                    if (!enabled) { Logger::info("      → SKIP (unchecked)"); continue; }

                    bool langOk = plugin::PluginLoader::pluginSupportsLanguage(p, m_language);
                    Logger::info("      langOk    = " + std::string(langOk ? "yes" : "no"));
                    if (!langOk) { Logger::info("      → SKIP (lang)"); continue; }

                    auto fn_can_apply = reinterpret_cast<obf_can_apply_fn>(p.fn_can_apply);
                    int canApply = fn_can_apply(current.c_str());
                    Logger::info("      canApply  = " + std::to_string(canApply));
                    if (!canApply) { Logger::info("      → SKIP (can_apply=0)"); continue; }

                    Logger::info("      → APPLYING...");
                    std::string out;
                    if (loader.applyPlugin(i, current, out)) {
                        Logger::info("      ✓ SUCCESS: " +
                            std::to_string(current.size()) + " → " +
                            std::to_string(out.size()) + " байт");
                        current = out;
                        ++appliedCount;
                    }
                    else {
                        Logger::info("      ✗ applyPlugin вернул false");
                    }
                }

                Logger::info("Применено плагинов: " + std::to_string(appliedCount));

                m_resultText = current;
                m_hasResult = true;
                Logger::info("Готово. Результат: " + std::to_string(current.size()) + " байт");
            }
            catch (const std::exception& ex) {
                Logger::error(std::string("Исключение: ") + ex.what());
            }
            catch (...) {
                Logger::error("Неизвестное исключение");
            }
            m_running = false;
            });
    }

    // ------------------------------------------------------------
    // Main loop
    // ------------------------------------------------------------
    void AppWindow::drawMainUI() {
        drawTopPanel();
        drawEditorPanel();
        drawLogPanel();
    }

    void AppWindow::mainLoop() {
        bool done = false;
        while (!done) {
            // ★ Проверяем флаг выхода
            if (g_shouldExit.load()) {
                done = true;
                break;
            }

            MSG msg;
            while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
                ::TranslateMessage(&msg);
                ::DispatchMessage(&msg);
                if (msg.message == WM_QUIT) done = true;
            }
            if (done) break;

            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            drawTitleBar();
            drawMainUI();

            ImGui::Render();

            ImVec4 bg = (m_theme == AppTheme::Dark)
                ? ImVec4(0.051f, 0.051f, 0.078f, 1.0f)
                : ImVec4(0.92f, 0.92f, 0.95f, 1.0f);

            const float clear[4] = { bg.x, bg.y, bg.z, 1.0f };

            // ★ Проверка: окно ещё живо?
            if (g_mainRenderTargetView && g_pd3dDeviceContext && g_pSwapChain) {
                g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
                g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear);
                ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
                g_pSwapChain->Present(1, 0);
            }
        }
    }

    // ------------------------------------------------------------
    // Init
    // ------------------------------------------------------------
    bool AppWindow::initWin32(void* hInstance) {
        (void)hInstance;

        WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L,
                           GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr,
                           L"obf_gui", nullptr };
        ::RegisterClassExW(&wc);

        int sw = GetSystemMetrics(SM_CXSCREEN);
        int sh = GetSystemMetrics(SM_CYSCREEN);

        const int ww = 1360;
        const int wh = 760;
        int wx = (sw - ww) / 2;
        int wy = (sh - wh) / 2;

        HWND hwnd = ::CreateWindowExW(
            0, wc.lpszClassName, L"",
            WS_POPUP | WS_VISIBLE | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
            wx, wy, ww, wh,
            nullptr, nullptr, wc.hInstance, nullptr);

        if (!hwnd) return false;
        g_hwnd = hwnd;
        m_hwnd = hwnd;

        int corner = DWMWCP_ROUND;
        DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));

        if (!CreateDeviceD3D(hwnd)) {
            CleanupDeviceD3D();
            ::DestroyWindow(hwnd);
            ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
            return false;
        }

        ::ShowWindow(hwnd, SW_SHOWDEFAULT);
        ::UpdateWindow(hwnd);
        return true;
    }

    bool AppWindow::initImGui() {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.IniFilename = nullptr;

        applyTheme();
        applyFonts();

        ImGui_ImplWin32_Init(g_hwnd);
        ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

        m_pluginEnabled.resize(plugin::PluginLoader::instance().plugins().size(), 1);
        return true;
    }

    int AppWindow::run() {
        if (!initWin32(nullptr)) return 1;
        if (!initImGui()) { shutdown(); return 2; }

        Logger::info("C++ Obfuscator запущен");
        mainLoop();
        shutdown();
        Logger::info("Готово");
        return 0;
    }

    // ------------------------------------------------------------
    // Shutdown — правильный порядок!
    // ------------------------------------------------------------
    void AppWindow::shutdown() {
        // ★ 1. Сначала — сигнал потоку "остановись"
        g_shouldExit.store(true);

        // ★ 2. Ждём завершения потока
        if (m_worker.joinable()) {
            m_worker.join();
        }

        // ★ 3. Теперь всё остальное
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        CleanupDeviceD3D();

        if (g_hwnd) {
            ::DestroyWindow(g_hwnd);
            ::UnregisterClassW(L"obf_gui", GetModuleHandle(nullptr));
            g_hwnd = nullptr;
        }
    }

} // namespace obf