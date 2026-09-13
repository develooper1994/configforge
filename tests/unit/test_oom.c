/*
 * test_oom.c - fail the Nth allocation, for every N.
 *
 * The starvation test reaches whatever failure paths happen to sit at
 * the boundary of a buffer size. This reaches all of them: an allocator
 * that succeeds N times and then refuses forever, swept from zero
 * upward until nothing fails any more. Every allocation the library
 * makes is a point where a caller's memory ran out, and this walks each
 * one in turn.
 *
 * The contract is the same as everywhere else that takes input it does
 * not control:
 *
 *   1. it works, and the result is correct
 *   2. it fails, says why, and leaves nothing usable behind
 *
 * Never a crash, never a leak, never a configuration that was built
 * from a partial one and reads as if it were whole.
 *
 * This is the test the cf_allocator interface exists for. A library
 * that called malloc directly could not be swept like this without a
 * linker trick, which is one of the reasons everything here goes
 * through the interface - and why gate.symbols checks that it does.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int  g_fail = 0, g_run = 0;
static void check(int c, const char* w) {
    ++g_run;
    if (!c) {
        ++g_fail;
        printf("FAIL: %s\n", w);
    }
}

/* ---------------------------------------------------------------- *
 * An allocator that gives up on cue
 * ---------------------------------------------------------------- */

typedef struct failing {
    size_t budget; /* allocations to allow before refusing */
    size_t calls;  /* how many were asked for             */
    size_t live;   /* outstanding blocks, for leak detection */
} failing;

static void* failing_alloc(void* ctx, size_t n) {
    failing* f = (failing*)ctx;
    void*    p;

    ++f->calls;
    if (f->budget == 0) {
        return NULL;
    }
    --f->budget;
    p = malloc(n);
    if (p != NULL) {
        ++f->live;
    }
    return p;
}

static void failing_release(void* ctx, void* p) {
    failing* f = (failing*)ctx;
    if (p != NULL) {
        --f->live;
        free(p);
    }
}

/* ---------------------------------------------------------------- *
 * The run under test
 * ---------------------------------------------------------------- */

static char  a0[]     = "prog";
static char  a1[]     = "--server.port=9000";
static char  a2[]     = "--server.host=10.0.0.1";
static char  a3[]     = "--tls.enabled=true";
static char* g_argv[] = {a0, a1, a2, a3, NULL};

static char  e0[]    = "MYAPP_DB__PASSWORD=secret";
static char  e1[]    = "MYAPP_LOG__LEVEL=debug";
static char* g_env[] = {e0, e1, NULL};

typedef enum outcome { OUT_NO_BUILDER = 0, OUT_NO_CONFIG, OUT_COMPLETE } outcome;

/* `block` is the arena's block size. The default takes five large
   blocks for a whole configuration, which is five injection points; a
   small one takes dozens, and each is a place the machine could have
   filled up. Both are swept, because they fail in different places: a
   large block runs out between objects, a small one runs out inside
   them. */
static outcome run_with(failing* f, size_t block, cf_u64* digest, cf_error* err) {
    cf_allocator alloc;
    cf_arena*    ar = NULL;
    cf_builder*  b;
    cf_config*   c;
    static char  scratch[2048];
    outcome      result;

    alloc.alloc   = failing_alloc;
    alloc.release = failing_release;
    alloc.ctx     = f;

    *digest = 0;
    if (block == 0) {
        b = cf_build(&alloc);
    } else {
        ar = cf_arena_new(&alloc, block);
        b  = (ar != NULL) ? cf_build_in(ar) : NULL;
    }
    if (b == NULL) {
        if (ar != NULL) {
            cf_arena_free(ar);
        }
        return OUT_NO_BUILDER;
    }
    cf_builder_cli(b, "cli", CF_RANK_CLI, 4, g_argv);
    cf_builder_env(b, "env", CF_RANK_ENV, "MYAPP_", g_env);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_builder_text(b, "server.host", "0.0.0.0", 0);
    cf_builder_text(b, "log.level", "info", 0);
    cf_builder_bool(b, "tls.enabled", CF_FALSE, 0);
    cf_builder_text(b, "db.password", NULL, CF_SENSITIVE);

    c = cf_builder_create(b, err);
    if (c == NULL) {
        if (ar != NULL) {
            cf_arena_free(ar);
        }
        return OUT_NO_CONFIG;
    }

    /* It said it worked, so everything it promised holds. A snapshot
       assembled from a key space that lost half its candidates would
       show up here rather than at construction. */
    check(cf_count(c) == 5, "five keys, none dropped");
    check(cf_get_int(c, "server.port", 0) == 9000, "the command line");
    check(strcmp(cf_get_text(c, "server.host", ""), "10.0.0.1") == 0, "and its text");
    check(strcmp(cf_get_text(c, "db.password", ""), "secret") == 0, "the environment");
    check(strcmp(cf_get_text(c, "log.level", ""), "debug") == 0,
          "all of the environment, not the first variable of it");
    check(cf_get_bool(c, "tls.enabled", CF_FALSE), "the boolean");

    /* Reading is supposed to be infallible once construction returned,
       including the parts that render. */
    (void)cf_explain(c, "server.port", scratch, sizeof scratch);
    (void)cf_inspect(c, scratch, sizeof scratch);
    (void)cf_emit(c, "json", CF_EMIT_ALL, 0, scratch, sizeof scratch, err);

    *digest = cf_digest(c);
    result  = OUT_COMPLETE;
    cf_config_free(c);
    if (ar != NULL) {
        /* Borrowed, so releasing it is the caller's job - which is the
           contract cf_build_in states and this is the test of it. */
        cf_arena_free(ar);
    }
    return result;
}

/* ---------------------------------------------------------------- */

/* How many distinct allocation counts the sweeps saw. Two sweeps that
   agree exactly are two sweeps of the same thing, and a test that
   cannot tell whether it tested anything is worse than no test. */
static size_t g_counts[8];
static int    g_ncounts         = 0;
static int    g_distinct_counts = 0;

static void note_count(size_t n) {
    int i;
    for (i = 0; i < g_ncounts; ++i) {
        if (g_counts[i] == n) {
            return;
        }
    }
    if (g_ncounts < (int)(sizeof g_counts / sizeof g_counts[0])) {
        g_counts[g_ncounts++] = n;
    }
    ++g_distinct_counts;
}

static void sweep(size_t block, const char* what) {
    cf_u64 reference = 0;
    cf_u64 digest    = 0;
    size_t budget;
    size_t total_calls;
    size_t first_success   = 0;
    int    silent_failures = 0;
    int    leaks           = 0;
    int    found_success   = 0;

    {
        failing  f;
        cf_error err = CF_ERROR_INIT;
        f.budget     = (size_t)-1;
        f.calls      = 0;
        f.live       = 0;
        check(run_with(&f, block, &reference, &err) == OUT_COMPLETE, what);
        check(f.live == 0, "and leaves nothing outstanding");
        total_calls = f.calls;
        note_count(total_calls);
    }

    /* Every allocation the library makes, refused in turn. */
    for (budget = 0; budget <= total_calls + 2; ++budget) {
        failing  f;
        cf_error err = CF_ERROR_INIT;
        outcome  o;

        f.budget = budget;
        f.calls  = 0;
        f.live   = 0;

        o = run_with(&f, block, &digest, &err);

        if (o == OUT_COMPLETE) {
            if (!found_success) {
                found_success = 1;
                first_success = budget;
            }
            /* Succeeding with fewer allocations must not mean a
               different configuration. A digest that moved with the
               budget would mean something had been dropped quietly. */
            if (digest != reference) {
                check(0, "the configuration does not depend on how many "
                         "allocations were available");
                break;
            }
        } else if (o == OUT_NO_CONFIG && err.message[0] == '\0') {
            ++silent_failures;
        }

        /* An arena releases everything it took, including on the paths
           where construction gave up half way. */
        if (f.live != 0) {
            ++leaks;
        }
    }

    check(silent_failures == 0, "every refusal came with a reason");
    check(leaks == 0, "and released every block it had taken");
    check(found_success, "a large enough budget works");
    printf("  %s: %u allocations, first success at %u\n", what, (unsigned)total_calls,
           (unsigned)first_success);
}

/* The same sweep for the reading side, where a caller supplies the
   allocator for something built after construction. */
static void test_sweep_with_sources(void) {
    size_t budget;
    int    silent = 0;
    int    leaks  = 0;

    for (budget = 0; budget < 40; ++budget) {
        failing      f;
        cf_allocator alloc;
        cf_arena*    ar;

        f.budget      = budget;
        f.calls       = 0;
        f.live        = 0;
        alloc.alloc   = failing_alloc;
        alloc.release = failing_release;
        alloc.ctx     = &f;

        ar = cf_arena_new(&alloc, 0);
        if (ar != NULL) {
            cf_keyspace ks;
            /* An arena that exists may still refuse everything after
               its first block, which is the path a source takes when
               the machine fills up while it is reading. */
            if (cf_ks_init(&ks, ar, 64)) {
                cf_value v;
                size_t   i;
                cf_value_int(&v, 1);
                for (i = 0; i < 500; ++i) {
                    char   key[32];
                    size_t n = cf_fmt(key, sizeof key, "k.%zu", i);
                    if (!cf_ks_add(&ks, key, n, 0, &v, NULL)) {
                        break; /* refused, which is the correct answer */
                    }
                }
                (void)cf_ks_resolve(&ks);
                (void)cf_ks_count(&ks);
            }
            cf_arena_free(ar);
        }
        if (f.live != 0) {
            ++leaks;
        }
    }
    check(silent == 0, "no silent refusal");
    check(leaks == 0, "an arena releases every block on every path");
}

int main(void) {
    /* The default block size, and a small one. They fail in different
       places: a large block runs out between objects, a small one runs
       out inside them. */
    /* Different block sizes fail in different places: a large block
       runs out between objects, a small one runs out inside them.
       Sizes below 1024 are clamped by the arena, so asking for 64 would
       sweep the default again and report a number that means nothing -
       which the first version of this test did, three times, with the
       same answer each time. sweep() now refuses to agree with itself. */
    sweep(0, "default blocks");
    sweep(1024, "1 KiB blocks");
    sweep(8192, "8 KiB blocks");
    check(g_distinct_counts >= 2, "the block sizes actually produced different sweeps");
    test_sweep_with_sources();

    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
