/*
 * refs.c - what "${" is, defined once (D-102). C89.
 *
 * A reference is "${key}"; a program that wants the text "${" writes
 * "$${". The copies that used to count, find, widen and narrow it (the
 * loader's validation, cf_emit and the CBOR writer) are these four
 * functions; each user keeps its own way of writing.
 */

#include "configforge/cf_text.h"

#include <stddef.h>

size_t cf_text_ref_count(const char* p, size_t n) {
    size_t i, count = 0;

    for (i = 0; i + 1 < n; ++i) {
        if (p[i] == '$' && p[i + 1] == '{') {
            ++count;
        }
    }
    return count;
}

size_t cf_text_ref_next(const char* p, size_t n, size_t from) {
    size_t i;

    for (i = from; i + 1 < n; ++i) {
        if (p[i] == '$' && p[i + 1] == '{') {
            return i;
        }
    }
    return n;
}

size_t cf_text_ref_unescape(const char* p, size_t n, char* out) {
    size_t i, j = 0;

    for (i = 0; i < n; ++i) {
        if (p[i] == '$' && i + 2 < n && p[i + 1] == '$' && p[i + 2] == '{') {
            ++i; /* drop one '$' of the pair */
        }
        out[j++] = p[i];
    }
    return j;
}

void cf_text_ref_widen(char* buf, size_t from, size_t end, size_t limit) {
    size_t i, extra = cf_text_ref_count(buf + from, end - from);
    char   next = '\0';

    i = end;
    while (i > from) {
        char c = buf[--i];
        if (extra < limit - i) {
            buf[i + extra] = c;
        }
        if (c == '$' && next == '{') {
            --extra;
            if (extra < limit - i) {
                buf[i + extra] = '$';
            }
        }
        next = c;
    }
}
