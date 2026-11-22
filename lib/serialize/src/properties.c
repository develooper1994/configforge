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
#include "configforge/cf_text.h"
#include "configforge/cf_build_options.h"
#include "configforge/cf_error.h"
#include "configforge/cf_port.h"
#include "configforge/cf_value.h"

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

/* The escapes of a .properties file, as data (D-102): the letters for a
   tab, a line feed, a carriage return and a form feed, and the four-digit
   escape with UTF-16 pairs, as Java writes anything above U+FFFF. Any other
   letter after a backslash stands for itself. */
static const cf_text_esc_simple prop_simple[] = {
    {'t', 9}, {'n', 10}, {'r', 13}, {'f', 12}, {0, 0}};
static const cf_text_esc_family prop_family = {
    prop_simple, "", "u", "", CF_ESC_PAIRS, CF_ESC_AT_LETTER, CF_ESC_NL_NONE};

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
            char        c = *s->p;
            cf_text_esc r;
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
                cf_text_esc_decode(&prop_family, s->p, eol, &r);
                if (r.kind == CF_ESC_E_DIGITS) {
                    return prop_fail(s, "\\"
                                        "u needs four hexadecimal digits");
                }
                if (r.kind == CF_ESC_E_HIGH || r.kind == CF_ESC_E_LOW) {
                    return prop_fail(s, "a \\"
                                        "u escape is half a surrogate pair");
                }
                if (r.kind == CF_ESC_CP) {
                    char   enc[4];
                    size_t ek = cf_text_utf8_encode(r.cp, enc);

                    if (n + ek > cap) {
                        return prop_fail(s, is_key ? "the key is too long"
                                                   : "the value is too long");
                    }
                    memcpy(out + n, enc, ek);
                    n += ek;
                    s->p += r.used;
                    continue;
                }
                /* No escape of this format: the letter stands for itself. */
                c = *s->p;
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

    /* A byte order mark is not part of the first key. */
    s.p          = in->p + cf_blob_skip_bom(in);
    s.end        = in->p + in->n;
    s.file       = (in->name != NULL) ? in->name : "<properties>";
    s.line       = 1;
    s.line_start = s.p;
    s.err        = err;

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

const cf_codec cf_codec_properties = {sizeof(cf_codec),
                                      "properties",
                                      ".properties",
                                      CF_CODEC_READ,
                                      prop_decode,
                                      NULL,
                                      0,
                                      NULL,
                                      NULL};

#endif /* CF_WITH_PROPERTIES */
