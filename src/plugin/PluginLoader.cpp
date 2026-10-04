#include "../../include/plugin/PluginLoader.h"
#include "../../include/plugin/IPlugin.h"
#include "../../include/core/Logger.h"

#include <filesystem>
#include <algorithm>
#include <cwctype>
#include <sstream>

namespace obf {
    namespace plugin {

        using namespace obf::core;

        // ============================================================
        // Singleton
        // ============================================================
        PluginLoader& PluginLoader::instance() {
            static PluginLoader inst;
            return inst;
        }

        PluginLoader::~PluginLoader() {
            unloadAll();
        }

        // ============================================================
        // Загрузка из папки
        // ============================================================
        void PluginLoader::loadFromDirectory(const std::wstring& dir) {
            namespace fs = std::filesystem;

            if (!fs::exists(dir)) {
                Logger::warn("Plugin directory does not exist: " +
                    std::string(dir.begin(), dir.end()));
                return;
            }

            int loaded = 0;
            for (const auto& entry : fs::directory_iterator(dir)) {
                if (!entry.is_regular_file()) continue;

                auto ext = entry.path().extension().wstring();
                std::transform(ext.begin(), ext.end(), ext.begin(),
                    [](wchar_t c) { return (wchar_t)::towlower(c); });

                if (ext == L".dll") {
                    if (load(entry.path().wstring())) {
                        ++loaded;
                    }
                }
            }

            Logger::info("Loaded " + std::to_string(loaded) + " plugins from directory");
        }

        // ============================================================
        // Загрузка одного плагина
        // ============================================================
        bool PluginLoader::load(const std::wstring& path) {
            HMODULE h = ::LoadLibraryW(path.c_str());
            if (!h) {
                Logger::error("LoadLibraryW failed for plugin");
                return false;
            }

            // ─── Резолвим все экспорты ───────────────────────────────
            auto fn_name = (obf_name_fn)        ::GetProcAddress(h, "obf_name");
            auto fn_description = (obf_description_fn) ::GetProcAddress(h, "obf_description");
            auto fn_api = (obf_api_version_fn) ::GetProcAddress(h, "obf_api_version");
            auto fn_languages = (obf_languages_fn)   ::GetProcAddress(h, "obf_languages");
            auto fn_can_apply = (obf_can_apply_fn)   ::GetProcAddress(h, "obf_can_apply");
            auto fn_apply = (obf_apply_fn)       ::GetProcAddress(h, "obf_apply");
            auto fn_free = (obf_free_fn)        ::GetProcAddress(h, "obf_free");

            if (!fn_name || !fn_description || !fn_api || !fn_languages ||
                !fn_can_apply || !fn_apply || !fn_free) {
                Logger::error("Plugin missing required exports");
                ::FreeLibrary(h);
                return false;
            }

            // ─── Проверка версии API ─────────────────────────────────
            int apiVer = fn_api();
            if (apiVer != OBFS_PLUGIN_API_VERSION) {
                Logger::error("Plugin API mismatch: got " + std::to_string(apiVer) +
                    ", expected " + std::to_string(OBFS_PLUGIN_API_VERSION));
                ::FreeLibrary(h);
                return false;
            }

            // ─── Заполняем Info ──────────────────────────────────────
            PluginInfo info;
            info.path = path;
            info.name = fn_name();
            info.description = fn_description();
            info.languages = fn_languages();
            info.apiVersion = apiVer;
            info.handle = h;
            info.fn_can_apply = reinterpret_cast<void*>(fn_can_apply);
            info.fn_apply = reinterpret_cast<void*>(fn_apply);
            info.fn_free = reinterpret_cast<void*>(fn_free);

            plugins_.push_back(info);

            Logger::info("Plugin loaded: " + info.name +
                " [" + info.languages + "] — " + info.description);
            return true;
        }

        // ============================================================
        // Выгрузка всех
        // ============================================================
        void PluginLoader::unloadAll() {
            for (auto& p : plugins_) {
                if (p.handle) {
                    ::FreeLibrary(p.handle);
                    p.handle = nullptr;
                }
            }
            plugins_.clear();
        }

        // ============================================================
        // ★ Проверка: поддерживает ли плагин язык
        //   Точное сравнение по токенам через запятую.
        // ============================================================
        bool PluginLoader::pluginSupportsLanguage(const PluginInfo& p,
            const std::string& lang) {
            if (p.languages == "*") return true;
            if (p.languages.empty()) return false;

            std::string token;
            auto flush = [&]() -> bool {
                // trim
                size_t s = token.find_first_not_of(" \t\r\n");
                size_t e = token.find_last_not_of(" \t\r\n");
                if (s == std::string::npos) {
                    token.clear();
                    return false;
                }
                std::string t = token.substr(s, e - s + 1);
                bool match = (t == lang);
                token.clear();
                return match;
                };

            for (char c : p.languages) {
                if (c == ',') {
                    if (flush()) return true;
                }
                else {
                    token += c;
                }
            }
            if (flush()) return true;

            return false;
        }

        // ============================================================
        // Применить один плагин
        // ============================================================
        bool PluginLoader::applyPlugin(size_t index, const std::string& code,
            std::string& out) {
            if (index >= plugins_.size()) return false;

            auto& p = plugins_[index];
            auto fn_can_apply = reinterpret_cast<obf_can_apply_fn>(p.fn_can_apply);
            auto fn_apply = reinterpret_cast<obf_apply_fn>(p.fn_apply);
            auto fn_free = reinterpret_cast<obf_free_fn>(p.fn_free);

            if (!fn_can_apply(code.c_str())) {
                out = code;
                return false;
            }

            size_t outSize = 0;
            char* result = fn_apply(code.c_str(), code.size(), &outSize);
            if (!result) {
                out = code;
                return false;
            }

            out.assign(result, outSize);
            fn_free(result);
            return true;
        }

        // ============================================================
        // Применить все плагины для языка
        // ============================================================
        void PluginLoader::applyForLanguage(const std::string& lang,
            const std::string& code,
            std::string& out,
            const std::vector<char>& enabled) {
            out = code;

            for (size_t i = 0; i < plugins_.size(); ++i) {
                if (i < enabled.size() && !enabled[i]) continue;
                if (!pluginSupportsLanguage(plugins_[i], lang)) continue;

                std::string r;
                if (applyPlugin(i, out, r)) {
                    Logger::info("  Applied [" + lang + "]: " + plugins_[i].name);
                    out = r;
                }
                else {
                    Logger::debug("  No change [" + lang + "]: " + plugins_[i].name);
                }
            }
        }

    } // namespace plugin
} // namespace obf