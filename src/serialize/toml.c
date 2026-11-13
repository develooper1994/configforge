/*
 * toml.c - the built-in TOML 1.1.0 reader and writer. C89.
 *
 * Written here rather than vendored for the reason the INI codec is:
 * streaming, allocation-free, and every value with its place (D-62 has
 * why no TOML library was adopted). Begun as D-23's subset, it now reads
 * all of TOML 1.1.0 (and so 1.0.0), which toml-test holds it to:
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
 *     text = """..."""         multi-line strings, basic and '''literal'''
 *     when = 1979-05-27T07:32:00Z   dates and times, as their text
 *     big = inf                inf and nan, as their text
 *
 * The library has no date type and holds no value that is not finite,
 * so a date, a time, inf and nan are their text as written, for a
 * definition to type. Refused, with a message that says what to write
 * instead: an array of tables reopened after another table came between,
 * which the event stream cannot express.
 *
 * What TOML forbids defining twice is refused, as the specification
 * says: a key set twice, a table with two [headers], an inline table or
 * a value added to afterwards, a dotted key adding to a table a [header]
 * defined. The record of what is defined lives in the sink's workspace
 * (reserve_work); a sink without one reads as 1.1 did, the later of two
 * keys winning. docs/Conformance.md has the score against toml-test.
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

/* Split after the backslash: the Portable C Compiler reads a "\\" followed
   by u as the start of a universal character name, and stops. */
#define TOML_UNICODE_DIGITS                                                            \
    "\\"                                                                               \
    "x needs two hex digits, \\"                                                       \
    "u four and \\"                                                                    \
    "U eight"

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

/* What the document has defined, for TOML's rule that a key or a table is
   defined once (toml.io/en/v1.0.0, "Keys", "Table", "Inline Table",
   "Array of Tables"). A tree of names: a node is a table, a value, or an
   element of an array, found by its parent and its name through a hash.
   It lives in the sink's workspace (cf_doc_sink.reserve_work), so the
   reader still owns no memory and a fixed arena bounds it. */
typedef enum toml_def {
    TOML_IMPLICIT, /* named on the way to a header: [a.b] makes a     */
    TOML_HEADER,   /* defined by [header], or an element of [[array]] */
    TOML_DOTTED,   /* made by a dotted key: a.b = 1 makes a           */
    TOML_INLINE,   /* an inline table, while it is being read         */
    TOML_SEALED,   /* an inline table, read: nothing may add to it    */
    TOML_VALUE,    /* a value, a value array among them               */
    TOML_TABLES    /* an array of tables                              */
} toml_def;

typedef struct toml_node {
    cf_u32 parent;
    cf_u32 chain; /* the next node in the same bucket; 0 ends it */
    cf_u32 name;  /* the name's offset in the pool, or the element's number */
    cf_u32 count; /* elements so far, for an array */
    cf_u16 len;
    cf_u8  kind; /* a toml_def */
    cf_u8  elem; /* an element of an array, named by its number */
} toml_node;

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
    cf_bool            closed_set;         /* and whether there was one */

    /* The definitions, when the sink lends workspace; node 0 is the root,
       and a bucket or a chain of 0 is empty, since the root is no one's
       child. `body` is the table a key = value line defines into. */
    cf_bool    track;
    toml_node* nodes;
    size_t     nnodes, cnodes;
    cf_u32*    buckets;
    size_t     nbuckets;
    char*      pool;
    size_t     npool, cpool;
    cf_u32     body;
    cf_u32     seed; /* what every hash of a name starts from */
} toml_scan;

/* A dotted key, its segments in one buffer for the same reason. A key
   may have as many segments as a document has levels, and no more: the
   encoder writes the whole path of a table in one header, so a smaller
   limit here is a document that is written and cannot be read back. */
#define TOML_KEY_PARTS TOML_MAX_DEPTH

typedef struct toml_dkey {
    char   buf[CF_KEY_MAX];
    cf_u16 at[TOML_KEY_PARTS];
    cf_u16 len[TOML_KEY_PARTS];
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

/* Space within a line. A carriage return is space only as the first half
   of a CRLF line ending: TOML has no bare CR. */
static void toml_blank(toml_scan* s) {
    while (s->p < s->end &&
           (*s->p == ' ' || *s->p == '\t' ||
            (*s->p == '\r' && s->end - s->p >= 2 && s->p[1] == '\n'))) {
        ++s->p;
    }
}

/* The length of the well-formed UTF-8 sequence at p (RFC 3629: no
   overlong form, no surrogate, nothing above U+10FFFF), or 0. */
static size_t toml_utf8_len(const char* p, const char* end) {
    const unsigned char* u = (const unsigned char*)p;
    size_t               n, i;
    cf_u32               cp;

    if (u[0] < 0x80u) {
        return 1;
    }
    if (u[0] >= 0xC2u && u[0] <= 0xDFu) {
        n  = 2;
        cp = u[0] & 0x1Fu;
    } else if (u[0] >= 0xE0u && u[0] <= 0xEFu) {
        n  = 3;
        cp = u[0] & 0x0Fu;
    } else if (u[0] >= 0xF0u && u[0] <= 0xF4u) {
        n  = 4;
        cp = u[0] & 0x07u;
    } else {
        return 0;
    }
    if ((size_t)(end - p) < n) {
        return 0;
    }
    for (i = 1; i < n; ++i) {
        if ((u[i] & 0xC0u) != 0x80u) {
            return 0;
        }
        cp = (cp << 6) | (u[i] & 0x3Fu);
    }
    if ((n == 3 && cp < 0x800u) || (n == 4 && (cp < 0x10000u || cp > 0x10FFFFu)) ||
        (cp >= 0xD800u && cp <= 0xDFFFu)) {
        return 0;
    }
    return n;
}

/* A comment, to the end of its line: any character but the controls
   TOML excludes (all below U+0020 but tab, and DEL), in UTF-8. */
static cf_bool toml_comment(toml_scan* s) {
    while (s->p < s->end && *s->p != '\n') {
        unsigned char c = (unsigned char)*s->p;
        size_t        n;
        if (c == '\r' && s->end - s->p >= 2 && s->p[1] == '\n') {
            ++s->p;
            continue;
        }
        if ((c < 0x20u && c != '\t') || c == 0x7Fu) {
            return toml_fail(s, s->p, "a comment may not contain a control character");
        }
        n = toml_utf8_len(s->p, s->end);
        if (n == 0) {
            return toml_fail(s, s->p, "a comment is not valid UTF-8");
        }
        s->p += n;
    }
    return CF_TRUE;
}

/* Space, newlines and comments: what may sit between array elements. */
static cf_bool toml_space_lines(toml_scan* s) {
    for (;;) {
        toml_blank(s);
        if (s->p < s->end && *s->p == '#' && !toml_comment(s)) {
            return CF_FALSE;
        }
        if (s->p < s->end && *s->p == '\n') {
            ++s->p;
            ++s->line;
            s->line_start = s->p;
            continue;
        }
        return CF_TRUE;
    }
}

/* After a value or a header: only a comment may follow on the line. */
static cf_bool toml_end_of_line(toml_scan* s) {
    toml_blank(s);
    if (s->p < s->end && *s->p == '#' && !toml_comment(s)) {
        return CF_FALSE;
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

/* Append k bytes, keeping room for the NUL; false, with the error set,
   when they do not fit. A NULL destination only counts. */
static cf_bool toml_put(toml_scan* s, char* dst, size_t* n, size_t cap, const char* p,
                        size_t k) {
    if (*n + k + 1 > cap) {
        return toml_fail(s, s->p, "the string is too long");
    }
    if (dst != NULL) {
        memcpy(dst + *n, p, k);
    }
    *n += k;
    return CF_TRUE;
}

/* A line break inside a multi-line string, LF or CRLF: counted as a
   line, so what follows has its own line and column. */
static size_t toml_newline(toml_scan* s) {
    size_t k = (*s->p == '\n')                                            ? 1u
               : (*s->p == '\r' && s->end - s->p >= 2 && s->p[1] == '\n') ? 2u
                                                                          : 0u;
    if (k > 0) {
        s->p += k;
        ++s->line;
        s->line_start = s->p;
    }
    return k;
}

/* Decode, or measure with a NULL destination, using the same grammar.
   `multi` allows the multi-line forms, """ and ''', which a key may not
   be: their opening line break is dropped, line breaks are LF, and in
   the basic form a backslash ending a line drops it and the space and
   line breaks after it. */
static size_t toml_string(toml_scan* s, char* dst, size_t cap, cf_bool multi) {
    char    q  = *s->p;
    size_t  n  = 0;
    cf_bool ml = CF_FALSE;

    if (s->end - s->p >= 3 && s->p[1] == q && s->p[2] == q) {
        if (!multi) {
            toml_fail(s, s->p, "a key may not be a multi-line string");
            return (size_t)-1;
        }
        ml = CF_TRUE;
        s->p += 3;
        (void)toml_newline(s);
    } else {
        ++s->p;
    }
    while (s->p < s->end) {
        unsigned char c = (unsigned char)*s->p;
        if (c == (unsigned char)q) {
            size_t run = 1;
            if (ml) {
                while (s->p + run < s->end && s->p[run] == q) {
                    ++run;
                }
                if (run < 3) {
                    if (!toml_put(s, dst, &n, cap, s->p, run)) {
                        return (size_t)-1;
                    }
                    s->p += run;
                    continue;
                }
                if (run > 5) {
                    toml_fail(s, s->p + 5,
                              "more than two quotes before the closing three");
                    return (size_t)-1;
                }
                /* """"" is two quotes in the string, then its end. */
                if (!toml_put(s, dst, &n, cap, s->p, run - 3)) {
                    return (size_t)-1;
                }
            }
            s->p += run;
            if (dst != NULL) {
                dst[n] = '\0';
            }
            return n;
        }
        if (ml && (c == '\n' || c == '\r')) {
            if (toml_newline(s) == 0) {
                toml_fail(s, s->p, "a string may not contain a control character");
                return (size_t)-1;
            }
            if (!toml_put(s, dst, &n, cap, "\n", 1)) {
                return (size_t)-1;
            }
            continue;
        }
        if (ml && c == '\\' && q == '"') {
            /* A backslash, then only space to the end of the line. */
            const char* t = s->p + 1;
            while (t < s->end && (*t == ' ' || *t == '\t')) {
                ++t;
            }
            if (t < s->end &&
                (*t == '\n' || (*t == '\r' && s->end - t >= 2 && t[1] == '\n'))) {
                s->p = t;
                for (;;) {
                    if (toml_newline(s) > 0) {
                        continue;
                    }
                    if (s->p < s->end && (*s->p == ' ' || *s->p == '\t')) {
                        ++s->p;
                        continue;
                    }
                    break;
                }
                continue;
            }
        }
        if (c == '\n') {
            toml_fail(s, s->p, "the string is not closed on its line");
            return (size_t)-1;
        }
        if ((c < 0x20u && c != '\t') || c == 0x7Fu) {
            toml_fail(s, s->p, "a string may not contain a control character");
            return (size_t)-1;
        }
        if (c >= 0x80u) {
            size_t k = toml_utf8_len(s->p, s->end);
            if (k == 0) {
                toml_fail(s, s->p, "a string is not valid UTF-8");
                return (size_t)-1;
            }
            if (n + k + 1 > cap) {
                toml_fail(s, s->p, "the string is too long");
                return (size_t)-1;
            }
            if (dst != NULL) {
                memcpy(dst + n, s->p, k);
            }
            n += k;
            s->p += k;
            continue;
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
            case 'e': /* TOML 1.1 */
                one = 27;
                break;
            case 'x': /* TOML 1.1: two hexadecimal digits, U+0000 to U+00FF */
                digits = 2;
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
                    toml_fail(s, s->p, TOML_UNICODE_DIGITS);
                    return (size_t)-1;
                }
                for (i = 0; i < digits; ++i) {
                    int h = toml_hex((unsigned char)s->p[i]);
                    if (h < 0) {
                        toml_fail(s, s->p, TOML_UNICODE_DIGITS);
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
                if (dst != NULL) {
                    memcpy(dst + n, enc, k);
                }
                n += k;
            } else {
                if (n + 2 > cap) {
                    toml_fail(s, s->p, "the string is too long");
                    return (size_t)-1;
                }
                if (dst != NULL) {
                    dst[n] = one;
                }
                ++n;
            }
            continue;
        }
        if (n + 2 > cap) {
            toml_fail(s, s->p, "the string is too long");
            return (size_t)-1;
        }
        if (dst != NULL) {
            dst[n] = (char)c;
        }
        ++n;
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
        return toml_string(s, dst, cap, CF_FALSE);
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
        k->at[k->n]  = (cf_u16)used;
        k->len[k->n] = (cf_u16)n;
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

/* ------------------------------------------------------------------ *
 * Definitions
 *
 * What TOML forbids defining twice, checked as the document is read: a
 * key set twice, a table given two [headers], a [header] or a dotted key
 * adding to an inline table or to a value, a dotted key adding to a
 * table a [header] defined, and [[name]] on what is not an array of
 * tables. Each costs a hash lookup, so a document of n keys costs O(n).
 * ------------------------------------------------------------------ */

static cf_bool toml_fail_named(toml_scan* s, const char* at, const char* name, size_t n,
                               const char* what) {
    /* cf_fmt has no precision for %s: the name is copied, cut short
       where the message would not hold it. */
    char   text[CF_MESSAGE_MAX];
    size_t w = strlen(what), k = 0;

    if (n + w + 4 > sizeof text) {
        n = (w + 4 < sizeof text) ? sizeof text - w - 4 : 0;
    }
    text[k++] = '\'';
    memcpy(text + k, name, n);
    k += n;
    text[k++] = '\'';
    text[k++] = ' ';
    memcpy(text + k, what, w < sizeof text - k - 1 ? w : sizeof text - k - 1);
    k += w < sizeof text - k - 1 ? w : sizeof text - k - 1;
    text[k] = '\0';
    return toml_fail(s, at, text);
}

static cf_bool toml_no_room(toml_scan* s) {
    return cf_error_set(s->err, CF_E_MEMORY, NULL, s->file,
                        "no room to record the TOML document's definitions");
}

/* The seed every hash of a name in this decode starts from, so that a
   document's author cannot choose names that all land in one bucket: the
   hash was FNV-1a from its fixed offset basis, which anyone can compute
   against. The library has no clock and no random source, so the seed is
   made of two addresses, the scan's and a local's, which differ from run to
   run where the system randomises addresses and which a document cannot
   know. Where it does not (no address randomisation), the seed is the
   same every run: no worse than the unseeded hash was, and no better.
   Nothing observable may depend on it: no table is walked in bucket
   order. The high bits of an address are the random ones and a bucket
   index is the low bits of a hash, so they are folded down first. The
   two addresses are often both on the stack, sharing their random high
   bits, so the second is multiplied before the XOR lest they cancel. */
static size_t toml_seed(const void* where) {
    char   here;
    size_t x = (size_t)where ^ ((size_t)&here * 2654435761u);

    x ^= x >> 12;
    x ^= x >> 24;
    x *= 16777619u;
    x = (size_t)(cf_u32)(x ^ (x >> 16 >> 16));
    return (x == 0) ? (size_t)2166136261u : x;
}

/* FNV-1a from the decode's seed, over the parent, and the name or the
   element's number. */
static cf_u32 toml_hash(cf_u32 seed, cf_u32 parent, cf_u8 elem, const char* p, size_t n,
                        cf_u32 index) {
    cf_u32 h = seed;
    size_t i;
    for (i = 0; i < 4; ++i) {
        h = (h ^ ((parent >> (8 * i)) & 0xFFu)) * 16777619u;
    }
    if (elem) {
        for (i = 0; i < 4; ++i) {
            h = (h ^ ((index >> (8 * i)) & 0xFFu)) * 16777619u;
        }
        return (h ^ 0x5Au) * 16777619u;
    }
    for (i = 0; i < n; ++i) {
        h = (h ^ (cf_u32)(unsigned char)p[i]) * 16777619u;
    }
    return h;
}

/* The child of `parent` named p[0..n), or element `index` of it; 0 when
   there is none. */
static cf_u32 toml_find(const toml_scan* s, cf_u32 parent, cf_u8 elem, const char* p,
                        size_t n, cf_u32 index) {
    cf_u32 i =
        s->buckets[toml_hash(s->seed, parent, elem, p, n, index) & (s->nbuckets - 1)];
    while (i != 0) {
        const toml_node* x = &s->nodes[i];
        if (x->parent == parent && x->elem == elem &&
            (elem ? x->name == index
                  : ((size_t)x->len == n && memcmp(s->pool + x->name, p, n) == 0))) {
            return i;
        }
        i = x->chain;
    }
    return 0;
}

/* Room for one more node: a block twice the size, the nodes copied, and
   a bucket array twice that, rehashed. What was outgrown stays where the
   sink put it until the decode is over. */
static cf_bool toml_room(toml_scan* s, size_t text) {
    if (s->nnodes + 1 > s->cnodes) {
        size_t     c = s->cnodes * 2, nb = c * 2, i;
        toml_node* nodes;
        cf_u32*    buckets;
        if (c > (cf_u32)-1 / 4 || nb > (size_t)-1 / sizeof *buckets) {
            return toml_no_room(s);
        }
        nodes   = (toml_node*)s->out->reserve_work(s->out->ctx, c * sizeof *nodes);
        buckets = (cf_u32*)s->out->reserve_work(s->out->ctx, nb * sizeof *buckets);
        if (nodes == NULL || buckets == NULL) {
            return toml_no_room(s);
        }
        memcpy(nodes, s->nodes, s->nnodes * sizeof *nodes);
        memset(buckets, 0, nb * sizeof *buckets);
        for (i = 1; i < s->nnodes; ++i) {
            toml_node*  x  = &nodes[i];
            const char* nm = x->elem ? NULL : s->pool + x->name;
            cf_u32      h;

            h = toml_hash(s->seed, x->parent, x->elem, nm, x->len, x->name);
            h &= (cf_u32)(nb - 1);

            x->chain   = buckets[h];
            buckets[h] = (cf_u32)i;
        }
        s->nodes    = nodes;
        s->cnodes   = c;
        s->buckets  = buckets;
        s->nbuckets = nb;
    }
    if (s->npool + text > s->cpool) {
        size_t c = s->cpool * 2;
        char*  pool;
        while (c < s->npool + text) {
            c *= 2;
        }
        if (c > (cf_u32)-1) {
            return toml_no_room(s);
        }
        pool = (char*)s->out->reserve_work(s->out->ctx, c);
        if (pool == NULL) {
            return toml_no_room(s);
        }
        memcpy(pool, s->pool, s->npool);
        s->pool  = pool;
        s->cpool = c;
    }
    return CF_TRUE;
}

/* A new child of `parent`; 0, with the error set, when there is no room.
   An element's name is its number, and an element is never looked up by
   text, so its hash takes the number in place of a name. */
static cf_u32 toml_add(toml_scan* s, cf_u32 parent, cf_u8 elem, const char* p, size_t n,
                       cf_u32 index, toml_def kind) {
    toml_node* x;
    cf_u32     h, i;

    if (!toml_room(s, elem ? 0 : n)) {
        return 0;
    }
    i         = (cf_u32)s->nnodes++;
    x         = &s->nodes[i];
    x->parent = parent;
    x->count  = 0;
    x->kind   = (cf_u8)kind;
    x->elem   = elem;
    if (elem) {
        x->name = index;
        x->len  = 0;
    } else {
        memcpy(s->pool + s->npool, p, n);
        x->name = (cf_u32)s->npool;
        x->len  = (cf_u16)n;
        s->npool += n;
    }
    h = toml_hash(s->seed, parent, elem, p, n, index) & (cf_u32)(s->nbuckets - 1);
    x->chain      = s->buckets[h];
    s->buckets[h] = i;
    return i;
}

/* The workspace for the definitions, when the sink lends it: a sink too
   old to have reserve_work, or one that leaves it NULL, reads as 1.1 did,
   the later of two keys winning. */
static cf_bool toml_track_init(toml_scan* s) {
    const cf_doc_sink* o = s->out;

    s->track   = CF_FALSE;
    s->nodes   = NULL;
    s->buckets = NULL;
    s->pool    = NULL;
    s->seed    = (cf_u32)toml_seed(s);
    if (o->size < offsetof(cf_doc_sink, reserve_work) + sizeof o->reserve_work ||
        o->reserve_work == NULL) {
        return CF_TRUE;
    }
    s->cnodes   = 64;
    s->nbuckets = 128;
    s->cpool    = 512;
    s->nodes    = (toml_node*)o->reserve_work(o->ctx, s->cnodes * sizeof *s->nodes);
    s->buckets  = (cf_u32*)o->reserve_work(o->ctx, s->nbuckets * sizeof *s->buckets);
    s->pool     = (char*)o->reserve_work(o->ctx, s->cpool);
    if (s->nodes == NULL || s->buckets == NULL || s->pool == NULL) {
        return toml_no_room(s);
    }
    memset(s->buckets, 0, s->nbuckets * sizeof *s->buckets);
    memset(&s->nodes[0], 0, sizeof s->nodes[0]);
    s->nodes[0].kind = (cf_u8)TOML_HEADER;
    s->nnodes        = 1;
    s->npool         = 0;
    s->track         = CF_TRUE;
    return CF_TRUE;
}

/* A key = value line's key: each leading segment a table the dotted key
   makes or one it made before, and the last one new. Returns the node
   the value defines, or 0 with the error set. */
static cf_u32 toml_define_key(toml_scan* s, const toml_dkey* k, const char* at,
                              toml_def kind) {
    cf_u32 node = s->body, c;
    int    i;

    for (i = 0; i + 1 < k->n; ++i) {
        c = toml_find(s, node, 0, TOML_SEG(k, i), k->len[i], 0);
        if (c == 0) {
            c = toml_add(s, node, 0, TOML_SEG(k, i), k->len[i], 0, TOML_DOTTED);
            if (c == 0) {
                return 0;
            }
        } else if (s->nodes[c].kind != (cf_u8)TOML_DOTTED) {
            const char* why;
            switch ((toml_def)s->nodes[c].kind) {
            case TOML_VALUE:
                why = "is a value, not a table";
                break;
            case TOML_SEALED:
            case TOML_INLINE:
                why = "is an inline table, which is complete as written";
                break;
            case TOML_TABLES:
                why = "is an array of tables; a dotted key may not add to it";
                break;
            case TOML_IMPLICIT:
            case TOML_HEADER:
            case TOML_DOTTED:
            default:
                why = "is a table a [header] defines; a dotted key may not add to it";
                break;
            }
            toml_fail_named(s, at, TOML_SEG(k, i), k->len[i], why);
            return 0;
        }
        node = c;
    }
    if (toml_find(s, node, 0, TOML_SEG(k, i), k->len[i], 0) != 0) {
        toml_fail_named(s, at, TOML_SEG(k, i), k->len[i],
                        "is already defined; TOML defines a key once");
        return 0;
    }
    return toml_add(s, node, 0, TOML_SEG(k, i), k->len[i], 0, kind);
}

/* A [header] or [[header]]: each leading segment a table, or the last
   element of an array of tables, and the last one a table defined here
   for the first time, or an array of tables given a new element. Sets
   the body the lines after it define into. */
static cf_bool toml_define_header(toml_scan* s, const toml_dkey* k, const char* at,
                                  cf_bool is_array, cf_bool* existed) {
    cf_u32 node = 0, c;
    int    i;

    *existed = CF_FALSE;
    for (i = 0; i + 1 < k->n; ++i) {
        c = toml_find(s, node, 0, TOML_SEG(k, i), k->len[i], 0);
        if (c == 0) {
            c = toml_add(s, node, 0, TOML_SEG(k, i), k->len[i], 0, TOML_IMPLICIT);
            if (c == 0) {
                return CF_FALSE;
            }
        } else {
            switch ((toml_def)s->nodes[c].kind) {
            case TOML_IMPLICIT:
            case TOML_HEADER:
            case TOML_DOTTED:
                break;
            case TOML_TABLES:
                c = toml_find(s, c, 1, NULL, 0, s->nodes[c].count - 1u);
                break;
            case TOML_VALUE:
                return toml_fail_named(s, at, TOML_SEG(k, i), k->len[i],
                                       "is a value, not a table");
            case TOML_INLINE:
            case TOML_SEALED:
            default:
                return toml_fail_named(s, at, TOML_SEG(k, i), k->len[i],
                                       "is an inline table, which is complete as "
                                       "written");
            }
        }
        node = c;
    }
    c = toml_find(s, node, 0, TOML_SEG(k, i), k->len[i], 0);
    if (is_array) {
        if (c == 0) {
            c = toml_add(s, node, 0, TOML_SEG(k, i), k->len[i], 0, TOML_TABLES);
            if (c == 0) {
                return CF_FALSE;
            }
        } else if (s->nodes[c].kind != (cf_u8)TOML_TABLES) {
            return toml_fail_named(s, at, TOML_SEG(k, i), k->len[i],
                                   "is already defined, and not as an array of tables");
        } else {
            *existed = CF_TRUE;
        }
        node = toml_add(s, c, 1, NULL, 0, s->nodes[c].count, TOML_HEADER);
        if (node == 0) {
            return CF_FALSE;
        }
        ++s->nodes[c].count;
    } else if (c == 0) {
        node = toml_add(s, node, 0, TOML_SEG(k, i), k->len[i], 0, TOML_HEADER);
        if (node == 0) {
            return CF_FALSE;
        }
    } else if (s->nodes[c].kind == (cf_u8)TOML_IMPLICIT) {
        s->nodes[c].kind = (cf_u8)TOML_HEADER;
        node             = c;
    } else {
        const char* why;
        switch ((toml_def)s->nodes[c].kind) {
        case TOML_HEADER:
            why = "is a table defined twice; TOML defines a table once";
            break;
        case TOML_DOTTED:
            why = "was defined by dotted keys; a [header] may not define it again";
            break;
        case TOML_TABLES:
            why = "is an array of tables; [[...]] adds an element to it";
            break;
        case TOML_VALUE:
            why = "is a value, not a table";
            break;
        case TOML_IMPLICIT:
        case TOML_INLINE:
        case TOML_SEALED:
        default:
            why = "is an inline table, which is complete as written";
            break;
        }
        return toml_fail_named(s, at, TOML_SEG(k, i), k->len[i], why);
    }
    s->body = node;
    return CF_TRUE;
}

/* One run of digits in `base`, as the TOML 1.0 ABNF writes it: a digit,
   then digits each optionally after one underscore - so an underscore
   only ever stands between two digits. Appended to buf without the
   underscores; false when there is no digit, or an underscore is not
   between two. */
static cf_bool toml_digits(toml_scan* s, int base, char* buf, size_t cap, size_t* n) {
    cf_bool any = CF_FALSE;

    for (;;) {
        int d = (s->p < s->end) ? toml_hex((unsigned char)*s->p) : -1;
        if (d >= 0 && d < base) {
            if (*n + 1 >= cap) {
                return CF_FALSE;
            }
            buf[(*n)++] = *s->p++;
            any         = CF_TRUE;
            continue;
        }
        if (any && s->p + 1 < s->end && *s->p == '_') {
            int next = toml_hex((unsigned char)s->p[1]);
            if (next >= 0 && next < base) {
                ++s->p;
                continue;
            }
        }
        return any;
    }
}

/* Integers and floats, by the TOML 1.0 grammar:
       dec-int   = [ "+" / "-" ] ( "0" / digit1-9 *( DIGIT / "_" DIGIT ) )
       hex-int   = "0x" HEXDIG *( HEXDIG / "_" HEXDIG )   (and 0o, 0b; unsigned)
       float     = dec-int ( exp / frac [ exp ] )
       frac      = "." zero-prefixable-int
       exp       = ( "e" / "E" ) [ "+" / "-" ] zero-prefixable-int
   The toml-test suite found 01, 1., .5, 1__0 and +0x1 accepted. Dates
   are recognised in order to refuse them. */
/* n digits at p as a number, or -1 when any is not a digit. */
static int toml_fixed(const char* p, const char* end, int n) {
    int v = 0, i;
    if (end - p < n) {
        return -1;
    }
    for (i = 0; i < n; ++i) {
        if (!cf_is_digit((unsigned char)p[i])) {
            return -1;
        }
        v = v * 10 + (p[i] - '0');
    }
    return v;
}

/* HH:MM[:SS[.fraction]] at s->p, as TOML 1.1 writes a time: seconds
   are optional, a fraction only after them, and 60 is a leap second. */
static cf_bool toml_time(toml_scan* s) {
    int h = toml_fixed(s->p, s->end, 2), m, sec;
    if (h < 0 || s->end - s->p < 5 || s->p[2] != ':') {
        return CF_FALSE;
    }
    m = toml_fixed(s->p + 3, s->end, 2);
    if (h > 23 || m < 0 || m > 59) {
        return CF_FALSE;
    }
    if (s->end - s->p < 6 || s->p[5] != ':') {
        s->p += 5;
        return CF_TRUE; /* no seconds */
    }
    sec = (s->end - s->p >= 8) ? toml_fixed(s->p + 6, s->end, 2) : -1;
    if (sec < 0 || sec > 60) {
        return CF_FALSE;
    }
    s->p += 8;
    if (s->p < s->end && *s->p == '.') {
        ++s->p;
        if (s->p >= s->end || !cf_is_digit((unsigned char)*s->p)) {
            return CF_FALSE;
        }
        while (s->p < s->end && cf_is_digit((unsigned char)*s->p)) {
            ++s->p;
        }
    }
    return CF_TRUE;
}

/* A date, a time, or both, with or without an offset: TOML's four kinds
   of date-time (RFC 3339, the date checked against its month, leap years
   included). The library has no date type, so the value is its text, as
   written, for a definition to type: a key defined as text takes it as
   it is, and nothing reads it as a number. Called only where the text
   begins as a date or a time does. */
static cf_bool toml_datetime(toml_scan* s, cf_value* v) {
    static const int days[12] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const char*      start    = s->p;

    if (toml_fixed(s->p, s->end, 4) >= 0 && s->end - s->p >= 10 && s->p[4] == '-' &&
        s->p[7] == '-') {
        int y = toml_fixed(s->p, s->end, 4), mo = toml_fixed(s->p + 5, s->end, 2),
            d    = toml_fixed(s->p + 8, s->end, 2);
        int leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
        if (mo < 1 || mo > 12 || d < 1 || d > days[mo - 1] ||
            (mo == 2 && d == 29 && !leap)) {
            return toml_fail(s, start,
                             "not a date: the month or the day is out of range");
        }
        s->p += 10;
        /* A space is the delimiter only when a time follows it. */
        if (s->p < s->end && (*s->p == 'T' || *s->p == 't' ||
                              (*s->p == ' ' && toml_fixed(s->p + 1, s->end, 2) >= 0 &&
                               s->end - s->p > 3 && s->p[3] == ':'))) {
            ++s->p;
            if (!toml_time(s)) {
                return toml_fail(s, start, "not a date-time: the time is malformed");
            }
            if (s->p < s->end && (*s->p == 'Z' || *s->p == 'z')) {
                ++s->p;
            } else if (s->p < s->end && (*s->p == '+' || *s->p == '-')) {
                int oh = toml_fixed(s->p + 1, s->end, 2), om;
                if (oh < 0 || s->end - s->p < 6 || s->p[3] != ':') {
                    return toml_fail(s, start,
                                     "not a date-time: the offset is malformed");
                }
                om = toml_fixed(s->p + 4, s->end, 2);
                if (oh > 23 || om < 0 || om > 59) {
                    return toml_fail(s, start,
                                     "not a date-time: the offset is out of range");
                }
                s->p += 6;
            }
        }
    } else if (!toml_time(s)) {
        return toml_fail(s, start, "not a time: HH:MM or HH:MM:SS");
    }
    if (s->p < s->end && (cf_is_alnum((unsigned char)*s->p) || *s->p == '.' ||
                          *s->p == ':' || *s->p == '-' || *s->p == '+')) {
        return toml_fail(s, start, "malformed date or time");
    }
    cf_value_text(v, start, (size_t)(s->p - start));
    return CF_TRUE;
}

static cf_bool toml_number(toml_scan* s, cf_value* v, cf_bool* textual) {
    const char* start = s->p;
    char        buf[CF_VALUE_TEXT_MAX + 8]; /* the digits of either kind */
    size_t      n       = 0;
    cf_bool     is_real = CF_FALSE;
    int         base    = 10;

    *textual = CF_FALSE;
    /* Four digits and '-', or two and ':', begin a date or a time. */
    if ((toml_fixed(s->p, s->end, 4) >= 0 && s->end - s->p > 4 && s->p[4] == '-') ||
        (toml_fixed(s->p, s->end, 2) >= 0 && s->end - s->p > 2 && s->p[2] == ':')) {
        *textual = CF_TRUE;
        return toml_datetime(s, v);
    }
    if (s->p < s->end && (*s->p == '+' || *s->p == '-')) {
        buf[n++] = *s->p++;
    }
    /* TOML's infinities and not-a-number. The library holds no value that
       is not finite (a range check means nothing against NaN), so the
       value is its text, as written: a key defined as real refuses it by
       name, a key defined as text takes it. */
    if (s->end - s->p >= 3 &&
        (memcmp(s->p, "inf", 3) == 0 || memcmp(s->p, "nan", 3) == 0)) {
        s->p += 3;
        if (s->p < s->end &&
            (cf_is_alnum((unsigned char)*s->p) || *s->p == '_' || *s->p == '.')) {
            return toml_fail(s, start, "malformed number");
        }
        *textual = CF_TRUE;
        cf_value_text(v, start, (size_t)(s->p - start));
        return CF_TRUE;
    }
    if (s->end - s->p >= 2 && s->p[0] == '0' &&
        (s->p[1] == 'x' || s->p[1] == 'o' || s->p[1] == 'b')) {
        if (n > 0) {
            return toml_fail(s, start,
                             "a hexadecimal, octal or binary integer has no sign");
        }
        base = (s->p[1] == 'x') ? 16 : (s->p[1] == 'o') ? 8 : 2;
        s->p += 2;
        if (!toml_digits(s, base, buf, sizeof buf, &n)) {
            return toml_fail(s, start, "malformed integer");
        }
    } else {
        size_t first = n;
        if (!toml_digits(s, 10, buf, sizeof buf, &n)) {
            return toml_fail(s, start,
                             (n == 0) ? "expected a value" : "malformed number");
        }
        /* Digits then '-' or ':' that were not a date or a time above. */
        if (s->p < s->end && (*s->p == ':' || (*s->p == '-' && first == 0))) {
            return toml_fail(s, start, "malformed date, time or number");
        }
        if (buf[first] == '0' && n - first > 1) {
            return toml_fail(s, start, "malformed number: a leading zero");
        }
        if (s->p < s->end && *s->p == '.') {
            is_real  = CF_TRUE;
            buf[n++] = *s->p++;
            if (!toml_digits(s, 10, buf, sizeof buf, &n)) {
                return toml_fail(s, start,
                                 "malformed number: no digit after the point");
            }
        }
        if (s->p < s->end && (*s->p == 'e' || *s->p == 'E')) {
            is_real  = CF_TRUE;
            buf[n++] = *s->p++;
            if (s->p < s->end && (*s->p == '+' || *s->p == '-') && n + 1 < sizeof buf) {
                buf[n++] = *s->p++;
            }
            if (!toml_digits(s, 10, buf, sizeof buf, &n)) {
                return toml_fail(s, start,
                                 "malformed number: no digit in the exponent");
            }
        }
    }
    /* What follows a number ends it: a letter or another digit sign means
       the text was not one (1x, 0x1.5, 1e5.0). */
    if (s->p < s->end && (cf_is_alpha((unsigned char)*s->p) || *s->p == '_' ||
                          *s->p == '.' || cf_is_digit((unsigned char)*s->p))) {
        return toml_fail(s, start, "malformed number");
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

static cf_bool toml_value(toml_scan* s, const char* name, size_t nlen, cf_u32 node);

/* Open the leading segments of a dotted key as tables, run the value,
   close them again. "a.b.c = 1" is a table a, a table b, a scalar c. */
static cf_bool toml_keyval(toml_scan* s) {
    toml_dkey   k;
    int         n, i;
    const char* at;
    cf_u32      node = 0;

    toml_blank(s);
    at = s->p;
    if (!toml_key(s, &k)) {
        return CF_FALSE;
    }
    n = k.n;
    if (s->p >= s->end || *s->p != '=') {
        return toml_fail(s, s->p, "expected '=' after the key");
    }
    ++s->p;
    toml_blank(s);
    if (s->track) {
        toml_def kind = (s->p < s->end && *s->p == '{') ? TOML_INLINE : TOML_VALUE;
        node          = toml_define_key(s, &k, at, kind);
        if (node == 0) {
            return CF_FALSE;
        }
    }
    for (i = 0; i + 1 < n; ++i) {
        if (!s->out->begin_table(s->out->ctx, TOML_SEG(&k, i), k.len[i])) {
            return toml_fail(s, s->p, "not a valid key");
        }
    }
    if (!toml_value(s, TOML_SEG(&k, n - 1), k.len[n - 1], node)) {
        return CF_FALSE;
    }
    for (i = 0; i + 1 < n; ++i) {
        if (!s->out->end(s->out->ctx)) {
            return toml_fail(s, s->p, "the document could not be recorded");
        }
    }
    return CF_TRUE;
}

/* `node`, when the definitions are kept, is the array's: an element that
   is a table or an array is recorded under it by number, so that a key
   twice in `[{a = 1, a = 2}]` is found as anywhere else. */
static cf_bool toml_array(toml_scan* s, const char* name, size_t nlen, cf_u32 node) {
    cf_u32 count = 0;

    if (++s->depth > TOML_MAX_DEPTH) {
        return toml_fail(s, s->p, "nesting is too deep");
    }
    if (!s->out->begin_array(s->out->ctx, name, nlen)) {
        return toml_fail(s, s->p, "not a valid key");
    }
    ++s->p; /* '[' */
    for (;;) {
        if (!toml_space_lines(s)) {
            return CF_FALSE;
        }
        if (s->p >= s->end) {
            return toml_fail(s, s->p, "the array is not closed");
        }
        if (*s->p == ']') {
            ++s->p;
            break;
        }
        {
            cf_u32 elem = 0;
            if (node != 0 && (*s->p == '{' || *s->p == '[')) {
                elem = toml_add(s, node, 1, NULL, 0, count,
                                (*s->p == '{') ? TOML_INLINE : TOML_VALUE);
                if (elem == 0) {
                    return CF_FALSE;
                }
            }
            ++count;
            if (!toml_value(s, NULL, 0, elem)) {
                return CF_FALSE;
            }
        }
        if (!toml_space_lines(s)) {
            return CF_FALSE;
        }
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

/* `node`, when the definitions are kept, is the table's: its keys are
   defined into it, and once it is read it is sealed, since TOML has an
   inline table complete as written. */
static cf_bool toml_inline_table(toml_scan* s, const char* name, size_t nlen,
                                 cf_u32 node) {
    cf_bool first = CF_TRUE;
    cf_u32  body  = s->body;

    if (++s->depth > TOML_MAX_DEPTH) {
        return toml_fail(s, s->p, "nesting is too deep");
    }
    if (!s->out->begin_table(s->out->ctx, name, nlen)) {
        return toml_fail(s, s->p, "not a valid key");
    }
    ++s->p; /* '{' */
    if (node != 0) {
        s->body = node;
    }
    /* TOML 1.1: over several lines, with comments, and a comma after the
       last key. */
    for (;;) {
        if (!toml_space_lines(s)) {
            return CF_FALSE;
        }
        if (s->p >= s->end) {
            return toml_fail(s, s->p, "the inline table is not closed");
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
            if (!toml_space_lines(s)) {
                return CF_FALSE;
            }
            if (s->p < s->end && *s->p == '}') {
                ++s->p;
                break;
            }
        }
        first = CF_FALSE;
        if (!toml_keyval(s)) {
            return CF_FALSE;
        }
    }
    if (node != 0) {
        s->nodes[node].kind = (cf_u8)TOML_SEALED;
        s->body             = body;
    }
    --s->depth;
    return s->out->end(s->out->ctx)
               ? CF_TRUE
               : toml_fail(s, s->p, "the document could not be recorded");
}

static cf_bool toml_value(toml_scan* s, const char* name, size_t nlen, cf_u32 node) {
    cf_value    v;
    cf_loc      at;
    const char* start   = s->p;
    cf_bool     textual = CF_FALSE;

    if (s->p >= s->end) {
        return toml_fail(s, s->p, "expected a value");
    }
    toml_loc(s, start, &at);
    switch (*s->p) {
    case '[':
        return toml_array(s, name, nlen, node);
    case '{':
        return toml_inline_table(s, name, nlen, node);
    case '"':
    case '\'': {
        char         local[256];
        char*        text       = local;
        unsigned int line       = s->line;
        const char*  line_start = s->line_start;
        size_t       n          = toml_string(s, NULL, (size_t)-1, CF_TRUE);
        if (n == (size_t)-1) {
            return CF_FALSE;
        }
        if (n >= sizeof local) {
            if (s->out->size <
                    offsetof(cf_doc_sink, reserve_text) + sizeof s->out->reserve_text ||
                s->out->reserve_text == NULL) {
                return toml_fail(s, start, "a long string needs sink text storage");
            }
            text = s->out->reserve_text(s->out->ctx, n + 1);
            if (text == NULL) {
                return cf_error_set(s->err, CF_E_MEMORY, NULL, s->file,
                                    "no room to decode a TOML string");
            }
        }
        /* The second pass reads the same lines again. */
        s->p          = start;
        s->line       = line;
        s->line_start = line_start;
        if (toml_string(s, text, n + 1, CF_TRUE) == (size_t)-1) {
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
        if (!toml_number(s, &v, &textual)) {
            return CF_FALSE;
        }
        break;
    }
    return s->out->scalar(s->out->ctx, name, nlen, &v, textual, &at)
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
            s->closed_set   = CF_TRUE;
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
    cf_bool     existed  = CF_FALSE;
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
    if (s->track && !toml_define_header(s, &k, at, is_array, &existed)) {
        return CF_FALSE;
    }

    /* How much of the open path this header agrees with. */
    keep = 0;
    i    = 0;
    for (o = 0; o < s->nopen; ++o) {
        if (s->open[o].kind == TOML_ELEMENT) {
            keep = o + 1;
            continue;
        }
        if (i >= n || (size_t)k.len[i] != (size_t)s->open[o].n ||
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
    /* An array of tables seen before and not open now is being reopened.
       With the definitions kept, that is known exactly - [[a.b]] in the
       second element of [[a]] is a new array, not the first one's; a sink
       without workspace falls back to the last array's name. */
    if (is_array &&
        (s->track ? existed
                  : (s->closed_set && strlen(s->closed) == (size_t)k.len[n - 1] &&
                     memcmp(s->closed, TOML_SEG(&k, n - 1), k.len[n - 1]) == 0))) {
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

    s.p          = in->p + cf_blob_skip_bom(in);
    s.end        = in->p + in->n;
    s.file       = (in->name != NULL) ? in->name : "<toml>";
    s.line_start = s.p;
    s.line       = 1;
    s.out        = out;
    s.err        = err;
    s.depth      = 0;
    s.nopen      = 0;
    s.nused      = 0;
    s.closed[0]  = '\0';
    s.closed_set = CF_FALSE;
    s.body       = 0;
    if (!toml_track_init(&s)) {
        return CF_FALSE;
    }

    if (!out->begin_table(out->ctx, NULL, 0)) {
        return cf_error_set(err, CF_E_SOURCE, NULL, s.file, "sink refused");
    }
    for (;;) {
        if (!toml_space_lines(&s)) {
            return CF_FALSE;
        }
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

#if CF_WITH_EMIT

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
    cf_u16   stack[TOML_MAX_DEPTH + 2]; /* lengths within path */
    int      depth;                     /* tables open outside any inline value */
    int      inl; /* inline levels open: arrays and inline tables */
    cf_u8    first[TOML_MAX_DEPTH + 2];
    cf_u8    is_array[TOML_MAX_DEPTH + 2];
    cf_bool  header_pending;
} toml_enc;

/* cf_emit keeps this on its own frame, in CF_ENCODER_STATE_MAX bytes. */
typedef char toml_enc_fits[(sizeof(toml_enc) <= CF_ENCODER_STATE_MAX) ? 1 : -1];

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
    /* The tables above it count too: the decoder allows the document and
       TOML_MAX_DEPTH - 1 levels inside it, however they are written. */
    if (e->depth + e->inl >= TOML_MAX_DEPTH) {
        return CF_FALSE;
    }
    toml_enc_lead(e, name, n);
    cf_sink_puts(e->out, array ? "[" : "{ ");
    ++e->inl;
    e->first[e->inl]    = CF_TRUE;
    e->is_array[e->inl] = (cf_u8)(array ? 1 : 0);
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
    e->stack[e->depth++] = (cf_u16)e->plen;
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
    sink->size        = offsetof(cf_doc_sink, reserve_text);
    sink->begin_table = toml_enc_table;
    sink->begin_array = toml_enc_array;
    sink->scalar      = toml_enc_scalar;
    sink->end         = toml_enc_end;
    sink->ctx         = e;
    return CF_TRUE;
}

const cf_codec cf_codec_toml = {sizeof(cf_codec),
                                "toml",
                                ".toml",
                                CF_CODEC_READ | CF_CODEC_WRITE | CF_CODEC_TYPED |
                                    CF_CODEC_ARRAYS,
                                toml_decode,
                                toml_encoder,
                                sizeof(toml_enc),
                                NULL,
                                NULL};

#else /* !CF_WITH_EMIT: read only */

const cf_codec cf_codec_toml = {sizeof(cf_codec),
                                "toml",
                                ".toml",
                                CF_CODEC_READ | CF_CODEC_TYPED | CF_CODEC_ARRAYS,
                                toml_decode,
                                NULL,
                                0,
                                NULL,
                                NULL};

#endif /* CF_WITH_EMIT */

#endif /* CF_WITH_TOML */
