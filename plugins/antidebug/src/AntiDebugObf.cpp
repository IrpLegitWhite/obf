#include "../../../include/plugin/IPlugin.h"

#include <string>
#include <cstdlib>
#include <cstring>

extern "C" __declspec(dllexport) const char* obf_name() {
    return "antidebug";
}

extern "C" __declspec(dllexport) const char* obf_description() {
    return "C/C++: inject anti-debug checks";
}

extern "C" __declspec(dllexport) int obf_api_version() {
    return OBFS_PLUGIN_API_VERSION;
}

extern "C" __declspec(dllexport) const char* obf_languages() {
    return "cpp,c";
}

extern "C" __declspec(dllexport) int obf_can_apply(const char* code) {
    if (!code) return 0;
    // Не применять повторно, если уже вставили
    return (std::strstr(code, "main") &&
        !std::strstr(code, "IsDebuggerPresent")) ? 1 : 0;
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

    std::string header;
    if (input.find("#include <windows.h>") == std::string::npos &&
        input.find("#include <Windows.h>") == std::string::npos) {
        header = "#include <windows.h>\n";
    }

    static const char* ad =
        "\n    // obf: anti-debug\n"
        "    if (IsDebuggerPresent()) {\n"
        "        volatile int _ad = 1;\n"
        "        while (_ad) { _ad = 0; }\n"
        "    }\n";

    std::string output;
    output.reserve(input.size() + 512);
    output = header;
    output += input.substr(0, brace + 1);
    output += ad;
    output += input.substr(brace + 1);

    char* out = (char*)std::malloc(output.size() + 1);
    if (!out) return nullptr;
    std::memcpy(out, output.c_str(), output.size() + 1);
    *out_size = output.size();
    return out;
}

extern "C" __declspec(dllexport) void obf_free(char* ptr) {
    std::free(ptr);
}