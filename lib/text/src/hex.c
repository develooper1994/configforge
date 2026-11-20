/*
 * hex.c - one hexadecimal digit, read once (D-102). C89.
 */

#include "configforge/cf_text.h"

int cf_text_hex_value(int c) {
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
