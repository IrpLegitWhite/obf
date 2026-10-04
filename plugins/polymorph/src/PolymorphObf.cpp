#include "../../../include/plugin/IPlugin.h"

#include <string>
#include <vector>
#include <random>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cctype>

static std::mt19937 rng(0xDEADBEEF);

static int rnd(int lo, int hi) {
    std::uniform_int_distribution<int> d(lo, hi);
    return d(rng);
}

// ─── Мусорные строки (без __asm!) ───────────────────────────
// ★ FIX: все генерируемые переменные теперь с префиксом "_jnk"
//         (было "_q","_k","_n" — rename их переименовывал)
static std::string makeJunk() {
    static unsigned long counter = 0;
    char buf[512];
    unsigned long id = counter++;
    int idx = rnd(0, 3);

    switch (idx) {
    case 0:
        std::snprintf(buf, sizeof(buf),
            "    volatile unsigned long _jnk%lu = %u;\n"
            "    _jnk%lu ^= 0x%X;\n",
            id, (unsigned)rnd(1, 0xFFFF), id, (unsigned)rnd(1, 0xFF));
        break;
    case 1:
        // ★ FIX: _q → _jnkq
        std::snprintf(buf, sizeof(buf),
            "    if ((%u * %u) == %u) { volatile int _jnkq%lu = 0; (void)_jnkq%lu; }\n",
            (unsigned)rnd(1, 100), (unsigned)rnd(1, 100),
            (unsigned)rnd(10000, 0xFFFF), id, id);
        break;
    case 2:
        // ★ FIX: _k → _jnkk
        std::snprintf(buf, sizeof(buf),
            "    for (volatile int _jnkk%lu = 0; _jnkk%lu < 0; ++_jnkk%lu) { break; }\n",
            id, id, id);
        break;
    case 3:
        // ★ FIX: _n → _jnkn
        std::snprintf(buf, sizeof(buf),
            "    volatile int _jnkn%lu = %u + %u - %u;\n"
            "    (void)_jnkn%lu;\n",
            id, (unsigned)rnd(1, 100), (unsigned)rnd(1, 100),
            (unsigned)rnd(1, 100), id);
        break;
    }
    return std::string(buf);
}

// ─── Разбить число на сумму ────────────────────────────────
static std::string splitNumber(unsigned long v) {
    if (v < 100) return std::to_string(v);
    unsigned long a = v / 3;
    unsigned long b = v / 3;
    unsigned long c = v - a - b;
    char buf[128];
    std::snprintf(buf, sizeof(buf), "(%lu + %lu + %lu)", a, b, c);
    return std::string(buf);
}

// ─── Мёртвые функции ────────────────────────────────────────
// ★ FIX: имена переменных теперь уникальны (суффикс id) и с префиксом "_dead_"
//         — rename их не трогает (префикс "_dead" в reservedPrefixes)
//         — нет одинаковых имён в разных _dead_* функциях
static std::string makeDeadFunction(unsigned id) {
    char buf[2048];
    std::snprintf(buf, sizeof(buf),
        "\nstatic void _dead_%u(void) {\n"
        "    volatile unsigned long _dead_a%u = %u;\n"
        "    volatile unsigned long _dead_b%u = %u;\n"
        "    volatile unsigned long _dead_c%u = _dead_a%u ^ _dead_b%u;\n"
        "    for (unsigned long _dead_i%u = 0; _dead_i%u < %u; ++_dead_i%u) {\n"
        "        _dead_c%u = (_dead_c%u * 31u) ^ (_dead_c%u >> 3);\n"
        "    }\n"
        "    if (_dead_c%u == %u) { _dead_c%u = 0; }\n"
        "}\n",
        id,
        id, (unsigned)rnd(0, 0xFFFF),                       // _dead_a%u
        id, (unsigned)rnd(0, 0xFFFF),                       // _dead_b%u
        id, id, id,                                          // _dead_c%u = _dead_a ^ _dead_b
        id, id, (unsigned)rnd(100, 1000), id,                // for (... _dead_i%u ...)
        id, id, id,                                          // _dead_c%u = ...
        id, (unsigned)rnd(0, 0xFFFF), id);                   // if (_dead_c%u == ...)
    return std::string(buf);
}

// ─── Вставить junk в каждый `{` ─────────────────────────────
static std::string injectJunk(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 2);

    size_t i = 0;
    bool inStr = false;
    char strChar = 0;

    while (i < in.size()) {
        char c = in[i];

        if (inStr) {
            out += c;
            if (c == '\\' && i + 1 < in.size()) { out += in[i + 1]; i += 2; continue; }
            if (c == strChar) inStr = false;
            ++i;
            continue;
        }

        if (c == '"' || c == '\'') { inStr = true; strChar = c; out += c; ++i; continue; }

        if (c == '{') {
            out += c;
            ++i;
            int n = rnd(1, 2);
            for (int k = 0; k < n; ++k) out += makeJunk();
            continue;
        }

        out += c;
        ++i;
    }
    return out;
}

// ─── Разбить числа ──────────────────────────────────────────
static std::string splitNumbers(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 2);

    size_t i = 0;
    bool inStr = false;
    char strChar = 0;

    while (i < in.size()) {
        char c = in[i];

        if (inStr) {
            out += c;
            if (c == '\\' && i + 1 < in.size()) { out += in[i + 1]; i += 2; continue; }
            if (c == strChar) inStr = false;
            ++i;
            continue;
        }

        if (c == '"' || c == '\'') { inStr = true; strChar = c; out += c; ++i; continue; }

        // Пропуск 0x...
        if (c == '0' && i + 1 < in.size() && (in[i + 1] == 'x' || in[i + 1] == 'X')) {
            size_t start = i;
            i += 2;
            while (i < in.size() && std::isxdigit((unsigned char)in[i])) ++i;
            while (i < in.size() && (in[i] == 'u' || in[i] == 'U' || in[i] == 'l' || in[i] == 'L')) ++i;
            out += in.substr(start, i - start);
            continue;
        }

        // Десятичное
        if (std::isdigit((unsigned char)c)) {
            size_t start = i;
            while (i < in.size() && std::isdigit((unsigned char)in[i])) ++i;
            std::string numStr = in.substr(start, i - start);
            bool isFloat = (i < in.size() && in[i] == '.');
            bool isSuffix = (i < in.size() && std::isalpha((unsigned char)in[i]));

            if (!isFloat && !isSuffix && numStr.size() >= 3) {
                try {
                    unsigned long v = std::stoul(numStr);
                    if (v >= 100 && v < 1000000) {
                        out += splitNumber(v);
                        continue;
                    }
                }
                catch (...) {}
            }
            out += numStr;
            continue;
        }

        out += c;
        ++i;
    }
    return out;
}

// ─── Добавить мёртвые функции ───────────────────────────────
static std::string appendDeadFuncs(const std::string& in) {
    std::string out = in;
    out += "\n\n/* polymorph: dead funcs */\n";
    int n = rnd(2, 4);
    for (int k = 0; k < n; ++k) out += makeDeadFunction(rnd(1000, 9999));
    return out;
}

// ─── Прагма после последнего #include ───────────────────────
static std::string insertPragma(const std::string& in) {
    size_t pos = 0;
    size_t lastInclude = 0;
    while ((pos = in.find("#include", pos)) != std::string::npos) {
        size_t eol = in.find('\n', pos);
        if (eol == std::string::npos) { lastInclude = in.size(); break; }
        lastInclude = eol + 1;
        pos = eol + 1;
    }

    std::string out;
    out += in.substr(0, lastInclude);
    out += "\n#ifdef _MSC_VER\n"
        "#pragma optimize(\"\", off)\n"
        "#endif\n\n";
    out += in.substr(lastInclude);
    return out;
}

// ============================================================
extern "C" __declspec(dllexport) const char* obf_name() {
    return "polymorph";
}

extern "C" __declspec(dllexport) const char* obf_description() {
    return "Polymorphic: junk code, dead funcs, split numbers";
}

extern "C" __declspec(dllexport) int obf_api_version() {
    return OBFS_PLUGIN_API_VERSION;
}

extern "C" __declspec(dllexport) const char* obf_languages() {
    return "cpp,c";
}

extern "C" __declspec(dllexport) int obf_can_apply(const char* code) {
    if (!code) return 0;
    // Только один раз — проверяем маркер
    return (std::strstr(code, "main") &&
        !std::strstr(code, "polymorph: dead funcs")) ? 1 : 0;
}

extern "C" __declspec(dllexport) char* obf_apply(const char* code, size_t size,
    size_t* out_size) {
    if (!code || !out_size) return nullptr;

    std::string s(code, size);
    s = splitNumbers(s);
    s = injectJunk(s);
    s = appendDeadFuncs(s);
    s = insertPragma(s);

    char* out = (char*)std::malloc(s.size() + 1);
    if (!out) return nullptr;
    std::memcpy(out, s.c_str(), s.size() + 1);
    *out_size = s.size();
    return out;
}

extern "C" __declspec(dllexport) void obf_free(char* ptr) {
    std::free(ptr);
}