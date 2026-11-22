/*
 * chain.c - a random document through a random chain of the six writing
 * formats, each hop held to the table of what a format can hold (D-109).
 *
 * pairs.c does the same for the documents the project knows; this is
 * libFuzzer's, so the input that reaches a new case is kept. The input:
 * byte 0 picks the source format - ini json json5 toml yaml cbor xml
 * properties - by its value modulo 8 (a format the build lacks ends the
 * run); byte 1 the number of hops, 1 to 4 (modulo 4, plus 1); then one
 * byte a hop picking the writer from ini json json5 toml yaml cbor
 * (modulo 6); the rest is the document, which the source's reader must
 * accept, else the run ends. A hop that is not what the table predicts
 * aborts, so libFuzzer keeps the input. C89, public headers only.
 */

#undef CF_INTERNAL_API
#define CF_INTERNAL_API 0

#include "chain.h"

#include <stdlib.h>

int LLVMFuzzerTestOneInput(const unsigned char* data, size_t size);

static const char* const g_sources[] = {"ini",  "json", "json5", "toml",
                                        "yaml", "cbor", "xml",   "properties"};
static const char* const g_writers[] = {"ini", "json", "json5", "toml", "yaml", "cbor"};

/* Whether `p` is well-formed UTF-8 (overlongs, surrogates and anything
   past U+10FFFF refused). */
static cf_bool fz_utf8(const char* p, size_t n) {
    size_t i = 0;

    while (i < n) {
        unsigned char c = (unsigned char)p[i];
        size_t        k;
        unsigned long cp;

        if (c < 0x80u) {
            ++i;
            continue;
        }
        if (c >= 0xC2u && c <= 0xDFu) {
            k  = 1;
            cp = c & 0x1Fu;
        } else if (c >= 0xE0u && c <= 0xEFu) {
            k  = 2;
            cp = c & 0x0Fu;
        } else if (c >= 0xF0u && c <= 0xF4u) {
            k  = 3;
            cp = c & 0x07u;
        } else {
            return CF_FALSE;
        }
        if (i + k >= n) {
            return CF_FALSE;
        }
        {
            size_t j;

            for (j = 1; j <= k; ++j) {
                unsigned char d = (unsigned char)p[i + j];

                if ((d & 0xC0u) != 0x80u) {
                    return CF_FALSE;
                }
                cp = (cp << 6) | (d & 0x3Fu);
            }
        }
        if ((k == 2 && (cp < 0x800ul || (cp >= 0xD800ul && cp <= 0xDFFFul))) ||
            (k == 3 && (cp < 0x10000ul || cp > 0x10FFFFul))) {
            return CF_FALSE;
        }
        i += k + 1u;
    }
    return CF_TRUE;
}

/* The documents this target holds the table to: names that are text
   without a control byte, and texts that are UTF-8. Outside it, found by
   this target and not settled by D-109: a reader that passes invalid UTF-8
   on (D-108 makes every reader refuse it), and a name with a control byte
   or a NUL, which the YAML writer writes in a form its reader refuses. */
static cf_bool fz_clean(const ch_node* n) {
    const ch_node* c;
    size_t         i;

    if (n->name != NULL) {
        if (!fz_utf8(n->name, n->nlen)) {
            return CF_FALSE;
        }
        for (i = 0; i < n->nlen; ++i) {
            if ((unsigned char)n->name[i] < 0x20u || n->name[i] == 0x7f) {
                return CF_FALSE;
            }
        }
    }
    if (n->kind == CH_SCALAR && n->value.type == CF_TYPE_TEXT &&
        !fz_utf8(n->value.u.t.p, n->value.u.t.n)) {
        return CF_FALSE;
    }
    for (c = n->first; c != NULL; c = c->next) {
        if (!fz_clean(c)) {
            return CF_FALSE;
        }
    }
    return CF_TRUE;
}

int LLVMFuzzerTestOneInput(const unsigned char* data, size_t size) {
    const cf_codec* src;
    const cf_codec* cur;
    cf_arena*       ar;
    cf_blob         in;
    const ch_tree*  tree;
    cf_error        err = CF_ERROR_INIT;
    size_t          hops;
    size_t          h;

    if (size < 2u) {
        return 0;
    }
    src  = cf_codec_by_name(g_sources[data[0] % 8u]);
    hops = (size_t)(data[1] % 4u) + 1u;
    if (src == NULL || !cf_codec_can_read(src) || size < 2u + hops) {
        return 0;
    }
    ar = cf_arena_new(NULL, 0);
    if (ar == NULL) {
        return 0;
    }
    cf_blob_mem(&in, (const char*)data + 2u + hops, size - 2u - hops, "chain");
    tree = ch_read(ar, &in, src, &err);
    if (tree == NULL || !fz_clean(tree)) {
        cf_arena_free(ar);
        return 0;
    }
    cur = src;
    for (h = 0; h < hops; ++h) {
        const cf_codec* to = cf_codec_by_name(g_writers[data[2u + h] % 6u]);
        ch_hop          hop;
        int             rc;

        if (to == NULL || to->encoder == NULL) {
            break;
        }
        rc = ch_hop_run(ar, &in, cur, to, &hop);
        if (rc == CH_BOUND) {
            break;
        }
        if (rc != CH_OK) {
            fprintf(stderr, "chain: hop %lu %s -> %s: %s\n", (unsigned long)h + 1u,
                    cur->name, to->name, hop.why);
            abort();
        }
        in  = hop.out;
        cur = to;
    }
    cf_arena_free(ar);
    return 0;
}
