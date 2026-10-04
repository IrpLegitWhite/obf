#include "../../../include/plugin/IPlugin.h"
#include <string>
#include <regex>
#include <cstdlib>
#include <cstring>
#include <cstdio>

extern "C" __declspec(dllexport) const char* obf_name() { return "js-mba"; }
extern "C" __declspec(dllexport) const char* obf_description() {
    return "JavaScript/TypeScript: MBA integers";
}
extern "C" __declspec(dllexport) int obf_api_version() { return OBFS_PLUGIN_API_VERSION; }
extern "C" __declspec(dllexport) const char* obf_languages() { return "js,typescript"; }
extern "C" __declspec(dllexport) int obf_can_apply(const char* code) {
    return code ? 1 : 0;
}
extern "C" __declspec(dllexport) char* obf_apply(const char* code, size_t size,
    size_t* out_size) {
    if (!code || !out_size) return nullptr;
    std::string input(code, size), result;
    result.reserve(input.size() * 2);
    std::regex re(R"(\b(\d{3,})\b)");
    std::string work = input; std::smatch m; int safety = 0;
    while (std::regex_search(work, m, re) && safety++ < 5000) {
        result += m.prefix().str();
        try {
            unsigned long v = std::stoul(m[1].str());
            char buf[128];
            std::snprintf(buf, sizeof(buf), "((%lu ^ 0xDEAD) ^ 0xDEAD)", v);
            result += buf;
        }
        catch (...) { result += m[0].str(); }
        work = m.suffix().str();
    }
    result += work;
    char* out = (char*)std::malloc(result.size() + 1);
    if (!out) return nullptr;
    std::memcpy(out, result.c_str(), result.size() + 1);
    *out_size = result.size();
    return out;
}
extern "C" __declspec(dllexport) void obf_free(char* ptr) { std::free(ptr); }