#include "../../../include/plugin/IPlugin.h"

#include <string>
#include <random>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cctype>

static std::mt19937 rng(0xB10B17E5);

static int rnd(int lo, int hi) {
    std::uniform_int_distribution<int> d(lo, hi);
    return d(rng);
}

// ─── Проверка: внутри #if 0 / #ifdef / #ifndef ──────────────
static bool isInsideIf0(const std::string& s, size_t pos) {
    int depth = 0;
    size_t i = 0;
    while (i < pos && i < s.size()) {
        if (s[i] == '#' && (i == 0 || s[i - 1] == '\n')) {
            size_t j = i + 1;
            while (j < s.size() && (s[j] == ' ' || s[j] == '\t')) ++j;
            if (s.compare(j, 4, "if 0") == 0 || s.compare(j, 5, "ifdef") == 0 ||
                s.compare(j, 6, "ifndef") == 0) {
                ++depth;
            }
            else if (s.compare(j, 5, "endif") == 0) {
                --depth;
                if (depth < 0) depth = 0;
            }
            while (i < s.size() && s[i] != '\n') ++i;
        }
        ++i;
    }
    return depth > 0;
}

// ─── Сломанные байтовые последовательности ──────────────────
static const char* brokenBytes[] = {
    "\xEF\xBF\xBD",                     // U+FFFD replacement
    "\xC0\x80",                          // overlong null
    "\xED\xA0\x80",                      // UTF-16 surrogate
    "\xFF\xFE",                          // BOM мусор
    "\xFE\xFF",                          // BOM мусор
    "\x80\x81\x82\x83",                  // continuation без start
    "\xF0\x28\x8C\x28",                  // invalid 4-byte
    "\xC0\xC1\xC2",                      // перебор
    "\x80",                              // один continuation
    "\xF8\x88\x80\x80\x80",              // 5-byte (запрещён)
    "\xFC\x84\x80\x80\x80\x80",          // 6-byte (запрещён)
    "\xE0\x80\x80",                      // overlong 3-byte
    "\xF0\x80\x80\x80",                  // overlong 4-byte
    "\xC2",                              // incomplete 2-byte
    "\xE2\x82",                          // incomplete 3-byte
};

static const int brokenN = sizeof(brokenBytes) / sizeof(brokenBytes[0]);

// ─── Тексты-приманки ────────────────────────────────────────
static const char* trapTexts[] = {
    "legacy decoder blob",
    "encrypted chunk",
    "stage2 payload",
    "config dump",
    "key material",
    "obfuscated data",
    "raw bytes",
    "binary blob",
    "unknown encoding",
    "corrupted buffer",
    "c2 beacon data",
    "shellcode stub",
    "encrypted config",
    "hidden module",
    "dump of memory",
    "serialized payload",
};
static const int trapTextsN = sizeof(trapTexts) / sizeof(trapTexts[0]);

// ─── 1. Сломанные байты в /* */ комментариях ────────────────
static std::string injectBrokenInBlock(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 2);

    size_t i = 0;
    bool inStr = false; char strCh = 0;
    int lineCount = 0;
    while (i < in.size()) {
        char c = in[i];
        if (inStr) {
            out += c;
            if (c == '\\' && i + 1 < in.size()) { out += in[i + 1]; i += 2; continue; }
            if (c == strCh) inStr = false;
            if (c == '\n') ++lineCount;
            ++i; continue;
        }
        if (c == '"' || c == '\'') { inStr = true; strCh = c; out += c; ++i; continue; }

        // не трогаем существующие комментарии
        if (c == '/' && i + 1 < in.size() && (in[i + 1] == '/' || in[i + 1] == '*')) {
            out += c; ++i;
            if (i < in.size()) { out += in[i]; ++i; }
            while (i < in.size() && in[i] != '\n') { out += in[i]; ++i; }
            continue;
        }

        if (c == '\n') {
            ++lineCount;
            out += c; ++i;
            if (lineCount > 0 && lineCount % 8 == 0 && rnd(0, 2) == 0 &&
                !isInsideIf0(in, i)) {
                int b1 = rnd(0, brokenN - 1);
                int b2 = rnd(0, brokenN - 1);
                int t = rnd(0, trapTextsN - 1);
                int indent = rnd(0, 10);
                for (int k = 0; k < indent; ++k) out += ' ';
                out += "/* ";
                out += brokenBytes[b1];
                out += " ";
                out += trapTexts[t];
                out += " ";
                out += brokenBytes[b2];
                out += " */\n";
            }
            continue;
        }
        out += c; ++i;
    }
    return out;
}

// ─── 2. Сломанные байты в // комментариях ───────────────────
static std::string injectBrokenInLine(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 2);

    size_t i = 0;
    bool inStr = false; char strCh = 0;
    int lineCount = 0;
    while (i < in.size()) {
        char c = in[i];
        if (inStr) {
            out += c;
            if (c == '\\' && i + 1 < in.size()) { out += in[i + 1]; i += 2; continue; }
            if (c == strCh) inStr = false;
            if (c == '\n') ++lineCount;
            ++i; continue;
        }
        if (c == '"' || c == '\'') { inStr = true; strCh = c; out += c; ++i; continue; }

        if (c == '/' && i + 1 < in.size() && in[i + 1] == '/') {
            out += "//"; i += 2;
            size_t j = i;
            while (j < in.size() && in[j] != '\n') ++j;
            // с вероятностью 1/3 добавляем сломанные байты в конец
            if (rnd(0, 2) == 0 && !isInsideIf0(in, i)) {
                int b = rnd(0, brokenN - 1);
                out += ' ';
                out += brokenBytes[b];
                out += ' ';
            }
            while (i < j) { out += in[i]; ++i; }
            continue;
        }

        out += c;
        if (c == '\n') ++lineCount;
        ++i;
    }
    return out;
}

// ─── 3. Hex-дамп «зашифрованных данных» в комментах ─────────
static std::string injectHexDump(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 3);

    size_t i = 0;
    bool inStr = false; char strCh = 0;
    int lineCount = 0;
    while (i < in.size()) {
        char c = in[i];
        if (inStr) {
            out += c;
            if (c == '\\' && i + 1 < in.size()) { out += in[i + 1]; i += 2; continue; }
            if (c == strCh) inStr = false;
            if (c == '\n') ++lineCount;
            ++i; continue;
        }
        if (c == '"' || c == '\'') { inStr = true; strCh = c; out += c; ++i; continue; }

        if (c == '\n') {
            ++lineCount;
            out += c; ++i;
            if (lineCount > 0 && lineCount % 14 == 0 && rnd(0, 2) == 0 &&
                !isInsideIf0(in, i)) {
                int indent = rnd(0, 4);
                for (int k = 0; k < indent; ++k) out += ' ';
                out += "/* dump:\n";
                int lines = rnd(2, 4);
                for (int L = 0; L < lines; ++L) {
                    for (int k = 0; k < indent + 4; ++k) out += ' ';
                    for (int col = 0; col < 16; ++col) {
                        char b[8];
                        std::snprintf(b, sizeof(b), "%02X ", (unsigned)rnd(0, 255));
                        out += b;
                    }
                    out += '\n';
                }
                for (int k = 0; k < indent; ++k) out += ' ';
                out += "*/\n";
            }
            continue;
        }
        out += c; ++i;
    }
    return out;
}

// ─── 4. Невалидные UTF-8 в «строках-литералах» (в комментах) ─
static std::string injectFakeStrings(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 3);

    size_t i = 0;
    bool inStr = false; char strCh = 0;
    int lineCount = 0;
    while (i < in.size()) {
        char c = in[i];
        if (inStr) {
            out += c;
            if (c == '\\' && i + 1 < in.size()) { out += in[i + 1]; i += 2; continue; }
            if (c == strCh) inStr = false;
            if (c == '\n') ++lineCount;
            ++i; continue;
        }
        if (c == '"' || c == '\'') { inStr = true; strCh = c; out += c; ++i; continue; }

        if (c == '\n') {
            ++lineCount;
            out += c; ++i;
            if (lineCount > 0 && lineCount % 20 == 0 && rnd(0, 3) == 0 &&
                !isInsideIf0(in, i)) {
                int indent = rnd(0, 8);
                for (int k = 0; k < indent; ++k) out += ' ';
                // «закомментированная» строка со сломанными байтами
                out += "// static const char* _x = \"";
                int n = rnd(4, 12);
                for (int k = 0; k < n; ++k) {
                    int b = rnd(0, brokenN - 1);
                    out += brokenBytes[b];
                }
                out += "\";\n";
            }
            continue;
        }
        out += c; ++i;
    }
    return out;
}

// ─── 5. Смесь \r\n и \n ─────────────────────────────────────
static std::string injectMixedNewlines(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 2);

    size_t i = 0;
    bool inStr = false; char strCh = 0;
    while (i < in.size()) {
        char c = in[i];
        if (inStr) {
            out += c;
            if (c == '\\' && i + 1 < in.size()) { out += in[i + 1]; i += 2; continue; }
            if (c == strCh) inStr = false;
            ++i; continue;
        }
        if (c == '"' || c == '\'') { inStr = true; strCh = c; out += c; ++i; continue; }

        if (c == '\n') {
            // иногда заменяем \n на \r\n
            if (rnd(0, 4) == 0) {
                out += '\r';
            }
            out += '\n';
            ++i;
            continue;
        }
        out += c; ++i;
    }
    return out;
}

// ════════════════════════════════════════════════════════════
extern "C" __declspec(dllexport) const char* obf_name() {
    return "broken_bytes";
}

extern "C" __declspec(dllexport) const char* obf_description() {
    return "Inject broken/invalid UTF-8 bytes in comments (visual chaos)";
}

extern "C" __declspec(dllexport) int obf_api_version() {
    return OBFS_PLUGIN_API_VERSION;
}

extern "C" __declspec(dllexport) const char* obf_languages() {
    return "cpp,c";
}

extern "C" __declspec(dllexport) int obf_can_apply(const char* code) {
    if (!code) return 0;
    if (std::strstr(code, "broken_bytes: applied")) return 0;
    return 1;
}

extern "C" __declspec(dllexport) char* obf_apply(const char* code, size_t size,
    size_t* out_size) {
    if (!code || !out_size) return nullptr;

    std::string s(code, size);

    s = injectBrokenInBlock(s);     // /* <broken> text <broken> */
    s = injectBrokenInLine(s);      // // ... <broken>
    s = injectHexDump(s);           // /* dump: hex-строки */
    s = injectFakeStrings(s);       // // static const char* = "<broken>"
    s = injectMixedNewlines(s);     // \r\n вперемешку с \n

    s = "/* broken_bytes: applied */\n" + s;

    char* out = (char*)std::malloc(s.size() + 1);
    if (!out) return nullptr;
    std::memcpy(out, s.c_str(), s.size() + 1);
    *out_size = s.size();
    return out;
}

extern "C" __declspec(dllexport) void obf_free(char* ptr) {
    std::free(ptr);
}