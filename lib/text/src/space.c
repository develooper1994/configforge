/*
 * space.c - blanks, comments and line breaks, by syntax. C89.
 */

#include "configforge/cf_text.h"
#include "configforge/cf_port.h"

#include <stddef.h>

size_t cf_text_break(unsigned rule, const char* p, const char* end) {
    if (p == end || rule == CF_NL_NONE) {
        return 0;
    }
    if (*p == '\n') {
        return 1;
    }
    if (*p == '\r' && rule != CF_NL_LF) {
        if (end - p >= 2 && p[1] == '\n') {
            return 2;
        }
        return rule == CF_NL_TOML ? 0 : 1;
    }
    if (rule == CF_NL_JSON5 && end - p >= 3 && (unsigned char)p[0] == 0xE2u &&
        (unsigned char)p[1] == 0x80u &&
        ((unsigned char)p[2] == 0xA8u || (unsigned char)p[2] == 0xA9u)) {
        return 3;
    }
    return 0;
}

const char* cf_text_eol(unsigned rule, const char* p, const char* end) {
    while (p < end && cf_text_break(rule, p, end) == 0) {
        ++p;
    }
    return p;
}

const char* cf_text_blanks(unsigned set, const char* p, const char* end) {
    while (p < end &&
           (*p == ' ' || *p == '\t' || (set == CF_BLANK_PROP && *p == '\f') ||
            (*p == '\r' && (set == CF_BLANK_INI ||
                            (set == CF_BLANK_TOML && end - p >= 2 && p[1] == '\n'))))) {
        ++p;
    }
    return p;
}

size_t cf_text_trim(const char* p, size_t n, const char** out) {
    const char* end = p + n;

    p = cf_text_blanks(CF_BLANK_INI, p, end);
    while (end > p && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r')) {
        --end;
    }
    *out = p;
    return (size_t)(end - p);
}

cf_bool cf_text_is_ecma_space(cf_u32 cp) {
    return (cp == 0x0Bu || cp == 0x0Cu || cp == 0xA0u || cp == 0xFEFFu ||
            cp == 0x1680u || (cp >= 0x2000u && cp <= 0x200Au) || cp == 0x2028u ||
            cp == 0x2029u || cp == 0x202Fu || cp == 0x205Fu || cp == 0x3000u)
               ? CF_TRUE
               : CF_FALSE;
}

int cf_text_comment(const cf_text_syntax* sy, const char** pp, const char* end,
                    const char** bad) {
    const char* p = *pp;

    if (!(sy->flags & CF_SYN_TOML_COMMENT)) {
        *pp = cf_text_eol(sy->rule, p, end);
        return CF_SKIP_OK;
    }
    while (p < end && *p != '\n') {
        unsigned char c = (unsigned char)*p;
        cf_u32        cp;
        size_t        n;

        if (c == '\r' && end - p >= 2 && p[1] == '\n') {
            ++p;
            continue;
        }
        if ((c < 0x20u && c != '\t') || c == 0x7Fu) {
            *pp = *bad = p;
            return CF_SKIP_CONTROL;
        }
        n = cf_text_utf8_decode(p, end, &cp);
        if (n == 0) {
            *pp = *bad = p;
            return CF_SKIP_UTF8;
        }
        p += n;
    }
    *pp = p;
    return CF_SKIP_OK;
}

/* Inside a block comment, where only the closing marker and breaks matter. */
static int text_block_comment(unsigned rule, cf_text_track* t, const char** pp,
                              const char* end, const char** bad) {
    const char* p = *pp;

    while (p < end) {
        size_t n;

        if (*p == '*' && end - p >= 2 && p[1] == '/') {
            *pp = p + 2;
            return CF_SKIP_OK;
        }
        n = cf_text_break(rule, p, end);
        if (n != 0) {
            p += n;
            cf_text_track_line(t, p);
        } else {
            ++p;
        }
    }
    *pp = *bad = end;
    return CF_SKIP_UNCLOSED;
}

int cf_text_skip(const cf_text_syntax* sy, cf_text_track* t, const char** pp,
                 const char* end, const char** bad) {
    const char* p = *pp;

    while (p < end) {
        size_t      n = cf_text_break(sy->rule, p, end);
        const char* next;

        if (n != 0) {
            p += n;
            cf_text_track_line(t, p);
            continue;
        }
        next = cf_text_blanks(sy->blanks, p, end);
        if (next != p) {
            p = next;
            continue;
        }
        if (sy->flags & CF_SYN_SLASH) {
            cf_u32 cp;

            if (*p == '/' && end - p >= 2 && p[1] == '/') {
                p = cf_text_eol(sy->rule, p + 2, end);
                continue;
            }
            if (*p == '/' && end - p >= 2 && p[1] == '*') {
                p += 2;
                if (text_block_comment(sy->rule, t, &p, end, bad) != CF_SKIP_OK) {
                    *pp = p;
                    return CF_SKIP_UNCLOSED;
                }
                continue;
            }
            n = cf_text_utf8_decode(p, end, &cp);
            if (n != 0 && cf_text_is_ecma_space(cp)) {
                p += n;
                continue;
            }
        }
        if ((sy->flags & CF_SYN_HASH) && *p == '#' &&
            (!(sy->flags & CF_SYN_HASH_BLANK) || p == t->line_start || p[-1] == ' ' ||
             p[-1] == '\t')) {
            int result = cf_text_comment(sy, &p, end, bad);
            if (result != CF_SKIP_OK) {
                *pp = p;
                return result;
            }
            continue;
        }
        break;
    }
    *pp = p;
    return CF_SKIP_OK;
}
