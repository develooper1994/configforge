/*
 * escape.c - the escapes of a family of formats, read and written once
 * (D-102). C89.
 *
 * Reading: the part every family shares (a letter that stands for one code
 * point, or opens 2, 4 or 8 hexadecimal digits, with UTF-16 pairs where the
 * family has them) is one function, and what differs is a table. Writing:
 * which code points are escaped, and how, is a set. What stays in each
 * reader is its messages, its limits and its own put.
 */

#include "configforge/cf_text.h"
#include "configforge/cf_port.h"

#include <stddef.h>

#include <string.h>

/* Whether c is one of the letters in the list. */
static int esc_in(const char* letters, char c) {
    return c != '\0' && strchr(letters, c) != NULL;
}

static void esc_fail(cf_text_esc* r, int kind, size_t at) {
    r->kind = kind;
    r->at   = at;
}

void cf_text_esc_decode(const cf_text_esc_family* f, const char* p, const char* end,
                        cf_text_esc* r) {
    const cf_text_esc_simple* s;
    char                      c;
    size_t                    n, k, brk;
    cf_u32                    v = 0;

    r->kind   = CF_ESC_OTHER;
    r->cp     = 0;
    r->used   = 0;
    r->at     = 0;
    r->letter = '\0';
    if (p >= end) {
        return;
    }
    c   = *p;
    brk = cf_text_break(f->breaks, p, end);
    if (brk != 0) {
        r->kind = CF_ESC_BREAK;
        r->used = brk;
        return;
    }
    if ((f->flags & CF_ESC_NO_OCTAL) != 0) {
        if (c == '0' && p + 1 < end && p[1] >= '0' && p[1] <= '9') {
            esc_fail(r, CF_ESC_E_NUL_DIGIT, 0);
            return;
        }
        if (c >= '1' && c <= '9') {
            esc_fail(r, CF_ESC_E_DIGIT, 0);
            return;
        }
    }
    for (s = f->simple; s->letter != '\0'; ++s) {
        if (s->letter == c) {
            r->kind = CF_ESC_CP;
            r->cp   = s->cp;
            r->used = 1;
            return;
        }
    }
    if (esc_in(f->hex2, c)) {
        n = 2;
    } else if (esc_in(f->hex4, c)) {
        n = 4;
    } else if (esc_in(f->hex8, c)) {
        n = 8;
    } else {
        return;
    }
    for (k = 0; k < n; ++k) {
        int d = (p + 1 + k < end) ? cf_text_hex_value((unsigned char)p[1 + k]) : -1;
        if (d < 0) {
            size_t at = 0;

            if (f->place == CF_ESC_AT_DIGITS) {
                at = 1;
            } else if (f->place == CF_ESC_AT_BAD) {
                at = 1 + k;
            }
            r->letter = c;
            esc_fail(r, CF_ESC_E_DIGITS, at);
            return;
        }
        v = (v << 4) | (cf_u32)d;
    }
    if ((f->flags & CF_ESC_PAIRS) != 0 && esc_in(f->hex4, c)) {
        if (v >= 0xD800u && v <= 0xDBFFu) {
            cf_u32 lo = 0;
            int    ok = (end - (p + 5) >= 6 && p[5] == '\\' && p[6] == 'u');

            for (k = 0; ok && k < 4; ++k) {
                int d = cf_text_hex_value((unsigned char)p[7 + k]);
                if (d < 0) {
                    ok = 0;
                } else {
                    lo = (lo << 4) | (cf_u32)d;
                }
            }
            if (!ok || lo < 0xDC00u || lo > 0xDFFFu) {
                esc_fail(r, CF_ESC_E_HIGH, 5);
                return;
            }
            r->kind = CF_ESC_CP;
            r->cp   = 0x10000u + ((v - 0xD800u) << 10) + (lo - 0xDC00u);
            r->used = 11;
            return;
        }
        if (v >= 0xDC00u && v <= 0xDFFFu) {
            esc_fail(r, CF_ESC_E_LOW, 5);
            return;
        }
    }
    if (v > 0x10FFFFu || (v >= 0xD800u && v <= 0xDFFFu)) {
        esc_fail(r, CF_ESC_E_SCALAR, 1 + n);
        return;
    }
    r->kind = CF_ESC_CP;
    r->cp   = v;
    r->used = 1 + n;
}

size_t cf_text_escape(const cf_text_escaper* e, cf_u32 cp, char out[8]) {
    static const char hex[] = "0123456789abcdef";
    const char*       q;
    size_t            i;
    int               must;

    for (q = e->named; q[0] != '\0'; q += 2) {
        if ((cf_u32)(unsigned char)q[0] == cp) {
            out[0] = '\\';
            out[1] = q[1];
            return 2;
        }
    }
    must = cp < e->below || (e->from <= e->to && cp >= e->from && cp <= e->to) ||
           ((e->flags & CF_ESCAPER_NONCHARS) != 0 && (cp == 0xFFFEu || cp == 0xFFFFu));
    if (!must) {
        return 0;
    }
    out[0] = '\\';
    if (cp < 0x100u && (e->flags & CF_ESCAPER_X) != 0) {
        out[1] = 'x';
        out[2] = hex[(cp >> 4) & 0xFu];
        out[3] = hex[cp & 0xFu];
        return 4;
    }
    out[1] = 'u';
    for (i = 0; i < 4; ++i) {
        out[2 + i] = hex[(cp >> (12 - 4 * i)) & 0xFu];
    }
    return 6;
}
