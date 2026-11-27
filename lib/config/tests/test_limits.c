/*
 * test_limits.c - docs/Limits.md says what the build does (A-13).
 *
 * The page's numbers are generated from the sources and compared by
 * gate.limits; this is the other half: the page's rows against the
 * values the library was compiled with. It reads the page named by
 * argv[1], and
 *
 *   - in a build that keeps the default limits, checks the five rows of
 *     "Build limits" against the compiled values and against
 *     cf_build_limit();
 *   - in any other build (the `minimal` preset) says so and does not
 *     compare those rows;
 *   - always checks the decode budget's constants and the depth rule.
 *
 * The behaviour at a limit is held by the tests named on the page.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cf_test.h"

#include "configforge/cf_build_options.h"
#include "configforge/cf_explain.h"
#include "configforge/cf_flatten.h"

static char* page = NULL;

static int load(const char* path) {
    FILE*  f = fopen(path, "rb");
    long   n;
    size_t got;

    if (f == NULL) {
        return 0;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    page = (char*)malloc((size_t)n + 1);
    if (page == NULL) {
        fclose(f);
        return 0;
    }
    got = fread(page, 1, (size_t)n, f);
    fclose(f);
    page[got] = '\0';
    return 1;
}

/* The number in the first cell after "| `name` | " in the page; -1 when
   the row is not there. */
static long row(const char* name) {
    char        key[64];
    const char* at;

    sprintf(key, "| `%s` | ", name);
    at = strstr(page, key);
    if (at == NULL) {
        return -1;
    }
    return strtol(at + strlen(key), NULL, 10);
}

static void same(const char* name, long compiled) {
    char msg[96];
    long stated = row(name);

    sprintf(msg, "limits: the page's row %s says %ld, the build has %ld", name, stated,
            compiled);
    check(stated == compiled, msg);
}

static void test_build_rows(void) {
    long key = row("CF_KEY_MAX"), message = row("CF_MESSAGE_MAX"),
         path = row("CF_PATH_MAX"), depth = row("CF_MAX_DEPTH");

    check(key > 0 && message > 0 && path > 0 && depth > 0,
          "limits: the page has the four build-limit rows");
    check(row("CF_STACK_BUDGET") >= 0, "limits: the page has the stack-budget row");
#ifndef CF_LIMITS_DEFAULT
    printf("limits: not a default build, page not compared\n");
    return;
#else
    same("CF_KEY_MAX", (long)cf_build_limit("key"));
    same("CF_MESSAGE_MAX", (long)cf_build_limit("message"));
    same("CF_PATH_MAX", (long)cf_build_limit("path"));
    same("CF_MAX_DEPTH", (long)cf_build_limit("depth"));
    same("CF_SOURCE_MAX", (long)cf_build_limit("source"));
#endif
}

static void test_fixed(void) {
    cf_error err = CF_ERROR_INIT;

    same("CF_SOURCE_MAX", (long)CF_SOURCE_MAX);
    check(sizeof(err.source) == (size_t)CF_SOURCE_MAX,
          "limits: CF_SOURCE_MAX is the size of cf_error's source");
    same("CF_MAX_SOURCES", (long)CF_MAX_SOURCES);
    same("CF_MAX_OPEN_PREFIXES", (long)CF_MAX_OPEN_PREFIXES);
    same("CF_EXPAND_MAX", (long)CF_EXPAND_MAX);
    same("CF_EXPAND_TOTAL_MAX", (long)CF_EXPAND_TOTAL_MAX);
}

static void test_budget(void) {
    char want[128];

    same("CF_DECODE_FACTOR", (long)CF_DECODE_FACTOR);
    same("CF_DECODE_FLOOR", (long)CF_DECODE_FLOOR);
    same("CF_DECODE_KEY_COST", (long)CF_DECODE_KEY_COST);
    check(cf_decode_budget(0) == (size_t)CF_DECODE_FLOOR,
          "limits: an empty document may make the floor");
    check(cf_decode_budget(1048576) == (size_t)64 * 1048576 + 1048576,
          "limits: 1 MiB may make 64 x 1 MiB and the floor");
    sprintf(want, "= %lu bytes", (unsigned long)cf_decode_budget(1048576));
    has(page, want, "limits: the page's 1 MiB row is the budget of 1 MiB");
    has(page, "| 1 MiB | 65 MiB |", "limits: the page's 1 MiB row says 65 MiB");
}

static void test_depth(void) {
    char want[96];

    if (row("CF_MAX_DEPTH") != (long)CF_MAX_DEPTH) {
        return; /* another build's depth: the page states the default */
    }
    sprintf(want, "`CF_MAX_DEPTH` %lu allows %lu levels",
            (unsigned long)cf_build_limit("depth"),
            (unsigned long)cf_build_limit("depth") - 1);
    has(page, want, "limits: the depth rule, as cf_build_limit(\"depth\") has it");
}

/* K-11a: the open-prefix limit is an option (cf_limit_options), with the
   old value, CF_MAX_OPEN_PREFIXES, as its default. */

/* Opens p0 .. p(n-1); how many were accepted before the first refusal. */
static int open_many(cf_builder* b, int n) {
    int i;

    for (i = 0; i < n; ++i) {
        char p[16];

        sprintf(p, "p%d", i);
        if (!cf_builder_open(b, p)) {
            break;
        }
    }
    return i;
}

/* The builder is created and freed; what the first failure said. */
static void create_refused(cf_builder* b, const char* text, const char* what) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = cf_builder_create(b, &err);

    check(c == NULL && err.code == CF_E_DEFINITION && strstr(err.message, text) != NULL,
          what);
    cf_config_free(c);
}

static void create_accepted(cf_builder* b, const char* what) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = cf_builder_create(b, &err);

    check(c != NULL, what);
    cf_config_free(c);
}

/* A source of the test's own that accepts a write, for the write path. */
static cf_bool local_load(const cf_source* s, cf_keyspace* ks) {
    (void)s;
    (void)ks;
    return CF_TRUE;
}

static cf_bool local_write(const cf_source* s, const cf_write_item* items, size_t count,
                           const char* meta, cf_error* err) {
    (void)s;
    (void)items;
    (void)count;
    (void)meta;
    (void)err;
    return CF_TRUE;
}

static const cf_source_vtable local_vt = {
    sizeof(cf_source_vtable), "local", local_load, NULL, NULL, local_write};

static cf_config* with_twenty_prefixes(cf_error* err) {
    cf_builder*      b = cf_build(NULL);
    cf_limit_options o;
    cf_source        s;

    cf_limit_options_init(&o);
    o.max_open_prefixes = 20;
    check(cf_builder_limits(b, &o) && open_many(b, 20) == 20,
          "open prefixes: twenty are accepted under a limit of twenty");
    memset(&s, 0, sizeof s);
    s.vt   = &local_vt;
    s.name = "local";
    s.rank = CF_RANK_USER_FILE;
    cf_builder_source(b, &s);
    cf_builder_writable(b, "local");
    cf_builder_text(b, "app.id", "x", 0);
    cf_builder_unknown(b, CF_UNKNOWN_REJECT);
    return cf_builder_create(b, err);
}

static void test_open_prefixes(void) {
    cf_error         err = CF_ERROR_INIT;
    cf_limit_options o, o2;
    cf_builder*      b;
    cf_config*       c;
    cf_config*       d;
    cf_write_item    item;

    /* The init function writes the size and the old value. */
    memset(&o, 0xAB, sizeof o);
    cf_limit_options_init(&o);
    check(o.size == sizeof o && o.max_open_prefixes == (size_t)CF_MAX_OPEN_PREFIXES,
          "open prefixes: the default is CF_MAX_OPEN_PREFIXES");
    cf_limit_options_init(NULL); /* does nothing */

    /* No call: the default; the ninth is refused. */
    b = cf_build(NULL);
    check(open_many(b, 9) == 8, "open prefixes: the ninth is refused by default");
    create_refused(b, "more than 8 open prefixes", "open prefixes: and says so");

    /* NULL is the default, too. */
    b = cf_build(NULL);
    check(cf_builder_limits(b, NULL) && open_many(b, 9) == 8,
          "open prefixes: NULL options are the default");
    create_refused(b, "more than 8 open prefixes", "open prefixes: NULL, and says so");

    /* A raised limit. */
    b                   = cf_build(NULL);
    o.max_open_prefixes = 20;
    check(cf_builder_limits(b, &o) && open_many(b, 21) == 20,
          "open prefixes: twenty accepted, the 21st refused");
    create_refused(b, "more than 20 open prefixes", "open prefixes: and says 20");

    /* 0 is unlimited. */
    b                   = cf_build(NULL);
    o.max_open_prefixes = 0;
    check(cf_builder_limits(b, &o) && open_many(b, 100) == 100,
          "open prefixes: 0 is unlimited");
    create_accepted(b, "open prefixes: a hundred prefixes make a configuration");

    /* A limit below the prefixes already opened is refused, and says so. */
    b                   = cf_build(NULL);
    o.max_open_prefixes = 20;
    check(cf_builder_limits(b, &o) && open_many(b, 10) == 10,
          "open prefixes: ten opened");
    o.max_open_prefixes = 5;
    check(!cf_builder_limits(b, &o), "open prefixes: a limit below the ten is refused");
    create_refused(b, "more than 5 open prefixes", "open prefixes: and says 5");

    /* So is the default, when more than eight are held. */
    b                   = cf_build(NULL);
    o.max_open_prefixes = 0;
    check(cf_builder_limits(b, &o) && open_many(b, 9) == 9 &&
              !cf_builder_limits(b, NULL),
          "open prefixes: NULL is the default, and nine are over it");
    create_refused(b, "more than 8 open prefixes", "open prefixes: and says 8");

    /* A builder and a size the call cannot use. */
    check(!cf_builder_limits(NULL, &o), "open prefixes: no builder");
    b       = cf_build(NULL);
    o2      = o;
    o2.size = sizeof(size_t); /* below the end of max_open_prefixes */
    check(!cf_builder_limits(b, &o2), "open prefixes: a size too small is refused");
    create_accepted(b, "open prefixes: and the builder is not failed by it");

    /* A derived configuration and a write keep the limit. */
    c = with_twenty_prefixes(&err);
    check(c != NULL, "open prefixes: twenty prefixes make a configuration");
    if (c != NULL) {
        d = cf_derive(c, "more", NULL, 0, &err);
        check(d != NULL, "open prefixes: a derived configuration keeps the limit");
        cf_config_free(d);

        memset(&item, 0, sizeof item);
        item.key = "p15.x";
        item.op  = CF_WRITE_SET;
        cf_value_text(&item.value, "1", 1);
        check(cf_write(c, "local", &item, 1, "test", &err),
              "open prefixes: a write under the 16th open prefix is kept");
        item.key = "q.x";
        check(!cf_write(c, "local", &item, 1, "test", &err),
              "open prefixes: and one under none is not");
    }
    cf_config_free(c);
}

/* K-11a: CF_MAX_SOURCES is the window of the summary's counters, not a
   limit. Forty sources, every one named: the sources after the 32nd were
   once left out of the notes. The last two supply the same key, the
   better-ranked one wins, and the other is told it won none. */
static void test_explain_sources(void) {
    enum { N = 40 };
    static char             names[N][8];
    static cf_source        src[N];
    static const cf_source* list[N];
    static const cf_pair    both[] = {{"shared.key", "v"}};
    static char             buf[8192];
    cf_arena*               ar = cf_arena_new(NULL, 0);
    cf_keyspace             ks;
    cf_source_view          sv;
    char                    want[64];
    int                     i;

    cf_ks_init(&ks, ar, 8);
    for (i = 0; i < N; ++i) {
        sprintf(names[i], "s%d", i);
        cf_source_memory(&src[i], names[i], i, both, (i >= N - 2) ? 1 : 0);
        cf_source_load(&src[i], &ks);
        list[i] = &src[i];
    }
    cf_ks_resolve(&ks);
    cf_ks_infer_untyped(&ks);
    sv.items = list;
    sv.count = N;

    (void)cf_ks_inspect(&ks, &sv, buf, sizeof buf);
    for (i = 0; i < N - 2; ++i) {
        sprintf(want, "note: source 's%d' supplied nothing", i);
        has(buf, want, "explain: every source of forty is named");
    }
    has(buf, "note: source 's39' supplied 1 values and won none",
        "explain: the 40th source's shadowed value is told");
    check(strstr(buf, "source 's38'") == NULL, "explain: the winner gets no note");
    cf_arena_free(ar);
}

int main(int argc, char** argv) {
    if (argc < 2 || !load(argv[1])) {
        printf("limits: the page (docs/Limits.md) is the one argument\n");
        return 1;
    }
    test_build_rows();
    test_fixed();
    test_budget();
    test_depth();
    test_explain_sources();
    test_open_prefixes();
    free(page);
    return cf_test_done();
}
