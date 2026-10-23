/*
 * toml.c - the built-in TOML subset. C89.
 *
 * The subset D-23 described, written here rather than vendored for the
 * reason the INI codec is: streaming, allocation-free, and small enough
 * to read in one sitting. It covers what hand-written configuration
 * uses:
 *
 *     # comment
 *     [server]                 tables, dotted:   [server.tls]
 *     port = 8080              integers:         1_000  0x1F  0o17  0b101
 *     ratio = 0.75             floats:           1e3  -2.5E-3
 *     name = "a\tb\u00e9"      basic strings, with escapes
 *     path = 'C:\raw'          literal strings
 *     on = true                booleans
 *     ports = [80, 443,]       arrays, over several lines, trailing comma
 *     tls = { cert = "x" }     inline tables
 *     db.pool.size = 4         dotted keys
 *     [[upstream]]             arrays of tables, written as one block
 *
 * Refused, each with a message that names the construct and says what
 * to write instead: dates and times, multi-line strings, inf and nan,
 * and an array of tables reopened after another table came between.
 *
 * One departure from the TOML specification, stated rather than
 * hidden: a key set twice is not an error here. Within one source the
 * later occurrence wins, as it does in every format this library
 * reads, and Doctor reports the earlier one as unreachable (CFD001) -
 * which is a better answer to "why did my edit do nothing" than refusing
 * to start.
 *
 * Every value carries its line and column, so explain() names the place
 * in a TOML file exactly as it does in any other.
 */

#include "configforge/cf_serialize.h"

#include <string.h>

#if CF_WITH_TOML

/* ================================================================== *
 * Decoding
 * ================================================================== */

#define TOML_MAX_DEPTH CF_MAX_DEPTH

/* What the open path is made of. A table segment is one header segment;
   an array of tables is one segment plus an element that has no name. */
typedef enum toml_kind { TOML_TABLE, TOML_ARRAY, TOML_ELEMENT } toml_kind;

/* Names are kept one after another in a single key-sized buffer, not a
   key-sized buffer each: the whole path has to fit in one configuration
   key anyway, and thirty-two of those would be eight kilobytes of stack
   spent on nothing (the budget is two). */
typedef struct toml_open {
    unsigned short at; /* the name, in toml_scan.names */
    unsigned short n;
    toml_kind      kind;
} toml_open;

typedef struct toml_scan {
    const char*        p;
    const char*        end;
    const char*        file;
    const char*        line_start;
    unsigned int       line;
    const cf_doc_sink* out;
    cf_error*          err;
    int                depth;                /* nesting inside one value */
    toml_open          open[TOML_MAX_DEPTH]; /* the header path currently open */
    int                nopen;
    char               names[CF_KEY_MAX];
    size_t             nused;
    char               closed[CF_KEY_MAX]; /* last array of tables left */
} toml_scan;

/* A dotted key, its segments in one buffer for the same reason. */
#define TOML_KEY_PARTS 8

typedef struct toml_dkey {
    char   buf[CF_KEY_MAX];
    size_t at[TOML_KEY_PARTS];
    size_t len[TOML_KEY_PARTS];
    int    n;
} toml_dkey;

#define TOML_SEG(k, i) ((k)->buf + (k)->at[i])

static void toml_loc(const toml_scan* s, const char* at, cf_loc* out) {
    out->file = s->file;
    out->line = s->line;
    out->col  = (unsigned int)(at - s->line_start) + 1u;
}

static cf_bool toml_fail(toml_scan* s, const char* at, const char* what) {
    cf_loc l;
    toml_loc(s, at, &l);
    return cf_error_set(s->err, CF_E_SOURCE, NULL, s->file, "%s:%u:%u: %s", l.file,
                        l.line, l.col, what);
}

/* Space within a line. */
static void toml_blank(toml_scan* s) {
    while (s->p < s->end && (*s->p == ' ' || *s->p == '\t' || *s->p == '\r')) {
        ++s->p;
    }
}

static void toml_to_eol(toml_scan* s) {
    while (s->p < s->end && *s->p != '\n') {
        ++s->p;
    }
}

/* Space, newlines and comments: what may sit between array elements. */
static void toml_space_lines(toml_scan* s) {
    for (;;) {
        toml_blank(s);
        if (s->p < s->end && *s->p == '#') {
            toml_to_eol(s);
        }
        if (s->p < s->end && *s->p == '\n') {
            ++s->p;
            ++s->line;
            s->line_start = s->p;
            continue;
        }
        return;
    }
}

/* After a value or a header: only a comment may follow on the line. */
static cf_bool toml_end_of_line(toml_scan* s) {
    toml_blank(s);
    if (s->p < s->end && *s->p == '#') {
        toml_to_eol(s);
    }
    if (s->p < s->end && *s->p != '\n') {
        return toml_fail(s, s->p, "expected the end of the line");
    }
    return CF_TRUE;
}

static int toml_hex(int c) {
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

static size_t toml_utf8(cf_u32 cp, char* out) {
    if (cp < 0x80u) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800u) {
        out[0] = (char)(0xC0u | (cp >> 6));
        out[1] = (char)(0x80u | (cp & 0x3Fu));
        return 2;
    }
    if (cp < 0x10000u) {
        out[0] = (char)(0xE0u | (cp >> 12));
        out[1] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        out[2] = (char)(0x80u | (cp & 0x3Fu));
        return 3;
    }
    out[0] = (char)(0xF0u | (cp >> 18));
    out[1] = (char)(0x80u | ((cp >> 12) & 0x3Fu));
    out[2] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
    out[3] = (char)(0x80u | (cp & 0x3Fu));
    return 4;
}

/* A basic "..." or literal '...' string into dst. Returns the length or
   (size_t)-1 having said why. */
static size_t toml_string(toml_scan* s, char* dst, size_t cap) {
    char   q = *s->p;
    size_t n = 0;

    if (s->end - s->p >= 3 && s->p[1] == q && s->p[2] == q) {
        toml_fail(s, s->p,
                  "multi-line strings are not in the TOML subset this library "
                  "reads; write the value on one line, with \\n for a newline");
        return (size_t)-1;
    }
    ++s->p;
    while (s->p < s->end) {
        unsigned char c = (unsigned char)*s->p;
        if (c == (unsigned char)q) {
            ++s->p;
            dst[n] = '\0';
            return n;
        }
        if (c == '\n') {
            toml_fail(s, s->p, "the string is not closed on its line");
            return (size_t)-1;
        }
        if (c < 0x20u && c != '\t') {
            toml_fail(s, s->p, "a string may not contain a control character");
            return (size_t)-1;
        }
        if (c == '\\' && q == '"') {
            cf_u32 cp     = 0;
            int    digits = 0;
            char   one    = 0;
            ++s->p;
            if (s->p >= s->end) {
                toml_fail(s, s->p, "escape at the end of the input");
                return (size_t)-1;
            }
            switch (*s->p) {
            case '"':
                one = '"';
                break;
            case '\\':
                one = '\\';
                break;
            case 'b':
                one = '\b';
                break;
            case 't':
                one = '\t';
                break;
            case 'n':
                one = '\n';
                break;
            case 'f':
                one = '\f';
                break;
            case 'r':
                one = '\r';
                break;
            case 'u':
                digits = 4;
                break;
            case 'U':
                digits = 8;
                break;
            default:
                toml_fail(s, s->p, "unknown escape");
                return (size_t)-1;
            }
            ++s->p;
            if (digits > 0) {
                char   enc[4];
                size_t k;
                int    i;
                if (s->end - s->p < digits) {
                    toml_fail(s, s->p, "\\u needs four hex digits and \\U eight");
                    return (size_t)-1;
                }
                for (i = 0; i < digits; ++i) {
                    int h = toml_hex((unsigned char)s->p[i]);
                    if (h < 0) {
                        toml_fail(s, s->p, "\\u needs four hex digits and \\U eight");
                        return (size_t)-1;
                    }
                    cp = (cp << 4) | (cf_u32)h;
                }
                s->p += digits;
                if (cp > 0x10FFFFu || (cp >= 0xD800u && cp <= 0xDFFFu)) {
                    toml_fail(s, s->p, "not a Unicode scalar value");
                    return (size_t)-1;
                }
                k = toml_utf8(cp, enc);
                if (n + k + 1 > cap) {
                    toml_fail(s, s->p, "the string is too long");
                    return (size_t)-1;
                }
                memcpy(dst + n, enc, k);
                n += k;
            } else {
                if (n + 2 > cap) {
                    toml_fail(s, s->p, "the string is too long");
                    return (size_t)-1;
                }
                dst[n++] = one;
            }
            continue;
        }
        if (n + 2 > cap) {
            toml_fail(s, s->p, "the string is too long");
            return (size_t)-1;
        }
        dst[n++] = (char)c;
        ++s->p;
    }
    toml_fail(s, s->p, "the string is not closed");
    return (size_t)-1;
}

static cf_bool toml_bare(int c) {
    return (cf_is_alnum(c) || c == '_' || c == '-') ? CF_TRUE : CF_FALSE;
}

/* One key segment: bare, "quoted" or 'quoted'. */
static size_t toml_key_segment(toml_scan* s, char* dst, size_t cap) {
    size_t n = 0;

    if (s->p < s->end && (*s->p == '"' || *s->p == '\'')) {
        return toml_string(s, dst, cap);
    }
    while (s->p < s->end && toml_bare((unsigned char)*s->p)) {
        if (n + 1 >= cap) {
            toml_fail(s, s->p, "the key is too long");
            return (size_t)-1;
        }
        dst[n++] = *s->p++;
    }
    if (n == 0) {
        toml_fail(s, s->p, "expected a key");
        return (size_t)-1;
    }
    dst[n] = '\0';
    return n;
}

/* A dotted key into segments. */
static cf_bool toml_key(toml_scan* s, toml_dkey* k) {
    size_t used = 0;

    k->n = 0;
    for (;;) {
        size_t n;

        toml_blank(s);
        if (k->n >= TOML_KEY_PARTS) {
            return toml_fail(s, s->p, "the key has too many parts");
        }
        if (used + 2 > sizeof k->buf) {
            return toml_fail(s, s->p, "the key is too long");
        }
        n = toml_key_segment(s, k->buf + used, sizeof k->buf - used);
        if (n == (size_t)-1) {
            return CF_FALSE;
        }
        k->at[k->n]  = used;
        k->len[k->n] = n;
        ++k->n;
        used += n + 1;
        toml_blank(s);
        if (s->p < s->end && *s->p == '.') {
            ++s->p;
            continue;
        }
        return CF_TRUE;
    }
}

/* Integers and floats, underscores between digits allowed. Dates are
   recognised in order to refuse them. */
static cf_bool toml_number(toml_scan* s, cf_value* v) {
    const char* start = s->p;
    char        buf[CF_VALUE_TEXT_MAX + 8]; /* the digits of either kind */
    size_t      n       = 0;
    cf_bool     is_real = CF_FALSE;
    int         base    = 10;

    if (s->p < s->end && (*s->p == '+' || *s->p == '-')) {
        buf[n++] = *s->p++;
    }
    if (s->end - s->p >= 3 &&
        (memcmp(s->p, "inf", 3) == 0 || memcmp(s->p, "nan", 3) == 0)) {
        return toml_fail(s, start,
                         "inf and nan are not in the TOML subset this library reads");
    }
    if (s->end - s->p >= 2 && s->p[0] == '0' &&
        (s->p[1] == 'x' || s->p[1] == 'o' || s->p[1] == 'b')) {
        base = (s->p[1] == 'x') ? 16 : (s->p[1] == 'o') ? 8 : 2;
        s->p += 2;
    }
    while (s->p < s->end) {
        char c = *s->p;
        if (c == '_') {
            ++s->p;
            continue;
        }
        /* A '-' inside a number is a date, unless it signs an exponent. */
        if (base == 10 && n > 0 &&
            (c == ':' || (c == '-' && buf[n - 1] != 'e' && buf[n - 1] != 'E'))) {
            return toml_fail(s, start,
                             "dates and times are not in the TOML subset this library "
                             "reads; quote it as a string");
        }
        if (base == 10 && (c == '.' || c == 'e' || c == 'E' ||
                           ((c == '+' || c == '-') && n > 0 &&
                            (buf[n - 1] == 'e' || buf[n - 1] == 'E')))) {
            is_real = CF_TRUE;
        } else if (!(toml_hex((unsigned char)c) >= 0 &&
                     toml_hex((unsigned char)c) < base) &&
                   !(base == 16 && toml_hex((unsigned char)c) >= 0)) {
            break;
        }
        if (n + 1 >= sizeof buf) {
            return toml_fail(s, start, "the number is too long");
        }
        buf[n++] = c;
        ++s->p;
    }
    if (n == 0 || (n == 1 && (buf[0] == '+' || buf[0] == '-'))) {
        return toml_fail(s, start, "expected a value");
    }
    if (base != 10) {
        cf_u64 acc = 0;
        size_t i;
        for (i = (buf[0] == '+' || buf[0] == '-') ? 1u : 0u; i < n; ++i) {
            cf_u64 d = (cf_u64)toml_hex((unsigned char)buf[i]);
            if (acc > (CF_U64_MAX - d) / (cf_u64)base) {
                return toml_fail(s, start, "the integer does not fit in 64 bits");
            }
            acc = acc * (cf_u64)base + d;
        }
        if (acc > (cf_u64)CF_I64_MAX || buf[0] == '-' || buf[0] == '+') {
            return toml_fail(s, start,
                             "a hexadecimal, octal or binary integer is unsigned and "
                             "must fit in 63 bits");
        }
        cf_value_int(v, (cf_i64)acc);
        return CF_TRUE;
    }
    if (!is_real) {
        cf_i64 i;
        if (!cf_text_to_i64(buf, n, &i, NULL)) {
            return toml_fail(s, start, "the integer does not fit in 64 bits");
        }
        cf_value_int(v, i);
        return CF_TRUE;
    }
#if CF_WITH_REAL
    {
        double d;
        if (!cf_text_to_f64(buf, n, &d, NULL)) {
            return toml_fail(s, start, "malformed number");
        }
        cf_value_real(v, d);
        return CF_TRUE;
    }
#else
    return toml_fail(s, start, "this build has no floating point support");
#endif
}

static cf_bool toml_value(toml_scan* s, const char* name, size_t nlen);

/* Open the leading segments of a dotted key as tables, run the value,
   close them again. "a.b.c = 1" is a table a, a table b, a scalar c. */
static cf_bool toml_keyval(toml_scan* s) {
    toml_dkey k;
    int       n, i;

    if (!toml_key(s, &k)) {
        return CF_FALSE;
    }
    n = k.n;
    if (s->p >= s->end || *s->p != '=') {
        return toml_fail(s, s->p, "expected '=' after the key");
    }
    ++s->p;
    toml_blank(s);
    for (i = 0; i + 1 < n; ++i) {
        if (!s->out->begin_table(s->out->ctx, TOML_SEG(&k, i), k.len[i])) {
            return toml_fail(s, s->p, "not a valid key");
        }
    }
    if (!toml_value(s, TOML_SEG(&k, n - 1), k.len[n - 1])) {
        return CF_FALSE;
    }
    for (i = 0; i + 1 < n; ++i) {
        if (!s->out->end(s->out->ctx)) {
            return toml_fail(s, s->p, "the document could not be recorded");
        }
    }
    return CF_TRUE;
}

static cf_bool toml_array(toml_scan* s, const char* name, size_t nlen) {
    if (++s->depth > TOML_MAX_DEPTH) {
        return toml_fail(s, s->p, "nesting is too deep");
    }
    if (!s->out->begin_array(s->out->ctx, name, nlen)) {
        return toml_fail(s, s->p, "not a valid key");
    }
    ++s->p; /* '[' */
    for (;;) {
        toml_space_lines(s);
        if (s->p >= s->end) {
            return toml_fail(s, s->p, "the array is not closed");
        }
        if (*s->p == ']') {
            ++s->p;
            break;
        }
        if (!toml_value(s, NULL, 0)) {
            return CF_FALSE;
        }
        toml_space_lines(s);
        if (s->p < s->end && *s->p == ',') {
            ++s->p; /* a trailing comma is allowed */
            continue;
        }
        if (s->p < s->end && *s->p == ']') {
            ++s->p;
            break;
        }
        return toml_fail(s, s->p, "expected ',' or ']'");
    }
    --s->depth;
    return s->out->end(s->out->ctx)
               ? CF_TRUE
               : toml_fail(s, s->p, "the document could not be recorded");
}

static cf_bool toml_inline_table(toml_scan* s, const char* name, size_t nlen) {
    cf_bool first = CF_TRUE;

    if (++s->depth > TOML_MAX_DEPTH) {
        return toml_fail(s, s->p, "nesting is too deep");
    }
    if (!s->out->begin_table(s->out->ctx, name, nlen)) {
        return toml_fail(s, s->p, "not a valid key");
    }
    ++s->p; /* '{' */
    for (;;) {
        toml_blank(s);
        if (s->p >= s->end || *s->p == '\n') {
            return toml_fail(s, s->p, "an inline table is closed on its own line");
        }
        if (*s->p == '}') {
            ++s->p;
            break;
        }
        if (!first) {
            if (*s->p != ',') {
                return toml_fail(s, s->p, "expected ',' or '}'");
            }
            ++s->p;
            toml_blank(s);
        }
        first = CF_FALSE;
        if (!toml_keyval(s)) {
            return CF_FALSE;
        }
    }
    --s->depth;
    return s->out->end(s->out->ctx)
               ? CF_TRUE
               : toml_fail(s, s->p, "the document could not be recorded");
}

static cf_bool toml_value(toml_scan* s, const char* name, size_t nlen) {
    cf_value    v;
    cf_loc      at;
    const char* start = s->p;

    if (s->p >= s->end) {
        return toml_fail(s, s->p, "expected a value");
    }
    toml_loc(s, start, &at);
    switch (*s->p) {
    case '[':
        return toml_array(s, name, nlen);
    case '{':
        return toml_inline_table(s, name, nlen);
    case '"':
    case '\'': {
        char   text[CF_MESSAGE_MAX];
        size_t n = toml_string(s, text, sizeof text);
        if (n == (size_t)-1) {
            return CF_FALSE;
        }
        cf_value_text(&v, text, n);
        return s->out->scalar(s->out->ctx, name, nlen, &v, CF_FALSE, &at)
                   ? CF_TRUE
                   : toml_fail(s, start, "not a valid key");
    }
    case 't':
        if (s->end - s->p < 4 || memcmp(s->p, "true", 4) != 0) {
            return toml_fail(s, start, "expected a value");
        }
        s->p += 4;
        cf_value_bool(&v, CF_TRUE);
        break;
    case 'f':
        if (s->end - s->p < 5 || memcmp(s->p, "false", 5) != 0) {
            return toml_fail(s, start, "expected a value");
        }
        s->p += 5;
        cf_value_bool(&v, CF_FALSE);
        break;
    default:
        if (!toml_number(s, &v)) {
            return CF_FALSE;
        }
        break;
    }
    return s->out->scalar(s->out->ctx, name, nlen, &v, CF_FALSE, &at)
               ? CF_TRUE
               : toml_fail(s, start, "not a valid key");
}

/* ------------------------------------------------------------------ *
 * Headers
 *
 * The document is events, so a header is "close what is open down to
 * where this path agrees with it, then open the rest". An array of
 * tables is one segment and an element with no name; a header that
 * extends the current element's path - [[a]] then [a.b] - is nested in
 * that element, which is what TOML means by it.
 * ------------------------------------------------------------------ */

static cf_bool toml_close_to(toml_scan* s, int keep) {
    while (s->nopen > keep) {
        toml_open* o = &s->open[s->nopen - 1];
        if (o->kind == TOML_ARRAY) {
            /* Remembered, so that reopening it later can be refused:
               an event stream cannot reopen an array, and appending
               to a closed one would restart its numbering. */
            memcpy(s->closed, s->names + o->at, o->n);
            s->closed[o->n] = '\0';
        }
        if (!s->out->end(s->out->ctx)) {
            return toml_fail(s, s->p, "the document could not be recorded");
        }
        s->nused = o->at;
        --s->nopen;
    }
    return CF_TRUE;
}

static cf_bool toml_push(toml_scan* s, const char* name, size_t n, toml_kind kind) {
    toml_open* o;
    cf_bool    ok;

    if (s->nopen >= TOML_MAX_DEPTH) {
        return toml_fail(s, s->p, "tables are nested too deeply");
    }
    o     = &s->open[s->nopen];
    o->at = (unsigned short)s->nused;
    o->n  = 0;
    if (kind == TOML_ELEMENT) {
        ok = s->out->begin_table(s->out->ctx, NULL, 0);
    } else {
        if (s->nused + n + 1 > sizeof s->names) {
            return toml_fail(s, s->p, "the key is too long");
        }
        ok = (kind == TOML_ARRAY) ? s->out->begin_array(s->out->ctx, name, n)
                                  : s->out->begin_table(s->out->ctx, name, n);
        memcpy(s->names + s->nused, name, n);
        s->names[s->nused + n] = '\0';
        o->n                   = (unsigned short)n;
        s->nused += n + 1;
    }
    if (!ok) {
        return toml_fail(s, s->p, "not a valid key");
    }
    o->kind = kind;
    ++s->nopen;
    return CF_TRUE;
}

static cf_bool toml_header(toml_scan* s) {
    toml_dkey   k;
    cf_bool     is_array = CF_FALSE;
    const char* at       = s->p;
    int         n, i, o, keep;

    ++s->p; /* '[' */
    if (s->p < s->end && *s->p == '[') {
        is_array = CF_TRUE;
        ++s->p;
    }
    if (!toml_key(s, &k)) {
        return CF_FALSE;
    }
    n = k.n;
    if (s->p >= s->end || *s->p != ']' ||
        (is_array && (s->end - s->p < 2 || s->p[1] != ']'))) {
        return toml_fail(s, s->p, is_array ? "expected ']]'" : "expected ']'");
    }
    s->p += is_array ? 2 : 1;

    /* How much of the open path this header agrees with. */
    keep = 0;
    i    = 0;
    for (o = 0; o < s->nopen; ++o) {
        if (s->open[o].kind == TOML_ELEMENT) {
            keep = o + 1;
            continue;
        }
        if (i >= n || k.len[i] != (size_t)s->open[o].n ||
            memcmp(TOML_SEG(&k, i), s->names + s->open[o].at, k.len[i]) != 0) {
            break;
        }
        /* The same array of tables again: a new element in it. */
        if (is_array && i == n - 1 && s->open[o].kind == TOML_ARRAY) {
            if (!toml_close_to(s, o + 1) || !toml_push(s, NULL, 0, TOML_ELEMENT)) {
                return CF_FALSE;
            }
            return toml_end_of_line(s);
        }
        ++i;
        keep = o + 1;
    }
    if (!toml_close_to(s, keep)) {
        return CF_FALSE;
    }
    if (is_array && strlen(s->closed) == k.len[n - 1] &&
        memcmp(s->closed, TOML_SEG(&k, n - 1), k.len[n - 1]) == 0) {
        return toml_fail(s, at,
                         "an array of tables reopened after another table; keep its "
                         "[[blocks]] together");
    }
    for (; i < n; ++i) {
        toml_kind kind = (is_array && i == n - 1) ? TOML_ARRAY : TOML_TABLE;
        if (!toml_push(s, TOML_SEG(&k, i), k.len[i], kind)) {
            return CF_FALSE;
        }
    }
    if (is_array && !toml_push(s, NULL, 0, TOML_ELEMENT)) {
        return CF_FALSE;
    }
    return toml_end_of_line(s);
}

static cf_bool toml_decode(const cf_blob* in, const cf_doc_sink* out, cf_error* err) {
    toml_scan s;

    s.p          = in->p;
    s.end        = in->p + in->n;
    s.file       = (in->name != NULL) ? in->name : "<toml>";
    s.line_start = in->p;
    s.line       = 1;
    s.out        = out;
    s.err        = err;
    s.depth      = 0;
    s.nopen      = 0;
    s.nused      = 0;
    s.closed[0]  = '\0';

    if (!out->begin_table(out->ctx, NULL, 0)) {
        return cf_error_set(err, CF_E_SOURCE, NULL, s.file, "sink refused");
    }
    for (;;) {
        toml_space_lines(&s);
        if (s.p >= s.end) {
            break;
        }
        if (*s.p == '[') {
            if (!toml_header(&s)) {
                return CF_FALSE;
            }
        } else if (!toml_keyval(&s) || !toml_end_of_line(&s)) {
            return CF_FALSE;
        }
    }
    if (!toml_close_to(&s, 0)) {
        return CF_FALSE;
    }
    out->end(out->ctx);
    return CF_TRUE;
}

/* ================================================================== *
 * Encoding
 *
 * A [header] for each top-level table, and everything inside it as a
 * dotted key relative to that header: [server] then limits.ports = [..].
 * Arrays and their elements are written inline.
 *
 * Why not a header per table: the emitter hands over a table's leaves
 * and then its containers in name order, so an array can arrive after a
 * sub-table's header has been written - and TOML forbids writing the
 * parent's header again. A top-level table is visited exactly once, so
 * headers at that level never repeat, and dotted keys below them are
 * valid in any order. The result is valid TOML for every reader, not
 * only for this one.
 * ================================================================== */

typedef struct toml_enc {
    cf_sink* out;
    char     path[CF_KEY_MAX];
    size_t   plen;
    size_t   hlen; /* how much of path the current [header] covers */
    size_t   stack[TOML_MAX_DEPTH + 2];
    int      depth; /* tables open outside any inline value */
    int      inl;   /* inline levels open: arrays and inline tables */
    cf_bool  first[TOML_MAX_DEPTH + 2];
    cf_bool  is_array[TOML_MAX_DEPTH + 2];
    cf_bool  header_pending;
} toml_enc;

static void toml_quote(cf_sink* out, const char* p, size_t n) {
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

/* What comes before a value. Inside an array: a separator only, since
   position is the name. Inside an inline table: a separator and the
   name. Outside both: the pending header, then the key relative to it. */
static void toml_enc_lead(toml_enc* e, const char* name, size_t n) {
    if (e->inl > 0) {
        if (!e->first[e->inl]) {
            cf_sink_puts(e->out, ", ");
        }
        e->first[e->inl] = CF_FALSE;
        if (!e->is_array[e->inl] && name != NULL) {
            cf_sink_write(e->out, name, n);
            cf_sink_puts(e->out, " = ");
        }
        return;
    }
    if (e->header_pending) {
        cf_sink_puts(e->out, "\n[");
        cf_sink_write(e->out, e->path, e->hlen);
        cf_sink_puts(e->out, "]\n");
        e->header_pending = CF_FALSE;
    }
    if (e->plen > e->hlen) {
        size_t from = (e->hlen > 0) ? e->hlen + 1 : 0;
        cf_sink_write(e->out, e->path + from, e->plen - from);
        cf_sink_putc(e->out, '.');
    }
    if (name != NULL) {
        cf_sink_write(e->out, name, n);
    }
    cf_sink_puts(e->out, " = ");
}

static cf_bool toml_enc_open_inline(toml_enc* e, const char* name, size_t n,
                                    cf_bool array) {
    if (e->inl >= TOML_MAX_DEPTH) {
        return CF_FALSE;
    }
    toml_enc_lead(e, name, n);
    cf_sink_puts(e->out, array ? "[" : "{ ");
    ++e->inl;
    e->first[e->inl]    = CF_TRUE;
    e->is_array[e->inl] = array;
    return CF_TRUE;
}

static cf_bool toml_enc_table(void* ctx, const char* name, size_t n) {
    toml_enc* e = (toml_enc*)ctx;

    if (e->inl > 0) {
        return toml_enc_open_inline(e, name, n, CF_FALSE);
    }
    if (e->depth >= TOML_MAX_DEPTH) {
        return CF_FALSE;
    }
    e->stack[e->depth++] = e->plen;
    if (name == NULL) {
        return CF_TRUE; /* the document itself */
    }
    if (e->plen + n + 2 > sizeof e->path) {
        return CF_FALSE;
    }
    if (e->plen > 0) {
        e->path[e->plen++] = '.';
    }
    memcpy(e->path + e->plen, name, n);
    e->plen += n;
    if (e->depth == 2) {
        /* A top-level table: its own header, written at its first leaf
           so that a table holding only tables gets none. */
        e->hlen           = e->plen;
        e->header_pending = CF_TRUE;
    }
    return CF_TRUE;
}

static cf_bool toml_enc_array(void* ctx, const char* name, size_t n) {
    return toml_enc_open_inline((toml_enc*)ctx, name, n, CF_TRUE);
}

static cf_bool toml_enc_scalar(void* ctx, const char* name, size_t n, const cf_value* v,
                               cf_bool textual, const cf_loc* at) {
    toml_enc* e = (toml_enc*)ctx;
    char      tmp[CF_VALUE_TEXT_MAX + 2];
    size_t    k;

    (void)textual;
    (void)at;
    if (v->type == CF_TYPE_NULL) {
        /* TOML has no null; an absent key is how it says nothing. */
        return CF_FALSE;
    }
    toml_enc_lead(e, name, n);
    switch (v->type) {
    case CF_TYPE_TEXT:
        toml_quote(e->out, v->u.t.p, v->u.t.n);
        break;
    case CF_TYPE_BOOL:
        cf_sink_puts(e->out, v->u.b ? "true" : "false");
        break;
    case CF_TYPE_INT:
        cf_sink_write(e->out, tmp, cf_value_to_text(v, tmp, sizeof tmp));
        break;
    case CF_TYPE_REAL:
        /* A real must look like one, or it reads back as an integer. */
        k = cf_value_to_text(v, tmp, sizeof tmp - 2);
        cf_sink_write(e->out, tmp, k);
        if (memchr(tmp, '.', k) == NULL && memchr(tmp, 'e', k) == NULL &&
            memchr(tmp, 'E', k) == NULL) {
            cf_sink_puts(e->out, ".0");
        }
        break;
    case CF_TYPE_NULL:
        break;
    }
    if (e->inl == 0) {
        cf_sink_putc(e->out, '\n');
    }
    return CF_TRUE;
}

static cf_bool toml_enc_end(void* ctx) {
    toml_enc* e = (toml_enc*)ctx;

    if (e->inl > 0) {
        cf_sink_puts(e->out, e->is_array[e->inl] ? "]" : " }");
        --e->inl;
        if (e->inl == 0) {
            cf_sink_putc(e->out, '\n');
        }
        return CF_TRUE;
    }
    if (e->depth <= 0) {
        return CF_FALSE;
    }
    e->plen = e->stack[--e->depth];
    if (e->depth <= 1) {
        e->hlen           = 0; /* back at the top: no header covers it */
        e->header_pending = CF_FALSE;
    }
    return CF_TRUE;
}

static cf_bool toml_encoder(cf_sink* out, void* state, cf_doc_sink* sink) {
    toml_enc* e = (toml_enc*)state;

    if (e == NULL || sink == NULL) {
        return CF_FALSE;
    }
    memset(e, 0, sizeof *e);
    e->out            = out;
    sink->begin_table = toml_enc_table;
    sink->begin_array = toml_enc_array;
    sink->scalar      = toml_enc_scalar;
    sink->end         = toml_enc_end;
    sink->ctx         = e;
    return CF_TRUE;
}

const cf_codec cf_codec_toml = {"toml",
                                ".toml",
                                CF_CODEC_READ | CF_CODEC_WRITE | CF_CODEC_TYPED |
                                    CF_CODEC_ARRAYS,
                                toml_decode,
                                toml_encoder,
                                sizeof(toml_enc)};

#endif /* CF_WITH_TOML */
