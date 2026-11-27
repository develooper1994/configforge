/*
 * key.c - validation of canonical configuration keys. C89.
 */

#include "configforge/cf_error.h"
#include "configforge/cf_build_options.h"
#include "configforge/cf_port.h"

#include <stddef.h>

cf_bool cf_key_valid(const char* key, size_t len) {
    /* CF_KEY_MAX counts the terminator, as every buffer that holds a key
       does: the longest key is one byte shorter. */
    return cfi_key_valid_n(key, len, (size_t)CF_KEY_MAX - 1u);
}

cf_bool cfi_key_valid_n(const char* key, size_t len, size_t limit) {
    size_t i;
    int    seg = 0;

    if (key == NULL || len == 0 || len > limit) {
        return CF_FALSE;
    }
    if (key[0] == '.' || key[len - 1] == '.') {
        return CF_FALSE;
    }
    for (i = 0; i < len; ++i) {
        char c = key[i];
        if (c == '.') {
            if (seg == 0) {
                return CF_FALSE; /* empty segment */
            }
            seg = 0;
            continue;
        }
        /* Canonical form is lowercase ASCII. Uppercase is rejected
           rather than folded, because folding is where two distinct
           keys quietly become one. */
        if (!cf_is_lower((unsigned char)c) && !cf_is_digit((unsigned char)c) &&
            c != '_' && c != '-') {
            return CF_FALSE;
        }
        ++seg;
    }
    return (seg > 0) ? CF_TRUE : CF_FALSE;
}
