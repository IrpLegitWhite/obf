#include "../../../include/plugin/IPlugin.h"

#include <string>
#include <unordered_map>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cstdio>

extern "C" __declspec(dllexport) const char* obf_name() {
    return "rename";
}

extern "C" __declspec(dllexport) const char* obf_description() {
    return "C/C++/C#: rename user identifiers";
}

extern "C" __declspec(dllexport) int obf_api_version() {
    return OBFS_PLUGIN_API_VERSION;
}

extern "C" __declspec(dllexport) const char* obf_languages() {
    return "cpp,c,csharp";
}

// ★ Не применять к HTML/XML и уже переименованному
extern "C" __declspec(dllexport) int obf_can_apply(const char* code) {
    if (!code) return 0;

    if (std::strstr(code, "_0x0000000")) return 0;

    if (std::strstr(code, "<!DOCTYPE") ||
        std::strstr(code, "<!doctype") ||
        std::strstr(code, "<html") ||
        std::strstr(code, "<HTML") ||
        std::strstr(code, "<?xml") ||
        std::strstr(code, "<?XML")) {
        return 0;
    }

    bool hasCpp = false;
    if (std::strstr(code, "#include")) hasCpp = true;
    if (std::strstr(code, "#define"))  hasCpp = true;
    if (std::strstr(code, "int main")) hasCpp = true;
    if (std::strstr(code, "void "))    hasCpp = true;
    if (std::strstr(code, "std::"))    hasCpp = true;
    if (std::strstr(code, "namespace")) hasCpp = true;
    if (std::strstr(code, "DllMain"))  hasCpp = true;
    if (std::strstr(code, "__declspec")) hasCpp = true;

    if (!hasCpp) return 0;

    return 1;
}

// ============================================================
// Зарезервированные
// ============================================================
static const char* reserved[] = {
    // C/C++ ключевые
    "int","char","float","double","void","bool","long","short",
    "unsigned","signed","if","else","for","while","do","switch","case",
    "break","continue","return","class","struct","enum","union",
    "namespace","using","public","private","protected","virtual",
    "static","const","constexpr","new","delete","this","true","false",
    "nullptr","auto","template","typename","try","catch","throw",
    "sizeof","typedef","volatile","register","extern","inline","friend",
    "operator","explicit","mutable","goto","default","wchar_t","char16_t",
    "char32_t","noexcept","decltype","alignas","alignof","thread_local",
    "static_cast","dynamic_cast","const_cast","reinterpret_cast",

    // C# ключевые
    "foreach","var","readonly","override","sealed","abstract","interface",
    "event","delegate","params","ref","out","base","object",
    "decimal","byte","sbyte","internal","checked","unchecked",
    "lock","partial","where","select","from","get","set",
    "string","String","Console","System",

    // Препроцессор
    "include","define","ifdef","ifndef","endif","pragma",
    "elif","error","warning","line","undef",

    // Стандартные функции
    "std","printf","scanf","malloc","free","main","cout","cin","endl",
    "iostream","fstream","sstream","algorithm","cmath","cstdio",
    "cstring","cstdlib","vector","map","set","list","queue","stack",
    "pair","make_pair","size","length","push_back","pop_back",
    "begin","end","c_str","data","substr","find","insert","erase",

    // ★ WinAPI / CRT
    "DWORD","FILE","BOOL","HANDLE","LONG","ULONG","NTSTATUS","HMODULE",
    "PVOID","PULONG","TCHAR","WCHAR","LPSTR","LPCSTR","LPWSTR","LPCWSTR",
    "GetTempPathA","GetTempPathW","GetTickCount","GetTickCount64",
    "strncat","strcat","strlen","strcmp","strcpy","strncpy",
    "fopen","fopen_s","fprintf","fclose","fwrite","fread",
    "CheckRemoteDebuggerPresent","OutputDebugStringA","OutputDebugStringW",
    "SetLastError","GetLastError","GetCurrentProcess","GetCurrentThread",
    "IsDebuggerPresent","ExitProcess","TerminateProcess",
    "GetProcAddress","GetModuleHandleA","GetModuleHandleW","LoadLibraryA",
    "VirtualProtect","VirtualAlloc","VirtualFree","Sleep",
    "WinMain","CreateFileA","CloseHandle","WriteFile","ReadFile",
    "GetThreadContext","SetThreadContext","CONTEXT",
    "DWORD64","UINT","UINT32","UINT64","INT","INT32","INT64",
    "uint32_t","uint64_t","int32_t","int64_t","uintptr_t","intptr_t",

    // ★★★ DLL / декларации
    "__declspec","__cdecl","__stdcall","__fastcall","__thiscall",
    "dllexport","dllimport","naked","noinline","noreturn","novtable",
    "align","allocate","deprecated","jitintrinsic","selectany",
    "restrict","thread","uuid","property","safebuffers",
    "__asm","__try","__except","__finally","__leave",
    "__int8","__int16","__int32","__int64",
    "__forceinline","__inline","__w64","__ptr32","__ptr64","__unaligned",
    "APIENTRY","WINAPI","CALLBACK","WINAPIV","APIPRIVATE","PASCAL",
    "CDECL","STDAPICALLTYPE","STDMETHODCALLTYPE","STDMETHOD",
    "STDAPI","STDAPI_","HRESULT","REFIID","REFCLSID","LPVOID",
    "BSTR","IUnknown","IClassFactory","ULONG",
    "DllMain","DllRegisterServer","DllUnregisterServer","DllGetClassObject",
    "CLASS_E_CLASSNOTAVAILABLE","S_OK","E_NOTIMPL","E_FAIL",
    "TRUE","FALSE","NULL","GUID","UUID","IID","CLSID",
    "DLL_PROCESS_ATTACH","DLL_THREAD_ATTACH",
    "DLL_THREAD_DETACH","DLL_PROCESS_DETACH",
    "LoadLibraryW","FreeLibrary","GetModuleFileNameA",

    // Уже обфусцированное
    "obf_dec_str","obf_dec","obfDec","ObfHelper",

    // Наши служебные (точные имена)
    "_op","_dc","_ad","_x","_dx","_jnk","_noise","_junk","_i","_k","_n","_q","_dead",

    nullptr
};

static bool isReserved(const std::string& s) {
    for (int i = 0; reserved[i]; ++i)
        if (s == reserved[i]) return true;
    return false;
}

// Префиксы, которые НЕ переименовываем
static const char* reservedPrefixes[] = {
    "_jnk", "_noise", "_junk", "_dead", "_op", "_dc", "_ad", "_dx",
    "_0x",
    nullptr
};

static bool startsWithReservedPrefix(const std::string& s) {
    for (int i = 0; reservedPrefixes[i]; ++i) {
        size_t n = std::strlen(reservedPrefixes[i]);
        if (s.size() >= n && s.compare(0, n, reservedPrefixes[i]) == 0) return true;
    }
    return false;
}

// Точная проверка коротких служебных имён вида _n0, _k1, _q7
static bool isLegacyPolymorphVar(const std::string& s) {
    if (s.size() < 2 || s.size() > 8) return false;
    if (s[0] != '_') return false;
    char c = s[1];
    if (c != 'n' && c != 'k' && c != 'q') return false;
    for (size_t i = 2; i < s.size(); ++i)
        if (!std::isdigit((unsigned char)s[i])) return false;
    return true;
}

// Грубая проверка HTML
static bool looksLikeHtml(const std::string& s) {
    size_t limit = s.size() < 4096 ? s.size() : 4096;
    std::string head = s.substr(0, limit);

    if (head.find("<!DOCTYPE") != std::string::npos) return true;
    if (head.find("<!doctype") != std::string::npos) return true;
    if (head.find("<html") != std::string::npos) return true;
    if (head.find("<HTML") != std::string::npos) return true;
    if (head.find("<?xml") != std::string::npos) return true;
    if (head.find("<?XML") != std::string::npos) return true;

    return false;
}

// ============================================================
extern "C" __declspec(dllexport) char* obf_apply(const char* code, size_t size,
    size_t* out_size) {
    if (!code || !out_size) return nullptr;

    std::string input(code, size);

    if (looksLikeHtml(input)) {
        char* r = (char*)std::malloc(input.size() + 1);
        if (!r) return nullptr;
        std::memcpy(r, input.c_str(), input.size() + 1);
        *out_size = input.size();
        return r;
    }

    std::string out;
    out.reserve(input.size() * 2);

    std::unordered_map<std::string, std::string> map;
    unsigned long counter = 0;

    size_t i = 0;
    while (i < input.size()) {
        char c = input[i];

        // 1. Препроцессор
        if (c == '#') {
            size_t start = i;
            while (i < input.size() && input[i] != '\n') ++i;
            out += input.substr(start, i - start);
            if (i < input.size()) { out += '\n'; ++i; }
            continue;
        }

        // 2. Комментарии
        if (c == '/' && i + 1 < input.size()) {
            if (input[i + 1] == '/') {
                size_t start = i;
                while (i < input.size() && input[i] != '\n') ++i;
                out += input.substr(start, i - start);
                continue;
            }
            if (input[i + 1] == '*') {
                out += "/*"; i += 2;
                while (i + 1 < input.size() &&
                    !(input[i] == '*' && input[i + 1] == '/')) out += input[i++];
                if (i + 1 < input.size()) { out += "*/"; i += 2; }
                continue;
            }
        }

        // 3. Строки
        if (c == '"' || c == '\'') {
            char q = c;
            out += q; ++i;
            while (i < input.size() && input[i] != q) {
                if (input[i] == '\\' && i + 1 < input.size()) {
                    out += input[i++]; out += input[i++]; continue;
                }
                out += input[i++];
            }
            if (i < input.size()) { out += q; ++i; }
            continue;
        }

        // Пропускаем HTML-теги
        if (c == '<' && i + 1 < input.size() &&
            (std::isalpha((unsigned char)input[i + 1]) || input[i + 1] == '/' || input[i + 1] == '!')) {
            size_t tagEnd = input.find('>', i);
            if (tagEnd != std::string::npos && tagEnd - i < 512) {
                out += input.substr(i, tagEnd - i + 1);
                i = tagEnd + 1;
                continue;
            }
        }

        // 4. HEX
        if (c == '0' && i + 1 < input.size() &&
            (input[i + 1] == 'x' || input[i + 1] == 'X')) {
            size_t start = i;
            i += 2;
            while (i < input.size() && std::isxdigit((unsigned char)input[i])) ++i;
            while (i < input.size() && (input[i] == 'u' || input[i] == 'U' ||
                input[i] == 'l' || input[i] == 'L' ||
                input[i] == 'f' || input[i] == 'F')) ++i;
            out += input.substr(start, i - start);
            continue;
        }

        // 5. Десятичные
        if (std::isdigit((unsigned char)c)) {
            size_t start = i;
            while (i < input.size() && std::isdigit((unsigned char)input[i])) ++i;
            if (i < input.size() && input[i] == '.') {
                ++i;
                while (i < input.size() && std::isdigit((unsigned char)input[i])) ++i;
            }
            while (i < input.size() && (input[i] == 'u' || input[i] == 'U' ||
                input[i] == 'l' || input[i] == 'L' ||
                input[i] == 'f' || input[i] == 'F')) ++i;
            out += input.substr(start, i - start);
            continue;
        }

        // 6. Идентификатор
        if (std::isalpha((unsigned char)c) || c == '_') {
            size_t start = i;
            while (i < input.size() &&
                (std::isalnum((unsigned char)input[i]) || input[i] == '_')) ++i;
            std::string w = input.substr(start, i - start);

            // ★ FIX: пропускаем только если ПЕРЕД идентификатором стоит `::`
            // (например, std::cout — "cout" не трогаем, но "std" — тоже не трогаем)
            bool afterScope = false;
            {
                size_t k = start;
                while (k > 0 && (input[k - 1] == ' ' || input[k - 1] == '\t')) --k;
                if (k >= 2 && input[k - 1] == ':' && input[k - 2] == ':') {
                    afterScope = true;
                }
            }
            if (afterScope) { out += w; continue; }

            // ★ FIX: если ПОСЛЕ идентификатора стоит `::` — это namespace/класс, не трогаем
            {
                size_t k = i;
                while (k < input.size() && (input[k] == ' ' || input[k] == '\t')) ++k;
                if (k + 1 < input.size() && input[k] == ':' && input[k + 1] == ':') {
                    out += w; continue;
                }
            }

            if (isReserved(w) || w.size() < 2 ||
                startsWithReservedPrefix(w) ||
                isLegacyPolymorphVar(w)) {
                out += w;
            }
            else {
                auto it = map.find(w);
                if (it == map.end()) {
                    char buf[32];
                    std::snprintf(buf, sizeof(buf), "_0x%08lX", counter++);
                    map[w] = buf;
                    out += buf;
                }
                else {
                    out += it->second;
                }
            }
            continue;
        }

        out += c;
        ++i;
    }

    char* r = (char*)std::malloc(out.size() + 1);
    if (!r) return nullptr;
    std::memcpy(r, out.c_str(), out.size() + 1);
    *out_size = out.size();
    return r;
}

extern "C" __declspec(dllexport) void obf_free(char* ptr) {
    std::free(ptr);
}