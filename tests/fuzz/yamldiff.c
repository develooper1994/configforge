/*
 * yamldiff.c - the same bytes to the YAML codec read through the built-in
 * parser and to the one read through libyaml, in one process (N-12).
 *
 * yamldiff_builtin.c and yamldiff_libyaml.c each include yaml.c, under
 * another name for its codec, so both are the real codec and the shared
 * UTF-8 check of cf_codec_decode runs before either. Each side's events
 * are written as lines, without a place (two parsers mark one differently,
 * which is not a difference of reading) and compared. Both refusing is
 * agreement. A difference that no rule of yamldiff.known covers aborts, so
 * libFuzzer keeps the input. A fourth side, anchor-colon (key `-`), covers any
 * difference, whatever its side, on an input in which an anchor or alias name
 * holds a `:` (YAML 1.2 reads it as a name character, libyaml cuts the name
 * there); yd_anchor_colon is its scan. C89, public headers only.
 */

#undef CF_INTERNAL_API
#define CF_INTERNAL_API 0

#include "configforge/cf_alloc.h"
#include "configforge/cf_error.h"
#include "configforge/cf_serialize.h"
#include "configforge/cf_value.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef YD_KNOWN
#error "YD_KNOWN names the file of documented differences"
#endif

extern const cf_codec cf_codec_yaml_builtin;
extern const cf_codec cf_codec_yaml_libyaml;

int LLVMFuzzerTestOneInput(const unsigned char* data, size_t size);

enum { YD_INPUT_MAX = 8192, YD_OUT_MAX = 262144, YD_RULES = 64, YD_NEEDLE_MAX = 64 };

enum { YD_LIB_REFUSES, YD_BI_REFUSES, YD_EVENTS, YD_SIDES };

static const char* const g_sides[YD_SIDES] = {"lib-refuses", "bi-refuses", "events"};

typedef struct yd_rule {
    int    side;
    size_t n;
    char   needle[YD_NEEDLE_MAX];
} yd_rule;

static yd_rule g_rules[YD_RULES];
static size_t  g_nrules;
static int     g_loaded;
static cf_bool g_anchor_colon;

/* One side: the lines of its events, and the room its reader asked for. */
typedef struct yd_out {
    char      b[YD_OUT_MAX];
    size_t    n;
    cf_bool   over;
    cf_arena* ar;
} yd_out;

static void yd_die(const char* what, unsigned line) {
    fprintf(stderr, "yamldiff: %s: line %u: %s\n", YD_KNOWN, line, what);
    abort();
}

static int yd_hex(int c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

/* A key's bytes, from its escaped text; the length, or 0 for a bad one. */
static size_t yd_unescape(const char* s, size_t len, char* out) {
    size_t i = 0;
    size_t n = 0;

    while (i < len) {
        char c = s[i++];

        if (n >= (size_t)YD_NEEDLE_MAX) {
            return 0;
        }
        if (c != '\\') {
            out[n++] = c;
        } else if (i < len && s[i] == 'n') {
            out[n++] = '\n';
            ++i;
        } else if (i < len && s[i] == 't') {
            out[n++] = '\t';
            ++i;
        } else if (i < len && s[i] == '\\') {
            out[n++] = '\\';
            ++i;
        } else if (i + 2 < len && s[i] == 'x' && yd_hex(s[i + 1]) >= 0 &&
                   yd_hex(s[i + 2]) >= 0) {
            out[n++] = (char)(yd_hex(s[i + 1]) * 16 + yd_hex(s[i + 2]));
            i += 3;
        } else {
            return 0;
        }
    }
    return n;
}

/* The rules, once. A file that is missing or has a bad line ends the run. */
static void yd_load(void) {
    FILE*    f = fopen(YD_KNOWN, "rb");
    char     line[512];
    unsigned no = 0;

    g_loaded = 1;
    if (f == NULL) {
        yd_die("cannot be opened", 0);
    }
    while (fgets(line, (int)sizeof line, f) != NULL) {
        size_t len = strlen(line);
        char*  t1;
        char*  t2;
        size_t k;
        int    side = -1;

        ++no;
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        if (len == 0 || line[0] == '#') {
            continue;
        }
        t1 = strchr(line, '\t');
        t2 = (t1 != NULL) ? strchr(t1 + 1, '\t') : NULL;
        if (t1 == NULL || t2 == NULL) {
            yd_die("a rule has three fields, tab-separated", no);
        }
        *t1 = '\0';
        *t2 = '\0';
        for (k = 0; k < (size_t)YD_SIDES; ++k) {
            if (strcmp(line, g_sides[k]) == 0) {
                side = (int)k;
            }
        }
        if (strcmp(line, "anchor-colon") == 0) {
            if (strcmp(t1 + 1, "-") != 0) {
                yd_die("the key of anchor-colon is -", no);
            }
            g_anchor_colon = CF_TRUE;
            continue;
        }
        if (side < 0) {
            yd_die("the side is lib-refuses, bi-refuses or events", no);
        }
        if (g_nrules >= (size_t)YD_RULES) {
            yd_die("too many rules", no);
        }
        g_rules[g_nrules].side = side;
        g_rules[g_nrules].n =
            yd_unescape(t1 + 1, strlen(t1 + 1), g_rules[g_nrules].needle);
        if (g_rules[g_nrules].n < ((side == YD_EVENTS) ? 3u : 8u)) {
            yd_die("a key is 3 bytes (events) or 8 bytes (a refusal) at least, escapes "
                   "valid",
                   no);
        }
        ++g_nrules;
    }
    (void)fclose(f);
}

static cf_bool yd_has(const unsigned char* d, size_t n, const char* needle, size_t m) {
    size_t i;

    for (i = 0; i + m <= n; ++i) {
        if (memcmp(d + i, needle, m) == 0) {
            return CF_TRUE;
        }
    }
    return CF_FALSE;
}

/* Whether an anchor or alias name of the input holds a `:`: a `&` or `*` at
   the start, or after white space, `[`, `{` or `,`, then its run of
   ns-anchor-char bytes (not white space, not a flow indicator). */
static cf_bool yd_is_space(unsigned char c) {
    return (cf_bool)(c == ' ' || c == '\t' || c == '\n' || c == '\r');
}

static cf_bool yd_anchor_colon(const unsigned char* d, size_t n) {
    size_t i;

    for (i = 0; i < n; ++i) {
        unsigned char p;

        if (d[i] != '&' && d[i] != '*') {
            continue;
        }
        p = (i == 0) ? (unsigned char)' ' : d[i - 1];
        if (!yd_is_space(p) && p != '[' && p != '{' && p != ',') {
            continue;
        }
        for (++i; i < n; ++i) {
            if (yd_is_space(d[i]) || d[i] == ',' || d[i] == '[' || d[i] == ']' ||
                d[i] == '{' || d[i] == '}') {
                break;
            }
            if (d[i] == ':') {
                return CF_TRUE;
            }
        }
    }
    return CF_FALSE;
}

/* Whether a rule covers the difference: for a refusal its key is in the
   message of the side that refuses, for events it is in the input. The
   anchor-colon rule looks at the input whatever the side. */
static cf_bool yd_covered(int side, const unsigned char* d, size_t n, const char* msg) {
    size_t r;

    if (g_anchor_colon && yd_anchor_colon(d, n)) {
        return CF_TRUE;
    }
    if (side != YD_EVENTS) {
        d = (const unsigned char*)msg;
        n = strlen(msg);
    }
    for (r = 0; r < g_nrules; ++r) {
        if (g_rules[r].side == side && yd_has(d, n, g_rules[r].needle, g_rules[r].n)) {
            return CF_TRUE;
        }
    }
    return CF_FALSE;
}

static void yd_put(yd_out* o, const char* s, size_t n) {
    if (o->over || o->n + n > sizeof o->b) {
        o->over = CF_TRUE;
        return;
    }
    memcpy(o->b + o->n, s, n);
    o->n += n;
}

static void yd_puts(yd_out* o, const char* s) {
    yd_put(o, s, strlen(s));
}

/* Text, quoted, with nothing in it that starts a line of its own. */
static void yd_quoted(yd_out* o, const char* p, size_t n) {
    static const char hex[] = "0123456789abcdef";
    size_t            i;

    yd_puts(o, "\"");
    for (i = 0; i < n; ++i) {
        unsigned char ch = (unsigned char)p[i];

        if (ch == '\\') {
            yd_puts(o, "\\\\");
        } else if (ch == '"') {
            yd_puts(o, "\\\"");
        } else if (ch < 0x20 || ch == 0x7f) {
            char e[4];

            e[0] = '\\';
            e[1] = 'x';
            e[2] = hex[ch >> 4];
            e[3] = hex[ch & 15u];
            yd_put(o, e, sizeof e);
        } else {
            yd_put(o, p + i, 1);
        }
    }
    yd_puts(o, "\"");
}

static cf_bool yd_table(void* ctx, const char* name, size_t n) {
    yd_puts((yd_out*)ctx, "T ");
    yd_quoted((yd_out*)ctx, name, n);
    yd_puts((yd_out*)ctx, "\n");
    return CF_TRUE;
}

static cf_bool yd_array(void* ctx, const char* name, size_t n) {
    yd_puts((yd_out*)ctx, "A ");
    yd_quoted((yd_out*)ctx, name, n);
    yd_puts((yd_out*)ctx, "\n");
    return CF_TRUE;
}

static cf_bool yd_scalar(void* ctx, const char* name, size_t n, const cf_value* v,
                         cf_bool textual, const cf_loc* at) {
    yd_out* o = (yd_out*)ctx;

    (void)at;
    yd_puts(o, "S ");
    yd_quoted(o, name, n);
    yd_puts(o, " ");
    yd_puts(o, cf_type_name(v->type));
    yd_puts(o, " ");
    if (v->type == CF_TYPE_TEXT) {
        yd_quoted(o, v->u.t.p, v->u.t.n);
    } else {
        char text[CF_VALUE_TEXT_MAX + 1];

        text[cf_value_to_text(v, text, sizeof text)] = '\0';
        yd_puts(o, text);
    }
    if (textual) {
        yd_puts(o, " (textual)");
    }
    yd_puts(o, "\n");
    return CF_TRUE;
}

static cf_bool yd_end(void* ctx) {
    yd_puts((yd_out*)ctx, "E\n");
    return CF_TRUE;
}

static char* yd_text(void* ctx, size_t n) {
    return (char*)cf_arena_alloc(((yd_out*)ctx)->ar, n);
}

static void* yd_work(void* ctx, size_t n) {
    return cf_arena_alloc(((yd_out*)ctx)->ar, n);
}

/* Reads `in` through `c` into `o`: the decode's result. */
static cf_bool yd_run(const cf_codec* c, const cf_blob* in, yd_out* o, cf_error* err) {
    cf_doc_sink sink;
    cf_bool     ok;

    o->n    = 0;
    o->over = CF_FALSE;
    o->ar   = cf_arena_new(NULL, 0);
    if (o->ar == NULL) {
        o->over = CF_TRUE;
        return CF_FALSE;
    }
    memset(&sink, 0, sizeof sink);
    sink.size         = sizeof sink;
    sink.begin_table  = yd_table;
    sink.begin_array  = yd_array;
    sink.scalar       = yd_scalar;
    sink.end          = yd_end;
    sink.ctx          = o;
    sink.reserve_text = yd_text;
    sink.reserve_work = yd_work;
    ok                = cf_codec_decode(c, in, &sink, err);
    cf_arena_free(o->ar);
    o->ar = NULL;
    return ok;
}

static void yd_tail(const char* who, const yd_out* o, size_t from) {
    size_t left = o->n - from;

    fprintf(stderr, "  %s events from the first difference:\n%.*s", who,
            (int)((left < 400u) ? left : 400u), o->b + from);
}

static void yd_report(const char* side, const unsigned char* d, size_t n, cf_bool a_ok,
                      const yd_out* a, const cf_error* ae, cf_bool b_ok,
                      const yd_out* b, const cf_error* be) {
    size_t i = 0;

    fprintf(stderr, "yamldiff: undocumented difference, side %s\n", side);
    fprintf(stderr, "  built-in: %s%s\n",
            a_ok ? "accepts" : "refuses: ", a_ok ? "" : ae->message);
    fprintf(stderr, "  libyaml:  %s%s\n",
            b_ok ? "accepts" : "refuses: ", b_ok ? "" : be->message);
    if (a_ok && b_ok) {
        while (i < a->n && i < b->n && a->b[i] == b->b[i]) {
            ++i;
        }
        while (i > 0 && a->b[i - 1] != '\n') {
            --i;
        }
        yd_tail("built-in", a, i);
        yd_tail("libyaml", b, i);
    }
    fprintf(stderr, "  input (%lu bytes, hex):", (unsigned long)n);
    for (i = 0; i < n; ++i) {
        fprintf(stderr, "%s%02x", (i % 32u == 0) ? "\n   " : " ", (unsigned)d[i]);
    }
    fprintf(stderr, "\n");
}

int LLVMFuzzerTestOneInput(const unsigned char* data, size_t size) {
    static yd_out a;
    static yd_out b;
    static char   in[YD_INPUT_MAX];
    cf_blob       blob;
    cf_error      ae = CF_ERROR_INIT;
    cf_error      be = CF_ERROR_INIT;
    cf_bool       a_ok;
    cf_bool       b_ok;
    int           side = -1;

    if (!g_loaded) {
        yd_load();
    }
    if (size > sizeof in) {
        size = sizeof in;
    }
    if (size > 0) {
        memcpy(in, data, size);
    }
    blob.p    = in;
    blob.n    = size;
    blob.name = "fuzz.yaml";
    a_ok      = yd_run(&cf_codec_yaml_builtin, &blob, &a, &ae);
    b_ok      = yd_run(&cf_codec_yaml_libyaml, &blob, &b, &be);
    if (a.over || b.over) {
        return 0;
    }
    if (a_ok && !b_ok) {
        side = YD_LIB_REFUSES;
    } else if (!a_ok && b_ok) {
        side = YD_BI_REFUSES;
    } else if (a_ok && (a.n != b.n || memcmp(a.b, b.b, a.n) != 0)) {
        side = YD_EVENTS;
    }
    if (side >= 0 && !yd_covered(side, (const unsigned char*)in, size,
                                 (side == YD_LIB_REFUSES) ? be.message : ae.message)) {
        yd_report(g_sides[side], (const unsigned char*)in, size, a_ok, &a, &ae, b_ok,
                  &b, &be);
        abort();
    }
    return 0;
}
