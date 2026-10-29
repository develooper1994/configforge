/*
 * properties.c - Java .properties, read. C89.
 *
 * The format java.util.Properties reads, which Spring Boot's
 * application.properties and most JVM configuration are written in:
 *
 *     # a comment, and so is a line starting with !
 *     server.port = 8080          key, '=' or ':' or whitespace, value
 *     greeting: hello world       the value runs to the end of the line
 *     path=C:\\data\\app          \\ \t \n \r \f and the u escape are escapes
 *     long = first \             a line ending in an odd number of
 *            second              backslashes continues on the next
 *
 * Keys are the library's keys, dots and all, and a key it cannot hold
 * is refused by name: Spring's relaxed spellings (camelCase, hyphens)
 * are Spring's to relax, and guessing which one was meant is exactly
 * what this library does not do. Values are text, typed by the contract
 * as INI's are (D-24). The file is read as UTF-8, the encoding every
 * other format here uses and the one Java has defaulted to for
 * resource bundles since 9; the u escape - a backslash, u and four hex
 * digits - writes anything else. (Not spelt out here: pcc's lexer reads
 * that sequence as a universal character name even in a comment.)
 * Read only: the
 * format has no quoting, so text written as 8080 would read back as a
 * number.
 *
 * Written here rather than taken from a library: there is no C reader
 * for it, and the grammar is a page.
 */

#include "configforge/cf_serialize.h"

#include <string.h>

#if CF_WITH_PROPERTIES

typedef struct prop_scan {
    const char*  p;
    const char*  end;
    const char*  file;
    unsigned int line;
    const char*  line_start;
    cf_error*    err;
} prop_scan;

static int prop_hex(int c) {
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

/* One code point as UTF-8 into out[at..]; the new length, or 0. */
static size_t prop_utf8(char* out, size_t at, size_t cap, unsigned long cp) {
    unsigned char b[4];
    size_t        n, i;

    if (cp < 0x80u) {
        b[0] = (unsigned char)cp;
        n    = 1;
    } else if (cp < 0x800u) {
        b[0] = (unsigned char)(0xC0u | (cp >> 6));
        b[1] = (unsigned char)(0x80u | (cp & 0x3Fu));
        n    = 2;
    } else if (cp < 0x10000u) {
        b[0] = (unsigned char)(0xE0u | (cp >> 12));
        b[1] = (unsigned char)(0x80u | ((cp >> 6) & 0x3Fu));
        b[2] = (unsigned char)(0x80u | (cp & 0x3Fu));
        n    = 3;
    } else {
        b[0] = (unsigned char)(0xF0u | (cp >> 18));
        b[1] = (unsigned char)(0x80u | ((cp >> 12) & 0x3Fu));
        b[2] = (unsigned char)(0x80u | ((cp >> 6) & 0x3Fu));
        b[3] = (unsigned char)(0x80u | (cp & 0x3Fu));
        n    = 4;
    }
    if (at + n > cap) {
        return 0;
    }
    for (i = 0; i < n; ++i) {
        out[at + i] = (char)b[i];
    }
    return at + n;
}

static cf_bool prop_fail(prop_scan* s, const char* what) {
    return cf_error_set(s->err, CF_E_SOURCE, NULL, s->file, "%s:%u: %s", s->file,
                        s->line, what);
}

/* The end of the physical line at s->p, without moving. */
static const char* prop_eol(const prop_scan* s) {
    const char* q = s->p;
    while (q < s->end && *q != '\n' && *q != '\r') {
        ++q;
    }
    return q;
}

/* Past a line break, counting it. */
static void prop_next_line(prop_scan* s) {
    if (s->p < s->end && *s->p == '\r') {
        ++s->p;
    }
    if (s->p < s->end && *s->p == '\n') {
        ++s->p;
    }
    ++s->line;
    s->line_start = s->p;
}

/* Read one logical line's element - the key, or the value - unescaping
   it into out. A key stops at an unescaped '=', ':' or whitespace; a
   value runs to the end of the logical line. Continuations are followed:
   the backslash and the line break go, and the next line's leading
   whitespace with them. */
static cf_bool prop_element(prop_scan* s, cf_bool is_key, char* out, size_t cap,
                            size_t* len) {
    size_t n = 0;

    for (;;) {
        const char* eol = prop_eol(s);
        while (s->p < eol) {
            char c = *s->p;
            if (is_key &&
                (c == '=' || c == ':' || c == ' ' || c == '\t' || c == '\f')) {
                *len = n;
                return CF_TRUE;
            }
            if (c == '\\') {
                if (s->p + 1 == eol) {
                    break; /* a continuation, handled below */
                }
                ++s->p;
                c = *s->p;
                if (c == 'u') {
                    unsigned long cp = 0;
                    int           k;
                    for (k = 0; k < 4; ++k) {
                        int h = (s->p + 1 + k < eol)
                                    ? prop_hex((unsigned char)s->p[1 + k])
                                    : -1;
                        if (h < 0) {
                            return prop_fail(s, "\\"
                                                "u needs four hexadecimal digits");
                        }
                        cp = cp * 16u + (unsigned long)h;
                    }
                    s->p += 4;
                    /* A surrogate pair, as Java writes anything above
                       U+FFFF. */
                    if (cp >= 0xD800u && cp <= 0xDBFFu && s->p + 6 < eol &&
                        s->p[1] == '\\' && s->p[2] == 'u') {
                        unsigned long lo   = 0;
                        cf_bool       good = CF_TRUE;
                        for (k = 0; k < 4; ++k) {
                            int h = (s->p + 3 + k < eol)
                                        ? prop_hex((unsigned char)s->p[3 + k])
                                        : -1;
                            if (h < 0) {
                                good = CF_FALSE;
                                break;
                            }
                            lo = lo * 16u + (unsigned long)h;
                        }
                        if (good && lo >= 0xDC00u && lo <= 0xDFFFu) {
                            cp = 0x10000u + ((cp - 0xD800u) << 10) + (lo - 0xDC00u);
                            s->p += 6;
                        }
                    }
                    if (cp >= 0xD800u && cp <= 0xDFFFu) {
                        return prop_fail(s, "a \\"
                                            "u escape is half a surrogate pair");
                    }
                    n = prop_utf8(out, n, cap, cp);
                    if (n == 0) {
                        return prop_fail(s, is_key ? "the key is too long"
                                                   : "the value is too long");
                    }
                    ++s->p;
                    continue;
                }
                c = (char)((c == 't')   ? '\t'
                           : (c == 'n') ? '\n'
                           : (c == 'r') ? '\r'
                           : (c == 'f') ? '\f'
                                        : c);
            }
            if (n >= cap) {
                return prop_fail(s, is_key ? "the key is too long"
                                           : "the value is too long");
            }
            out[n++] = c;
            ++s->p;
        }
        /* Escaped backslashes went in pairs above, so a backslash left
           alone at the end of the line is the one that continues it;
           stopping anywhere else ended the element. */
        if (!(s->p + 1 == eol && *s->p == '\\')) {
            break;
        }
        /* A continuation: past the backslash and the break, and past the
           next line's leading whitespace. */
        s->p = eol;
        prop_next_line(s);
        while (s->p < s->end && (*s->p == ' ' || *s->p == '\t' || *s->p == '\f')) {
            ++s->p;
        }
        if (s->p >= s->end) {
            break;
        }
    }
    *len = n;
    return CF_TRUE;
}

static cf_bool prop_decode(const cf_blob* in, const cf_doc_sink* out, cf_error* err) {
    prop_scan s;
    char      key[CF_KEY_MAX];
    /* A value as long as two paths: room for a file name or a URL with
       its parameters, and a frame that grows and shrinks with the build's
       limits (D-37), as the minimal preset's 1 KB budget needs. A longer
       value is refused as too long, by name. */
    char val[CF_PATH_MAX * 2];

    s.p          = in->p;
    s.end        = in->p + in->n;
    s.file       = (in->name != NULL) ? in->name : "<properties>";
    s.line       = 1;
    s.line_start = in->p;
    s.err        = err;

    /* A byte order mark is not part of the first key. */
    if (in->n >= 3 && memcmp(in->p, "\xEF\xBB\xBF", 3) == 0) {
        s.p += 3;
    }
    if (!out->begin_table(out->ctx, NULL, 0)) {
        return cf_error_set(err, CF_E_SOURCE, NULL, s.file, "sink refused");
    }
    while (s.p < s.end) {
        size_t       klen = 0, vlen = 0;
        unsigned int line;
        unsigned int col;
        cf_value     v;
        cf_loc       at;

        while (s.p < s.end && (*s.p == ' ' || *s.p == '\t' || *s.p == '\f')) {
            ++s.p;
        }
        if (s.p >= s.end) {
            break;
        }
        if (*s.p == '\n' || *s.p == '\r') {
            prop_next_line(&s);
            continue;
        }
        if (*s.p == '#' || *s.p == '!') {
            s.p = prop_eol(&s);
            continue;
        }
        line = s.line;
        col  = (unsigned int)(s.p - s.line_start) + 1u;
        if (!prop_element(&s, CF_TRUE, key, sizeof key, &klen)) {
            return CF_FALSE;
        }
        /* Whitespace, then at most one '=' or ':', then whitespace. */
        while (s.p < s.end && (*s.p == ' ' || *s.p == '\t' || *s.p == '\f')) {
            ++s.p;
        }
        if (s.p < s.end && (*s.p == '=' || *s.p == ':')) {
            ++s.p;
            while (s.p < s.end && (*s.p == ' ' || *s.p == '\t' || *s.p == '\f')) {
                ++s.p;
            }
        }
        if (!prop_element(&s, CF_FALSE, val, sizeof val, &vlen)) {
            return CF_FALSE;
        }
        if (klen == 0) {
            return cf_error_set(err, CF_E_SOURCE, NULL, s.file, "%s:%u: empty key",
                                s.file, line);
        }
        at.file = s.file;
        at.line = line;
        at.col  = col;
        cf_value_text(&v, val, vlen);
        if (!out->scalar(out->ctx, key, klen, &v, CF_TRUE, &at)) {
            return cf_error_set(err, CF_E_KEY, NULL, s.file,
                                "%s:%u: '%S' is not a valid key", s.file, line, key,
                                klen);
        }
    }
    out->end(out->ctx);
    return CF_TRUE;
}

const cf_codec cf_codec_properties = {"properties", ".properties", CF_CODEC_READ,
                                      prop_decode,  NULL,          0};

#endif /* CF_WITH_PROPERTIES */
