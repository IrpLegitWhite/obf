#include "../../../include/plugin/IPlugin.h"

#include <string>
#include <regex>
#include <cstdlib>
#include <cstring>
#include <cstdio>

static const unsigned char KEY = 0x42;

// ─── Утилиты ─────────────────────────────────────────────────
static std::string toHexEscapes(const std::string& data) {
    std::string out;
    char b[8];
    for (unsigned char c : data) {
        std::snprintf(b, sizeof(b), "\\x%02X", c);
        out += b;
    }
    return out;
}

// ─── Метаданные ──────────────────────────────────────────────
extern "C" __declspec(dllexport) const char* obf_name() {
    return "strings";
}

extern "C" __declspec(dllexport) const char* obf_description() {
    return "C/C++/C#: XOR-encrypt string literals";
}

extern "C" __declspec(dllexport) int obf_api_version() {
    return OBFS_PLUGIN_API_VERSION;
}

extern "C" __declspec(dllexport) const char* obf_languages() {
    return "cpp,c,csharp";
}

extern "C" __declspec(dllexport) int obf_can_apply(const char* code) {
    return (code && std::strchr(code, '"')) ? 1 : 0;
}

// ─── Логика обфускации ───────────────────────────────────────
extern "C" __declspec(dllexport) char* obf_apply(const char* code, size_t size,
    size_t* out_size) {
    if (!code || !out_size) return nullptr;

    std::string input(code, size);
    std::string result;
    result.reserve(input.size() * 2);

    // Ищем "..." без lookbehind (MSVC не поддерживает (?<!...))
    std::regex re("\"((?:[^\"\\\\]|\\\\.)*)\"");

    std::string work = input;
    std::smatch m;
    int safety = 0;

    while (std::regex_search(work, m, re) && safety++ < 10000) {
        result += m.prefix().str();
        std::string orig = m[1].str();

        // Пропускаем короткие и уже обфусцированные
        if (orig.size() >= 3 && orig.find("obf_dec_str") == std::string::npos) {
            std::string enc;
            for (unsigned char c : orig) enc += (char)(c ^ KEY);

            result += "obf_dec_str(\"";
            result += toHexEscapes(enc);
            result += "\", ";
            result += std::to_string(enc.size());
            result += ")";
        }
        else {
            result += m[0].str();
        }
        work = m.suffix().str();
    }
    result += work;

    // C/C++/C# helper
    static const char* helper =
        "static const char* obf_dec_str(const char* d, int n) {\n"
        "    static char b[4096];\n"
        "    for (int i = 0; i < n && i < 4095; ++i)\n"
        "        b[i] = (char)(d[i] ^ 0x42);\n"
        "    b[n < 4095 ? n : 4095] = 0;\n"
        "    return b;\n"
        "}\n"
        "\n";

    std::string finalCode = std::string(helper) + result;

    char* out = (char*)std::malloc(finalCode.size() + 1);
    if (!out) return nullptr;
    std::memcpy(out, finalCode.c_str(), finalCode.size() + 1);
    *out_size = finalCode.size();
    return out;
}

extern "C" __declspec(dllexport) void obf_free(char* ptr) {
    std::free(ptr);
}