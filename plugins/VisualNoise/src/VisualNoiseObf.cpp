#include "../../../include/plugin/IPlugin.h"

#include <string>
#include <random>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cctype>

static std::mt19937 rng(0xC0FFEE42);

static int rnd(int lo, int hi) {
    std::uniform_int_distribution<int> d(lo, hi);
    return d(rng);
}

// ════════════════════════════════════════════════════════════
//  СЛУЖЕБНЫЕ
// ════════════════════════════════════════════════════════════

// Грубая проверка: находимся ли внутри #if 0 ... #endif до позиции pos
static bool isInsideIf0(const std::string& s, size_t pos) {
    int depth = 0;
    size_t i = 0;
    while (i < pos && i < s.size()) {
        if (s[i] == '#' && (i == 0 || s[i - 1] == '\n')) {
            size_t j = i + 1;
            while (j < s.size() && (s[j] == ' ' || s[j] == '\t')) ++j;
            if (s.compare(j, 4, "if 0") == 0 || s.compare(j, 5, "ifdef") == 0 ||
                s.compare(j, 6, "ifndef") == 0) {
                ++depth;
            }
            else if (s.compare(j, 5, "endif") == 0) {
                --depth;
                if (depth < 0) depth = 0;
            }
            while (i < s.size() && s[i] != '\n') ++i;
        }
        ++i;
    }
    return depth > 0;
}

// Грубая проверка: находимся ли внутри { } до позиции pos
static bool isInsideFunction(const std::string& s, size_t pos) {
    int depth = 0;
    size_t i = 0;
    bool inStr = false; char strCh = 0;
    while (i < pos && i < s.size()) {
        char c = s[i];
        if (inStr) {
            if (c == '\\' && i + 1 < s.size()) { i += 2; continue; }
            if (c == strCh) inStr = false;
            ++i; continue;
        }
        if (c == '"' || c == '\'') { inStr = true; strCh = c; ++i; continue; }
        if (c == '/' && i + 1 < s.size() && (s[i + 1] == '/' || s[i + 1] == '*')) {
            // пропускаем комментарий
            if (s[i + 1] == '/') {
                while (i < s.size() && s[i] != '\n') ++i;
            }
            else {
                i += 2;
                while (i + 1 < s.size() && !(s[i] == '*' && s[i + 1] == '/')) ++i;
                if (i + 1 < s.size()) i += 2;
            }
            continue;
        }
        if (c == '{') ++depth;
        else if (c == '}') --depth;
        ++i;
    }
    return depth > 0;
}

// ════════════════════════════════════════════════════════════
//  БАЗОВЫЕ ФУНКЦИИ
// ════════════════════════════════════════════════════════════

static std::string noiseHexCase(const std::string& in) {
    std::string out;
    out.reserve(in.size());

    size_t i = 0;
    bool inStr = false; char strCh = 0;
    while (i < in.size()) {
        char c = in[i];
        if (inStr) {
            out += c;
            if (c == '\\' && i + 1 < in.size()) { out += in[i + 1]; i += 2; continue; }
            if (c == strCh) inStr = false;
            ++i; continue;
        }
        if (c == '"' || c == '\'') { inStr = true; strCh = c; out += c; ++i; continue; }

        if (c == '0' && i + 1 < in.size() && (in[i + 1] == 'x' || in[i + 1] == 'X')) {
            out += c; out += in[i + 1]; i += 2;
            while (i < in.size() && std::isxdigit((unsigned char)in[i])) {
                char h = in[i];
                if (h >= 'a' && h <= 'f') out += (rnd(0, 1) ? (char)std::toupper(h) : h);
                else if (h >= 'A' && h <= 'F') out += (rnd(0, 1) ? (char)std::tolower(h) : h);
                else out += h;
                ++i;
            }
            continue;
        }
        out += c; ++i;
    }
    return out;
}

static std::string noiseIndent(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 2);
    bool atLineStart = true;

    for (size_t i = 0; i < in.size(); ++i) {
        char c = in[i];
        if (atLineStart && (c == ' ' || c == '\t')) {
            int n = rnd(0, 8);
            for (int k = 0; k < n; ++k) out += (rnd(0, 3) == 0) ? '\t' : ' ';
            while (i < in.size() && (in[i] == ' ' || in[i] == '\t')) ++i;
            --i;
            atLineStart = false;
            continue;
        }
        out += c;
        atLineStart = (c == '\n');
    }
    return out;
}

static std::string injectFakeComments(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 2);

    static const char* traps[] = {
        "/* TODO: remove before release */",
        "/* FIXME: leak here */",
        "/* XXX: don't touch */",
        "/* HACK: works, don't ask */",
        "/* NOTE: legacy code */",
        "/* WARNING: undefined behavior */",
        "/* BUG: see issue #1337 */",
        "/* DEBUG: printf removed */",
        "/* SECRET: password=admin */",
        "/* key: 0xDEADBEEF */",
        "/* db: mysql://root:toor@10.0.0.1/prod */",
        "/* jira: PROJ-4821 */",
        "/* author: ivan.petrov@corp.local */",
        "/* автор: Иван Петров */",
        "/* σχόλιο: μη αγγίζεις */",
        "/* コメント: 触るな */",
    };
    const int trapsN = sizeof(traps) / sizeof(traps[0]);

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

        if (c == '/' && i + 1 < in.size() && (in[i + 1] == '/' || in[i + 1] == '*')) {
            out += c; ++i;
            if (i < in.size()) { out += in[i]; ++i; }
            while (i < in.size() && in[i] != '\n') { out += in[i]; ++i; }
            continue;
        }

        if (c == '\n') {
            ++lineCount;
            out += c; ++i;
            if (lineCount % 5 == 0 && rnd(0, 2) == 0 && !isInsideIf0(in, i)) {
                int t = rnd(0, trapsN - 1);
                int indent = rnd(0, 12);
                for (int k = 0; k < indent; ++k) out += ' ';
                out += traps[t];
                out += '\n';
            }
            continue;
        }
        out += c; ++i;
    }
    return out;
}

static std::string breakKeywords(const std::string& in) {
    static const char* kws[] = {
        "return","continue","break","static","const","volatile",
        "unsigned","signed","struct","class","public","private",
        nullptr
    };

    std::string out;
    out.reserve(in.size() * 2);

    size_t i = 0;
    bool inStr = false; char strCh = 0;
    while (i < in.size()) {
        char c = in[i];
        if (inStr) {
            out += c;
            if (c == '\\' && i + 1 < in.size()) { out += in[i + 1]; i += 2; continue; }
            if (c == strCh) inStr = false;
            ++i; continue;
        }
        if (c == '"' || c == '\'') { inStr = true; strCh = c; out += c; ++i; continue; }

        if (std::isalpha((unsigned char)c)) {
            size_t start = i;
            while (i < in.size() && (std::isalnum((unsigned char)in[i]) || in[i] == '_')) ++i;
            std::string w = in.substr(start, i - start);

            size_t j = start;
            while (j > 0 && (in[j - 1] == ' ' || in[j - 1] == '\t')) --j;
            bool afterHash = (j > 0 && in[j - 1] == '#');

            bool matched = false;
            if (!afterHash && !isInsideIf0(in, start)) {
                for (int k = 0; kws[k]; ++k) {
                    if (w == kws[k] && rnd(0, 3) == 0) {
                        size_t pos = (size_t)rnd(1, (int)w.size() - 1);
                        out.append(w, 0, pos);
                        out += "\\\n";
                        out.append(w, pos, std::string::npos);
                        matched = true;
                        break;
                    }
                }
            }
            if (!matched) out += w;
            continue;
        }
        out += c; ++i;
    }
    return out;
}

// ════════════════════════════════════════════════════════════
//  ЛОВУШКИ
// ════════════════════════════════════════════════════════════

static std::string injectFakeIf0(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 3);

    static const char* fakeBodies[] = {
        "#if 0\n"
        "    static const unsigned char _key[16] = {\n"
        "        0xDE,0xAD,0xBE,0xEF,0xCA,0xFE,0xBA,0xBE,\n"
        "        0x13,0x37,0x42,0x42,0xAB,0xCD,0xEF,0x00\n"
        "    };\n"
        "    static int _unlock(const unsigned char* k) {\n"
        "        int s = 0;\n"
        "        for (int i = 0; i < 16; ++i) s ^= k[i] << (i & 3);\n"
        "        return s;\n"
        "    }\n"
        "#endif\n",

        "#if 0\n"
        "    extern \"C\" int _decrypt(const char* buf, int len);\n"
        "#endif\n",

        "#if 0\n"
        "    struct _HiddenConfig {\n"
        "        unsigned long magic;\n"
        "        char password[32];\n"
        "        int   flags;\n"
        "    };\n"
        "    static _HiddenConfig _cfg = { 0xDEADBEEF, \"admin123\", 0xFF };\n"
        "#endif\n",

        "#ifdef _NEVER_DEFINED_\n"
        "    void _backdoor(void) { system(\"calc.exe\"); }\n"
        "#endif\n",

        "#if 0\n"
        "    #define SECRET_KEY 0x1337C0DE\n"
        "    #define API_ENDPOINT \"https://evil.example.com/api\"\n"
        "#endif\n",
    };
    const int bodiesN = sizeof(fakeBodies) / sizeof(fakeBodies[0]);

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
            if (lineCount > 0 && lineCount % 12 == 0 && rnd(0, 3) == 0 &&
                !isInsideIf0(in, i)) {
                int b = rnd(0, bodiesN - 1);
                out += "\n";
                out += fakeBodies[b];
                out += "\n";
            }
            continue;
        }
        out += c; ++i;
    }
    return out;
}

static std::string homoglyphComments(const std::string& in) {
    struct Map { char lat; const char* uni; };
    static const Map map[] = {
        {'a', "\xD0\xB0"}, {'c', "\xD1\x81"}, {'e', "\xD0\xB5"},
        {'o', "\xD0\xBE"}, {'p', "\xD1\x80"}, {'x', "\xD1\x85"},
        {'y', "\xD1\x83"}, {'A', "\xD0\x90"}, {'B', "\xD0\x92"},
        {'C', "\xD0\xA1"}, {'E', "\xD0\x95"}, {'H', "\xD0\x9D"},
        {'K', "\xD0\x9A"}, {'M', "\xD0\x9C"}, {'O', "\xD0\x9E"},
        {'P', "\xD0\xA0"}, {'T', "\xD0\xA2"}, {'X', "\xD0\xA5"},
    };
    const int mapN = sizeof(map) / sizeof(map[0]);

    std::string out;
    out.reserve(in.size() * 2);

    size_t i = 0;
    bool inStr = false; char strCh = 0;
    while (i < in.size()) {
        char c = in[i];
        if (inStr) {
            out += c;
            if (c == '\\' && i + 1 < in.size()) { out += in[i + 1]; i += 2; continue; }
            if (c == strCh) inStr = false;
            ++i; continue;
        }
        if (c == '"' || c == '\'') { inStr = true; strCh = c; out += c; ++i; continue; }

        if (c == '/' && i + 1 < in.size() && in[i + 1] == '/') {
            out += "//"; i += 2;
            while (i < in.size() && in[i] != '\n') {
                char ch = in[i];
                bool replaced = false;
                if (rnd(0, 1) == 0) {
                    for (int k = 0; k < mapN; ++k) {
                        if (ch == map[k].lat) { out += map[k].uni; replaced = true; break; }
                    }
                }
                if (!replaced) out += ch;
                ++i;
            }
            continue;
        }

        if (c == '/' && i + 1 < in.size() && in[i + 1] == '*') {
            out += "/*"; i += 2;
            while (i + 1 < in.size() && !(in[i] == '*' && in[i + 1] == '/')) {
                char ch = in[i];
                bool replaced = false;
                if (rnd(0, 1) == 0) {
                    for (int k = 0; k < mapN; ++k) {
                        if (ch == map[k].lat) { out += map[k].uni; replaced = true; break; }
                    }
                }
                if (!replaced) out += ch;
                ++i;
            }
            if (i + 1 < in.size()) { out += "*/"; i += 2; }
            continue;
        }

        out += c; ++i;
    }
    return out;
}

static std::string injectGotos(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 2);

    static unsigned long gotoId = 0;
    size_t i = 0;
    bool inStr = false; char strCh = 0;
    while (i < in.size()) {
        char c = in[i];
        if (inStr) {
            out += c;
            if (c == '\\' && i + 1 < in.size()) { out += in[i + 1]; i += 2; continue; }
            if (c == strCh) inStr = false;
            ++i; continue;
        }
        if (c == '"' || c == '\'') { inStr = true; strCh = c; out += c; ++i; continue; }

        if (c == '{') {
            size_t j = i;
            while (j > 0 && (in[j - 1] == ' ' || in[j - 1] == '\t' || in[j - 1] == '\n')) --j;
            bool isBlock = false;
            if (j > 0) {
                char p = in[j - 1];
                if (p == ')' || p == 'e' || p == 'o' || p == 'y') isBlock = true;
            }
            if (j >= 2) {
                size_t k = j - 1;
                while (k > 0 && (in[k - 1] == ' ' || in[k - 1] == '\t')) --k;
                if (k > 0 && in[k - 1] == '=') isBlock = false;
            }
            out += c;
            ++i;
            if (isBlock && rnd(0, 4) == 0 && !isInsideIf0(in, i)) {
                char buf[128];
                unsigned long id = gotoId++;
                std::snprintf(buf, sizeof(buf),
                    "\n    if (0) goto _lbl_%lu;\n_lbl_%lu: ;\n", id, id);
                out += buf;
            }
            continue;
        }
        out += c; ++i;
    }
    return out;
}

static std::string makeBlobArray(unsigned id) {
    char head[256];
    std::snprintf(head, sizeof(head),
        "\nstatic const unsigned char _blob_%u[256] = {\n", id);

    std::string out = head;
    for (int row = 0; row < 16; ++row) {
        out += "    ";
        for (int col = 0; col < 16; ++col) {
            char b[8];
            std::snprintf(b, sizeof(b), "0x%02X,", (unsigned)rnd(0, 255));
            out += b;
        }
        out += "\n";
    }
    out += "};\n";
    return out;
}

static std::string appendBlobArrays(const std::string& in) {
    std::string out = in;
    out += "\n\n/* polymorph: data section */\n";
    int n = rnd(1, 3);
    for (int k = 0; k < n; ++k) out += makeBlobArray((unsigned)rnd(1000, 9999));
    return out;
}

static std::string noiseWeirdWhitespace(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 2);
    bool atLineStart = true;

    size_t i = 0;
    while (i < in.size()) {
        char c = in[i];
        if (atLineStart && (c == ' ' || c == '\t')) {
            if (rnd(0, 3) == 0) {
                int n = rnd(1, 3);
                for (int k = 0; k < n; ++k) {
                    int w = rnd(0, 1);
                    out += (w == 0) ? '\v' : '\f';
                }
            }
            while (i < in.size() && (in[i] == ' ' || in[i] == '\t')) ++i;
            atLineStart = false;
            continue;
        }
        out += c;
        atLineStart = (c == '\n');
        ++i;
    }
    return out;
}

// ════════════════════════════════════════════════════════════
//  ЛЮТАЯ ЗАПУТАНИЦА
// ════════════════════════════════════════════════════════════

static std::string injectFakeAsm(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 3);

    static const char* asmBlocks[] = {
        "#if 0\n"
        "    __asm {\n"
        "        push    ebp\n"
        "        mov     ebp, esp\n"
        "        sub     esp, 0x40\n"
        "        xor     eax, eax\n"
        "        mov     [ebp-4], eax\n"
        "        mov     [ebp-8], eax\n"
        "        leave\n"
        "        ret\n"
        "    }\n"
        "#endif\n",

        "#if 0\n"
        "    __asm {\n"
        "        mov     eax, fs:[0x30]\n"
        "        movzx   eax, byte ptr [eax+2]\n"
        "        test    eax, eax\n"
        "        jnz     _debug_detected\n"
        "    }\n"
        "#endif\n",

        "#if 0\n"
        "    __asm {\n"
        "        rdtsc\n"
        "        mov     [ebp-0x10], eax\n"
        "        mov     [ebp-0x14], edx\n"
        "    }\n"
        "#endif\n",

        "#if 0\n"
        "    __asm {\n"
        "        call    _get_eip\n"
        "_get_eip:\n"
        "        pop     eax\n"
        "        sub     eax, 5\n"
        "    }\n"
        "#endif\n",
    };
    const int asmN = sizeof(asmBlocks) / sizeof(asmBlocks[0]);

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
            if (lineCount > 0 && lineCount % 18 == 0 && rnd(0, 2) == 0 &&
                !isInsideIf0(in, i)) {
                int b = rnd(0, asmN - 1);
                out += "\n";
                out += asmBlocks[b];
                out += "\n";
            }
            continue;
        }
        out += c; ++i;
    }
    return out;
}

static std::string injectFakeSecrets(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 3);

    static const char* secrets[] = {
        "#if 0\n"
        "    const char* _db_url = \"mysql://root:Pr0d_P@ss!@10.0.0.42/prod\";\n"
        "    const char* _api_key = \"sk-live-4eC39HqLyjWDarjtT1zdp7dc\";\n"
        "#endif\n",

        "#if 0\n"
        "    #define AWS_ACCESS_KEY_ID     \"AKIAIOSFODNN7EXAMPLE\"\n"
        "    #define AWS_SECRET_ACCESS_KEY \"wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY\"\n"
        "#endif\n",

        "#if 0\n"
        "    const char* _webhook = \"https://hooks.slack.com/services/T000/B000/XXX\";\n"
        "    const char* _discord = \"https://discord.com/api/webhooks/123/abc\";\n"
        "#endif\n",

        "#if 0\n"
        "    const char* _private_key =\n"
        "        \"-----BEGIN RSA PRIVATE KEY-----\\n\"\n"
        "        \"MIIEowIBAAKCAQEA1234567890abcdef...\\n\"\n"
        "        \"-----END RSA PRIVATE KEY-----\\n\";\n"
        "#endif\n",

        "#if 0\n"
        "    const char* _stripe = \"sk_test_51H8xYzK9mNpQrStUvWxYz\";\n"
        "    const char* _sendgrid = \"SG.xxxx.yyyy\";\n"
        "#endif\n",
    };
    const int secretsN = sizeof(secrets) / sizeof(secrets[0]);

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
                int b = rnd(0, secretsN - 1);
                out += "\n";
                out += secrets[b];
                out += "\n";
            }
            continue;
        }
        out += c; ++i;
    }
    return out;
}

static std::string injectRtlOverride(const std::string& in) {
    static const char* RTL = "\xE2\x80\xAE";
    static const char* PDF = "\xE2\x80\xAC";

    std::string out;
    out.reserve(in.size() * 2);

    static const char* payloads[] = {
        "return true;",
        "if (x == 0) return;",
        "password = \"admin\";",
        "secret = 0xDEADBEEF;",
        "goto exit;",
        "while (1) {}",
    };
    const int payloadsN = sizeof(payloads) / sizeof(payloads[0]);

    size_t i = 0;
    bool inStr = false; char strCh = 0;
    while (i < in.size()) {
        char c = in[i];
        if (inStr) {
            out += c;
            if (c == '\\' && i + 1 < in.size()) { out += in[i + 1]; i += 2; continue; }
            if (c == strCh) inStr = false;
            ++i; continue;
        }
        if (c == '"' || c == '\'') { inStr = true; strCh = c; out += c; ++i; continue; }

        if (c == '/' && i + 1 < in.size() && in[i + 1] == '/') {
            out += "//"; i += 2;
            size_t j = i;
            while (j < in.size() && in[j] != '\n') ++j;
            if (rnd(0, 2) == 0) {
                int p = rnd(0, payloadsN - 1);
                out += ' ';
                out += RTL;
                out += payloads[p];
                out += PDF;
                out += ' ';
            }
            while (i < j) { out += in[i]; ++i; }
            continue;
        }

        out += c; ++i;
    }
    return out;
}

static std::string injectFakePragmas(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 2);

    static const char* pragmas[] = {
        "#pragma warning(disable: 4996)\n",
        "#pragma warning(disable: 4244)\n",
        "#pragma warning(disable: 4100)\n",
        "#pragma intrinsic(memcpy, memset)\n",
        "#pragma optimize(\"\", on)\n",
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
            if (lineCount > 0 && lineCount % 15 == 0 && rnd(0, 2) == 0 &&
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

static std::string injectFakeAntiDebug(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 3);

    static const char* antiDebug[] = {
        "#if 0\n"
        "    static bool _is_debugged(void) {\n"
        "        if (IsDebuggerPresent()) return true;\n"
        "        BOOL dbg = FALSE;\n"
        "        CheckRemoteDebuggerPresent(GetCurrentProcess(), &dbg);\n"
        "        if (dbg) return true;\n"
        "        return false;\n"
        "    }\n"
        "#endif\n",

        "#if 0\n"
        "    DWORD _t1 = GetTickCount();\n"
        "    for (volatile int i = 0; i < 1000000; ++i) {}\n"
        "    DWORD _t2 = GetTickCount();\n"
        "    if ((_t2 - _t1) > 100) { ExitProcess(0); }\n"
        "#endif\n",

        "#if 0\n"
        "    typedef NTSTATUS (NTAPI* pNtQIT)(HANDLE, ULONG, PVOID, ULONG, PULONG);\n"
        "    HMODULE _ntdll = GetModuleHandleA(\"ntdll.dll\");\n"
        "    pNtQIT _fn = (pNtQIT)GetProcAddress(_ntdll, \"NtQueryInformationThread\");\n"
        "    if (_fn) { }\n"
        "#endif\n",
    };
    const int antiN = sizeof(antiDebug) / sizeof(antiDebug[0]);

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
            if (lineCount > 0 && lineCount % 25 == 0 && rnd(0, 2) == 0 &&
                !isInsideIf0(in, i)) {
                int b = rnd(0, antiN - 1);
                out += "\n";
                out += antiDebug[b];
                out += "\n";
            }
            continue;
        }
        out += c; ++i;
    }
    return out;
}

static std::string injectFakeThreads(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 3);

    static const char* threads[] = {
        "#if 0\n"
        "    #include <thread>\n"
        "    #include <mutex>\n"
        "    static std::mutex _mtx;\n"
        "    static std::vector<std::thread> _workers;\n"
        "    for (int i = 0; i < 4; ++i) {\n"
        "        _workers.emplace_back([i]() {\n"
        "            std::lock_guard<std::mutex> _lk(_mtx);\n"
        "        });\n"
        "    }\n"
        "    for (auto& t : _workers) t.join();\n"
        "#endif\n",

        "#if 0\n"
        "    std::atomic<bool> _stop{false};\n"
        "    std::thread _mon([&]() {\n"
        "        while (!_stop.load()) { }\n"
        "    });\n"
        "    _mon.detach();\n"
        "#endif\n",

        "#if 0\n"
        "    static std::condition_variable _cv;\n"
        "    static std::unique_lock<std::mutex> _ul(_mtx);\n"
        "    _cv.wait_for(_ul, std::chrono::seconds(5));\n"
        "#endif\n",
    };
    const int threadsN = sizeof(threads) / sizeof(threads[0]);

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
            if (lineCount > 0 && lineCount % 22 == 0 && rnd(0, 2) == 0 &&
                !isInsideIf0(in, i)) {
                int b = rnd(0, threadsN - 1);
                out += "\n";
                out += threads[b];
                out += "\n";
            }
            continue;
        }
        out += c; ++i;
    }
    return out;
}

static std::string injectFakeTemplates(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 3);

    static const char* tmpls[] = {
        "#if 0\n"
        "    template<typename T, int N>\n"
        "    struct _Meta { static constexpr int value = N * sizeof(T); };\n"
        "    static_assert(_Meta<int, 4>::value == 16, \"meta fail\");\n"
        "#endif\n",

        "#if 0\n"
        "    template<int N>\n"
        "    struct _Fact { static constexpr int value = N * _Fact<N-1>::value; };\n"
        "    template<> struct _Fact<0> { static constexpr int value = 1; };\n"
        "    static_assert(_Fact<5>::value == 120, \"fact fail\");\n"
        "#endif\n",

        "#if 0\n"
        "    template<typename T>\n"
        "    constexpr T _clamp(T v, T lo, T hi) {\n"
        "        return v < lo ? lo : (v > hi ? hi : v);\n"
        "    }\n"
        "    static_assert(_clamp(5, 0, 10) == 5, \"clamp fail\");\n"
        "#endif\n",
    };
    const int tmplsN = sizeof(tmpls) / sizeof(tmpls[0]);

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
            if (lineCount > 0 && lineCount % 24 == 0 && rnd(0, 2) == 0 &&
                !isInsideIf0(in, i)) {
                int b = rnd(0, tmplsN - 1);
                out += "\n";
                out += tmpls[b];
                out += "\n";
            }
            continue;
        }
        out += c; ++i;
    }
    return out;
}

static std::string injectFakeBase64(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 2);

    static const char* payloads[] = {
        "/* payload: aGVsbG8gd29ybGQ= */",
        "/* blob: SGVsbG8gV29ybGQhIFRoaXMgaXMgYSBmYWtlIHBheWxvYWQ= */",
        "/* encrypted: 5Y2B5LiA5Liq5rWL6K+V */",
        "/* xor_key: 0x42 */",
        "/* aes_key: 0123456789abcdef0123456789abcdef */",
        "/* iv: 000102030405060708090a0b0c0d0e0f */",
        "/* hmac: deadbeefcafebabe13374242 */",
        "/* stage2_url: aHR0cHM6Ly9ldmlsLmV4YW1wbGUuY29t */",
    };
    const int payloadsN = sizeof(payloads) / sizeof(payloads[0]);

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
            if (lineCount > 0 && lineCount % 7 == 0 && rnd(0, 2) == 0 &&
                !isInsideIf0(in, i)) {
                int b = rnd(0, payloadsN - 1);
                int indent = rnd(0, 8);
                for (int k = 0; k < indent; ++k) out += ' ';
                out += payloads[b];
                out += '\n';
            }
            continue;
        }
        out += c; ++i;
    }
    return out;
}

// ★ FIX: теперь #include вставляем ТОЛЬКО вне функций
static std::string injectFakeIncludes(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 3);

    static const char* includes[] = {
        "#if 0\n"
        "    #include <openssl/aes.h>\n"
        "    #include <openssl/rsa.h>\n"
        "    #include <openssl/sha.h>\n"
        "#endif\n",

        "#if 0\n"
        "    #include <boost/asio.hpp>\n"
        "    #include <boost/beast.hpp>\n"
        "#endif\n",

        "#if 0\n"
        "    #include <curl/curl.h>\n"
        "    #include <zlib.h>\n"
        "#endif\n",

        "#if 0\n"
        "    #include <winsock2.h>\n"
        "    #include <ws2tcpip.h>\n"
        "#endif\n",
    };
    const int incN = sizeof(includes) / sizeof(includes[0]);

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
            // ★ FIX: только на верхнем уровне (вне функций) и вне #if 0
            if (lineCount > 0 && lineCount % 16 == 0 && rnd(0, 2) == 0 &&
                !isInsideIf0(in, i) && !isInsideFunction(in, i)) {
                int b = rnd(0, incN - 1);
                out += "\n";
                out += includes[b];
                out += "\n";
            }
            continue;
        }
        out += c; ++i;
    }
    return out;
}

static std::string injectFakeExports(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 3);

    static const char* exports[] = {
        "#if 0\n"
        "    extern \"C\" __declspec(dllexport)\n"
        "    void __stdcall _hidden_export_A(void) {}\n"
        "    extern \"C\" __declspec(dllexport)\n"
        "    int __cdecl _hidden_export_B(int x) { return x * 2; }\n"
        "#endif\n",

        "#if 0\n"
        "    extern \"C\" __declspec(dllexport) __declspec(noinline)\n"
        "    unsigned long _compute_checksum(const void* p, unsigned long n) {\n"
        "        const unsigned char* b = (const unsigned char*)p;\n"
        "        unsigned long h = 2166136261u;\n"
        "        for (unsigned long i = 0; i < n; ++i) { h ^= b[i]; h *= 16777619u; }\n"
        "        return h;\n"
        "    }\n"
        "#endif\n",
    };
    const int expN = sizeof(exports) / sizeof(exports[0]);

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
                !isInsideIf0(in, i) && !isInsideFunction(in, i)) {
                int b = rnd(0, expN - 1);
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

// ════════════════════════════════════════════════════════════
//  ★ НОВЫЕ 3 ЛОВУШКИ
// ════════════════════════════════════════════════════════════

// ─── 20. Ложные #error / #warning / #pragma message ─────────
static std::string injectFakeErrors(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 3);

    static const char* errors[] = {
        "#if 0\n"
        "    #error \"This file requires a valid license key\"\n"
        "#endif\n",

        "#if 0\n"
        "    #warning \"Deprecated API usage detected\"\n"
        "#endif\n",

        "#if 0\n"
        "    #pragma message(\"INFO: encryption enabled\")\n"
        "    #pragma message(\"WARN: fallback to software mode\")\n"
        "#endif\n",

        "#ifdef _REQUIRE_LICENSE_\n"
        "    #error \"Missing license: contact vendor@example.com\"\n"
        "#endif\n",

        "#if 0\n"
        "    #error \"Unsupported platform: only x64 Windows builds are allowed\"\n"
        "#endif\n",
    };
    const int errN = sizeof(errors) / sizeof(errors[0]);

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
            if (lineCount > 0 && lineCount % 19 == 0 && rnd(0, 2) == 0 &&
                !isInsideIf0(in, i)) {
                int b = rnd(0, errN - 1);
                out += "\n";
                out += errors[b];
                out += "\n";
            }
            continue;
        }
        out += c; ++i;
    }
    return out;
}

// ─── 21. Ложные std::map / unordered_map ────────────────────
static std::string injectFakeMaps(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 3);

    static const char* maps[] = {
        "#if 0\n"
        "    static const std::unordered_map<std::string, int> _opcodes = {\n"
        "        { \"mov\",  0x88 }, { \"push\", 0x50 }, { \"pop\",  0x58 },\n"
        "        { \"call\", 0xE8 }, { \"ret\",  0xC3 }, { \"jmp\",  0xE9 },\n"
        "        { \"nop\",  0x90 }, { \"int3\", 0xCC }, { \"xor\",  0x31 }\n"
        "    };\n"
        "#endif\n",

        "#if 0\n"
        "    static const std::map<std::string, std::string> _config = {\n"
        "        { \"host\",    \"10.0.0.42\" },\n"
        "        { \"port\",    \"8443\" },\n"
        "        { \"user\",    \"admin\" },\n"
        "        { \"pass\",    \"Pr0d_P@ss!\" },\n"
        "        { \"timeout\", \"30\" }\n"
        "    };\n"
        "#endif\n",

        "#if 0\n"
        "    static const std::unordered_map<char, int> _b64 = {\n"
        "        { 'A', 0 }, { 'B', 1 }, { 'C', 2 }, /* ... */ { 'z', 51 },\n"
        "        { '0', 52 }, { '1', 53 }, { '+', 62 }, { '/', 63 }\n"
        "    };\n"
        "#endif\n",

        "#if 0\n"
        "    static const std::map<int, const char*> _errors = {\n"
        "        { 0x00, \"OK\" }, { 0x01, \"INVALID_PARAM\" },\n"
        "        { 0x02, \"NOT_FOUND\" }, { 0x03, \"ACCESS_DENIED\" },\n"
        "        { 0x04, \"TIMEOUT\" }, { 0x05, \"INTERNAL\" }\n"
        "    };\n"
        "#endif\n",
    };
    const int mapsN = sizeof(maps) / sizeof(maps[0]);

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
            if (lineCount > 0 && lineCount % 23 == 0 && rnd(0, 2) == 0 &&
                !isInsideIf0(in, i)) {
                int b = rnd(0, mapsN - 1);
                out += "\n";
                out += maps[b];
                out += "\n";
            }
            continue;
        }
        out += c; ++i;
    }
    return out;
}

// ─── 22. Ложные лямбды / reinterpret_cast / std::function ───
static std::string injectFakeLambdas(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 3);

    static const char* lambdas[] = {
        "#if 0\n"
        "    auto _worker = [&](int x) -> int {\n"
        "        return x * 2 + _offset;\n"
        "    };\n"
        "    _worker(42);\n"
        "#endif\n",

        "#if 0\n"
        "    uintptr_t _addr = reinterpret_cast<uintptr_t>(&_cfg);\n"
        "    void* _p = reinterpret_cast<void*>(_addr ^ 0xDEADBEEF);\n"
        "    unsigned char* _b = reinterpret_cast<unsigned char*>(_p);\n"
        "#endif\n",

        "#if 0\n"
        "    std::function<int(int)> _fn = std::bind(&_handler, this, std::placeholders::_1);\n"
        "    auto _on_event = [this](int code) {\n"
        "        if (code == 0) { _dispatch(0); return; }\n"
        "        _logger->log(code);\n"
        "    };\n"
        "#endif\n",

        "#if 0\n"
        "    constexpr int _crc_table[16] = {\n"
        "        0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,\n"
        "        0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF\n"
        "    };\n"
        "#endif\n",

        "#if 0\n"
        "    union _Pun { float f; uint32_t u; };\n"
        "    _Pun _p; _p.f = 3.14f;\n"
        "    uint32_t _bits = _p.u;\n"
        "#endif\n",
    };
    const int lambdasN = sizeof(lambdas) / sizeof(lambdas[0]);

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
            if (lineCount > 0 && lineCount % 21 == 0 && rnd(0, 2) == 0 &&
                !isInsideIf0(in, i)) {
                int b = rnd(0, lambdasN - 1);
                out += "\n";
                out += lambdas[b];
                out += "\n";
            }
            continue;
        }
        out += c; ++i;
    }
    return out;
}

// ════════════════════════════════════════════════════════════
extern "C" __declspec(dllexport) const char* obf_name() {
    return "visual_noise";
}

extern "C" __declspec(dllexport) const char* obf_description() {
    return "Visual noise + heavy decoys (asm, secrets, threads, errors, maps, lambdas, RTL)";
}

extern "C" __declspec(dllexport) int obf_api_version() {
    return OBFS_PLUGIN_API_VERSION;
}

extern "C" __declspec(dllexport) const char* obf_languages() {
    return "cpp,c";
}

extern "C" __declspec(dllexport) int obf_can_apply(const char* code) {
    if (!code) return 0;
    if (std::strstr(code, "visual_noise: applied")) return 0;
    return 1;
}

extern "C" __declspec(dllexport) char* obf_apply(const char* code, size_t size,
    size_t* out_size) {
    if (!code || !out_size) return nullptr;

    std::string s(code, size);

    // ── базовый шум ──
    s = noiseHexCase(s);
    s = noiseIndent(s);
    s = noiseWeirdWhitespace(s);
    s = injectFakeComments(s);

    // ── ловушки ──
    s = injectFakeIf0(s);
    s = injectFakeAsm(s);
    s = injectFakeSecrets(s);
    s = injectFakePragmas(s);
    s = injectFakeAntiDebug(s);
    s = injectFakeThreads(s);
    s = injectFakeTemplates(s);
    s = injectFakeBase64(s);
    s = injectFakeIncludes(s);      // ★ FIX — только вне функций
    s = injectFakeExports(s);
    s = injectFakeErrors(s);        // ★ 20
    s = injectFakeMaps(s);          // ★ 21
    s = injectFakeLambdas(s);       // ★ 22
    s = injectGotos(s);
    s = appendBlobArrays(s);
    s = injectRtlOverride(s);
    s = homoglyphComments(s);

    // ── разрывы ──
    s = breakKeywords(s);

    s = "/* visual_noise: applied */\n" + s;

    char* out = (char*)std::malloc(s.size() + 1);
    if (!out) return nullptr;
    std::memcpy(out, s.c_str(), s.size() + 1);
    *out_size = s.size();
    return out;
}

extern "C" __declspec(dllexport) void obf_free(char* ptr) {
    std::free(ptr);
}