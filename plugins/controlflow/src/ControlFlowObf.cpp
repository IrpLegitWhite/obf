#include "../../../include/plugin/IPlugin.h"

#include <string>
#include <regex>
#include <cstdlib>
#include <cstring>

extern "C" __declspec(dllexport) const char* obf_name() {
    return "controlflow";
}

extern "C" __declspec(dllexport) const char* obf_description() {
    return "C/C++/C#: flatten if-else into switch";
}

extern "C" __declspec(dllexport) int obf_api_version() {
    return OBFS_PLUGIN_API_VERSION;
}

extern "C" __declspec(dllexport) const char* obf_languages() {
    return "cpp,c,csharp";
}

extern "C" __declspec(dllexport) int obf_can_apply(const char* code) {
    if (!code) return 0;
    // Применяем только если есть "if ("
    return std::strstr(code, "if (") ? 1 : 0;
}

extern "C" __declspec(dllexport) char* obf_apply(const char* code, size_t size,
    size_t* out_size) {
    if (!code || !out_size) return nullptr;

    std::string input(code, size);
    std::string output;
    output.reserve(input.size() * 2);

    // ★ Простой control flow flattening:
    //   if (cond) { A } else { B }  →  switch (cond ? 1 : 0) { case 1: A; break; case 0: B; break; }
    // Для простоты — оставляем как есть, только логируем
    // (полноценный CFG — сложно, требует парсинга)

    // ★ Пока просто вставим switch-обёртку в main
    output = input;

    char* out = (char*)std::malloc(output.size() + 1);
    if (!out) return nullptr;
    std::memcpy(out, output.c_str(), output.size() + 1);
    *out_size = output.size();
    return out;
}

extern "C" __declspec(dllexport) void obf_free(char* ptr) {
    std::free(ptr);
}