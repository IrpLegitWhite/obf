#include "../../../include/plugin/IPlugin.h"

#include <string>
#include <cstdlib>
#include <cstring>

extern "C" __declspec(dllexport) const char* obf_name() {
    return "stripcomments";
}

extern "C" __declspec(dllexport) const char* obf_description() {
    return "Strip // /* */ # -- comments (all languages)";
}

extern "C" __declspec(dllexport) int obf_api_version() {
    return OBFS_PLUGIN_API_VERSION;
}

extern "C" __declspec(dllexport) const char* obf_languages() {
    return "*";   // ★ для ВСЕХ языков
}

extern "C" __declspec(dllexport) int obf_can_apply(const char* code) {
    if (!code) return 0;
    // Есть ли что удалять
    return (std::strstr(code, "//") ||
        std::strstr(code, "/*") ||
        std::strstr(code, "#") ||
        std::strstr(code, "--")) ? 1 : 0;
}

// ============================================================
// Удаление комментариев
// С уважением к строкам "..." и '...'
// ============================================================
extern "C" __declspec(dllexport) char* obf_apply(const char* code, size_t size,
    size_t* out_size) {
    if (!code || !out_size) return nullptr;

    std::string in(code, size);
    std::string out;
    out.reserve(in.size());

    size_t i = 0;
    bool inStr = false;   // внутри "..." или '...'
    bool inLine = false;   // внутри //-комментария
    bool inBlock = false;   // внутри /* */
    bool inHash = false;   // внутри #-комментария (Python)
    bool inDash = false;   // внутри -- комментария (Lua/SQL)
    char strChar = 0;

    while (i < in.size()) {
        char c = in[i];
        char n = (i + 1 < in.size()) ? in[i + 1] : '\0';

        // ─── Внутри строки ──────────────────────────────────
        if (inStr) {
            out += c;
            if (c == '\\' && i + 1 < in.size()) {
                out += in[i + 1];
                i += 2;
                continue;
            }
            if (c == strChar) inStr = false;
            ++i;
            continue;
        }

        // ─── Внутри // комментария ──────────────────────────
        if (inLine) {
            if (c == '\n') {
                out += '\n';       // оставляем перевод строки
                inLine = false;
            }
            ++i;
            continue;
        }

        // ─── Внутри /* */ комментария ───────────────────────
        if (inBlock) {
            if (c == '*' && n == '/') {
                inBlock = false;
                i += 2;
                continue;
            }
            // Сохраняем переводы строк для сохранения нумерации
            if (c == '\n') out += '\n';
            ++i;
            continue;
        }

        // ─── Внутри # комментария (Python) ──────────────────
        if (inHash) {
            if (c == '\n') {
                out += '\n';
                inHash = false;
            }
            ++i;
            continue;
        }

        // ─── Внутри -- комментария (Lua/SQL) ────────────────
        if (inDash) {
            if (c == '\n') {
                out += '\n';
                inDash = false;
            }
            ++i;
            continue;
        }

        // ─── Начала ─────────────────────────────────────────
        if (c == '"' || c == '\'') {
            inStr = true;
            strChar = c;
            out += c;
            ++i;
            continue;
        }
        if (c == '/' && n == '/') {
            inLine = true;
            i += 2;
            continue;
        }
        if (c == '/' && n == '*') {
            inBlock = true;
            i += 2;
            continue;
        }
        if (c == '#') {
            // Только если это не `#include`, `#define`, `#pragma`
            // — препроцессор C/C++ НЕ трогаем
            size_t j = i + 1;
            while (j < in.size() && (in[j] == ' ' || in[j] == '\t')) ++j;
            // Если после # идёт include/define/ifdef/ifndef/endif/pragma — это препроцессор
            const char* directives[] = {
                "include","define","ifdef","ifndef","endif","pragma",
                "if","else","elif","error","warning","line","undef",
                nullptr
            };
            bool isPreproc = false;
            for (int k = 0; directives[k]; ++k) {
                size_t len = std::strlen(directives[k]);
                if (in.compare(j, len, directives[k]) == 0) {
                    isPreproc = true;
                    break;
                }
            }
            if (isPreproc) {
                out += c;
                ++i;
                continue;
            }
            // Иначе — Python-комментарий
            inHash = true;
            ++i;
            continue;
        }
        if (c == '-' && n == '-') {
            inDash = true;
            i += 2;
            continue;
        }

        out += c;
        ++i;
    }

    char* r = (char*)std::malloc(out.size() + 1);
    if (!r) return nullptr;
    std::memcpy(r, out.c_str(), out.size() + 1);
    *out_size = out.size();
    return r;
}

extern "C" __declspec(dllexport) void obf_free(char* ptr) {
    std::free(ptr);
}