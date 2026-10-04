#include "../../../include/plugin/IPlugin.h"

#include <string>
#include <random>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cctype>

static std::mt19937 rng(0xBADC0DE);

static int rnd(int lo, int hi) {
    std::uniform_int_distribution<int> d(lo, hi);
    return d(rng);
}

// ════════════════════════════════════════════════════════════
//  БЕЗОПАСНЫЕ БЛОКИ ЗАЩИТЫ
//  — без ExitProcess
//  — без __readfsdword
//  — без __try/__except
//  — без VM detection
//  — все имена с префиксом _jnk (rename не трогает)
// ════════════════════════════════════════════════════════════

// ─── 1. IsDebuggerPresent ───────────────────────────────────
static const char* blockIsDebuggerPresent() {
    return
        "    if (IsDebuggerPresent()) { _jnk_obf_dbg = 1; }\n";
}

// ─── 2. CheckRemoteDebuggerPresent ──────────────────────────
static const char* blockRemoteDebugger() {
    return
        "    { BOOL _jnk_dbg2 = FALSE; "
        "CheckRemoteDebuggerPresent(GetCurrentProcess(), &_jnk_dbg2); "
        "if (_jnk_dbg2) { _jnk_obf_dbg = 1; } }\n";
}

// ─── 3. OutputDebugString trick ─────────────────────────────
static const char* blockOutputDebugString() {
    return
        "    { SetLastError(0); OutputDebugStringA(\"x\"); "
        "if (GetLastError() != 0) { _jnk_obf_dbg = 1; } }\n";
}

// ─── 4. Логирование (опционально) ───────────────────────────
static const char* blockLogCheck() {
    return
        "    if (_jnk_obf_dbg) { _jnk_obf_log(\"check triggered\"); }\n";
}

// ════════════════════════════════════════════════════════════
//  ТАБЛИЦА БЛОКОВ
// ════════════════════════════════════════════════════════════
typedef const char* (*BlockFn)();

static BlockFn g_blocks[] = {
    blockIsDebuggerPresent,
    blockRemoteDebugger,
    blockOutputDebugString,
    blockLogCheck,
};
static const int g_blocksCount = sizeof(g_blocks) / sizeof(g_blocks[0]);

// ════════════════════════════════════════════════════════════
//  СЛУЖЕБНЫЙ КОД
//  ★ ВСЕ ИМЕНА С ПРЕФИКСОМ _jnk — rename их НЕ трогает
//  ★ MAX_PATH заменён на 260 (жёстко)
// ════════════════════════════════════════════════════════════
static const char* PROTECT_HEADER =
"/* ─── protect: runtime checks ─── */\n"
"#ifndef _JNK_OBF_PROTECT_H_\n"
"#define _JNK_OBF_PROTECT_H_\n"
"\n"
"#ifdef _WIN32\n"
"#include <windows.h>\n"
"#include <stdio.h>\n"
"#include <string.h>\n"
"\n"
"static volatile int _jnk_obf_dbg = 0;\n"
"\n"
"static void _jnk_obf_log(const char* _jnk_msg) {\n"
"    char _jnk_path[260];\n"
"    DWORD _jnk_n = GetTempPathA(260, _jnk_path);\n"
"    if (_jnk_n == 0) return;\n"
"    strncat(_jnk_path, \"obf_dbg.log\", 260 - strlen(_jnk_path) - 1);\n"
"    FILE* _jnk_f = fopen(_jnk_path, \"a\");\n"
"    if (_jnk_f) {\n"
"        fprintf(_jnk_f, \"[%lu] %s\\n\", (unsigned long)GetTickCount(), _jnk_msg);\n"
"        fclose(_jnk_f);\n"
"    }\n"
"}\n"
"\n"
"#else\n"
"static volatile int _jnk_obf_dbg = 0;\n"
"static void _jnk_obf_log(const char* _jnk_msg) { (void)_jnk_msg; }\n"
"#endif\n"
"\n"
"#endif /* _JNK_OBF_PROTECT_H_ */\n"
"\n";

// ════════════════════════════════════════════════════════════
//  ВСТАВКА ЗАЩИТЫ
// ════════════════════════════════════════════════════════════
static std::string injectProtection(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 2);

    int blockCount = g_blocksCount;
    BlockFn* blocks = g_blocks;

    size_t i = 0;
    bool inStr = false; char strCh = 0;
    bool inIf0 = false;
    int braceDepth = 0;
    int fnCounter = 0;

    while (i < in.size()) {
        char c = in[i];

        if (inStr) {
            out += c;
            if (c == '\\' && i + 1 < in.size()) { out += in[i + 1]; i += 2; continue; }
            if (c == strCh) inStr = false;
            ++i; continue;
        }
        if (c == '"' || c == '\'') { inStr = true; strCh = c; out += c; ++i; continue; }

        if (c == '#' && (i == 0 || in[i - 1] == '\n')) {
            size_t j = i + 1;
            while (j < in.size() && (in[j] == ' ' || in[j] == '\t')) ++j;
            if (in.compare(j, 4, "if 0") == 0 || in.compare(j, 5, "ifdef") == 0 ||
                in.compare(j, 6, "ifndef") == 0) {
                inIf0 = true;
            }
            else if (in.compare(j, 5, "endif") == 0) {
                inIf0 = false;
            }
        }

        if (c == '{') {
            ++braceDepth;
            out += c;
            ++i;

            bool isFunctionStart = false;
            if (braceDepth == 1 && !inIf0) {
                size_t j = i;
                while (j > 0) {
                    char p = in[j - 1];
                    if (p == ' ' || p == '\t' || p == '\n' || p == '\r') { --j; continue; }
                    if (p == ')') { isFunctionStart = true; }
                    break;
                }
            }

            if (isFunctionStart && rnd(0, 1) == 0) {
                ++fnCounter;
                int n = rnd(1, 2);
                for (int k = 0; k < n; ++k) {
                    int idx = rnd(0, blockCount - 1);
                    out += "\n";
                    out += blocks[idx]();
                }
            }
            continue;
        }

        if (c == '}') {
            if (braceDepth > 0) --braceDepth;
            out += c;
            ++i;
            continue;
        }

        out += c;
        ++i;
    }

    return out;
}

// ════════════════════════════════════════════════════════════
//  ДОБАВИТЬ HEADER
// ════════════════════════════════════════════════════════════
static std::string prependHeader(const std::string& in) {
    if (in.find("_JNK_OBF_PROTECT_H_") != std::string::npos) return in;

    std::string out;
    out.reserve(in.size() + 1024);
    out += PROTECT_HEADER;
    out += "\n";
    out += in;
    return out;
}

// ════════════════════════════════════════════════════════════
extern "C" __declspec(dllexport) const char* obf_name() {
    return "protect";
}

extern "C" __declspec(dllexport) const char* obf_description() {
    return "Safe anti-debug checks (log only, no crashes)";
}

extern "C" __declspec(dllexport) int obf_api_version() {
    return OBFS_PLUGIN_API_VERSION;
}

extern "C" __declspec(dllexport) const char* obf_languages() {
    return "cpp,c";
}

extern "C" __declspec(dllexport) int obf_can_apply(const char* code) {
    if (!code) return 0;
    if (std::strstr(code, "_JNK_OBF_PROTECT_H_")) return 0;
    if (!std::strstr(code, "main")) return 0;
    return 1;
}

extern "C" __declspec(dllexport) char* obf_apply(const char* code, size_t size,
    size_t* out_size) {
    if (!code || !out_size) return nullptr;

    std::string s(code, size);

    s = injectProtection(s);
    s = prependHeader(s);

    char* out = (char*)std::malloc(s.size() + 1);
    if (!out) return nullptr;
    std::memcpy(out, s.c_str(), s.size() + 1);
    *out_size = s.size();
    return out;
}

extern "C" __declspec(dllexport) void obf_free(char* ptr) {
    std::free(ptr);
}