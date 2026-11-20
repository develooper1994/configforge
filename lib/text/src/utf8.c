/*
 * utf8.c - UTF-8 decoded, validated and encoded, once (D-102). C89.
 *
 * RFC 3629's: the shortest form only, no surrogate U+D800-U+DFFF, nothing
 * above U+10FFFF.
 */

#include "configforge/cf_text.h"

#include "lanes/lanes.h"

size_t cf_text_utf8_decode(const char* p, const char* end, cf_u32* cp) {
    const unsigned char* u;
    size_t               k;
    size_t               i;
    cf_u32               v;

    if (p >= end) {
        return 0;
    }
    u = (const unsigned char*)p;
    if (u[0] < 0x80u) {
        *cp = u[0];
        return 1;
    }
    if (u[0] >= 0xC2u && u[0] <= 0xDFu) {
        k = 2;
        v = u[0] & 0x1Fu;
    } else if (u[0] >= 0xE0u && u[0] <= 0xEFu) {
        k = 3;
        v = u[0] & 0x0Fu;
    } else if (u[0] >= 0xF0u && u[0] <= 0xF4u) {
        k = 4;
        v = u[0] & 0x07u;
    } else {
        return 0;
    }
    if ((size_t)(end - p) < k) {
        return 0;
    }
    for (i = 1; i < k; ++i) {
        if ((u[i] & 0xC0u) != 0x80u) {
            return 0;
        }
        v = (v << 6) | (cf_u32)(u[i] & 0x3Fu);
    }
    if ((k == 3 && v < 0x800u) || (k == 4 && (v < 0x10000u || v > 0x10FFFFu)) ||
        (v >= 0xD800u && v <= 0xDFFFu)) {
        return 0;
    }
    *cp = v;
    return k;
}

size_t cf_text_utf8_valid(const char* p, size_t n) {
    return lanes_scalar()->utf8_valid((const unsigned char*)p, n);
}

size_t cf_text_utf8_encode(cf_u32 cp, char out[4]) {
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
