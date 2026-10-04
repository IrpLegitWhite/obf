#pragma once

#include <string>
#include <vector>
#include <windows.h>

namespace obf {
    namespace plugin {

        struct PluginInfo {
            std::wstring path;
            std::string  name;
            std::string  description;
            std::string  languages;   // ★ "cpp,c,java" — через запятую
            int          apiVersion;

            HMODULE handle;
            void* fn_can_apply;
            void* fn_apply;
            void* fn_free;
        };

        class PluginLoader {
        public:
            static PluginLoader& instance();

            void loadFromDirectory(const std::wstring& dir);
            bool load(const std::wstring& path);
            void unloadAll();

            const std::vector<PluginInfo>& plugins() const { return plugins_; }

            // ★ Применить все плагины, поддерживающие указанный язык
            void applyForLanguage(const std::string& lang,
                const std::string& code,
                std::string& out,
                const std::vector<char>& enabled);

            // ★ Применить конкретный плагин
            bool applyPlugin(size_t index, const std::string& code, std::string& out);

            // ★ Проверить, поддерживает ли плагин язык
            static bool pluginSupportsLanguage(const PluginInfo& p, const std::string& lang);

        private:
            PluginLoader() = default;
            ~PluginLoader();
            PluginLoader(const PluginLoader&) = delete;
            PluginLoader& operator=(const PluginLoader&) = delete;

            std::vector<PluginInfo> plugins_;
        };

    } // namespace plugin
} // namespace obf