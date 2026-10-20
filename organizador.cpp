#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace jp {

enum class T { NULO, BOOL, NUM, STR, ARR, OBJ };

struct V {
    T t = T::NULO;
    bool bl = false;
    double num = 0;
    std::string str;
    std::vector<V> arr;
    std::map<std::string, V> obj;

    const V* get(const std::string& k) const {
        if (t != T::OBJ) return nullptr;
        auto it = obj.find(k);
        return it == obj.end() ? nullptr : &it->second;
    }
    const std::string* getStr(const std::string& k) const {
        const V* v = get(k);
        return (v && v->t == T::STR) ? &v->str : nullptr;
    }
};

struct Error {};

inline void ws(const char*& p) {
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
}

inline void expect(const char*& p, char c) {
    if (*p != c) throw Error();
    ++p;
}

inline unsigned hexv(char c) {
    if (c >= '0' && c <= '9') return (unsigned)(c - '0');
    if (c >= 'a' && c <= 'f') return (unsigned)(c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return (unsigned)(c - 'A' + 10);
    throw Error();
}

static void pushUtf8(std::string& out, unsigned cp) {
    if (cp < 0x80) {
        out += (char)cp;
    } else if (cp < 0x800) {
        out += (char)(0xC0 | (cp >> 6));
        out += (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += (char)(0xE0 | (cp >> 12));
        out += (char)(0x80 | ((cp >> 6) & 0x3F));
        out += (char)(0x80 | (cp & 0x3F));
    } else {
        out += (char)(0xF0 | (cp >> 18));
        out += (char)(0x80 | ((cp >> 12) & 0x3F));
        out += (char)(0x80 | ((cp >> 6) & 0x3F));
        out += (char)(0x80 | (cp & 0x3F));
    }
}

std::string parseString(const char*& p) {
    expect(p, '"');
    std::string out;
    while (*p && *p != '"') {
        char c = *p;
        if (c == '\\') {
            ++p;
            if (!*p) throw Error();
            switch (*p) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    unsigned cp = 0;
                    for (int i = 0; i < 4; ++i) {
                        ++p;
                        if (!*p) throw Error();
                        cp = (cp << 4) | hexv(*p);
                    }
                    if (cp >= 0xD800 && cp <= 0xDBFF && p[1] == '\\' && p[2] == 'u') {
                        const char* q = p + 2;
                        unsigned lo = 0;
                        for (int i = 0; i < 4; ++i) {
                            ++q;
                            if (!*q) throw Error();
                            lo = (lo << 4) | hexv(*q);
                        }
                        if (lo >= 0xDC00 && lo <= 0xDFFF) {
                            unsigned code = 0x10000u + (((cp - 0xD800u) << 10) | (lo - 0xDC00u));
                            pushUtf8(out, code);
                            p = q;
                            break;
                        }
                    }
                    pushUtf8(out, cp);
                    break;
                }
                default: throw Error();
            }
        } else {
            out += c;
        }
        ++p;
    }
    expect(p, '"');
    return out;
}

V parseValue(const char*& p);

V parseArray(const char*& p) {
    expect(p, '[');
    V v; v.t = T::ARR;
    ws(p);
    if (*p == ']') { ++p; return v; }
    for (;;) {
        ws(p);
        v.arr.push_back(parseValue(p));
        ws(p);
        if (*p == ',') { ++p; continue; }
        expect(p, ']');
        break;
    }
    return v;
}

V parseObject(const char*& p) {
    expect(p, '{');
    V v; v.t = T::OBJ;
    ws(p);
    if (*p == '}') { ++p; return v; }
    for (;;) {
        ws(p);
        std::string k = parseString(p);
        ws(p);
        expect(p, ':');
        ws(p);
        v.obj.emplace(std::move(k), parseValue(p));
        ws(p);
        if (*p == ',') { ++p; continue; }
        expect(p, '}');
        break;
    }
    return v;
}

V parseValue(const char*& p) {
    ws(p);
    switch (*p) {
        case 'n': {
            static const char n[] = "null";
            for (int i = 0; i < 4; ++i)
                if (p[i] != n[i]) throw Error();
            p += 4;
            V v;
            return v;
        }
        case 't': {
            static const char tt[] = "true";
            for (int i = 0; i < 4; ++i)
                if (p[i] != tt[i]) throw Error();
            p += 4;
            V v; v.t = T::BOOL; v.bl = true;
            return v;
        }
        case 'f': {
            static const char ff[] = "false";
            for (int i = 0; i < 5; ++i)
                if (p[i] != ff[i]) throw Error();
            p += 5;
            V v; v.t = T::BOOL; v.bl = false;
            return v;
        }
        case '"': {
            V v; v.t = T::STR; v.str = parseString(p);
            return v;
        }
        case '[': return parseArray(p);
        case '{': return parseObject(p);
        default: {
            const char* s = p;
            if (*p == '-') ++p;
            if (*p < '0' || *p > '9') throw Error();
            while (*p >= '0' && *p <= '9') ++p;
            if (*p == '.') {
                ++p;
                while (*p >= '0' && *p <= '9') ++p;
            }
            if (*p == 'e' || *p == 'E') {
                ++p;
                if (*p == '+' || *p == '-') ++p;
                if (*p < '0' || *p > '9') throw Error();
                while (*p >= '0' && *p <= '9') ++p;
            }
            V v; v.t = T::NUM; v.num = std::strtod(s, nullptr);
            return v;
        }
    }
}

V parse(const std::string& data) {
    const char* p = data.c_str();
    V v = parseValue(p);
    ws(p);
    if (*p) throw Error();
    return v;
}

static void escStr(const std::string& s, std::string& out) {
    out += '"';
    for (char ch : s) {
        unsigned char c = (unsigned char)ch;
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char b[8];
                    snprintf(b, sizeof b, "\\u%04x", c);
                    out += b;
                } else {
                    out += (char)c;
                }
        }
    }
    out += '"';
}

void serialize(const V& v, std::string& out) {
    switch (v.t) {
        case T::NULO: out += "null"; break;
        case T::BOOL: out += v.bl ? "true" : "false"; break;
        case T::NUM: {
            char b[32];
            snprintf(b, sizeof b, "%.17g", v.num);
            out += b;
            break;
        }
        case T::STR: escStr(v.str, out); break;
        case T::ARR: {
            out += '[';
            bool first = true;
            for (auto& e : v.arr) {
                if (!first) out += ',';
                first = false;
                serialize(e, out);
            }
            out += ']';
            break;
        }
        case T::OBJ: {
            out += '{';
            bool first = true;
            for (auto& kv : v.obj) {
                if (!first) out += ',';
                first = false;
                escStr(kv.first, out);
                out += ':';
                serialize(kv.second, out);
            }
            out += '}';
            break;
        }
    }
}

}  // namespace jp

static bool endsWith(const std::wstring& s, const std::wstring& suf) {
    return s.size() >= suf.size() &&
           s.compare(s.size() - suf.size(), suf.size(), suf) == 0;
}

static std::wstring toLowerW(const std::wstring& s) {
    std::wstring r = s;
    for (auto& c : r) c = (wchar_t)towlower(c);
    return r;
}

static std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring out((size_t)n, L'\0');
    if (n > 0)
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], n);
    return out;
}

static std::string wideToUtf8(const std::wstring& s) {
    if (s.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    std::string out((size_t)n, '\0');
    if (n > 0)
        WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], n, nullptr, nullptr);
    return out;
}

static void output(const std::wstring& s) {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h == nullptr || h == INVALID_HANDLE_VALUE || s.empty()) return;
    DWORD mode = 0;
    if (GetConsoleMode(h, &mode)) {
        DWORD n = 0;
        WriteConsoleW(h, s.c_str(), (DWORD)s.size(), &n, nullptr);
    } else {
        // Redirigido a archivo/tubería: escribir UTF-8.
        std::string u = wideToUtf8(s);
        DWORD n = 0;
        WriteFile(h, u.data(), (DWORD)u.size(), &n, nullptr);
    }
}

// Compilado como app de ventanas (-mwindows): si NO hay salida heredada (doble
// clic desde Explorer), intenta adjuntarse a la consola del padre. Si ya hay un
// handle de salida válido (consola, tubería o archivo), no se toca.
static void prepararConsola() {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h != nullptr && h != INVALID_HANDLE_VALUE) return;
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        (void)freopen("CONOUT$", "w", stdout);
        (void)freopen("CONOUT$", "w", stderr);
        (void)freopen("CONIN$", "r", stdin);
    }
}

static void outln(const std::wstring& s) { output(s + L"\n"); }

static bool pathExists(const std::wstring& p) {
    return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES;
}

// Ruta absoluta y canonica (resuelve . , .. y separadores) para comparar de
// forma fiable archivos propios (exe, reglas.json, log) aunque el origen use
// / o \ mezclados. Devuelve la original si no se puede resolver.
static std::wstring rutaCanonica(const std::wstring& p) {
    if (p.empty()) return p;
    DWORD n = GetFullPathNameW(p.c_str(), 0, nullptr, nullptr);
    if (n == 0) return p;
    std::wstring buf(n, L'\0');
    DWORD m = GetFullPathNameW(p.c_str(), n, &buf[0], nullptr);
    if (m == 0 || m >= n) return p;
    buf.resize(m);
    for (auto& ch : buf) if (ch == L'/') ch = L'\\';
    while (buf.size() > 3 && buf.back() == L'\\') buf.pop_back();
    return buf;
}

static bool createDirRecursive(const std::wstring& p) {
    if (p.empty()) return false;
    if (pathExists(p)) return true;

    // Prefijo: conserva \\?\ , \\?\UNC\ , \\server\share o \ para no perderlo.
    std::wstring prefijo;
    std::wstring resto = p;
    if (resto.size() >= 4 &&
        (resto.compare(0, 4, L"\\\\?\\") == 0)) {
        if (resto.size() >= 8 && _wcsnicmp(resto.c_str(), L"\\\\?\\UNC\\", 8) == 0) {
            prefijo = L"\\\\?\\UNC\\";
            resto = resto.substr(8);
        } else {
            prefijo = L"\\\\?\\";
            resto = resto.substr(4);
        }
    } else if (resto.size() >= 2 && resto[0] == L'\\' && resto[1] == L'\\') {
        prefijo = L"\\\\";
        resto = resto.substr(2);
    } else if (resto[0] == L'\\' || resto[0] == L'/') {
        prefijo = L"\\";
        resto = resto.substr(1);
    }

    std::vector<std::wstring> parts;
    std::wstring cur;
    for (wchar_t c : resto) {
        if (c == L'\\' || c == L'/') {
            if (!cur.empty()) parts.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) parts.push_back(cur);

    std::wstring acc = prefijo;
    for (auto& part : parts) {
        if (!acc.empty() && acc.back() != L'\\' && acc.back() != L'/') acc += L'\\';
        acc += part;
        if (!pathExists(acc)) {
            if (!CreateDirectoryW(acc.c_str(), nullptr) &&
                GetLastError() != ERROR_ALREADY_EXISTS)
                return false;
        }
    }
    return true;
}

static bool readAllBytes(const std::wstring& ruta, std::string& out) {
    HANDLE h = CreateFileW(ruta.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz{};
    GetFileSizeEx(h, &sz);
    if (sz.QuadPart < 0 || sz.QuadPart > (LONGLONG)512 * 1024 * 1024) {
        CloseHandle(h);
        return false;
    }
    out.resize((size_t)sz.QuadPart);
    DWORD leido = 0;
    BOOL ok = TRUE;
    if (!out.empty())
        ok = ReadFile(h, &out[0], (DWORD)out.size(), &leido, nullptr);
    CloseHandle(h);
    if (!ok) return false;
    out.resize(leido);
    return true;
}

static bool writeAllBytes(const std::wstring& ruta, const std::string& data) {
    HANDLE h = CreateFileW(ruta.c_str(), GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD n = 0;
    BOOL ok = TRUE;
    if (!data.empty())
        ok = WriteFile(h, data.c_str(), (DWORD)data.size(), &n, nullptr);
    CloseHandle(h);
    return ok && n == data.size();
}

// Escribe a un temporal y lo renombra atómicamente (evita logs corruptos a medias).
static bool escribirArchivoAtomico(const std::wstring& ruta, const std::string& data) {
    std::wstring tmp = ruta + L".tmp";
    if (!writeAllBytes(tmp, data)) return false;
    if (!MoveFileExW(tmp.c_str(), ruta.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tmp.c_str());
        return false;
    }
    return true;
}

static std::wstring nombreArchivo(const std::wstring& p) {
    size_t s = p.find_last_of(L"\\/");
    return s == std::wstring::npos ? p : p.substr(s + 1);
}

static std::wstring nombreBaseOrigen(const std::wstring& p) {
    std::wstring d = p;
    while (!d.empty() && (d.back() == L'\\' || d.back() == L'/')) d.pop_back();
    size_t s = d.find_last_of(L"\\/");
    std::wstring b = (s == std::wstring::npos) ? d : d.substr(s + 1);
    if (b.empty()) b = L"Origen";
    for (auto& c : b)
        if (c == L'<' || c == L'>' || c == L':' || c == L'"' || c == L'/' ||
            c == L'\\' || c == L'|' || c == L'?' || c == L'*')
            c = L'_';
    return b;
}

static bool matchPatron(const std::wstring& s, const std::wstring& p) {
    size_t si = 0, pi = 0, star = std::wstring::npos, match = 0;
    while (si < s.size()) {
        if (pi < p.size() && (p[pi] == L'?' || p[pi] == s[si])) {
            ++si; ++pi;
        } else if (pi < p.size() && p[pi] == L'*') {
            star = pi++; match = si;
        } else if (star != std::wstring::npos) {
            pi = star + 1; si = ++match;
        } else {
            return false;
        }
    }
    while (pi < p.size() && p[pi] == L'*') ++pi;
    return pi == p.size();
}

struct Categoria {
    std::wstring nombre;
    std::vector<std::wstring> exts;  // sin punto, minúsculas; soporta multi-parte (tar.gz)
};

struct Config {
    std::wstring destinoBase = L"D:\\Archivos_Organizados";
    std::vector<std::wstring> origenes;
    std::vector<Categoria> categorias;
    std::set<std::wstring> excluidas;    // incompletos / accesos directos
    std::vector<std::wstring> proteccion;  // patrones a no tocar
    std::wstring subcarpeta = L"anio-mes";  // anio-mes | anio | origen | plano
    int reintentos = 5;
    int esperaMs = 300;
    bool ignorarOcultosSistema = true;      // descarta FILE_ATTRIBUTE_HIDDEN|SYSTEM
    int edadMinimaSeg = 2;                  // antigüedad mínima antes de mover
    int esperaEstableMs = 2000;             // espera máxima a que el tamaño se estabilice
    std::wstring usarFecha = L"creacion";   // creacion | modificacion
    std::wstring logPath;                   // resuelto en cargarConfig
};

static std::wstring exeDir = L".";

static bool cargarConfig(Config& c, bool estricto) {
    c.logPath = exeDir + L"\\movimientos.json";
    std::wstring archivo = exeDir + L"\\reglas.json";
    std::string raw;

    if (!pathExists(archivo)) {
        if (estricto) outln(L"[error] No se encontró reglas.json junto al ejecutable. Abortando (fail-closed).");
        return false;
    }
    if (!readAllBytes(archivo, raw)) {
        if (estricto) outln(L"[error] No se pudo leer reglas.json. Abortando (fail-closed).");
        return false;
    }
    if (raw.size() >= 3 && (unsigned char)raw[0] == 0xEF &&
        (unsigned char)raw[1] == 0xBB && (unsigned char)raw[2] == 0xBF)
        raw.erase(0, 3);

    jp::V root;
    try {
        root = jp::parse(raw);
    } catch (...) {
        if (estricto) outln(L"[error] reglas.json no es JSON válido. Abortando (fail-closed).");
        return false;
    }
    if (root.t != jp::T::OBJ) {
        if (estricto) outln(L"[error] reglas.json debe ser un objeto JSON. Abortando (fail-closed).");
        return false;
    }

    if (const std::string* d = root.getStr("destinoBase"))
        c.destinoBase = utf8ToWide(*d);
    if (const jp::V* o = root.get("origenes"))
        if (o->t == jp::T::ARR)
            for (auto& e : o->arr)
                if (e.t == jp::T::STR) c.origenes.push_back(utf8ToWide(e.str));
    if (const jp::V* x = root.get("excluirExtensiones"))
        if (x->t == jp::T::ARR)
            for (auto& e : x->arr)
                if (e.t == jp::T::STR) {
                    std::wstring ext = toLowerW(utf8ToWide(e.str));
                    if (!ext.empty() && ext[0] == L'.') ext.erase(0, 1);
                    if (!ext.empty()) c.excluidas.insert(ext);
                }
    if (const jp::V* pr = root.get("proteccion"))
        if (pr->t == jp::T::ARR)
            for (auto& e : pr->arr)
                if (e.t == jp::T::STR) c.proteccion.push_back(utf8ToWide(e.str));
    if (const std::string* sp = root.getStr("subcarpeta")) {
        c.subcarpeta = toLowerW(utf8ToWide(*sp));
        if (c.subcarpeta != L"anio-mes" && c.subcarpeta != L"anio" &&
            c.subcarpeta != L"origen" && c.subcarpeta != L"plano")
            c.subcarpeta = L"anio-mes";
    }
    if (const std::string* uf = root.getStr("usarFecha")) {
        c.usarFecha = toLowerW(utf8ToWide(*uf));
        if (c.usarFecha != L"creacion" && c.usarFecha != L"modificacion")
            c.usarFecha = L"creacion";
    }
    auto numClamp = [](const jp::V* v, long long def, long long lo, long long hi) -> long long {
        if (!v || v->t != jp::T::NUM) return def;
        double d = v->num;
        if (d < (double)lo) return lo;
        if (d > (double)hi) return hi;
        return (long long)d;
    };
    if (const jp::V* rn = root.get("reintentos"))
        c.reintentos = (int)numClamp(rn, c.reintentos, 0, 1000);
    if (const jp::V* em = root.get("esperaReintentoMs"))
        c.esperaMs = (int)numClamp(em, c.esperaMs, 0, 600000);
    if (const jp::V* io = root.get("ignorarOcultosSistema"))
        if (io->t == jp::T::BOOL) c.ignorarOcultosSistema = io->bl;
    if (const jp::V* ed = root.get("edadMinimaSeg"))
        c.edadMinimaSeg = (int)numClamp(ed, c.edadMinimaSeg, 0, 86400);
    if (const jp::V* es = root.get("esperaEstableMs"))
        c.esperaEstableMs = (int)numClamp(es, c.esperaEstableMs, 0, 600000);
    if (const jp::V* cg = root.get("categorias"))
        if (cg->t == jp::T::ARR)
            for (auto& e : cg->arr) {
                if (e.t != jp::T::OBJ) continue;
                const std::string* n = e.getStr("nombre");
                const jp::V* ex = e.get("extensiones");
                if (!n) continue;
                Categoria cat;
                cat.nombre = utf8ToWide(*n);
                if (cat.nombre.empty()) continue;
                if (ex && ex->t == jp::T::ARR)
                    for (auto& exx : ex->arr)
                        if (exx.t == jp::T::STR) {
                            std::wstring ext = toLowerW(utf8ToWide(exx.str));
                            if (!ext.empty() && ext[0] == L'.') ext.erase(0, 1);
                            if (!ext.empty()) cat.exts.push_back(ext);
                        }
                c.categorias.push_back(std::move(cat));
            }

    if (c.categorias.empty()) {
        if (estricto) outln(L"[error] reglas.json no define categorías. Abortando (fail-closed).");
        return false;
    }
    if (estricto && c.destinoBase.empty()) {
        outln(L"[error] destinoBase está vacío. Abortando (fail-closed).");
        return false;
    }
    if (c.reintentos < 0) c.reintentos = 0;
    if (c.esperaMs < 0) c.esperaMs = 0;
    if (c.edadMinimaSeg < 0) c.edadMinimaSeg = 0;
    if (c.esperaEstableMs < 0) c.esperaEstableMs = 0;
    return true;
}

// Índice de categoría o -1 (sin coincidencia) por extensión más larga primero.
static int categoriaPorExtension(const std::vector<Categoria>& cats, const std::wstring& nombre) {
    std::wstring lower = toLowerW(nombre);
    int best = -1;
    size_t bestLen = 0;
    for (size_t i = 0; i < cats.size(); ++i) {
        for (auto& ext : cats[i].exts) {
            if (ext.size() <= bestLen) continue;
            if (endsWith(lower, L"." + ext)) {
                best = (int)i;
                bestLen = ext.size();
            }
        }
    }
    return best;
}

static bool estaExcluido(const Config& c, const std::wstring& nombre) {
    std::wstring lower = toLowerW(nombre);
    // Nombres reservados del propio programa: nunca se mueven, estén donde estén
    // (log, su temporal, config y config de ejemplo).
    if (lower == L"movimientos.json" || lower == L"movimientos.json.tmp" ||
        lower == L"reglas.json" || lower == L"reglas.example.json")
        return true;
    for (auto& ext : c.excluidas)
        if (endsWith(lower, L"." + ext)) return true;
    return false;
}

static bool estaProtegido(const Config& c, const std::wstring& nombre) {
    std::wstring n = toLowerW(nombre);
    for (auto& p : c.proteccion)
        if (matchPatron(n, toLowerW(p))) return true;
    return false;
}

static bool sig(const unsigned char* b, const unsigned char* s, size_t n) {
    return memcmp(b, s, n) == 0;
}

// Magic bytes. Solo se usa cuando la extensión no define categoría.
static std::wstring categoriaPorContenido(const std::wstring& ruta) {
    HANDLE h = CreateFileW(ruta.c_str(), GENERIC_READ,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return L"";
    unsigned char b[16]{};
    DWORD leido = 0;
    BOOL ok = ReadFile(h, b, 16, &leido, nullptr);
    CloseHandle(h);
    if (!ok || leido < 4) return L"";

    static const unsigned char PNG[] = {0x89, 'P', 'N', 'G'};
    static const unsigned char JPEG[] = {0xFF, 0xD8, 0xFF};
    static const unsigned char GIF[] = {'G', 'I', 'F', '8'};
    static const unsigned char RIFF[] = {'R', 'I', 'F', 'F'};
    static const unsigned char WEBP[] = {'W', 'E', 'B', 'P'};
    static const unsigned char WAVE[] = {'W', 'A', 'V', 'E'};
    static const unsigned char PDF[] = {'%', 'P', 'D', 'F'};
    static const unsigned char ZIP[] = {'P', 'K', 0x03, 0x04};
    static const unsigned char RAR[] = {'R', 'a', 'r', '!'};
    static const unsigned char S7Z[] = {'7', 'z', 0xBC, 0xAF, 0x27, 0x1C};
    static const unsigned char OGG[] = {'O', 'g', 'g', 'S'};
    static const unsigned char FLAC[] = {'f', 'L', 'a', 'C'};
    static const unsigned char ID3[] = {'I', 'D', '3'};
    static const unsigned char MKV[] = {0x1A, 0x45, 0xDF, 0xA3};
    static const unsigned char FTYP[] = {'f', 't', 'y', 'p'};
    static const unsigned char OLE[] = {0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1};
    static const unsigned char MZ[] = {'M', 'Z'};

    if (sig(b, PNG, 4) || sig(b, JPEG, 3) || sig(b, GIF, 4)) return L"Imágenes";
    if (sig(b, RIFF, 4) && leido >= 12) {
        if (sig(b + 8, WEBP, 4)) return L"Imágenes";
        if (sig(b + 8, WAVE, 4)) return L"Videos y Música";
    }
    if (sig(b, PDF, 4)) return L"Documentos";
    if (sig(b, ZIP, 4) || sig(b, RAR, 4) || sig(b, S7Z, 6) ||
        (b[0] == 0x1F && b[1] == 0x8B))
        return L"Comprimidos";
    if (sig(b, OGG, 4) || sig(b, FLAC, 4) || sig(b, MKV, 4) || sig(b, ID3, 3))
        return L"Videos y Música";
    if (leido >= 8 && sig(b + 4, FTYP, 4)) return L"Videos y Música";  // MP4/MOV
    if (leido >= 8 && sig(b, OLE, 8)) return L"Documentos";
    if (b[0] == 0xFF && (b[1] & 0xE0) == 0xE0) return L"Videos y Música";  // MPEG
    if (sig(b, MZ, 2)) return L"Instaladores";
    return L"";
}

static bool existeCategoria(const Config& c, const std::wstring& nombre) {
    for (auto& cat : c.categorias)
        if (toLowerW(cat.nombre) == toLowerW(nombre)) return true;
    return false;
}

static std::wstring subDirFecha(const std::wstring& ruta, bool conMes,
                                const std::wstring& usarFecha) {
    HANDLE h = CreateFileW(ruta.c_str(), FILE_READ_ATTRIBUTES,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return L"";
    FILETIME creacion{}, acceso{}, escritura{};
    BOOL ok = GetFileTime(h, &creacion, &acceso, &escritura);
    CloseHandle(h);
    if (!ok) return L"";
    FILETIME ft = (usarFecha == L"modificacion") ? escritura : creacion;
    FILETIME local{};
    if (!FileTimeToLocalFileTime(&ft, &local)) return L"";
    SYSTEMTIME st{};
    if (!FileTimeToSystemTime(&local, &st)) return L"";
    wchar_t b[16];
    if (!conMes) {
        swprintf(b, 16, L"%04u", st.wYear);
        return b;
    }
    swprintf(b, 16, L"%04u\\%02u", st.wYear, st.wMonth);
    return b;
}

// Carpeta destino relativa dentro del destino base: Categoría\YYYY\MM ...
static std::wstring carpetaDestino(const Config& c, const std::wstring& cat,
                                   const std::wstring& origen, const std::wstring& ruta) {
    if (c.subcarpeta == L"plano") return cat;
    if (c.subcarpeta == L"origen") return cat + L"\\" + nombreBaseOrigen(origen);
    std::wstring fecha = subDirFecha(ruta, c.subcarpeta != L"anio", c.usarFecha);
    if (fecha.empty()) return cat;
    if (c.subcarpeta == L"anio") return cat + L"\\" + fecha;
    return cat + L"\\" + fecha;  // anio-mes
}

// Sufijo _1, _2 ... antes de la última extensión si el destino ya existe.
static std::wstring nombreUnico(const std::wstring& dirDestino, const std::wstring& archivo) {
    std::wstring base = archivo;
    std::wstring ext;
    size_t dot = archivo.find_last_of(L'.');
    if (dot != std::wstring::npos && dot != 0) {
        base = archivo.substr(0, dot);
        ext = archivo.substr(dot);
    }
    std::wstring candidato = archivo;
    if (pathExists(dirDestino + L"\\" + candidato)) {
        for (int n = 1; n <= 1000000; ++n) {
            candidato = base + L"_" + std::to_wstring(n) + ext;
            if (!pathExists(dirDestino + L"\\" + candidato)) break;
        }
    }
    return candidato;
}

static bool icaseEquiv(const std::wstring& a, const std::wstring& b) {
    return toLowerW(a) == toLowerW(b);
}

static std::wstring ahora() {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    wchar_t b[40];
    swprintf(b, 40, L"%04u-%02u-%02u %02u:%02u:%02u",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return b;
}

static bool moverConReintentos(const std::wstring& src, const std::wstring& dst,
                               int reintentos, DWORD esperaMs, DWORD& err) {
    for (int i = 0; i <= reintentos; ++i) {
        if (MoveFileExW(src.c_str(), dst.c_str(), MOVEFILE_COPY_ALLOWED)) return true;
        err = GetLastError();
        bool reintenta = (err == ERROR_SHARING_VIOLATION || err == ERROR_LOCK_VIOLATION ||
                          err == ERROR_ACCESS_DENIED);
        if (!reintenta) return false;
        if (esperaMs) Sleep(esperaMs);
    }
    return false;
}

// ¿El archivo parece estar ya escrito (no en curso)? Rápido si es antiguo.
static bool estaEstable(const std::wstring& ruta, int edadMinimaSeg) {
    WIN32_FILE_ATTRIBUTE_DATA fad{};
    if (!GetFileAttributesExW(ruta.c_str(), GetFileExInfoStandard, &fad)) return false;
    FILETIME nowFt{};
    GetSystemTimeAsFileTime(&nowFt);
    ULARGE_INTEGER now, mtime;
    now.LowPart = nowFt.dwLowDateTime;
    now.HighPart = nowFt.dwHighDateTime;
    mtime.LowPart = fad.ftLastWriteTime.dwLowDateTime;
    mtime.HighPart = fad.ftLastWriteTime.dwHighDateTime;
    long long edadSeg = (long long)((now.QuadPart - mtime.QuadPart) / 10000000ULL);
    if (edadSeg >= edadMinimaSeg) return true;  // antiguo: se da por terminado
    ULARGE_INTEGER sz1;
    sz1.LowPart = fad.nFileSizeLow;
    sz1.HighPart = fad.nFileSizeHigh;
    Sleep(300);
    WIN32_FILE_ATTRIBUTE_DATA fad2{};
    if (!GetFileAttributesExW(ruta.c_str(), GetFileExInfoStandard, &fad2)) return false;
    ULARGE_INTEGER sz2;
    sz2.LowPart = fad2.nFileSizeLow;
    sz2.HighPart = fad2.nFileSizeHigh;
    return sz1.QuadPart == sz2.QuadPart;
}

static bool esperarEstable(const std::wstring& ruta, int edadMinimaSeg, int esperaMs) {
    DWORD inicio = GetTickCount();
    for (;;) {
        if (estaEstable(ruta, edadMinimaSeg)) return true;
        if ((int)(GetTickCount() - inicio) >= esperaMs) return false;
        Sleep(250);
    }
}

struct Resumen {
    int movidos = 0;
    int omitidos = 0;
    int protegidos = 0;
    int fallidos = 0;
    std::vector<std::wstring> fallos;
};

struct Movimiento {
    std::wstring origen;
    std::wstring destino;
    std::wstring carpeta;
};

struct Lote {
    std::wstring id;
    std::wstring ts;
    std::vector<Movimiento> movs;
};

static void organizarOrigen(const Config& c, const std::wstring& origen, bool dryRun,
                            Resumen& res, Lote& lote) {
    std::wstring dir = origen;
    if (!dir.empty() && dir.back() != L'\\' && dir.back() != L'/') dir += L'\\';
    if (!pathExists(dir)) {
        ++res.fallidos;
        res.fallos.push_back(L"origen inexistente: " + origen);
        return;
    }

    std::vector<std::wstring> entradas;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) {
        ++res.fallidos;
        res.fallos.push_back(L"no se pudo enumerar: " + origen);
        return;
    }
    do {
        std::wstring n = fd.cFileName;
        if (n == L"." || n == L"..") continue;
        entradas.push_back(n);
    } while (FindNextFileW(h, &fd));
    FindClose(h);

    std::sort(entradas.begin(), entradas.end(),
              [](const std::wstring& a, const std::wstring& b) {
                  return toLowerW(a) < toLowerW(b);
              });

    const std::wstring exeFull = rutaCanonica([&]() {
        wchar_t p[MAX_PATH]{};
        GetModuleFileNameW(nullptr, p, MAX_PATH);
        return std::wstring(p);
    }());
    const std::wstring reglasPath = rutaCanonica(exeDir + L"\\reglas.json");
    const std::wstring logPathC = rutaCanonica(c.logPath);
    const std::wstring logTmpC = rutaCanonica(c.logPath + L".tmp");

    for (auto& nombre : entradas) {
        std::wstring ruta = dir + nombre;
        DWORD attrs = GetFileAttributesW(ruta.c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES) continue;
        if (attrs & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (attrs & FILE_ATTRIBUTE_REPARSE_POINT) continue;
        if (c.ignorarOcultosSistema &&
            (attrs & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM))) {
            ++res.omitidos;
            continue;
        }
        if (estaExcluido(c, nombre)) { ++res.omitidos; continue; }
        if (estaProtegido(c, nombre)) {
            ++res.protegidos;
            if (dryRun) outln(L"  [protegido] " + nombre);
            continue;
        }
        if (icaseEquiv(rutaCanonica(ruta), exeFull) ||
            icaseEquiv(rutaCanonica(ruta), reglasPath) ||
            icaseEquiv(rutaCanonica(ruta), logPathC) ||
            icaseEquiv(rutaCanonica(ruta), logTmpC))
            continue;

        int idx = categoriaPorExtension(c.categorias, nombre);
        std::wstring cat;
        if (idx >= 0) {
            cat = c.categorias[(size_t)idx].nombre;
        } else {
            std::wstring m = categoriaPorContenido(ruta);
            if (!m.empty() && existeCategoria(c, m)) cat = m;
            else cat = L"Otros";
        }

        std::wstring carpeta = carpetaDestino(c, cat, origen, ruta);
        std::wstring dirDestino = c.destinoBase + L"\\" + carpeta;
        std::wstring final = nombreUnico(dirDestino, nombre);
        std::wstring rutaFinal = dirDestino + L"\\" + final;

        if (dryRun) {
            if (final == nombre) outln(L"  -> " + carpeta + L" / " + nombre);
            else outln(L"  -> " + carpeta + L" / " + nombre + L"  (colisión -> " + final + L")");
            continue;
        }

        if (!esperarEstable(ruta, c.edadMinimaSeg, c.esperaEstableMs)) {
            ++res.omitidos;
            outln(L"  [en escritura] " + nombre + L" (se omite hasta que termine)");
            continue;
        }

        if (!createDirRecursive(dirDestino)) {
            ++res.fallidos;
            res.fallos.push_back(nombre + L" (no se creó " + dirDestino + L")");
            continue;
        }

        DWORD err = 0;
        if (moverConReintentos(ruta, rutaFinal, c.reintentos, (DWORD)c.esperaMs, err)) {
            ++res.movidos;
            lote.movs.push_back(Movimiento{ruta, rutaFinal, carpeta});
            outln(L"  movido: " + carpeta + L" / " + final);
        } else {
            ++res.fallidos;
            res.fallos.push_back(nombre + L" (error " + std::to_wstring(err) + L")");
        }
    }
}

static void organizarTodo(const Config& c, const std::vector<std::wstring>& origenes,
                          bool dryRun, Resumen& res, Lote& lote) {
    for (auto& o : origenes) organizarOrigen(c, o, dryRun, res, lote);
}

static bool cargarLoteLog(const Config& c, jp::V& root) {
    std::string raw;
    if (!pathExists(c.logPath)) return false;
    if (!readAllBytes(c.logPath, raw)) return false;
    try {
        root = jp::parse(raw);
    } catch (...) {
        return false;
    }
    if (root.t != jp::T::OBJ) return false;
    return true;
}

static bool guardarLote(const Config& c, const Lote& lote, int retencion) {
    jp::V root;
    std::string raw;
    bool parseado = false;
    if (pathExists(c.logPath)) {
        // El log existe: si no se puede leer o parsear, NO se sobrescribe (evita
        // perder el historial de deshacer por un bloqueo del antivirus o un JSON
        // corrupto). Se avisa y se conserva intacto.
        if (!readAllBytes(c.logPath, raw)) {
            outln(L"[aviso] No se pudo leer el log; se conserva sin cambios (no se registra este lote): " + c.logPath);
            return false;
        }
        try {
            root = jp::parse(raw);
            parseado = (root.t == jp::T::OBJ);
        } catch (...) {
            parseado = false;
        }
        if (!parseado) {
            outln(L"[aviso] El log no es JSON valido; se conserva sin cambios (no se registra este lote): " + c.logPath);
            return false;
        }
    } else {
        root.t = jp::T::OBJ;
        root.obj["lotes"] = jp::V();
        root.obj["lotes"].t = jp::T::ARR;
    }
    if (!root.get("lotes") || root.obj["lotes"].t != jp::T::ARR) {
        root.obj["lotes"] = jp::V();
        root.obj["lotes"].t = jp::T::ARR;
    }

    jp::V lj;
    lj.t = jp::T::OBJ;
    lj.obj["id"] = jp::V();
    lj.obj["id"].t = jp::T::STR;
    lj.obj["id"].str = wideToUtf8(lote.id);
    lj.obj["ts"] = jp::V();
    lj.obj["ts"].t = jp::T::STR;
    lj.obj["ts"].str = wideToUtf8(lote.ts);
    jp::V arr;
    arr.t = jp::T::ARR;
    for (auto& m : lote.movs) {
        jp::V mv;
        mv.t = jp::T::OBJ;
        auto put = [&](const char* k, const std::wstring& v) {
            jp::V s;
            s.t = jp::T::STR;
            s.str = wideToUtf8(v);
            mv.obj[k] = std::move(s);
        };
        put("origen", m.origen);
        put("destino", m.destino);
        put("carpeta", m.carpeta);
        arr.arr.push_back(std::move(mv));
    }
    lj.obj["movimientos"] = std::move(arr);

    root.obj["lotes"].arr.push_back(std::move(lj));
    while ((int)root.obj["lotes"].arr.size() > retencion)
        root.obj["lotes"].arr.erase(root.obj["lotes"].arr.begin());

    std::string out;
    jp::serialize(root, out);
    out += "\n";
    if (!escribirArchivoAtomico(c.logPath, out)) {
        outln(L"[aviso] No se pudo escribir el log: " + c.logPath);
        return false;
    }
    return true;
}

static int deshacer(const Config& c) {
    jp::V root;
    if (!cargarLoteLog(c, root) || !root.get("lotes") ||
        root.obj["lotes"].t != jp::T::ARR || root.obj["lotes"].arr.empty()) {
        outln(L"[aviso] No hay lotes registrados para deshacer (" + c.logPath + L").");
        return 1;
    }
    jp::V& lote = root.obj["lotes"].arr.back();
    const jp::V* movs = lote.get("movimientos");
    if (!movs || movs->t != jp::T::ARR || movs->arr.empty()) {
        outln(L"[aviso] El último lote está vacío.");
        return 1;
    }
    const std::string* ts = lote.getStr("ts");
    outln(L"Deshaciendo lote de: " + (ts ? utf8ToWide(*ts) : L"?") +
          L" (" + std::to_wstring(movs->arr.size()) + L" movimientos)");

    int ok = 0, descartados = 0, pendientesN = 0;
    std::vector<jp::V> pendientes;
    for (auto it = movs->arr.rbegin(); it != movs->arr.rend(); ++it) {
        const std::string* so = it->getStr("origen");
        const std::string* sd = it->getStr("destino");
        if (!so || !sd) { ++descartados; continue; }  // entrada inválida: descartar
        std::wstring origen = utf8ToWide(*so);
        std::wstring destino = utf8ToWide(*sd);

        if (!pathExists(destino)) {
            // El archivo ya no está donde lo dejamos (se borró/movió a mano):
            // no tiene sentido reintentarlo nunca; se descarta.
            ++descartados;
            outln(L"  [descartado] ya no está en destino: " + nombreArchivo(destino));
            continue;
        }
        if (pathExists(origen)) {
            // Reintentable: el origen volvió a estar ocupado.
            ++pendientesN;
            pendientes.push_back(*it);
            outln(L"  [pendiente] origen ocupado: " + nombreArchivo(origen));
            continue;
        }
        size_t sep = origen.find_last_of(L"\\/");
        if (sep != std::wstring::npos) createDirRecursive(origen.substr(0, sep));
        DWORD err = 0;
        if (moverConReintentos(destino, origen, c.reintentos, (DWORD)c.esperaMs, err)) {
            ++ok;
            outln(L"  restaurado: " + nombreArchivo(origen));
        } else {
            // Reintentable: archivo bloqueado u otro error transitorio.
            ++pendientesN;
            pendientes.push_back(*it);
            outln(L"  [pendiente] " + nombreArchivo(destino) +
                  L" (error " + std::to_wstring(err) + L")");
        }
    }

    if (pendientesN == 0) {
        root.obj["lotes"].arr.pop_back();
    } else {
        jp::V arr;
        arr.t = jp::T::ARR;
        arr.arr = std::move(pendientes);
        root.obj["lotes"].arr.back().obj["movimientos"] = std::move(arr);
    }

    std::string out;
    jp::serialize(root, out);
    out += "\n";
    if (!escribirArchivoAtomico(c.logPath, out))
        outln(L"[aviso] No se pudo actualizar el log: " + c.logPath);
    if (pendientesN > 0)
        outln(L"[aviso] " + std::to_wstring(pendientesN) +
              L" movimiento(s) no se pudieron deshacer; siguen registrados.");
    if (descartados > 0)
        outln(L"[aviso] " + std::to_wstring(descartados) +
              L" movimiento(s) descartado(s) (destino ya no existe).");

    outln(L"Restaurados: " + std::to_wstring(ok) +
          L" | descartados " + std::to_wstring(descartados) +
          L" | pendientes " + std::to_wstring(pendientesN));
    return ok == 0 && pendientesN == 0 ? (descartados ? 0 : 1) : (pendientesN ? 3 : 0);
}

// ---------------- Modo vigilante: bandeja + ReadDirectoryChangesW ----------------
#define WM_TRAY (WM_APP + 1)
#define WM_FSCHANGE (WM_APP + 2)
#define IDI_TRAY 1
#define IDM_AHORA 1001
#define IDM_SALIR 1002
#define IDT_SCAN 1
#define IDT_PERIODIC 2
#define PERIODIC_MS 60000

static HWND g_hwnd = nullptr;
static const Config* g_cfg = nullptr;
static const std::vector<std::wstring>* g_origenes = nullptr;
static std::atomic<bool> g_scanning{false};
static HICON g_trayIcon = nullptr;

// El escaneo corre en un hilo aparte: la espera por archivos inestables no debe
// congelar el bucle de mensajes de la bandeja.
static DWORD WINAPI scanThread(LPVOID) {
    Resumen res;
    Lote lote;
    lote.id = ahora();
    lote.ts = ahora();
    organizarTodo(*g_cfg, *g_origenes, false, res, lote);
    if (!lote.movs.empty()) guardarLote(*g_cfg, lote, 50);
    g_scanning = false;
    return 0;
}

static void pasadaVigilante() {
    if (!g_cfg || !g_origenes) return;
    bool esperado = false;
    if (!g_scanning.compare_exchange_strong(esperado, true)) return;  // ya hay una
    HANDLE t = CreateThread(nullptr, 0, scanThread, nullptr, 0, nullptr);
    if (t) CloseHandle(t);
    else g_scanning = false;
}

static void mostrarMenuTray(HWND h) {
    POINT pt{};
    GetCursorPos(&pt);
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, IDM_AHORA, L"Organizar ahora");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, IDM_SALIR, L"Salir");
    SetForegroundWindow(h);
    TrackPopupMenu(m, TPM_RIGHTBUTTON, pt.x, pt.y, 0, h, nullptr);
    PostMessageW(h, WM_NULL, 0, 0);
    DestroyMenu(m);
}

static LRESULT CALLBACK wndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_TRAY:
            if (l == WM_RBUTTONUP || l == WM_CONTEXTMENU) mostrarMenuTray(h);
            else if (l == WM_LBUTTONDBLCLK) pasadaVigilante();
            return 0;
        case WM_FSCHANGE:
            SetTimer(h, IDT_SCAN, 800, nullptr);
            return 0;
        case WM_TIMER:
            if (w == IDT_SCAN) {
                KillTimer(h, IDT_SCAN);
                pasadaVigilante();
            } else if (w == IDT_PERIODIC) {
                pasadaVigilante();
            }
            return 0;
        case WM_COMMAND:
            if (LOWORD(w) == IDM_SALIR) DestroyWindow(h);
            else if (LOWORD(w) == IDM_AHORA) pasadaVigilante();
            return 0;
        case WM_DESTROY:
            for (int i = 0; i < 60 && g_scanning.load(); ++i) Sleep(50);  // no cortar un movimiento a medias
            if (g_trayIcon) {
                NOTIFYICONDATAW nid{};
                nid.cbSize = sizeof nid;
                nid.hWnd = h;
                nid.uID = IDI_TRAY;
                Shell_NotifyIconW(NIM_DELETE, &nid);
            }
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(h, m, w, l);
    }
}

static DWORD WINAPI watchThread(LPVOID p) {
    std::wstring* dirPtr = (std::wstring*)p;
    std::wstring dir = *dirPtr;
    delete dirPtr;
    if (!dir.empty() && dir.back() != L'\\') dir += L'\\';

    alignas(DWORD) unsigned char buf[64 * 1024];
    for (;;) {
        HANDLE hDir = CreateFileW(dir.c_str(), FILE_LIST_DIRECTORY,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                  nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        if (hDir == INVALID_HANDLE_VALUE) {
            Sleep(2000);  // carpeta ausente: reintentar sin morir
            continue;
        }
        for (;;) {
            DWORD n = 0;
            BOOL ok = ReadDirectoryChangesW(hDir, buf, sizeof(buf), FALSE,
                                            FILE_NOTIFY_CHANGE_FILE_NAME |
                                                FILE_NOTIFY_CHANGE_SIZE |
                                                FILE_NOTIFY_CHANGE_LAST_WRITE,
                                            &n, nullptr, nullptr);
            if (!ok) break;  // carpeta borrada / error: cerrar y reabrir
            // n==0 => desbordamiento del búfer => forzar escaneo completo
            if (g_hwnd) PostMessageW(g_hwnd, WM_FSCHANGE, 0, 0);
        }
        CloseHandle(hDir);
        Sleep(1000);
    }
    return 0;
}

static int modoVigilante(Config& c, const std::vector<std::wstring>& origenes) {
    HANDLE inst = CreateMutexW(nullptr, TRUE, L"Global\\OrganizadorVigilanteUnico");
    if (inst && GetLastError() == ERROR_ALREADY_EXISTS) {
        outln(L"[aviso] Ya hay un vigilante en ejecución (mira la bandeja).");
        return 0;
    }

    g_cfg = &c;
    g_origenes = &origenes;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = wndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"OrganizadorTrayCls";
    RegisterClassExW(&wc);

    g_hwnd = CreateWindowExW(0, wc.lpszClassName, L"Organizador", 0, 0, 0, 0, 0,
                             HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
    if (!g_hwnd) {
        outln(L"[error] No se creó la ventana del vigilante.");
        return 1;
    }

    std::wstring icoPath = exeDir + L"\\organizador.ico";
    g_trayIcon = (HICON)LoadImageW(nullptr, icoPath.c_str(), IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
    if (!g_trayIcon) g_trayIcon = LoadIconW(nullptr, IDI_APPLICATION);

    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof nid;
    nid.hWnd = g_hwnd;
    nid.uID = IDI_TRAY;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAY;
    nid.hIcon = g_trayIcon;
    lstrcpynW(nid.szTip, L"Organizador de Archivos (vigilante)", 128);
    if (!Shell_NotifyIconW(NIM_ADD, &nid)) {
        outln(L"[aviso] No se pudo añadir el icono de bandeja.");
    }

    for (auto& o : origenes) {
        auto* d = new std::wstring(o);
        HANDLE t = CreateThread(nullptr, 0, watchThread, d, 0, nullptr);
        if (t) CloseHandle(t);
        else delete d;
    }

    SetTimer(g_hwnd, IDT_PERIODIC, PERIODIC_MS, nullptr);  // red de seguridad

    outln(L"Vigilante activo en bandeja. Orígenes:");
    for (auto& o : origenes) outln(L"  - " + o);
    outln(L"Destino: " + c.destinoBase + L"   (doble clic en el icono = organizar ahora)");

    pasadaVigilante();

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}

static int ejecutarSelftest() {
    int fallas = 0;
    auto chk = [&](bool cond, const wchar_t* nombre) {
        outln(std::wstring(cond ? L"  [OK]    " : L"  [FALLA] ") + nombre);
        if (!cond) ++fallas;
    };

    outln(L"-- matchPatron --");
    chk(matchPatron(L"hola.txt", L"*.txt"), L"*.txt coincide con hola.txt");
    chk(!matchPatron(L"hola.pdf", L"*.txt"), L"*.txt no coincide con hola.pdf");
    chk(matchPatron(L"reglas.json", L"reglas.json"), L"coincidencia exacta");
    chk(matchPatron(L"a.b.c", L"a.*"), L"comodin * al final");
    chk(matchPatron(L"abc", L"a?c"), L"? coincide con un caracter");
    chk(!matchPatron(L"ac", L"a?c"), L"? exige un caracter");
    chk(matchPatron(L"x", L"*"), L"* coincide con todo");

    outln(L"-- endsWith --");
    chk(endsWith(L"a.tar.gz", L".tar.gz"), L"termina en .tar.gz");
    chk(!endsWith(L"a.zip", L".tar.gz"), L"no termina en .tar.gz");

    outln(L"-- categoriaPorExtension --");
    std::vector<Categoria> cats;
    { Categoria c; c.nombre = L"Documentos"; c.exts = {L"pdf", L"docx"}; cats.push_back(c); }
    { Categoria c; c.nombre = L"Comprimidos"; c.exts = {L"zip", L"tar.gz"}; cats.push_back(c); }
    chk(categoriaPorExtension(cats, L"informe.PDF") == 0, L"PDF -> Documentos (case-insensitive)");
    chk(categoriaPorExtension(cats, L"x.TAR.GZ") == 1, L"tar.gz -> Comprimidos (multiparte)");
    chk(categoriaPorExtension(cats, L"x.unknown") == -1, L"extension desconocida -> -1");

    outln(L"-- nombreUnico --");
    wchar_t tmp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, tmp);
    std::wstring dir = std::wstring(tmp) + L"org_selftest_tmp";
    createDirRecursive(dir);
    writeAllBytes(dir + L"\\a.txt", "x");
    chk(nombreUnico(dir, L"a.txt") == L"a_1.txt", L"colision -> a_1.txt");
    writeAllBytes(dir + L"\\a_1.txt", "x");
    chk(nombreUnico(dir, L"a.txt") == L"a_2.txt", L"segunda colision -> a_2.txt");
    DeleteFileW((dir + L"\\a.txt").c_str());
    DeleteFileW((dir + L"\\a_1.txt").c_str());
    RemoveDirectoryW(dir.c_str());

    outln(L"-- parser JSON --");
    try {
        jp::V r = jp::parse("{\"a\":1e5,\"b\":1.5E-3,\"e\":\"\\ud83d\\ude00\"}");
        const jp::V* a = r.get("a");
        const jp::V* b = r.get("b");
        const std::string* e = r.getStr("e");
        chk(a && b && e, L"claves a/b/e presentes");
        chk(r.get("a")->t == jp::T::NUM && r.get("a")->num == 100000.0,
            L"1e5 -> 100000 (exponente sin signo)");
        double dif = r.get("b")->num - 0.0015;
        if (dif < 0) dif = -dif;
        chk(dif < 1e-12, L"1.5E-3 -> 0.0015");
        chk(e && *e == std::string("\xF0\x9F\x98\x80"), L"surrogate \\ud83d\\ude00 -> UTF-8");
    } catch (...) {
        chk(false, L"parser JSON lanzo excepcion");
    }

    outln(L"-- fail-closed (los [error] siguientes son esperados) --");
    {
        std::wstring saved = exeDir;
        std::wstring t2 = std::wstring(tmp) + L"org_selftest_cfg";
        createDirRecursive(t2);
        exeDir = t2;
        Config cc;
        chk(!cargarConfig(cc, true), L"sin reglas.json -> false");
        writeAllBytes(t2 + L"\\reglas.json", "{ no es json");
        Config cc2;
        chk(!cargarConfig(cc2, true), L"JSON invalido -> false");
        writeAllBytes(t2 + L"\\reglas.json", "{\"destinoBase\":\"x\"}");
        Config cc3;
        chk(!cargarConfig(cc3, true), L"sin categorias -> false");
        exeDir = saved;
        DeleteFileW((t2 + L"\\reglas.json").c_str());
        RemoveDirectoryW(t2.c_str());
    }

    outln(L"-- deshacer (flujo) --");
    {
        std::wstring t3 = std::wstring(tmp) + L"org_selftest_undo";
        createDirRecursive(t3);
        writeAllBytes(t3 + L"\\dest_ok.txt", "x");         // destino existe
        writeAllBytes(t3 + L"\\origen_ocupado.txt", "y");  // origen ocupado -> pendiente
        Config cc;
        cc.logPath = t3 + L"\\movimientos.json";
        cc.reintentos = 0;
        cc.esperaMs = 0;
        cc.destinoBase = t3;
        Lote lote;
        lote.id = L"t";
        lote.ts = L"t";
        lote.movs.push_back(Movimiento{t3 + L"\\origen_ocupado.txt", t3 + L"\\dest_ok.txt", L"c"});
        lote.movs.push_back(Movimiento{t3 + L"\\desaparecido.txt", t3 + L"\\no_existe.txt", L"c"});
        guardarLote(cc, lote, 50);
        int rc = deshacer(cc);
        chk(rc == 3, L"deshacer -> 3 con pendientes");
        int n = -1;
        std::string raw;
        if (pathExists(cc.logPath) && readAllBytes(cc.logPath, raw)) {
            try {
                jp::V r = jp::parse(raw);
                const jp::V* l = r.get("lotes");
                if (l && l->t == jp::T::ARR && !l->arr.empty()) {
                    const jp::V* mm = l->arr.back().get("movimientos");
                    if (mm && mm->t == jp::T::ARR) n = (int)mm->arr.size();
                }
            } catch (...) {}
        }
        chk(n == 1, L"solo queda el movimiento reintentable (se descarta el desaparecido)");
        DeleteFileW((t3 + L"\\dest_ok.txt").c_str());
        DeleteFileW((t3 + L"\\origen_ocupado.txt").c_str());
        DeleteFileW((t3 + L"\\movimientos.json").c_str());
        RemoveDirectoryW(t3.c_str());
    }

    outln(fallas ? L"-- selftest con fallas --" : L"-- selftest OK --");
    return fallas;
}

static void uso() {
    outln(L"Organizador Automático de Archivos");
    outln(L"Uso: organizador.exe [<ORIGEN>...] [--dest <DESTINO>] [--dry-run]");
    outln(L"                      [--watch] [--deshacer]");
    outln(L"");
    outln(L"  Sin argumentos: usa los orígenes de reglas.json (junto al exe).");
    outln(L"  <ORIGEN>: directorio a organizar (sobrescribe los de reglas.json).");
    outln(L"  --dest <DESTINO>: base destino (por defecto D:\\Archivos_Organizados).");
    outln(L"  --dry-run: solo informa qué se movería, sin tocar nada.");
    outln(L"  --watch: modo vigilante con icono en bandeja (mueve al aparecer).");
    outln(L"  --deshacer: revierte el último lote de movimientos registrado.");
    outln(L"  --selftest: ejecuta pruebas internas y sale (no toca archivos).");
    outln(L"");
    outln(L"  Sin reglas.json válido junto al exe, el programa aborta (código 2).");
}

int wmain(int argc, wchar_t** argv) {
    prepararConsola();
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    exeDir = path;
    size_t slash = exeDir.find_last_of(L'\\');
    if (slash != std::wstring::npos) exeDir = exeDir.substr(0, slash);

    bool dryRun = false;
    bool watch = false;
    bool undo = false;
    bool selftest = false;
    bool hayDest = false;
    std::wstring dest;
    std::vector<std::wstring> origenesCli;

    for (int i = 1; i < argc; ++i) {
        std::wstring a = argv[i];
        if (a == L"--dry-run" || a == L"-n") dryRun = true;
        else if (a == L"--watch" || a == L"-w") watch = true;
        else if (a == L"--deshacer" || a == L"--undo") undo = true;
        else if (a == L"--selftest") selftest = true;
        else if (a == L"--dest") {
            if (i + 1 < argc) { dest = argv[++i]; hayDest = true; }
            else { uso(); return 2; }
        } else if (a == L"--help" || a == L"-h" || a == L"/?") {
            uso();
            return 0;
        } else {
            origenesCli.push_back(a);
        }
    }

    if (selftest) {
        int fallas = ejecutarSelftest();
        outln(fallas ? L"[selftest] FALLARON " + std::to_wstring(fallas) + L" pruebas."
                     : L"[selftest] Todas las pruebas pasaron.");
        return fallas ? 1 : 0;
    }

    if (undo) {
        if (dryRun) {
            outln(L"[error] --dry-run no se puede combinar con --deshacer (deshacer siempre mueve archivos).");
            return 2;
        }
        Config c;
        cargarConfig(c, false);  // tolerante: solo necesitamos el log
        if (hayDest) c.destinoBase = dest;
        return deshacer(c);
    }

    if (watch && dryRun) {
        outln(L"[error] --dry-run no se puede combinar con --watch (el vigilante mueve archivos al detectarlos).");
        return 2;
    }

    Config c;
    if (!cargarConfig(c, true)) return 2;  // fail-closed
    if (hayDest) c.destinoBase = dest;

    const std::vector<std::wstring>& origenes =
        origenesCli.empty() ? c.origenes : origenesCli;

    if (origenes.empty()) {
        outln(L"[error] No hay orígenes definidos (reglas.json vacío o sin argumento <ORIGEN>).");
        return 2;
    }
    for (auto& o : origenes) {
        if (o.empty()) {
            outln(L"[error] Hay un origen vacío en la configuración. Abortando (fail-closed).");
            return 2;
        }
        DWORD at = GetFileAttributesW(o.c_str());
        if (at != INVALID_FILE_ATTRIBUTES && !(at & FILE_ATTRIBUTE_DIRECTORY)) {
            outln(L"[error] El origen no es una carpeta: " + o + L"  Abortando (fail-closed).");
            return 2;
        }
    }

    if (watch) return modoVigilante(c, origenes);

    if (dryRun) outln(L"[SIMULACIÓN] --dry-run: no se moverá ningún archivo.");
    outln(L"Destino base: " + c.destinoBase);

    Resumen res;
    Lote lote;
    lote.id = ahora();
    lote.ts = ahora();
    organizarTodo(c, origenes, dryRun, res, lote);

    outln(L"");
    outln(L"Resumen: movidos " + std::to_wstring(res.movidos) +
          L" | omitidos " + std::to_wstring(res.omitidos) +
          L" | protegidos " + std::to_wstring(res.protegidos) +
          L" | fallidos " + std::to_wstring(res.fallidos));
    for (auto& f : res.fallos) outln(L"  [fallido] " + f);

    if (!dryRun && !lote.movs.empty()) {
        if (guardarLote(c, lote, 50))
            outln(L"Lote registrado (" + std::to_wstring(lote.movs.size()) +
                  L" movs). Deshacer: organizador.exe --deshacer");
        else
            outln(L"[aviso] Los " + std::to_wstring(lote.movs.size()) +
                  L" movimientos NO se registraron en el log; no se podrán deshacer.");
    }
    return res.fallidos ? 3 : 0;
}