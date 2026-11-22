/*
 * test_text_escape.c - the escapes of the text formats, read and written by
 * one function each (A-7, D-102).
 *
 * One: the decoder, a table of cases for each family (JSON, JSON5, TOML,
 * YAML, .properties). Two: the writers' escapes, held to the old
 * functions, which are copied here as they were before the change, for every
 * byte or code point. Three: the "${" of a reference, the three old copies
 * against the four functions, over every short string of "$", "{", "}" and
 * "a". Four: the places a reader gives the refusals of an escape, and the
 * one refusal this card adds, in a YAML tag.
 *
 * It includes cf_serialize.h, so by A-1b's rule it is a test of the
 * serialization library.
 */

#include <configforge/cf_serialize.h>
#include <configforge/cf_text.h>
#include <configforge/cf_yamlparse.h>

#include <stdio.h>
#include <string.h>

#include "cf_test.h"

/* ------------------------------------------------------------------ *
 * The families, as the readers hold them
 * ------------------------------------------------------------------ */

static const cf_text_esc_simple t_js_simple[]  = {{'"', 34}, {'\\', 92}, {'/', 47},
                                                  {'b', 8},  {'f', 12},  {'n', 10},
                                                  {'r', 13}, {'t', 9},   {0, 0}};
static const cf_text_esc_simple t_js5_simple[] = {
    {'"', 34}, {'\\', 92}, {'/', 47}, {'b', 8}, {'f', 12}, {'n', 10},
    {'r', 13}, {'t', 9},   {'v', 11}, {'0', 0}, {0, 0}};
static const cf_text_esc_simple t_toml_simple[] = {{'"', 34}, {'\\', 92}, {'b', 8},
                                                   {'t', 9},  {'n', 10},  {'f', 12},
                                                   {'r', 13}, {'e', 27},  {0, 0}};
static const cf_text_esc_simple t_yp_simple[]   = {
    {'0', 0},    {'a', 7},      {'b', 8},      {'t', 9},   {'\t', 9},
    {'n', 10},   {'v', 11},     {'f', 12},     {'r', 13},  {'e', 27},
    {' ', 32},   {'"', 34},     {'/', 47},     {'\\', 92}, {'N', 0x85},
    {'_', 0xA0}, {'L', 0x2028}, {'P', 0x2029}, {0, 0}};
static const cf_text_esc_simple t_prop_simple[] = {
    {'t', 9}, {'n', 10}, {'r', 13}, {'f', 12}, {0, 0}};

static const cf_text_esc_family t_js = {
    t_js_simple, "", "u", "", CF_ESC_PAIRS, CF_ESC_AT_LETTER, CF_ESC_NL_NONE};
static const cf_text_esc_family t_js5 = {
    t_js5_simple,   "x", "u", "", CF_ESC_PAIRS | CF_ESC_NO_OCTAL, CF_ESC_AT_LETTER,
    CF_ESC_NL_JSON5};
static const cf_text_esc_family t_toml = {t_toml_simple,    "x",           "u", "U", 0,
                                          CF_ESC_AT_DIGITS, CF_ESC_NL_NONE};
static const cf_text_esc_family t_yp   = {t_yp_simple, "x",           "u",          "U",
                                          0,           CF_ESC_AT_BAD, CF_ESC_NL_ANY};
static const cf_text_esc_family t_prop = {
    t_prop_simple, "", "u", "", CF_ESC_PAIRS, CF_ESC_AT_LETTER, CF_ESC_NL_NONE};

/* ------------------------------------------------------------------ *
 * Part one: the decoder
 * ------------------------------------------------------------------ */

/* in (after the backslash), read with `endoff` bytes of it (-1: all). */
static void dc(const cf_text_esc_family* f, const char* fname, const char* in,
               long endoff, int kind, cf_u32 cp, size_t used, size_t at, char letter) {
    cf_text_esc r;
    size_t      n = (endoff < 0) ? strlen(in) : (size_t)endoff;
    char        what[160];
    int         ok;

    memset(&r, 0, sizeof r);
    cf_text_esc_decode(f, in, in + n, &r);
    ok = (r.kind == kind);
    if (ok && kind == CF_ESC_CP) {
        ok = (r.cp == cp && r.used == used);
    } else if (ok && kind == CF_ESC_BREAK) {
        ok = (r.used == used);
    } else if (ok && kind >= CF_ESC_E_DIGITS) {
        ok = (r.at == at);
        if (ok && kind == CF_ESC_E_DIGITS) {
            ok = (r.letter == letter);
        }
    }
    sprintf(what, "%s: \"%s\"(%ld) kind %d (want %d), cp %lx used %lu at %lu", fname,
            in, endoff, r.kind, kind, (unsigned long)r.cp, (unsigned long)r.used,
            (unsigned long)r.at);
    check(ok, what);
}
#define CP(f, in, cp, used) dc(&f, #f, in, -1, CF_ESC_CP, cp, used, 0, 0)
#define BRK(f, in, used) dc(&f, #f, in, -1, CF_ESC_BREAK, 0, used, 0, 0)
#define OTHER(f, in) dc(&f, #f, in, -1, CF_ESC_OTHER, 0, 0, 0, 0)
#define REFUSE(f, in, kind, at, letter) dc(&f, #f, in, -1, kind, 0, 0, at, letter)

static void test_decoder(void) {
    cf_text_esc r;

    /* JSON */
    CP(t_js, "n", 10, 1);
    CP(t_js, "\"", 34, 1);
    CP(t_js, "/", 47, 1);
    CP(t_js, "u0041", 0x41, 5);
    CP(t_js, "u00e9", 0xE9, 5);
    CP(t_js,
       "uD83D\\"
       "uDE00",
       0x1F600, 11);
    CP(t_js,
       "ud83d\\"
       "ude00",
       0x1F600, 11);
    REFUSE(t_js, "uD83D", CF_ESC_E_HIGH, 5, 0);
    REFUSE(t_js, "uD83Dx", CF_ESC_E_HIGH, 5, 0);
    REFUSE(t_js, "uD83DA", CF_ESC_E_HIGH, 5, 0);
    REFUSE(t_js,
           "uD83D\\"
           "uDE0",
           CF_ESC_E_HIGH, 5, 0);
    REFUSE(t_js,
           "uD83D\\"
           "u0041",
           CF_ESC_E_HIGH, 5, 0);
    REFUSE(t_js, "uDE00", CF_ESC_E_LOW, 5, 0);
    REFUSE(t_js, "u004", CF_ESC_E_DIGITS, 0, 'u');
    REFUSE(t_js, "u00zz", CF_ESC_E_DIGITS, 0, 'u');
    OTHER(t_js, "q");
    OTHER(t_js, "x41");
    OTHER(t_js, "0");
    OTHER(t_js, "");

    /* JSON5 */
    CP(t_js5, "v", 11, 1);
    CP(t_js5, "0", 0, 1);
    CP(t_js5, "0a", 0, 1);
    REFUSE(t_js5, "01", CF_ESC_E_NUL_DIGIT, 0, 0);
    REFUSE(t_js5, "7", CF_ESC_E_DIGIT, 0, 0);
    CP(t_js5, "x41", 0x41, 3);
    CP(t_js5, "xE9", 0xE9, 3);
    REFUSE(t_js5, "x4", CF_ESC_E_DIGITS, 0, 'x');
    CP(t_js5, "u0041", 0x41, 5);
    CP(t_js5,
       "uD83D\\"
       "uDE00",
       0x1F600, 11);
    BRK(t_js5, "\n", 1);
    BRK(t_js5, "\r\n", 2);
    BRK(t_js5, "\ra", 1);
    BRK(t_js5, "\xE2\x80\xA8", 3);
    BRK(t_js5, "\xE2\x80\xA9", 3);
    OTHER(t_js5, "'");
    OTHER(t_js5, "q");
    OTHER(t_js5, "\xE2\x80\xAA");

    /* TOML */
    CP(t_toml, "e", 27, 1);
    CP(t_toml, "b", 8, 1);
    CP(t_toml, "x41", 0x41, 3);
    CP(t_toml, "u00e9", 0xE9, 5);
    CP(t_toml, "U0001F600", 0x1F600, 9);
    REFUSE(t_toml, "u004", CF_ESC_E_DIGITS, 1, 'u');
    REFUSE(t_toml, "uZ000", CF_ESC_E_DIGITS, 1, 'u');
    REFUSE(t_toml, "uD800", CF_ESC_E_SCALAR, 5, 0);
    REFUSE(t_toml, "U00110000", CF_ESC_E_SCALAR, 9, 0);
    REFUSE(t_toml, "U0000D800", CF_ESC_E_SCALAR, 9, 0);
    OTHER(t_toml, "/");
    OTHER(t_toml, "v");
    OTHER(t_toml, "\n");

    /* YAML */
    CP(t_yp, "0", 0, 1);
    CP(t_yp, "a", 7, 1);
    CP(t_yp, "\t", 9, 1);
    CP(t_yp, "v", 11, 1);
    CP(t_yp, "e", 27, 1);
    CP(t_yp, " ", 32, 1);
    CP(t_yp, "/", 47, 1);
    CP(t_yp, "N", 0x85, 1);
    CP(t_yp, "_", 0xA0, 1);
    CP(t_yp, "L", 0x2028, 1);
    CP(t_yp, "P", 0x2029, 1);
    CP(t_yp, "x41", 0x41, 3);
    CP(t_yp, "U0001F600", 0x1F600, 9);
    REFUSE(t_yp, "x4g", CF_ESC_E_DIGITS, 2, 'x');
    REFUSE(t_yp, "x4", CF_ESC_E_DIGITS, 2, 'x');
    REFUSE(t_yp, "xg1", CF_ESC_E_DIGITS, 1, 'x');
    REFUSE(t_yp, "uD800", CF_ESC_E_SCALAR, 5, 0);
    REFUSE(t_yp, "U00110000", CF_ESC_E_SCALAR, 9, 0);
    BRK(t_yp, "\n", 1);
    BRK(t_yp, "\r\n", 2);
    BRK(t_yp, "\r", 1);
    OTHER(t_yp, "q");
    OTHER(t_yp, "");

    /* .properties */
    CP(t_prop, "t", 9, 1);
    CP(t_prop, "u0041", 0x41, 5);
    CP(t_prop,
       "uD83D\\"
       "uDE00",
       0x1F600, 11);
    REFUSE(t_prop, "uD83D", CF_ESC_E_HIGH, 5, 0);
    REFUSE(t_prop, "uDE00", CF_ESC_E_LOW, 5, 0);
    REFUSE(t_prop, "u00", CF_ESC_E_DIGITS, 0, 'u');
    OTHER(t_prop, "=");
    /* The bound is honoured: four bytes are not the five an escape needs. */
    dc(&t_prop, "t_prop", "u00e9", 4, CF_ESC_E_DIGITS, 0, 0, 0, 'u');
    dc(&t_js, "t_js", "n", 0, CF_ESC_OTHER, 0, 0, 0, 0);

    /* Moves nothing, and writes nothing it was not asked to. */
    memset(&r, 0xAB, sizeof r);
    {
        static const char one[] = "nx";
        cf_text_esc_decode(&t_js, one, &one[1], &r);
    }
    check(r.kind == CF_ESC_CP && r.cp == 10 && r.used == 1, "a decoded escape, whole");
}

/* ------------------------------------------------------------------ *
 * Part two: the writers
 * ------------------------------------------------------------------ */

/* cf_sink_json_string as it was (blob.c), verbatim. */
static void old_json(cf_sink* out, const char* p, size_t n) {
    size_t i;

    cf_sink_putc(out, '"');
    for (i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)p[i];
        switch (c) {
        case '"':
            cf_sink_puts(out, "\\\"");
            break;
        case '\\':
            cf_sink_puts(out, "\\\\");
            break;
        case '\b':
            cf_sink_puts(out, "\\b");
            break;
        case '\f':
            cf_sink_puts(out, "\\f");
            break;
        case '\n':
            cf_sink_puts(out, "\\n");
            break;
        case '\r':
            cf_sink_puts(out, "\\r");
            break;
        case '\t':
            cf_sink_puts(out, "\\t");
            break;
        default:
            if (c < 0x20u) {
                cf_sink_fmt(out, "\\u00%c%c", "0123456789abcdef"[(c >> 4) & 0xF],
                            "0123456789abcdef"[c & 0xF]);
            } else {
                cf_sink_putc(out, (char)c);
            }
            break;
        }
    }
    cf_sink_putc(out, '"');
}

/* toml_quote as it was (toml.c), verbatim. */
static void old_toml(cf_sink* out, const char* p, size_t n) {
    size_t i;

    cf_sink_putc(out, '"');
    for (i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)p[i];
        switch (c) {
        case '"':
            cf_sink_puts(out, "\\\"");
            break;
        case '\\':
            cf_sink_puts(out, "\\\\");
            break;
        case '\n':
            cf_sink_puts(out, "\\n");
            break;
        case '\r':
            cf_sink_puts(out, "\\r");
            break;
        case '\t':
            cf_sink_puts(out, "\\t");
            break;
        default:
            if (c < 0x20u || c == 0x7Fu) {
                cf_sink_fmt(out, "\\u00%c%c", "0123456789abcdef"[(c >> 4) & 0xF],
                            "0123456789abcdef"[c & 0xF]);
            } else {
                cf_sink_putc(out, (char)c);
            }
            break;
        }
    }
    cf_sink_putc(out, '"');
}

/* yaml_quote as it was (yaml.c), verbatim. */
static cf_bool old_yaml(cf_sink* out, const char* p, size_t n) {
    static const char hex[] = "0123456789abcdef";
    size_t            i     = 0;

    cf_sink_putc(out, '"');
    while (i < n) {
        cf_u32 cp;
        size_t len = cf_text_utf8_decode(p + i, p + n, &cp);

        if (len == 0) {
            return CF_FALSE;
        }
        if (cp == '"' || cp == '\\') {
            cf_sink_putc(out, '\\');
            cf_sink_putc(out, (char)cp);
        } else if (cp == '\n') {
            cf_sink_puts(out, "\\n");
        } else if (cp == '\t') {
            cf_sink_puts(out, "\\t");
        } else if (cp < 0x20u || (cp >= 0x7Fu && cp <= 0x9Fu)) {
            cf_sink_fmt(out, "\\x%c%c", hex[(cp >> 4) & 0xFu], hex[cp & 0xFu]);
        } else if (cp == 0xFFFEu || cp == 0xFFFFu) {
            cf_sink_fmt(out, "\\ufff%c", hex[cp & 0xFu]);
        } else {
            cf_sink_write(out, p + i, len);
        }
        i += len;
    }
    cf_sink_putc(out, '"');
    return CF_TRUE;
}

/* The three sets, as the writers hold them. */
static const cf_text_escaper t_json_escaper = {"\"\""
                                               "\\\\"
                                               "\bb"
                                               "\ff"
                                               "\nn"
                                               "\rr"
                                               "\tt",
                                               0x20u, 1u, 0u, 0u};
static const cf_text_escaper t_toml_escaper = {"\"\""
                                               "\\\\"
                                               "\nn"
                                               "\rr"
                                               "\tt",
                                               0x20u, 0x7Fu, 0x7Fu, 0u};
static const cf_text_escaper t_yaml_escaper = {"\"\""
                                               "\\\\"
                                               "\nn"
                                               "\tt",
                                               0x20u, 0x7Fu, 0x9Fu,
                                               CF_ESCAPER_X | CF_ESCAPER_NONCHARS};

/* The new loops: each byte (JSON, TOML) or code point (YAML) through
   cf_text_escape. */
static void new_bytes(const cf_text_escaper* e, cf_sink* out, const char* p, size_t n) {
    size_t i;

    cf_sink_putc(out, '"');
    for (i = 0; i < n; ++i) {
        char   esc[8];
        size_t len = cf_text_escape(e, (unsigned char)p[i], esc);
        if (len == 0) {
            cf_sink_putc(out, p[i]);
        } else {
            cf_sink_write(out, esc, len);
        }
    }
    cf_sink_putc(out, '"');
}

static cf_bool new_yaml(cf_sink* out, const char* p, size_t n) {
    size_t i = 0;

    cf_sink_putc(out, '"');
    while (i < n) {
        cf_u32 cp;
        char   esc[8];
        size_t len = cf_text_utf8_decode(p + i, p + n, &cp);
        size_t elen;

        if (len == 0) {
            return CF_FALSE;
        }
        elen = cf_text_escape(&t_yaml_escaper, cp, esc);
        if (elen == 0) {
            cf_sink_write(out, p + i, len);
        } else {
            cf_sink_write(out, esc, elen);
        }
        i += len;
    }
    cf_sink_putc(out, '"');
    return CF_TRUE;
}

typedef struct two {
    char    a[64], b[64];
    cf_sink sa, sb;
} two;

static void two_init(two* t) {
    memset(t->a, 0, sizeof t->a);
    memset(t->b, 0, sizeof t->b);
    cf_sink_init(&t->sa, t->a, sizeof t->a);
    cf_sink_init(&t->sb, t->b, sizeof t->b);
}

static int two_same(const two* t) {
    return t->sa.len == t->sb.len && memcmp(t->a, t->b, sizeof t->a) == 0;
}

static void test_writers(void) {
    unsigned c;
    cf_u32   cp;
    two      t;
    char     in[8];
    size_t   bad_json = 0, bad_toml = 0, bad_yaml = 0, n;

    for (c = 0; c < 256u; ++c) {
        /* alone, and between two letters */
        in[0] = (char)c;
        n     = 1;
        two_init(&t);
        old_json(&t.sa, in, n);
        new_bytes(&t_json_escaper, &t.sb, in, n);
        bad_json += two_same(&t) ? 0u : 1u;
        two_init(&t);
        old_toml(&t.sa, in, n);
        new_bytes(&t_toml_escaper, &t.sb, in, n);
        bad_toml += two_same(&t) ? 0u : 1u;
        in[0] = 'a';
        in[1] = (char)c;
        in[2] = 'z';
        n     = 3;
        two_init(&t);
        old_json(&t.sa, in, n);
        new_bytes(&t_json_escaper, &t.sb, in, n);
        bad_json += two_same(&t) ? 0u : 1u;
        two_init(&t);
        old_toml(&t.sa, in, n);
        new_bytes(&t_toml_escaper, &t.sb, in, n);
        bad_toml += two_same(&t) ? 0u : 1u;
    }
    check(bad_json == 0, "the JSON escaper writes every byte as the old function did");
    check(bad_toml == 0, "the TOML escaper writes every byte as the old function did");

    for (cp = 0; cp <= 0x10FFFFu; ++cp) {
        cf_bool ok_old, ok_new;
        if (cp >= 0xD800u && cp <= 0xDFFFu) {
            continue; /* no UTF-8 for a surrogate */
        }
        n         = cf_text_utf8_encode(cp, in + 1);
        in[0]     = 'a';
        in[1 + n] = 'z';
        two_init(&t);
        ok_old = old_yaml(&t.sa, in, n + 2);
        ok_new = new_yaml(&t.sb, in, n + 2);
        if (!two_same(&t) || ok_old != ok_new) {
            ++bad_yaml;
        }
    }
    check(bad_yaml == 0,
          "the YAML escaper writes every code point as the old function did");

    /* A string that is not UTF-8 is refused by both, at the same place. */
    two_init(&t);
    check(old_yaml(&t.sa, "a\xE9z", 3) == CF_FALSE &&
              new_yaml(&t.sb, "a\xE9z", 3) == CF_FALSE && two_same(&t),
          "a byte that is not UTF-8 stops both");

    /* The tables obey the card: `to` is at most 0xFFFF, and a length of 0
       is "as it is". */
    check(t_json_escaper.to <= 0xFFFFu && t_toml_escaper.to <= 0xFFFFu &&
              t_yaml_escaper.to <= 0xFFFFu,
          "no range reaches above U+FFFF");
    {
        char esc[8];
        check(cf_text_escape(&t_json_escaper, 'a', esc) == 0,
              "a letter is written as it is");
        check(cf_text_escape(&t_json_escaper, 0x7Fu, esc) == 0, "JSON leaves DEL");
        check(cf_text_escape(&t_toml_escaper, 0x08u, esc) == 6 &&
                  memcmp(esc, "\\u0008", 6) == 0,
              "TOML has no \\b");
        check(cf_text_escape(&t_yaml_escaper, 0x0Du, esc) == 4 &&
                  memcmp(esc, "\\x0d", 4) == 0,
              "YAML writes \\r as \\x0d");
        check(cf_text_escape(&t_yaml_escaper, 0xFFFEu, esc) == 6 &&
                  memcmp(esc, "\\ufffe", 6) == 0,
              "YAML writes U+FFFE");
    }
}

/* ------------------------------------------------------------------ *
 * Part three: "${"
 * ------------------------------------------------------------------ */

/* validate.c's unescape as it was, verbatim but for the arena: the loop. */
static size_t old_unescape(const char* p, size_t n, char* out) {
    size_t i, j = 0;

    for (i = 0; i < n; ++i) {
        if (p[i] == '$' && i + 2 < n && p[i + 1] == '$' && p[i + 2] == '{') {
            ++i; /* drop one '$' of the pair */
        }
        out[j++] = p[i];
    }
    out[j] = '\0';
    return j;
}

/* emit.c's escape_written as it was, verbatim. */
static void old_widen(cf_sink* out, size_t from) {
    size_t limit, end, i, extra = 0;
    char   next = '\0';

    if (out->buf == NULL || out->cap == 0) {
        return;
    }
    limit = out->cap - 1;
    end   = out->len < limit ? out->len : limit;
    if (from >= end) {
        return;
    }
    for (i = from; i + 1 < end; ++i) {
        if (out->buf[i] == '$' && out->buf[i + 1] == '{') {
            ++extra;
        }
    }
    i = end;
    while (i > from) {
        char c = out->buf[--i];
        if (extra < limit - i) {
            out->buf[i + extra] = c;
        }
        if (c == '$' && next == '{') {
            --extra;
            if (extra < limit - i) {
                out->buf[i + extra] = '$';
            }
        }
        next = c;
    }
}

/* The same through cf_text_ref_widen, with the lines emit.c keeps. */
static void new_widen(cf_sink* out, size_t from) {
    size_t limit, end;

    if (out->buf == NULL || out->cap == 0) {
        return;
    }
    limit = out->cap - 1;
    end   = out->len < limit ? out->len : limit;
    if (from >= end) {
        return;
    }
    cf_text_ref_widen(out->buf, from, end, limit);
}

/* A stand-in for cbor_put_head, so that the loop under test is the only
   difference: the length, in decimal, and a bar. */
static void put_head(cf_sink* out, size_t n) {
    cf_sink_fmt(out, "<%u>", (unsigned)n);
}

/* cbor_put_text as it was (cbor.c), verbatim but for the head. */
static void old_cbor_text(cf_sink* out, const char* p, size_t n, cf_bool escape) {
    size_t i, from = 0, extra = 0;

    if (escape) {
        for (i = 0; i + 1 < n; ++i) {
            if (p[i] == '$' && p[i + 1] == '{') {
                ++extra;
            }
        }
    }
    put_head(out, n + extra);
    if (extra != 0) {
        for (i = 0; i + 1 < n; ++i) {
            if (p[i] == '$' && p[i + 1] == '{') {
                cf_sink_write(out, p + from, i + 1 - from);
                cf_sink_putc(out, '$');
                from = i + 1;
            }
        }
    }
    cf_sink_write(out, p + from, n - from);
}

/* The same with the four functions, as cbor.c will write it. */
static void new_cbor_text(cf_sink* out, const char* p, size_t n, cf_bool escape) {
    size_t i, from = 0, extra = 0;

    if (escape) {
        extra = cf_text_ref_count(p, n);
    }
    put_head(out, n + extra);
    if (extra != 0) {
        for (i = cf_text_ref_next(p, n, 0); i < n; i = cf_text_ref_next(p, n, i + 1)) {
            cf_sink_write(out, p + from, i + 1 - from);
            cf_sink_putc(out, '$');
            from = i + 1;
        }
    }
    cf_sink_write(out, p + from, n - from);
}

static size_t g_refs_checked;

static void one_string(const char* s, size_t n) {
    char     o1[16], o2[16];
    size_t   m1, m2, limit, from, i, count = 0;
    two      t;
    char     what[96];
    unsigned bad = 0;

    /* unescape */
    m1     = old_unescape(s, n, o1);
    m2     = cf_text_ref_unescape(s, n, o2);
    o2[m2] = '\0';
    if (m1 != m2 || memcmp(o1, o2, m1 + 1) != 0) {
        ++bad;
    }
    /* the count and the next */
    for (i = 0; i + 1 < n; ++i) {
        if (s[i] == '$' && s[i + 1] == '{') {
            ++count;
        }
    }
    if (cf_text_ref_count(s, n) != count) {
        ++bad;
    }
    for (i = 0; i <= n + 1; ++i) {
        size_t j = i;
        while (j + 1 < n && !(s[j] == '$' && s[j + 1] == '{')) {
            ++j;
        }
        if (j + 1 >= n) {
            j = n;
        }
        if (cf_text_ref_next(s, n, i) != j) {
            ++bad;
        }
    }
    /* widen, with every limit, from the start and from one in */
    for (from = 0; from < 2; ++from) {
        for (limit = 0; limit <= n + count + 2; ++limit) {
            two_init(&t);
            memset(t.a, 'Z', 40);
            memset(t.b, 'Z', 40);
            memcpy(t.a, s, n);
            memcpy(t.b, s, n);
            cf_sink_init(&t.sa, t.a, limit + 1);
            cf_sink_init(&t.sb, t.b, limit + 1);
            t.sa.len = t.sb.len = n;
            old_widen(&t.sa, from);
            new_widen(&t.sb, from);
            if (memcmp(t.a, t.b, 40) != 0) {
                ++bad;
            }
        }
    }
    /* the cbor loop */
    two_init(&t);
    old_cbor_text(&t.sa, s, n, CF_TRUE);
    new_cbor_text(&t.sb, s, n, CF_TRUE);
    if (!two_same(&t)) {
        ++bad;
    }
    two_init(&t);
    old_cbor_text(&t.sa, s, n, CF_FALSE);
    new_cbor_text(&t.sb, s, n, CF_FALSE);
    if (!two_same(&t)) {
        ++bad;
    }
    ++g_refs_checked;
    if (bad != 0) {
        sprintf(what, "\"$ {\" copies differ on \"%.*s\"", (int)n, s);
        check(0, what);
    }
}

/* Every string of length len over the alphabet. */
static void all_strings(size_t len) {
    static const char alpha[] = "${}a";
    char              s[16];
    size_t            count = 1, i, k;

    for (i = 0; i < len; ++i) {
        count *= 4;
    }
    for (i = 0; i < count; ++i) {
        size_t rest = i;
        for (k = 0; k < len; ++k) {
            s[k] = alpha[rest % 4];
            rest /= 4;
        }
        s[len] = '\0';
        one_string(s, len);
    }
}

static void test_refs(void) {
    size_t len;

    for (len = 0; len <= 8; ++len) {
        all_strings(len);
    }
    check(g_refs_checked == 87381u, "every string of length 0 to 8 was tried");
}

/* ------------------------------------------------------------------ *
 * Part four: the readers' places, and the tag
 * ------------------------------------------------------------------ */

static cf_bool nul_table(void* ctx, const char* name, size_t n) {
    (void)ctx;
    (void)name;
    (void)n;
    return CF_TRUE;
}
static cf_bool nul_scalar(void* ctx, const char* name, size_t n, const cf_value* v,
                          cf_bool textual, const cf_loc* at) {
    (void)ctx;
    (void)name;
    (void)n;
    (void)v;
    (void)textual;
    (void)at;
    return CF_TRUE;
}
static cf_bool nul_end(void* ctx) {
    (void)ctx;
    return CF_TRUE;
}
static char* nul_reserve(void* ctx, size_t n) {
    static char buf[1024];
    (void)ctx;
    return (n <= sizeof buf) ? buf : NULL;
}

static void* nul_work(void* ctx, size_t n) {
    static char   buf[1 << 15];
    static size_t used;
    void*         p;

    (void)ctx;
    if (n == 0) {
        used = 0; /* a new document */
        return NULL;
    }
    n = (n + 15u) & ~(size_t)15u;
    if (n > sizeof buf - used) {
        return NULL;
    }
    p = buf + used;
    used += n;
    return p;
}

/* Reads `doc` as the document named `name` through the codec `format`:
   the refusal's message, or "" when it is read. */
static const char* read_doc(const char* format, const char* name, const char* doc) {
    static cf_error err;
    const cf_codec* codec = cf_codec_by_name(format);
    cf_blob         b;
    cf_doc_sink     sink;

    err.size = sizeof err;
    cf_error_clear(&err);
    if (codec == NULL) {
        return NULL; /* a build without it */
    }
    cf_blob_mem(&b, doc, strlen(doc), name);
    memset(&sink, 0, sizeof sink);
    sink.size         = sizeof(cf_doc_sink);
    sink.begin_table  = nul_table;
    sink.begin_array  = nul_table;
    sink.scalar       = nul_scalar;
    sink.end          = nul_end;
    sink.reserve_text = nul_reserve;
    sink.reserve_work = nul_work;
    (void)nul_work(NULL, 0);
    (void)cf_codec_decode(codec, &b, &sink, &err);
    return err.message;
}

static void places(const char* format, const char* name, const char* doc,
                   const char* want) {
    const char* got = read_doc(format, name, doc);
    if (got != NULL) {
        check_text(got, want, doc);
    }
}

static void test_places(void) {
    /* What the readers say today, the places the decoder must give back. */
    places("json", "t.json",
           "{\"a\":\"\\"
           "u12\"}",
           "t.json:1:8: \\"
           "u needs four hexadecimal digits");
    places("toml", "t.toml",
           "a = \"\\"
           "u12\"",
           "t.toml:1:8: \\"
           "x needs two hex digits, \\"
           "u four and \\"
           "U eight");
#if !CF_YAML_LIBYAML && !CF_YAML_LIBFYAML
    places("yaml", "t.yaml", "a: \"\\x4g\"",
           "t.yaml:1:8: an escape needs its hexadecimal digits");
#endif
    places("properties", "t.properties",
           "a=\\"
           "u12",
           "t.properties:1: \\"
           "u needs four hexadecimal digits");
}

static void test_tag(void) {
#if !CF_YAML_LIBYAML && !CF_YAML_LIBFYAML
    const char* got;

    /* Hexadecimal digits only. Before A-7 an '@' or a backquote was read as
       the digit 9, so these tags were taken for bytes. */
    got = read_doc("yaml", "t.yaml", "a: !<tag:x,2000:%@0> 1\n");
    if (got != NULL) {
        has(got, "an escape in a tag is two hexadecimal digits", "the digit '@'");
    }
    got = read_doc("yaml", "t.yaml", "a: !<tag:x,2000:%`0> 1\n");
    if (got != NULL) {
        has(got, "an escape in a tag is two hexadecimal digits", "the digit backquote");
    }
    got = read_doc("yaml", "t.yaml", "a: !<tag:x,2000:%0@> 1\n");
    if (got != NULL) {
        has(got, "an escape in a tag is two hexadecimal digits", "the second digit");
    }
    /* What was read stays read: %73tr, %73Tr's cousins in either case. */
    got = read_doc("yaml", "t.yaml", "a: !<tag:yaml.org,2002:%73tr> 12\n");
    if (got != NULL) {
        check_text(got, "", "%73 is s");
    }
    got = read_doc("yaml", "t.yaml", "a: !<tag:yaml.org,2002:%69%6e%74> 12\n");
    if (got != NULL) {
        check_text(got, "", "%6e is n");
    }
    got = read_doc("yaml", "t.yaml", "a: !<tag:yaml.org,2002:%69%6E%74> 12\n");
    if (got != NULL) {
        check_text(got, "", "%6E is n");
    }
    /* A '%' with fewer than two bytes after it passes as it is. */
    got = read_doc("yaml", "t.yaml", "a: !<tag:yaml.org,2002:int%4> 12\n");
    if (got != NULL) {
        check(strstr(got, "an escape in a tag") == NULL, "a short '%' is no escape");
    }
#endif
}

int main(void) {
    test_decoder();
    test_writers();
    test_refs();
    test_places();
    test_tag();
    return cf_test_done();
}
