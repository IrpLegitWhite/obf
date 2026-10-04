#include "../../../include/plugin/IPlugin.h"

#include <string>
#include <random>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cctype>

static std::mt19937 rng(0x7A11DEE5);

static int rnd(int lo, int hi) {
    std::uniform_int_distribution<int> d(lo, hi);
    return d(rng);
}

// ─── Служебная: внутри <!-- --> ? ────────────────────────────
static bool isInsideComment(const std::string& s, size_t pos) {
    size_t open = s.rfind("<!--", pos);
    if (open == std::string::npos) return false;
    size_t close = s.find("-->", open);
    if (close == std::string::npos) return true;   // открыт, не закрыт
    return close >= pos;                            // закрыт после pos
}

// ─── Служебная: внутри <script> ? ────────────────────────────
static bool isInsideScript(const std::string& s, size_t pos) {
    size_t open = s.rfind("<script", pos);
    if (open == std::string::npos) return false;
    size_t close = s.find("</script>", open);
    if (close == std::string::npos) return true;
    return close >= pos;
}

// ─── 1. HTML-комментарии-ловушки ────────────────────────────
static std::string injectHtmlComments(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 2);

    static const char* traps[] = {
        "<!-- TODO: remove before prod -->",
        "<!-- FIXME: XSS here -->",
        "<!-- XXX: do not touch -->",
        "<!-- HACK: works, don't ask -->",
        "<!-- NOTE: legacy code -->",
        "<!-- WARNING: undefined behavior -->",
        "<!-- BUG: see issue #1337 -->",
        "<!-- DEBUG: logging disabled -->",
        "<!-- SECRET: password=admin123 -->",
        "<!-- key: 0xDEADBEEF -->",
        "<!-- DB: mysql://root:toor@10.0.0.1/prod -->",
        "<!-- JIRA: PROJ-4821 -->",
        "<!-- author: ivan.petrov@corp.local -->",
        "<!-- admin panel: /admin?debug=1 -->",
        "<!-- API_KEY: sk-live-4eC39HqLyjWDarjtT1zdp7dc -->",
        "<!-- JWT: eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiIxMjM0NTY3ODkwIn0 -->",
        "<!-- автор: Иван Петров -->",
        "<!-- σχόλιο: μη αγγίζεις -->",
        "<!-- コメント: 触るな -->",
    };
    const int trapsN = sizeof(traps) / sizeof(traps[0]);

    size_t i = 0;
    int lineCount = 0;
    while (i < in.size()) {
        char c = in[i];

        // не трогаем существующие комментарии
        if (c == '<' && i + 3 < in.size() && in.compare(i, 4, "<!--") == 0) {
            size_t j = in.find("-->", i);
            if (j == std::string::npos) { out += in.substr(i); break; }
            out += in.substr(i, j + 3 - i);
            i = j + 3;
            continue;
        }

        if (c == '\n') {
            ++lineCount;
            out += c; ++i;
            if (rnd(0, 2) == 0) {   // ★ каждые 3 строки (в среднем)
                int t = rnd(0, trapsN - 1);
                int indent = rnd(0, 8);
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

// ─── 2. Мусорные HTML-атрибуты ──────────────────────────────
static std::string injectFakeAttrs(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 2);

    static const char* attrs[] = {
        " data-debug=\"1\"",
        " data-secret=\"admin123\"",
        " data-api-key=\"sk-live-xxxxx\"",
        " data-token=\"eyJhbGciOiJIUzI1NiJ9\"",
        " data-env=\"production\"",
        " data-user=\"root\"",
        " data-backdoor=\"calc.exe\"",
        " data-jira=\"PROJ-4821\"",
        " data-trace=\"true\"",
        " data-admin=\"1\"",
        " data-internal=\"yes\"",
        " data-stage=\"prod\"",
        " data-key=\"0xDEADBEEF\"",
    };
    const int attrsN = sizeof(attrs) / sizeof(attrs[0]);

    size_t i = 0;
    while (i < in.size()) {
        char c = in[i];
        if (c == '<' && i + 1 < in.size() && std::isalpha((unsigned char)in[i + 1])) {
            size_t tagEnd = in.find('>', i);
            if (tagEnd == std::string::npos) { out += in.substr(i); break; }

            std::string tagContent = in.substr(i, tagEnd - i);

            // ★ FIX: если внутри тега уже есть '=', не трогаем
            if (tagContent.find('=') != std::string::npos) {
                out += in.substr(i, tagEnd - i + 1);
                i = tagEnd + 1;
                continue;
            }

            // ★ 1/2 шанс — вставляем мусорный атрибут
            if (rnd(0, 1) == 0) {
                int a = rnd(0, attrsN - 1);
                out += tagContent;
                out += attrs[a];
                out += '>';
                i = tagEnd + 1;
                continue;
            }
        }
        out += c; ++i;
    }
    return out;
}

// ─── 3. Ложные <script> и <style> ───────────────────────────
static std::string injectFakeScripts(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 3);

    static const char* scripts[] = {
        "<script>/* analytics: UA-XXXXX-Y */</script>\n",
        "<script>var _gaq = _gaq || []; _gaq.push(['_setAccount', 'UA-XXXXX-Y']);</script>\n",
        "<script>window.__debug = { api: 'sk-live-xxx', user: 'root' };</script>\n",
        "<script>window.__config = { env: 'prod', key: '0xDEADBEEF' };</script>\n",
        "<script type=\"application/json\">{\"key\":\"0xDEADBEEF\",\"user\":\"root\"}</script>\n",
        "<style>/* theme: dark, version: 1.2.3 */</style>\n",
        "<style>body { background: #000; /* hidden */ }</style>\n",
        "<noscript>Please enable JS to see the secret.</noscript>\n",
        "<meta name=\"generator\" content=\"secret-tool 1.0\">\n",
        "<meta name=\"api-endpoint\" content=\"https://api.example.com/v1\">\n",
        "<link rel=\"preload\" href=\"/admin/secret.js\" as=\"script\">\n",
        "<form action=\"https://evil.example.com/login\" method=\"post\"></form>\n",
    };
    const int scriptsN = sizeof(scripts) / sizeof(scripts[0]);

    size_t i = 0;
    int lineCount = 0;
    while (i < in.size()) {
        char c = in[i];

        if (c == '\n') {
            ++lineCount;
            out += c; ++i;
            if (rnd(0, 1) == 0) {   // ★ каждые 2 строки
                int s = rnd(0, scriptsN - 1);
                int indent = rnd(0, 4);
                for (int k = 0; k < indent; ++k) out += ' ';
                out += scripts[s];
                out += '\n';
            }
            continue;
        }
        out += c; ++i;
    }
    return out;
}

// ─── 4. Гомоглифы в HTML-комментариях ───────────────────────
static std::string homoglyphHtmlComments(const std::string& in) {
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
    while (i < in.size()) {
        if (in[i] == '<' && i + 3 < in.size() && in.compare(i, 4, "<!--") == 0) {
            out += "<!--";
            i += 4;
            while (i + 2 < in.size() && in.compare(i, 3, "-->") != 0) {
                char c = in[i];
                bool replaced = false;
                if (rnd(0, 1) == 0) {
                    for (int k = 0; k < mapN; ++k) {
                        if (c == map[k].lat) { out += map[k].uni; replaced = true; break; }
                    }
                }
                if (!replaced) out += c;
                ++i;
            }
            if (i + 2 < in.size()) { out += "-->"; i += 3; }
            continue;
        }
        out += in[i++];
    }
    return out;
}

// ─── 5. Сломанные байты в HTML-комментариях ─────────────────
static std::string injectBrokenHtmlBytes(const std::string& in) {
    static const char* broken[] = {
        "\xEF\xBF\xBD", "\xC0\x80", "\xED\xA0\x80",
        "\xFF\xFE", "\xFE\xFF", "\x80\x81\x82\x83",
        "\xF0\x28\x8C\x28", "\xC0\xC1\xC2", "\x80",
        "\xF8\x88\x80\x80\x80", "\xFC\x84\x80\x80\x80\x80",
    };
    const int brokenN = sizeof(broken) / sizeof(broken[0]);

    static const char* texts[] = {
        "stage2 payload", "encrypted config", "raw blob",
        "hidden module", "c2 beacon", "key material",
        "aes_key", "xor_key", "hmac", "iv",
    };
    const int textsN = sizeof(texts) / sizeof(texts[0]);

    std::string out;
    out.reserve(in.size() * 2);

    size_t i = 0;
    int lineCount = 0;
    while (i < in.size()) {
        char c = in[i];
        if (c == '\n') {
            ++lineCount;
            out += c; ++i;
            if (rnd(0, 2) == 0) {
                int b1 = rnd(0, brokenN - 1);
                int b2 = rnd(0, brokenN - 1);
                int t = rnd(0, textsN - 1);
                out += "<!-- ";
                out += broken[b1];
                out += " ";
                out += texts[t];
                out += " ";
                out += broken[b2];
                out += " -->\n";
            }
            continue;
        }
        out += c; ++i;
    }
    return out;
}

// ─── 6. Ложные inline-стили ─────────────────────────────────
static std::string injectFakeInlineStyles(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 2);

    static const char* styles[] = {
        " style=\"display:none\"",
        " style=\"visibility:hidden\"",
        " style=\"opacity:0\"",
        " style=\"position:absolute;left:-9999px\"",
        " style=\"width:0;height:0;overflow:hidden\"",
    };
    const int stylesN = sizeof(styles) / sizeof(styles[0]);

    size_t i = 0;
    while (i < in.size()) {
        char c = in[i];
        if (c == '<' && i + 1 < in.size() && std::isalpha((unsigned char)in[i + 1])) {
            size_t tagEnd = in.find('>', i);
            if (tagEnd == std::string::npos) { out += in.substr(i); break; }

            std::string tagContent = in.substr(i, tagEnd - i);

            if (tagContent.find('=') != std::string::npos) {
                out += in.substr(i, tagEnd - i + 1);
                i = tagEnd + 1;
                continue;
            }

            if (rnd(0, 2) == 0) {
                int s = rnd(0, stylesN - 1);
                out += tagContent;
                out += styles[s];
                out += '>';
                i = tagEnd + 1;
                continue;
            }
        }
        out += c; ++i;
    }
    return out;
}

// ─── 7. Ложные data-* с base64 ──────────────────────────────
static std::string injectFakeBase64Attrs(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 2);

    static const char* b64[] = {
        " data-payload=\"aGVsbG8gd29ybGQ=\"",
        " data-blob=\"SGVsbG8gV29ybGQhIFRoaXMgaXMgYSBmYWtlIHBheWxvYWQ=\"",
        " data-enc=\"5Y2B5LiA5Liq5rWL6K+V\"",
        " data-stage2=\"aHR0cHM6Ly9ldmlsLmV4YW1wbGUuY29t\"",
    };
    const int b64N = sizeof(b64) / sizeof(b64[0]);

    size_t i = 0;
    while (i < in.size()) {
        char c = in[i];
        if (c == '<' && i + 1 < in.size() && std::isalpha((unsigned char)in[i + 1])) {
            size_t tagEnd = in.find('>', i);
            if (tagEnd == std::string::npos) { out += in.substr(i); break; }

            std::string tagContent = in.substr(i, tagEnd - i);
            if (tagContent.find('=') != std::string::npos) {
                out += in.substr(i, tagEnd - i + 1);
                i = tagEnd + 1;
                continue;
            }

            if (rnd(0, 2) == 0) {
                int b = rnd(0, b64N - 1);
                out += tagContent;
                out += b64[b];
                out += '>';
                i = tagEnd + 1;
                continue;
            }
        }
        out += c; ++i;
    }
    return out;
}

// ─── 8. Ложные meta-теги ────────────────────────────────────
static std::string injectFakeMeta(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 3);

    static const char* metas[] = {
        "<meta name=\"api-key\" content=\"sk-live-4eC39HqLyjWDarjtT1zdp7dc\">\n",
        "<meta name=\"jwt\" content=\"eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9\">\n",
        "<meta name=\"build\" content=\"internal-secret-v1.2.3\">\n",
        "<meta name=\"env\" content=\"production\">\n",
        "<meta name=\"admin\" content=\"/admin?debug=1\">\n",
        "<meta http-equiv=\"refresh\" content=\"9999\">\n",
    };
    const int metasN = sizeof(metas) / sizeof(metas[0]);

    size_t i = 0;
    int lineCount = 0;
    while (i < in.size()) {
        char c = in[i];
        if (c == '\n') {
            ++lineCount;
            out += c; ++i;
            if (rnd(0, 1) == 0) {
                int m = rnd(0, metasN - 1);
                out += metas[m];
            }
            continue;
        }
        out += c; ++i;
    }
    return out;
}

// ─── 9. Hex-дамп в комментариях ─────────────────────────────
static std::string injectHexDump(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 3);

    size_t i = 0;
    int lineCount = 0;
    while (i < in.size()) {
        char c = in[i];
        if (c == '\n') {
            ++lineCount;
            out += c; ++i;
            if (rnd(0, 3) == 0) {
                out += "<!-- dump:\n";
                int lines = rnd(2, 4);
                for (int L = 0; L < lines; ++L) {
                    out += "     ";
                    for (int col = 0; col < 16; ++col) {
                        char b[8];
                        std::snprintf(b, sizeof(b), "%02X ", (unsigned)rnd(0, 255));
                        out += b;
                    }
                    out += '\n';
                }
                out += "-->\n";
            }
            continue;
        }
        out += c; ++i;
    }
    return out;
}

// ════════════════════════════════════════════════════════════
extern "C" __declspec(dllexport) const char* obf_name() {
    return "html_noise";
}

extern "C" __declspec(dllexport) const char* obf_description() {
    return "HTML/JS/CSS: comments, attrs, scripts, base64, hex-dump, broken bytes";
}

extern "C" __declspec(dllexport) int obf_api_version() {
    return OBFS_PLUGIN_API_VERSION;
}

extern "C" __declspec(dllexport) const char* obf_languages() {
    return "html,js,css";
}

extern "C" __declspec(dllexport) int obf_can_apply(const char* code) {
    if (!code) return 0;
    if (std::strstr(code, "html_noise: applied")) return 0;
    if (std::strstr(code, "html_noise: footer")) return 0;
    // только HTML/XML
    if (!std::strstr(code, "<!DOCTYPE") &&
        !std::strstr(code, "<html") &&
        !std::strstr(code, "<HTML") &&
        !std::strstr(code, "<?xml")) {
        return 0;
    }
    return 1;
}

extern "C" __declspec(dllexport) char* obf_apply(const char* code, size_t size,
    size_t* out_size) {
    if (!code || !out_size) return nullptr;

    std::string s(code, size);

    // ── пропорциональная частота: чем больше файл, тем чаще ловушки ──
    int totalLines = 1;
    for (char c : s) if (c == '\n') ++totalLines;

    // базовые ловушки
    s = injectHtmlComments(s);
    s = injectFakeAttrs(s);
    s = injectFakeInlineStyles(s);
    s = injectFakeBase64Attrs(s);
    s = injectFakeScripts(s);
    s = injectFakeMeta(s);
    s = homoglyphHtmlComments(s);
    s = injectBrokenHtmlBytes(s);
    s = injectHexDump(s);

    // ★ Гарантированный футер — работает даже на 1 строке
    s += "\n\n<!-- ═══════════════════════════════════════════════ -->\n";
    s += "<!-- html_noise: footer -->\n";
    s += "<!-- TODO: remove before prod -->\n";
    s += "<!-- FIXME: XSS here -->\n";
    s += "<!-- XXX: do not touch -->\n";
    s += "<!-- HACK: works, don't ask -->\n";
    s += "<!-- DB: mysql://root:toor@10.0.0.1/prod -->\n";
    s += "<!-- JWT: eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiIxMjM0NTY3ODkwIn0 -->\n";
    s += "<!-- password: admin123 -->\n";
    s += "<!-- API_KEY: sk-live-4eC39HqLyjWDarjtT1zdp7dc -->\n";
    s += "<!-- AWS: AKIAIOSFODNN7EXAMPLE -->\n";
    s += "<!-- Stripe: sk_test_51H8xYzK9mNpQrStUvWxYz -->\n";
    s += "<!-- SendGrid: SG.xxxx.yyyy -->\n";
    s += "<!-- admin panel: /admin?debug=1 -->\n";
    s += "<!-- stage2: https://evil.example.com/api -->\n";
    s += "<!-- ═══════════════════════════════════════════════ -->\n";
    s += "\n";
    s += "<script>window.__debug = { api: 'sk-live-xxx', user: 'root' };</script>\n";
    s += "<script>window.__config = { env: 'prod', key: '0xDEADBEEF' };</script>\n";
    s += "<script>var _gaq = _gaq || []; _gaq.push(['_setAccount', 'UA-XXXXX-Y']);</script>\n";
    s += "<script type=\"application/json\">{\"key\":\"0xDEADBEEF\",\"user\":\"root\"}</script>\n";
    s += "<style>/* theme: dark, version: 1.2.3 */</style>\n";
    s += "<style>body { background: #000; /* hidden */ }</style>\n";
    s += "<meta name=\"api-key\" content=\"sk-live-4eC39HqLyjWDarjtT1zdp7dc\">\n";
    s += "<meta name=\"env\" content=\"production\">\n";
    s += "<meta name=\"admin\" content=\"/admin?debug=1\">\n";
    s += "<link rel=\"preload\" href=\"/admin/secret.js\" as=\"script\">\n";
    s += "<form action=\"https://evil.example.com/login\" method=\"post\"></form>\n";
    s += "<noscript>Please enable JS to see the secret.</noscript>\n";
    s += "\n";
    s += "<!-- dump:\n";
    for (int L = 0; L < 4; ++L) {
        s += "     ";
        for (int col = 0; col < 16; ++col) {
            char b[8];
            std::snprintf(b, sizeof(b), "%02X ", (unsigned)rnd(0, 255));
            s += b;
        }
        s += '\n';
    }
    s += "-->\n";
    s += "\n";
    s += "<!--  stage2 payload  -->\n";
    s += "<!--  aes_key: 0123456789abcdef  -->\n";
    s += "<!--  hmac: deadbeefcafebabe13374242  -->\n";
    s += "<!--  iv: 000102030405060708090a0b0c0d0e0f  -->\n";
    s += "<!--  xor_key: 0x42  -->\n";

    s = "<!-- html_noise: applied -->\n" + s;

    char* out = (char*)std::malloc(s.size() + 1);
    if (!out) return nullptr;
    std::memcpy(out, s.c_str(), s.size() + 1);
    *out_size = s.size();
    return out;
}

extern "C" __declspec(dllexport) void obf_free(char* ptr) {
    std::free(ptr);
}