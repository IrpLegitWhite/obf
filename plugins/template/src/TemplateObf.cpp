#include "../../../include/plugin/IPlugin.h"

#include <string>
#include <cstdlib>
#include <cstring>

// ============================================================
// Шаблон плагина — копируй и правь под свой язык
// ============================================================

extern "C" __declspec(dllexport) const char* obf_name() {
    return "template";
}

extern "C" __declspec(dllexport) const char* obf_description() {
    return "Template plugin";
}

extern "C" __declspec(dllexport) int obf_api_version() {
    return OBFS_PLUGIN_API_VERSION;
}

// ★ Список языков через запятую (без пробелов)
extern "C" __declspec(dllexport) const char* obf_languages() {
    return "rust,go";   // ← замени на свои
}

extern "C" __declspec(dllexport) int obf_can_apply(const char* code) {
    return code ? 1 : 0;
}

extern "C" __declspec(dllexport) char* obf_apply(const char* code, size_t size,
    size_t* out_size) {
    if (!code || !out_size) return nullptr;

    std::string input(code, size);
    // ★ Логика обфускации

    char* out = (char*)std::malloc(input.size() + 1);
    if (!out) return nullptr;
    std::memcpy(out, input.c_str(), input.size() + 1);
    *out_size = input.size();
    return out;
}

extern "C" __declspec(dllexport) void obf_free(char* ptr) {
    std::free(ptr);
}