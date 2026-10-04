#include "../../../include/plugin/IPlugin.h"

#include <string>
#include <cstdlib>
#include <cstring>

extern "C" __declspec(dllexport) const char* obf_name() {
    return "opaque";
}

extern "C" __declspec(dllexport) const char* obf_description() {
    return "C/C++/C#: wrap code in opaque predicates";
}

extern "C" __declspec(dllexport) int obf_api_version() {
    return OBFS_PLUGIN_API_VERSION;
}

extern "C" __declspec(dllexport) const char* obf_languages() {
    return "cpp,c,csharp";
}

extern "C" __declspec(dllexport) int obf_can_apply(const char* code) {
    if (!code) return 0;
    // Не применять повторно, если уже есть _op
    return (std::strstr(code, "main") &&
        !std::strstr(code, "_op")) ? 1 : 0;
}

extern "C" __declspec(dllexport) char* obf_apply(const char* code, size_t size,
    size_t* out_size) {
    if (!code || !out_size) return nullptr;

    std::string input(code, size);

    size_t pos = input.find("main");
    if (pos == std::string::npos) {
        char* out = (char*)std::malloc(input.size() + 1);
        std::memcpy(out, input.c_str(), input.size() + 1);
        *out_size = input.size();
        return out;
    }

    size_t brace = input.find('{', pos);
    if (brace == std::string::npos) {
        char* out = (char*)std::malloc(input.size() + 1);
        std::memcpy(out, input.c_str(), input.size() + 1);
        *out_size = input.size();
        return out;
    }

    // Ищем закрывающую } main
    int depth = 1;
    size_t end = brace + 1;
    while (end < input.size() && depth > 0) {
        if (input[end] == '{') depth++;
        else if (input[end] == '}') depth--;
        if (depth == 0) break;
        end++;
    }

    if (end >= input.size()) {
        char* out = (char*)std::malloc(input.size() + 1);
        std::memcpy(out, input.c_str(), input.size() + 1);
        *out_size = input.size();
        return out;
    }

    // ★ Вставляем opaque внутри main, оставляя структуру целой
    std::string output;
    output.reserve(input.size() + 256);
    output = input.substr(0, brace + 1);
    output += "\n    // obf: opaque predicate\n"
        "    volatile int _op = 0x42;\n"
        "    if (_op * _op >= 0) {\n";
    output += input.substr(brace + 1, end - brace - 1);
    output += "    }\n";
    output += input.substr(end);   // включая закрывающую }

    char* out = (char*)std::malloc(output.size() + 1);
    if (!out) return nullptr;
    std::memcpy(out, output.c_str(), output.size() + 1);
    *out_size = output.size();
    return out;
}

extern "C" __declspec(dllexport) void obf_free(char* ptr) {
    std::free(ptr);
}