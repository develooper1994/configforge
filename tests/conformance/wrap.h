/*
 * wrap.h - a conformance suite's document as a configuration: the one
 * static function conform.c and tests/codec/pairs.c share. C89.
 */

#ifndef CONFIGFORGE_TESTS_CONFORMANCE_WRAP_H
#define CONFIGFORGE_TESTS_CONFORMANCE_WRAP_H

#include "configforge/cf_serialize.h"

#include <string.h>

/* JSON's grammar allows any value at the root, and the configuration
   model, enforced by the reader, a table only; a suite's lone value or
   array is measured as the member of one, `{"v": ...}`, which keeps an
   invalid document invalid. JSON5's is always wrapped: a comment may
   come before the root, and one may end the last line, so the brace
   closing the wrapper goes on a line of its own. A CBOR vector is mostly
   a lone value and a configuration a map: it is measured as the value
   of the member "v" (A1 61 76 before it), which keeps a malformed item
   malformed - the map's value is cut short, or holds a stray break.
   `blob` becomes the wrapped bytes, named `name`, in a static buffer of
   1 MiB; false when they do not fit. Other formats are left as they
   are. */
static cf_bool conform_wrap(const char* format, const char* name, cf_blob* blob) {
    static char wrapped[1 << 20];

    if (strcmp(format, "json5") == 0) {
        if (blob->n + 8 > sizeof wrapped) {
            return CF_FALSE;
        }
        memcpy(wrapped, "{\"v\":", 5);
        memcpy(wrapped + 5, blob->p, blob->n);
        memcpy(wrapped + 5 + blob->n, "\n}", 2);
        cf_blob_mem(blob, wrapped, blob->n + 7, name);
    } else if (strcmp(format, "cbor") == 0) {
        if (blob->n + 3 > sizeof wrapped) {
            return CF_FALSE;
        }
        memcpy(wrapped, "\xA1\x61\x76", 3);
        memcpy(wrapped + 3, blob->p, blob->n);
        cf_blob_mem(blob, wrapped, blob->n + 3, name);
    } else if (strcmp(format, "json") == 0) {
        size_t k = 0;

        while (k < blob->n && (blob->p[k] == ' ' || blob->p[k] == '\t' ||
                               blob->p[k] == '\n' || blob->p[k] == '\r')) {
            ++k;
        }
        if (k >= blob->n || blob->p[k] != '{') {
            if (blob->n + 7 > sizeof wrapped) {
                return CF_FALSE;
            }
            memcpy(wrapped, "{\"v\":", 5);
            memcpy(wrapped + 5, blob->p, blob->n);
            wrapped[5 + blob->n] = '}';
            cf_blob_mem(blob, wrapped, blob->n + 6, name);
        }
    }
    return CF_TRUE;
}

#endif /* CONFIGFORGE_TESTS_CONFORMANCE_WRAP_H */
