#pragma once

#include "imgui.h"
#include "TextEditor.h"

#include <string>
#include <thread>
#include <vector>
#include <deque>

namespace obf {

    enum class AppTheme { Dark, Light };

    class AppWindow {
    public:
        AppWindow();
        ~AppWindow();

        bool init(void* hInstance);
        int  run();

    private:
        // Lifecycle
        bool initWin32(void* hInstance);
        bool initImGui();
        void shutdown();
        void mainLoop();

        // Панели
        void drawTitleBar();
        void drawMainUI();
        void drawTopPanel();
        void drawEditorPanel();
        void drawLogPanel();

        // Тема / шрифты
        void applyTheme();
        void applyFonts();
        void toggleTheme();

        // Действия
        void runObfuscatorAsync();
        void openFileDialog();
        void saveFileDialog();

        // Утилиты
        std::string detectLanguage(const std::string& path);

        // Визуальные помощники
        void drawSectionHeader(const char* text);
        bool titleBarIconButton(const char* id, const char* glyph,
            const ImVec4& hoverColor);

        // State
        std::string   m_inputPath;
        std::string   m_sourceText;
        std::string   m_resultText;
        std::string   m_language = "cpp";
        bool          m_hasResult = false;
        bool          m_running = false;
        bool          m_verbose = true;   // ★ по умолчанию вкл
        AppTheme      m_theme = AppTheme::Dark;

        // Window
        void* m_hwnd = nullptr;
        bool          m_maximized = false;
        int           m_restoreX = 0;
        int           m_restoreY = 0;
        int           m_restoreW = 0;
        int           m_restoreH = 0;

        // Worker
        std::thread   m_worker;
        TextEditor    m_editorRight;

        // Плагины
        std::vector<char> m_pluginEnabled;

        // Шрифты
        ImFont* m_fontText = nullptr;
        ImFont* m_fontIcon = nullptr;
        ImFont* m_fontMono = nullptr;

        // Лог
        std::deque<std::string> m_logLines;
        bool                    m_logDirty = false;
        bool                    m_autoScroll = true;
    };

} // namespace obf