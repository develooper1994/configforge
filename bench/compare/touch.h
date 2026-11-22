// touch.h - the work every adapter does, and how an adapter is declared.
//
// Every adapter walks what its library gave it and fills one Touch: a table
// or array is one container (the document's root is not counted); a string,
// key or value, adds one to `strings` and its length to `string_bytes`; a
// number adds one to `numbers` and its value, read into a 64-bit integer or a
// double, is folded into `acc` (which keeps the conversion in the loop: acc is
// never compared, only printed, so no compiler drops it). A format whose
// values are all text (INI, XML) counts every value as a string. (D-111)
#ifndef CFB_TOUCH_H
#define CFB_TOUCH_H

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

struct Touch {
    uint64_t containers = 0, strings = 0, string_bytes = 0, numbers = 0, bools = 0,
             nulls = 0, acc = 0;
};

inline void touch_string(Touch& t, size_t n) {
    ++t.strings;
    t.string_bytes += n;
}

inline void touch_int(Touch& t, int64_t v) {
    ++t.numbers;
    t.acc = t.acc * 1099511628211ull + (uint64_t)v;
}

inline void touch_real(Touch& t, double d) {
    uint64_t b;
    std::memcpy(&b, &d, sizeof b);
    ++t.numbers;
    t.acc = t.acc * 1099511628211ull + b;
}

// One adapter: a library, the format it reads, and one parse. `parse` returns
// whether the library accepted the document; `strict` turns on every check the
// library offers that ConfigForge makes, otherwise the library's own default.
struct Adapter {
    const char* name;    // "yyjson", "simdjson-dom", ...
    const char* lib;     // the library directory: "yyjson", "simdjson", ...
    const char* format;  // "json", "yaml", "toml", "cbor", "ini", "xml"
    const char* version; // the pinned tag or date
    bool (*parse)(const char* p, size_t n, bool strict, Touch& t);
};

struct Registrar {
    explicit Registrar(const Adapter* a);
};

// YAML 1.2 core schema, for the readers that give plain scalars as text
// (libyaml, rapidyaml): the value is told, converted, and folded.
enum ScalarKind { SK_STR, SK_NULL, SK_BOOL, SK_INT, SK_REAL };

inline ScalarKind resolve_plain(const char* s, size_t n, int64_t* iv, double* dv) {
    char  buf[64];
    char* end;
    if (n == 0 || (n == 1 && s[0] == '~'))
        return SK_NULL;
    if (n == 4 && (!std::memcmp(s, "null", 4) || !std::memcmp(s, "Null", 4) ||
                   !std::memcmp(s, "NULL", 4)))
        return SK_NULL;
    if (n == 4 && (!std::memcmp(s, "true", 4) || !std::memcmp(s, "True", 4) ||
                   !std::memcmp(s, "TRUE", 4)))
        return SK_BOOL;
    if (n == 5 && (!std::memcmp(s, "false", 5) || !std::memcmp(s, "False", 5) ||
                   !std::memcmp(s, "FALSE", 5)))
        return SK_BOOL;
    size_t i = 0;
    if (s[0] == '-' || s[0] == '+')
        i = 1;
    if (i >= n)
        return SK_STR;
    if (!((s[i] >= '0' && s[i] <= '9') || s[i] == '.'))
        return SK_STR;
    if (n >= sizeof buf)
        return SK_STR;
    bool digits = true, real = false;
    for (size_t k = i; k < n; ++k) {
        char c = s[k];
        if (c >= '0' && c <= '9')
            continue;
        digits = false;
        if (c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') {
            real = true;
            continue;
        }
        return SK_STR;
    }
    std::memcpy(buf, s, n);
    buf[n] = '\0';
    if (digits) {
        *iv = (int64_t)std::strtoll(buf, &end, 10);
        return *end == '\0' ? SK_INT : SK_STR;
    }
    if (!real)
        return SK_STR;
    *dv = std::strtod(buf, &end);
    return *end == '\0' ? SK_REAL : SK_STR;
}

#endif
