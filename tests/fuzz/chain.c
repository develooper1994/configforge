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

int LLVMFuzzerTestOneInput(const unsigned char* data, size_t size) {
    const cf_codec* src;
    const cf_codec* cur;
    cf_arena*       ar;
    cf_blob         in;
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
    if (ch_read(ar, &in, src, &err) == NULL) {
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
