/*
 * test_holds.c - what each format can hold (D-109), held to the codecs.
 *
 * cf_format_holds is the one table of what a format's syntax carries. The
 * converter, the pairs test and the page in docs/Formats.md read it, so
 * this test is where it is held to what each codec says about itself: a
 * bit that contradicts the codec's caps, or a writer added without a row,
 * fails here. It includes cf_serialize.h and nothing above it.
 */

/* The tests' directory defines CF_INTERNAL_API as 1; a program does not. */
#undef CF_INTERNAL_API
#define CF_INTERNAL_API 0

#include <configforge/cf_serialize.h>

#include <stdio.h>
#include <string.h>

static int g_failed;

static void check(int ok, const char* what) {
    if (!ok) {
        printf("FAIL: %s\n", what);
        g_failed = 1;
    }
}

static const char* const g_writers[] = {"ini", "json", "json5", "toml", "yaml", "cbor"};

#define WRITERS (sizeof g_writers / sizeof g_writers[0])

static int is_writer(const char* name) {
    size_t i;

    for (i = 0; i < WRITERS; ++i) {
        if (strcmp(g_writers[i], name) == 0) {
            return 1;
        }
    }
    return 0;
}

/* Every bit of `mask` is set in `holds`. */
static int has_all(unsigned holds, unsigned mask) {
    return (holds & mask) == mask;
}

static void test_known(void) {
    size_t   i;
    unsigned untouched = 0x5A5Au;

    for (i = 0; i < WRITERS; ++i) {
        unsigned holds = 0;
        char     what[96];

        sprintf(what, "cf_format_holds knows %s", g_writers[i]);
        check(cf_format_holds(g_writers[i], &holds) == CF_TRUE, what);
        check((holds & ~(CF_HOLDS_ALL | CF_REFUSES_NULL)) == 0, "no bit beyond the table's");
        check(holds != 0, "a format holds something");
    }
    check(cf_format_holds("xml", &untouched) == CF_FALSE && untouched == 0x5A5Au,
          "xml has no writer: false, holds untouched");
    check(cf_format_holds("properties", &untouched) == CF_FALSE && untouched == 0x5A5Au,
          "properties has no writer: false, holds untouched");
    check(cf_format_holds("nosuch", &untouched) == CF_FALSE && untouched == 0x5A5Au,
          "an unknown name: false, holds untouched");
}

/* The table says what the codec says about itself. */
static void test_caps(void) {
    size_t i;

    for (i = 0; i < WRITERS; ++i) {
        const cf_codec* c = cf_codec_by_name(g_writers[i]);
        unsigned        holds;
        char            what[128];

        if (c == NULL) {
            printf("%s is not compiled in; its row is the table's alone\n",
                   g_writers[i]);
            continue;
        }
        holds = 0;
        check(cf_format_holds(g_writers[i], &holds) == CF_TRUE,
              "a compiled-in codec has a row");
        sprintf(what, "%s: arrays of values iff the caps say an array", g_writers[i]);
        check(((c->caps & (CF_CODEC_ARRAYS | CF_CODEC_SCALAR_ARRAYS)) != 0u) ==
                  ((holds & CF_HOLDS_ARRAYS) != 0u),
              what);
        sprintf(what, "%s: empty, table and nested arrays iff CF_CODEC_ARRAYS",
                g_writers[i]);
        if ((c->caps & CF_CODEC_ARRAYS) != 0u) {
            check(has_all(holds, CF_HOLDS_EMPTY_ARRAY | CF_HOLDS_TABLE_ARRAYS |
                                     CF_HOLDS_NESTED_ARRAYS),
                  what);
        } else {
            check((holds & (CF_HOLDS_EMPTY_ARRAY | CF_HOLDS_TABLE_ARRAYS |
                            CF_HOLDS_NESTED_ARRAYS)) == 0u,
                  what);
        }
        sprintf(what, "%s: integers, reals and booleans iff CF_CODEC_TYPED",
                g_writers[i]);
        if ((c->caps & CF_CODEC_TYPED) != 0u) {
            check(has_all(holds, CF_HOLDS_INT | CF_HOLDS_REAL | CF_HOLDS_BOOL), what);
        } else {
            check((holds & (CF_HOLDS_INT | CF_HOLDS_REAL | CF_HOLDS_BOOL)) == 0u, what);
        }
        sprintf(what, "%s: CF_REFUSES_NULL only without CF_HOLDS_NULL", g_writers[i]);
        check((holds & CF_REFUSES_NULL) == 0u || (holds & CF_HOLDS_NULL) == 0u, what);
    }
}

/* A writer the table has no row for fails here. */
static void test_every_writer(void) {
    size_t i;

    for (i = 0; i < cf_codec_count(); ++i) {
        const cf_codec* c = cf_codec_at(i);

        if (c->encoder != NULL && (c->caps & CF_CODEC_WRITE) != 0u) {
            char what[128];

            sprintf(what, "the codec '%s' writes and is one of the six", c->name);
            check(is_writer(c->name), what);
        }
    }
}

int main(void) {
    test_known();
    test_caps();
    test_every_writer();
    if (g_failed) {
        return 1;
    }
    printf("ok\n");
    return 0;
}
