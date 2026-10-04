#include "../../../include/plugin/IPlugin.h"

#include <string>
#include <random>
#include <cstdlib>
#include <cstring>
#include <cstdio>

static std::mt19937 rng(0xCAFEBABE);

static int rnd(int lo, int hi) {
    std::uniform_int_distribution<int> d(lo, hi);
    return d(rng);
}

// ============================================================
// Runtime-хелперы (вставляются в код перед main)
// ============================================================

// RDTSC-проверка (anti-debug через время)
static const char* kTimingCheck =
"static volatile unsigned long long _antidbg_rdtsc(void) {\n"
"    unsigned int _lo, _hi;\n"
"    __asm { rdtsc }\n"                        // для x86
"    return ((unsigned long long)_hi << 32) | _lo;\n"
"}\n"
"\n"
"static int _antidbg_check(void) {\n"
"    unsigned long long _t0 = _antidbg_rdtsc();\n"
"    for (volatile int _i = 0; _i < 1000; ++_i) {}\n"
"    unsigned long long _t1 = _antidbg_rdtsc();\n"
"    return (_t1 - _t0) > 100000;\n"           // если медленно — отладка
"}\n";

// Проверка целостности (FNV-1a hash)
static const char* kIntegrityCheck =
"static unsigned int _antidbg_hash(const char* s, unsigned int n) {\n"
"    unsigned int h = 0x811C9DC5u;\n"
"    for (unsigned int i = 0; i < n; ++i) {\n"
"        h ^= (unsigned char)s[i];\n"
"        h *= 0x01000193u;\n"
"    }\n"
"    return h;\n"
"}\n"
"\n"
"static int _antidbg_selfcheck(void) {\n"
"    const char* _marker = \"%s\";\n"
"    return _antidbg_hash(_marker, %u) != 0x%08X;\n"
"}\n";

// Проверка на отладчик через PEB
static const char* kPebCheck =
"static int _antidbg_peb(void) {\n"
"#ifdef _WIN64\n"
"    unsigned char _peb[2] = {0};\n"
"    __try {\n"
"        unsigned long long _peb_addr = __readgsqword(0x60);\n"
"        unsigned char* _p = (unsigned char*)_peb_addr;\n"
"        _peb[0] = _p[2];   // BeingDebugged\n"
"    } __except(1) {}\n"
"    return _peb[0] != 0;\n"
"#else\n"
"    return 0;\n"
"#endif\n"
"}\n";

// Мусорные decrypt-функции (никогда не вызываются, но выглядят важно)
static std::string makeFakeDecrypt(unsigned id) {
    char buf[512];
    std::snprintf(buf, sizeof(buf),
        "\nstatic void _fake_dec_%u(void) {\n"
        "    volatile unsigned char _buf[%u] = {0};\n"
        "    for (int _i = 0; _i < %u; ++_i)\n"
        "        _buf[_i] = (unsigned char)(_buf[_i] ^ 0x%X);\n"
        "    (void)_buf;\n"
        "}\n",
        id,
        (unsigned)rnd(4, 32),
        (unsigned)rnd(4, 32),
        (unsigned)rnd(1, 0xFF));
    return std::string(buf);
}

// Фейковая "расшифровка" в main (проверяет, что отладчика нет)
static const char* kFakeDecryptBlock =
"    // --- runtime check ---\n"
"    if (_antidbg_check() || _antidbg_peb()) {\n"
"        volatile int _x = 0;\n"
"        while (1) { _x = (_x + 1) & 0xFF; if (_x == 256) break; }\n"
"    }\n";

// ============================================================
// Вставляем runtime-хелперы после последнего #include
// ============================================================
static std::string insertRuntime(const std::string& in, unsigned hash) {
    size_t pos = 0, lastInclude = 0;
    while ((pos = in.find("#include", pos)) != std::string::npos) {
        size_t eol = in.find('\n', pos);
        if (eol == std::string::npos) { lastInclude = in.size(); break; }
        lastInclude = eol + 1;
        pos = eol + 1;
    }

    char selfcheck[1024];
    std::snprintf(selfcheck, sizeof(selfcheck), kIntegrityCheck,
        "antideobf_marker_0xCAFEBABE", 23u, hash);

    std::string runtime;
    runtime += "\n// ═══ anti-deobf runtime ═══\n";
    runtime += kTimingCheck;
    runtime += "\n";
    runtime += selfcheck;
    runtime += "\n";
    runtime += kPebCheck;
    runtime += "\n";

    std::string out;
    out += in.substr(0, lastInclude);
    out += runtime;
    out += in.substr(lastInclude);
    return out;
}

// ============================================================
// Вставляем проверки в начало main
// ============================================================
static std::string insertChecksIntoMain(const std::string& in) {
    size_t pos = in.find("main");
    if (pos == std::string::npos) return in;

    size_t brace = in.find('{', pos);
    if (brace == std::string::npos) return in;

    std::string out;
    out += in.substr(0, brace + 1);
    out += "\n";
    out += kFakeDecryptBlock;
    out += in.substr(brace + 1);
    return out;
}

// ============================================================
// Множественная XOR-обфускация чисел
//   N → (((N ^ A) ^ B) ^ (A ^ B))
// ============================================================
static std::string obfuscateNumbers(const std::string& in) {
    // Пока просто оставим — числа обфусцирует плагин mba
    return in;
}

// ============================================================
// Мёртвые fake-decrypt функции
// ============================================================
static std::string appendFakeDecrypts(const std::string& in) {
    std::string out = in;
    out += "\n\n// ═══ fake decrypts (never called) ═══\n";
    int n = rnd(3, 6);
    for (int k = 0; k < n; ++k) out += makeFakeDecrypt(rnd(1000, 9999));
    return out;
}

// ============================================================
// Главный API плагина
// ============================================================
extern "C" __declspec(dllexport) const char* obf_name() {
    return "antideobf";
}

extern "C" __declspec(dllexport) const char* obf_description() {
    return "Anti-deobfuscation: anti-debug, timing, self-check, fake decrypts";
}

extern "C" __declspec(dllexport) int obf_api_version() {
    return OBFS_PLUGIN_API_VERSION;
}

extern "C" __declspec(dllexport) const char* obf_languages() {
    return "cpp,c";
}

extern "C" __declspec(dllexport) int obf_can_apply(const char* code) {
    if (!code) return 0;
    // Не применять повторно
    return (std::strstr(code, "main") &&
        !std::strstr(code, "anti-deobf runtime")) ? 1 : 0;
}

extern "C" __declspec(dllexport) char* obf_apply(const char* code, size_t size,
    size_t* out_size) {
    if (!code || !out_size) return nullptr;

    std::string s(code, size);

    // Хэш маркера (для integrity-check)
    unsigned int hash = 0x811C9DC5u;
    const char* marker = "antideobf_marker_0xCAFEBABE";
    for (const char* p = marker; *p; ++p) {
        hash ^= (unsigned char)*p;
        hash *= 0x01000193u;
    }

    s = insertRuntime(s, hash);
    s = insertChecksIntoMain(s);
    s = appendFakeDecrypts(s);

    char* out = (char*)std::malloc(s.size() + 1);
    if (!out) return nullptr;
    std::memcpy(out, s.c_str(), s.size() + 1);
    *out_size = s.size();
    return out;
}

extern "C" __declspec(dllexport) void obf_free(char* ptr) {
    std::free(ptr);
}