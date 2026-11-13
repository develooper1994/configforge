/*
 * test_kinds.c - what kind of format a codec is, told to the program:
 * cf_codec_select finds codecs by their capability bits, cf_codec_kind
 * says them in words (D-97).
 */

#include <configforge/configforge.h>

#include <stddef.h>
#include <string.h>

#include "cf_test.h"

/* A build without cf_emit has no encoders: every codec only reads. */
#if CF_WITH_EMIT
#define RW "read and written"
#else
#define RW "read"
#endif

static int has_name(const cf_codec* const* list, size_t n, const char* name) {
    size_t i;
    for (i = 0; i < n; ++i) {
        if (list[i] != NULL && strcmp(list[i]->name, name) == 0) {
            return 1;
        }
    }
    return 0;
}

static void test_select(void) {
    const cf_codec* got[32] = {NULL};
    size_t          n, all, i;

    all = cf_codec_select(0, 0, got, 32);
    check(all == cf_codec_count(), "no bits asked for, no bits refused: every codec");
    for (i = 0; i < all; ++i) {
        check(got[i] == cf_codec_at(i), "in registry order");
    }

    n = cf_codec_select(CF_CODEC_BINARY, 0, got, 32);
#if CF_WITH_CBOR
    check(has_name(got, n, "cbor"), "binary: CBOR");
#endif
    check(!has_name(got, n, "json"), "binary: not JSON");
    for (i = 0; i < n; ++i) {
        check(got[i] != NULL && (got[i]->caps & CF_CODEC_BINARY) != 0,
              "each one chosen is binary");
    }

    n = cf_codec_select(0, CF_CODEC_BINARY, got, 32);
#if CF_WITH_JSON
    check(has_name(got, n, "json"), "not binary: JSON");
#endif
    check(!has_name(got, n, "cbor"), "not binary: not CBOR");

    n = cf_codec_select(CF_CODEC_BINARY, 0, NULL, 0);
    check(n == cf_codec_select(CF_CODEC_BINARY, 0, got, 32),
          "no room at all still counts them");
    check(cf_codec_select(CF_CODEC_SCHEMA, 0, got, 32) == 0,
          "none of the built-in codecs needs a schema");
    check(cf_codec_select(CF_CODEC_READ | CF_CODEC_WRITE, CF_CODEC_READ, got, 32) == 0,
          "a bit both asked for and refused matches nothing");

    if (all >= 2) {
        const cf_codec* one[1] = {NULL};
        size_t          m      = cf_codec_select(0, 0, one, 1);
        check(m == all, "a cap smaller than the matches still returns all of them");
        check(one[0] == cf_codec_at(0), "and fills only what the cap holds");
    }
}

static void test_kind(void) {
    char        buf[160];
    char        tiny[8];
    const char* json_kind = "text, self-describing, typed, arrays, " RW;
    size_t      n;

#if CF_WITH_JSON
    n = cf_codec_kind(cf_codec_by_name("json"), buf, sizeof buf);
    check_text(buf, json_kind, "JSON's kind");
    check(n == strlen(json_kind), "and its length");
    memset(tiny, 'x', sizeof tiny);
    n = cf_codec_kind(cf_codec_by_name("json"), tiny, sizeof tiny);
    check(n == strlen(json_kind), "a short buffer still reports the length needed");
    check_text(tiny, "text, s", "and holds what fits, ended");
    check(cf_codec_kind(cf_codec_by_name("json"), NULL, 0) == strlen(json_kind),
          "no buffer at all reports it too");
#else
    (void)json_kind;
    (void)tiny;
    (void)n;
#endif
#if CF_WITH_INI
    cf_codec_kind(cf_codec_by_name("ini"), buf, sizeof buf);
    check_text(buf, "text, self-describing, untyped, arrays of values, " RW,
               "INI's kind");
#endif
#if CF_WITH_CBOR
    cf_codec_kind(cf_codec_by_name("cbor"), buf, sizeof buf);
    check_text(buf, "binary, self-describing, typed, arrays, " RW, "CBOR's kind");
#endif
#if CF_WITH_PROPERTIES
    cf_codec_kind(cf_codec_by_name("properties"), buf, sizeof buf);
    check_text(buf, "text, self-describing, untyped, no arrays, read",
               "a reader only says so");
#endif
    buf[0] = 'x';
    check(cf_codec_kind(NULL, buf, sizeof buf) == 0 && buf[0] == '\0',
          "no codec: nothing, and an empty string");
}

int main(void) {
    test_select();
    test_kind();
    return cf_test_done();
}
