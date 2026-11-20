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
        check((holds & ~(unsigned)(CF_HOLDS_ALL | CF_REFUSES_NULL)) == 0u, "no bit beyond the table's");
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

/* ------------------------------------------------------------------ *
 * The limits: each number is what the format's reader does, read at the
 * limit and one past it.
 * ------------------------------------------------------------------ */

static char   g_doc[4096];
static size_t g_len;

static void put(const char* s) {
    size_t n = strlen(s);

    if (g_len + n < sizeof g_doc) {
        memcpy(g_doc + g_len, s, n);
        g_len += n;
        g_doc[g_len] = '\0';
    }
}

static void rep(const char* s, size_t times) {
    size_t i;

    for (i = 0; i < times; ++i) {
        put(s);
    }
}

static void put_name(size_t n) {
    size_t i;

    for (i = 0; i < n && g_len + 1u < sizeof g_doc; ++i) {
        g_doc[g_len++] = 'n';
    }
    g_doc[g_len] = '\0';
}

static cf_bool on_open(void* ctx, const char* name, size_t n) {
    (void)ctx;
    (void)name;
    (void)n;
    return CF_TRUE;
}

static cf_bool on_end(void* ctx) {
    (void)ctx;
    return CF_TRUE;
}

static cf_bool on_scalar(void* ctx, const char* name, size_t n, const cf_value* v,
                         cf_bool textual, const cf_loc* at) {
    (void)ctx;
    (void)name;
    (void)n;
    (void)v;
    (void)textual;
    (void)at;
    return CF_TRUE;
}

static char* on_text(void* ctx, size_t n) {
    return (char*)cf_arena_alloc((cf_arena*)ctx, n);
}

static void* on_work(void* ctx, size_t n) {
    return cf_arena_alloc((cf_arena*)ctx, n);
}

/* Whether `c` reads g_doc. */
static int reads(const cf_codec* c) {
    cf_arena*   ar = cf_arena_new(NULL, 0);
    cf_doc_sink sink;
    cf_blob     in;
    cf_error    err = CF_ERROR_INIT;
    int         ok;

    if (ar == NULL) {
        return 0;
    }
    memset(&sink, 0, sizeof sink);
    sink.size         = sizeof sink;
    sink.begin_table  = on_open;
    sink.begin_array  = on_open;
    sink.scalar       = on_scalar;
    sink.end          = on_end;
    sink.ctx          = ar;
    sink.reserve_text = on_text;
    sink.reserve_work = on_work;
    cf_blob_mem(&in, g_doc, g_len, "limit");
    ok = cf_codec_decode(c, &in, &sink, &err) ? 1 : 0;
    cf_arena_free(ar);
    return ok;
}

/* `count` tables one inside another below the document, in the syntax
   of `format`, the last holding a value. */
static void deep(const char* format, size_t count) {
    size_t i;

    g_len = 0;
    g_doc[0] = '\0';
    if (strcmp(format, "json") == 0 || strcmp(format, "json5") == 0) {
        put("{");
        rep("\"a\":{", count);
        put("\"k\":1");
        rep("}", count + 1u);
    } else if (strcmp(format, "toml") == 0) {
        put("[a");
        rep(".a", count - 1u);
        put("]\nk = 1\n");
    } else if (strcmp(format, "yaml") == 0) {
        for (i = 0; i < count; ++i) {
            rep("  ", i);
            put("a:\n");
        }
        rep("  ", count);
        put("k: 1\n");
    } else if (strcmp(format, "cbor") == 0) {
        for (i = 0; i < count; ++i) {
            put("\xa1\x61\x61");
        }
        put("\xa1\x61\x6b\x01");
    } else if (strcmp(format, "xml") == 0) {
        put("<r>");
        rep("<a>", count);
        put("<k>1</k>");
        rep("</a>", count);
        put("</r>");
    }
}

/* A header of `plen` bytes of joined path, as one name or, with `dotted`,
   as `a.` and one name. */
static void header(size_t plen, int dotted) {
    g_len = 0;
    g_doc[0] = '\0';
    put("[");
    if (dotted) {
        put("a.");
        put_name(plen - 2u);
    } else {
        put_name(plen);
    }
    put("]\nk = 1\n");
}

static void test_limits(void) {
    static const char* const names[] = {"ini",  "json", "json5", "toml",
                                        "yaml", "cbor", "xml"};
    size_t                   i;

    for (i = 0; i < sizeof names / sizeof names[0]; ++i) {
        const cf_codec* c = cf_codec_by_name(names[i]);
        size_t          max_path  = 0;
        size_t          max_depth = 0;
        char            what[128];

        check(cf_format_limits(names[i], &max_path, &max_depth) == CF_TRUE,
              "cf_format_limits knows the format");
        if (c == NULL || !cf_codec_can_read(c)) {
            continue;
        }
        if (max_depth != (size_t)-1 && strcmp(names[i], "ini") != 0) {
            deep(names[i], max_depth);
            sprintf(what, "%s reads %lu tables below the document", names[i],
                    (unsigned long)max_depth);
            check(reads(c), what);
            deep(names[i], max_depth + 1u);
            sprintf(what, "%s refuses %lu tables below the document", names[i],
                    (unsigned long)(max_depth + 1u));
            check(!reads(c), what);
        }
        if (max_path != (size_t)-1 &&
            (strcmp(names[i], "ini") == 0 || strcmp(names[i], "toml") == 0)) {
            int dotted;

            for (dotted = 0; dotted < 2; ++dotted) {
                header(max_path, dotted);
                sprintf(what, "%s reads a header path of %lu bytes (dotted %d)", names[i],
                        (unsigned long)max_path, dotted);
                check(reads(c), what);
            }
        }
    }
    check(cf_format_limits("nosuch", &i, &i) == CF_FALSE, "an unknown name has no limits");
}

int main(void) {
    test_known();
    test_caps();
    test_every_writer();
    test_limits();
    if (g_failed) {
        return 1;
    }
    printf("ok\n");
    return 0;
}
