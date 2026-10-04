#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <filesystem>

#include "../include/AppWindow.h"
#include "../include/plugin/PluginLoader.h"
#include "../include/core/Logger.h"

int main() {
    // ★ Устанавливаем CWD = папка с obf.exe
    wchar_t exePath[MAX_PATH] = {};
    ::GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();
    std::filesystem::current_path(exeDir);

    // ★ Теперь "plugins" ищется относительно bin/
    auto& loader = obf::plugin::PluginLoader::instance();
    loader.loadFromDirectory(L"plugins");

    obf::core::Logger::info(
        "Загружено плагинов: " +
        std::to_string(loader.plugins().size()));

    obf::AppWindow app;
    return app.run();
}