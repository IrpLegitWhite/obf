#include "../../../include/plugin/IPlugin.h"

#include <string>
#include <regex>
#include <cstdlib>
#include <cstring>
#include <cstdio>

static const unsigned char KEY = 0x42;

static std::string toHexEscapes(const std::string& data) {
    std::string out;
    char b[8];
    for (unsigned char c : data) {
        std::snprintf(b, sizeof(b), "\\x%02X", c);
        out += b;
    }
    return out;
}

extern "C" __declspec(dllexport) const char* obf_name() {
    return "swift-strings";
}
extern "C" __declspec(dllexport) const char* obf_description() {
    return "Swift: XOR strings";
}
extern "C" __declspec(dllexport) int obf_api_version() {
    return OBFS_PLUGIN_API_VERSION;
}
extern "C" __declspec(dllexport) const char* obf_languages() {
    return "swift";
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
            result += "obfDec(\"";
            result += toHexEscapes(enc);
            result += "\")";
        }
        else {
            result += m[0].str();
        }
        work = m.suffix().str();
    }
    result += work;

    static const char* helper =
        "func obfDec(_ s: String) -> String {\n"
        "    var out = \"\"\n"
        "    for scalar in s.unicodeScalars {\n"
        "        out.append(Character(UnicodeScalar(scalar.value ^ 0x42)!))\n"
        "    }\n"
        "    return out\n"
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