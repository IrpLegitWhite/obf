#include "../../../include/plugin/IPlugin.h"

#include <string>
#include <random>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cctype>

static std::mt19937 rng(0xD11D11);

static int rnd(int lo, int hi) {
    std::uniform_int_distribution<int> d(lo, hi);
    return d(rng);
}

static bool isInsideIf0(const std::string& s, size_t pos) {
    int depth = 0;
    size_t i = 0;
    while (i < pos && i < s.size()) {
        if (s[i] == '#' && (i == 0 || s[i - 1] == '\n')) {
            size_t j = i + 1;
            while (j < s.size() && (s[j] == ' ' || s[j] == '\t')) ++j;
            if (s.compare(j, 4, "if 0") == 0 || s.compare(j, 5, "ifdef") == 0 ||
                s.compare(j, 6, "ifndef") == 0) ++depth;
            else if (s.compare(j, 5, "endif") == 0) { --depth; if (depth < 0) depth = 0; }
            while (i < s.size() && s[i] != '\n') ++i;
        }
        ++i;
    }
    return depth > 0;
}

// ─── 1. Ложные __declspec(dllexport) функции ────────────────
static std::string injectFakeExports(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 3);

    static const char* exports[] = {
        "#if 0\n"
        "    extern \"C\" __declspec(dllexport)\n"
        "    int __stdcall DllRegisterServer(void) { return 0; }\n"
        "    extern \"C\" __declspec(dllexport)\n"
        "    int __stdcall DllUnregisterServer(void) { return 0; }\n"
        "#endif\n",

        "#if 0\n"
        "    extern \"C\" __declspec(dllexport)\n"
        "    void __cdecl _hidden_entry(void) {}\n"
        "    extern \"C\" __declspec(dllexport)\n"
        "    int __fastcall _fast_entry(int x) { return x * 2; }\n"
        "#endif\n",

        "#if 0\n"
        "    extern \"C\" __declspec(dllexport) __declspec(noinline)\n"
        "    unsigned long __stdcall _compute_crc(const void* p, unsigned long n) {\n"
        "        const unsigned char* b = (const unsigned char*)p;\n"
        "        unsigned long h = 0xFFFFFFFFu;\n"
        "        for (unsigned long i = 0; i < n; ++i) {\n"
        "            h ^= b[i];\n"
        "            for (int k = 0; k < 8; ++k) h = (h >> 1) ^ (0xEDB88320u & -(h & 1));\n"
        "        }\n"
        "        return ~h;\n"
        "    }\n"
        "#endif\n",

        "#if 0\n"
        "    BOOL APIENTRY DllMain(HMODULE h, DWORD reason, LPVOID r) {\n"
        "        switch (reason) {\n"
        "            case DLL_PROCESS_ATTACH: break;\n"
        "            case DLL_THREAD_ATTACH:  break;\n"
        "            case DLL_THREAD_DETACH:  break;\n"
        "            case DLL_PROCESS_DETACH: break;\n"
        "        }\n"
        "        return TRUE;\n"
        "    }\n"
        "#endif\n",
    };
    const int exportsN = sizeof(exports) / sizeof(exports[0]);

    size_t i = 0;
    bool inStr = false; char strCh = 0;
    int lineCount = 0;
    while (i < in.size()) {
        char c = in[i];
        if (inStr) {
            out += c;
            if (c == '\\' && i + 1 < in.size()) { out += in[i + 1]; i += 2; continue; }
            if (c == strCh) inStr = false;
            if (c == '\n') ++lineCount;
            ++i; continue;
        }
        if (c == '"' || c == '\'') { inStr = true; strCh = c; out += c; ++i; continue; }

        if (c == '\n') {
            ++lineCount;
            out += c; ++i;
            if (lineCount > 0 && lineCount % 20 == 0 && rnd(0, 2) == 0 &&
                !isInsideIf0(in, i)) {
                int b = rnd(0, exportsN - 1);
                out += "\n";
                out += exports[b];
                out += "\n";
            }
            continue;
        }
        out += c; ++i;
    }
    return out;
}

// ─── 2. Ложные COM-интерфейсы ───────────────────────────────
static std::string injectFakeCom(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 3);

    static const char* com[] = {
        "#if 0\n"
        "    interface __declspec(uuid(\"00000000-0000-0000-0000-000000000001\"))\n"
        "    IHidden : public IUnknown {\n"
        "        virtual HRESULT __stdcall QueryInterface(REFIID, void**) = 0;\n"
        "        virtual ULONG   __stdcall AddRef() = 0;\n"
        "        virtual ULONG   __stdcall Release() = 0;\n"
        "        virtual HRESULT __stdcall Execute(BSTR cmd) = 0;\n"
        "    };\n"
        "#endif\n",

        "#if 0\n"
        "    STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {\n"
        "        return CLASS_E_CLASSNOTAVAILABLE;\n"
        "    }\n"
        "#endif\n",

        "#if 0\n"
        "    class _SecretFactory : public IClassFactory {\n"
        "    public:\n"
        "        HRESULT __stdcall QueryInterface(REFIID, void**) override { return E_NOTIMPL; }\n"
        "        ULONG   __stdcall AddRef() override { return 1; }\n"
        "        ULONG   __stdcall Release() override { return 1; }\n"
        "        HRESULT __stdcall CreateInstance(IUnknown*, REFIID, void**) override { return E_NOTIMPL; }\n"
        "        HRESULT __stdcall LockServer(BOOL) override { return S_OK; }\n"
        "    };\n"
        "#endif\n",
    };
    const int comN = sizeof(com) / sizeof(com[0]);

    size_t i = 0;
    bool inStr = false; char strCh = 0;
    int lineCount = 0;
    while (i < in.size()) {
        char c = in[i];
        if (inStr) {
            out += c;
            if (c == '\\' && i + 1 < in.size()) { out += in[i + 1]; i += 2; continue; }
            if (c == strCh) inStr = false;
            if (c == '\n') ++lineCount;
            ++i; continue;
        }
        if (c == '"' || c == '\'') { inStr = true; strCh = c; out += c; ++i; continue; }

        if (c == '\n') {
            ++lineCount;
            out += c; ++i;
            if (lineCount > 0 && lineCount % 26 == 0 && rnd(0, 2) == 0 &&
                !isInsideIf0(in, i)) {
                int b = rnd(0, comN - 1);
                out += "\n";
                out += com[b];
                out += "\n";
            }
            continue;
        }
        out += c; ++i;
    }
    return out;
}

// ─── 3. Ложные .def-секции ──────────────────────────────────
static std::string injectFakeDef(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 3);

    static const char* defs[] = {
        "#if 0\n"
        "    /* fake .def section */\n"
        "    LIBRARY   _secret\n"
        "    EXPORTS\n"
        "        DllRegisterServer   @1\n"
        "        DllUnregisterServer @2\n"
        "        _hidden_entry       @3\n"
        "        _fast_entry         @4\n"
        "        _compute_crc        @5\n"
        "#endif\n",
    };
    const int defsN = sizeof(defs) / sizeof(defs[0]);

    size_t i = 0;
    bool inStr = false; char strCh = 0;
    int lineCount = 0;
    while (i < in.size()) {
        char c = in[i];
        if (inStr) {
            out += c;
            if (c == '\\' && i + 1 < in.size()) { out += in[i + 1]; i += 2; continue; }
            if (c == strCh) inStr = false;
            if (c == '\n') ++lineCount;
            ++i; continue;
        }
        if (c == '"' || c == '\'') { inStr = true; strCh = c; out += c; ++i; continue; }

        if (c == '\n') {
            ++lineCount;
            out += c; ++i;
            if (lineCount > 0 && lineCount % 30 == 0 && rnd(0, 2) == 0 &&
                !isInsideIf0(in, i)) {
                out += "\n";
                out += defs[0];
                out += "\n";
            }
            continue;
        }
        out += c; ++i;
    }
    return out;
}

// ─── 4. Ложные #pragma comment(linker, ...) ─────────────────
static std::string injectFakeLinker(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 2);

    static const char* pragmas[] = {
        "#pragma comment(linker, \"/EXPORT:DllRegisterServer\")\n",
        "#pragma comment(linker, \"/EXPORT:DllUnregisterServer\")\n",
        "#pragma comment(linker, \"/EXPORT:_hidden_entry\")\n",
        "#pragma comment(linker, \"/ENTRY:DllMain\")\n",
        "#pragma comment(linker, \"/SECTION:.text,ERW\")\n",
        "#pragma comment(linker, \"/MERGE:.rdata=.text\")\n",
        "#pragma comment(linker, \"/DYNAMICBASE\")\n",
        "#pragma comment(linker, \"/NXCOMPAT\")\n",
        "#pragma comment(linker, \"/GUARD:CF\")\n",
    };
    const int pragmasN = sizeof(pragmas) / sizeof(pragmas[0]);

    size_t i = 0;
    bool inStr = false; char strCh = 0;
    int lineCount = 0;
    while (i < in.size()) {
        char c = in[i];
        if (inStr) {
            out += c;
            if (c == '\\' && i + 1 < in.size()) { out += in[i + 1]; i += 2; continue; }
            if (c == strCh) inStr = false;
            if (c == '\n') ++lineCount;
            ++i; continue;
        }
        if (c == '"' || c == '\'') { inStr = true; strCh = c; out += c; ++i; continue; }

        if (c == '\n') {
            ++lineCount;
            out += c; ++i;
            if (lineCount > 0 && lineCount % 14 == 0 && rnd(0, 2) == 0 &&
                !isInsideIf0(in, i)) {
                int b = rnd(0, pragmasN - 1);
                out += pragmas[b];
            }
            continue;
        }
        out += c; ++i;
    }
    return out;
}

// ─── 5. Ложные GUID ─────────────────────────────────────────
static std::string injectFakeGuids(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 2);

    static const char* guids[] = {
        "/* CLSID: {DEADBEEF-CAFE-BABE-1337-424242424242} */",
        "/* IID:   {00000000-0000-0000-C000-000000000046} */",
        "/* GUID:  {12345678-1234-1234-1234-123456789012} */",
        "/* UUID:  {ABCDEF00-1234-5678-90AB-CDEF01234567} */",
    };
    const int guidsN = sizeof(guids) / sizeof(guids[0]);

    size_t i = 0;
    bool inStr = false; char strCh = 0;
    int lineCount = 0;
    while (i < in.size()) {
        char c = in[i];
        if (inStr) {
            out += c;
            if (c == '\\' && i + 1 < in.size()) { out += in[i + 1]; i += 2; continue; }
            if (c == strCh) inStr = false;
            if (c == '\n') ++lineCount;
            ++i; continue;
        }
        if (c == '"' || c == '\'') { inStr = true; strCh = c; out += c; ++i; continue; }

        if (c == '\n') {
            ++lineCount;
            out += c; ++i;
            if (lineCount > 0 && lineCount % 9 == 0 && rnd(0, 2) == 0 &&
                !isInsideIf0(in, i)) {
                int b = rnd(0, guidsN - 1);
                int indent = rnd(0, 8);
                for (int k = 0; k < indent; ++k) out += ' ';
                out += guids[b];
                out += '\n';
            }
            continue;
        }
        out += c; ++i;
    }
    return out;
}

// ════════════════════════════════════════════════════════════
extern "C" __declspec(dllexport) const char* obf_name() {
    return "dll_noise";
}

extern "C" __declspec(dllexport) const char* obf_description() {
    return "DLL: fake exports, COM interfaces, .def, linker pragmas, GUIDs";
}

extern "C" __declspec(dllexport) int obf_api_version() {
    return OBFS_PLUGIN_API_VERSION;
}

extern "C" __declspec(dllexport) const char* obf_languages() {
    return "cpp,c";
}

extern "C" __declspec(dllexport) int obf_can_apply(const char* code) {
    if (!code) return 0;
    if (std::strstr(code, "dll_noise: applied")) return 0;
    return 1;
}

extern "C" __declspec(dllexport) char* obf_apply(const char* code, size_t size,
    size_t* out_size) {
    if (!code || !out_size) return nullptr;

    std::string s(code, size);

    s = injectFakeExports(s);
    s = injectFakeCom(s);
    s = injectFakeDef(s);
    s = injectFakeLinker(s);
    s = injectFakeGuids(s);

    s = "/* dll_noise: applied */\n" + s;

    char* out = (char*)std::malloc(s.size() + 1);
    if (!out) return nullptr;
    std::memcpy(out, s.c_str(), s.size() + 1);
    *out_size = s.size();
    return out;
}

extern "C" __declspec(dllexport) void obf_free(char* ptr) {
    std::free(ptr);
}