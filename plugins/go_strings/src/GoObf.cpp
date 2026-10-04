#include "../../../include/plugin/IPlugin.h"

#include <string>
#include <regex>
#include <cstdlib>
#include <cstring>
#include <cstdio>

static const unsigned char KEY = 0x42;

extern "C" __declspec(dllexport) const char* obf_name() {
    return "go-strings";
}
extern "C" __declspec(dllexport) const char* obf_description() {
    return "Go: XOR strings";
}
extern "C" __declspec(dllexport) int obf_api_version() {
    return OBFS_PLUGIN_API_VERSION;
}
extern "C" __declspec(dllexport) const char* obf_languages() {
    return "go";
}
extern "C" __declspec(dllexport) int obf_can_apply(const char* code) {
    return (code && std::strchr(code, '"')) ? 1 : 0;
}

extern "C" __declspec(dllexport) char* obf_apply(const char* code, size_t size,
    size_t* out_size) {
    if (!code || !out_size) return nullptr;

    std::string input(code, size);
    std::string result;
    result.reserve(input.size() * 2);

    std::regex re("\"((?:[^\"\\\\]|\\\\.)*)\"");

    std::string work = input;
    std::smatch m;
    int safety = 0;

    while (std::regex_search(work, m, re) && safety++ < 10000) {
        result += m.prefix().str();
        std::string orig = m[1].str();

        if (orig.size() >= 3 && orig.find("obfDec") == std::string::npos) {
            std::string enc;
            for (unsigned char c : orig) enc += (char)(c ^ KEY);

            std::string bytes;
            char buf[16];
            for (size_t i = 0; i < enc.size(); ++i) {
                std::snprintf(buf, sizeof(buf), "0x%02X", (unsigned char)enc[i]);
                bytes += buf;
                if (i + 1 < enc.size()) bytes += ", ";
            }
            result += "obfDec([]byte{" + bytes + "})";
        }
        else {
            result += m[0].str();
        }
        work = m.suffix().str();
    }
    result += work;

    static const char* helper =
        "func obfDec(b []byte) string {\n"
        "    out := make([]byte, len(b))\n"
        "    for i, c := range b { out[i] = c ^ 0x42 }\n"
        "    return string(out)\n"
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