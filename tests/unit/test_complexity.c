/*
 * test_complexity.c - the claims that are shapes, not numbers.
 *
 * The design says resolution is O(m log m) in candidates and lookup is
 * O(log n) in keys. test_scale checks that a large configuration is
 * correct; nothing checked that it is correct *quickly*, and a
 * regression from a binary search to a scan, or from one merge sort to
 * an insertion per key, would pass every test in this suite while
 * turning a pause into a hang.
 *
 * The measurement is deliberately coarse. Separating O(n log n) from
 * O(n squared) across a fourfold size step is the difference between
 * about 4.3x and 16x - so the bounds here are wide enough to be quiet
 * on a loaded CI runner and still fail the moment a complexity class
 * changes. Noise alone does not close that gap; a cache boundary
 * between the two sizes did, which is why resolution is now timed at
 * sizes that stay in cache, over the same total work.
 *
 * Where the work is too small to measure, the test says so and skips
 * rather than asserting something about the clock's granularity.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int  g_fail = 0, g_run = 0;
static void check(int c, const char* w) {
    ++g_run;
    if (!c) {
        ++g_fail;
        printf("FAIL: %s\n", w);
    }
}

/* Milliseconds. clock() rather than a platform call: it is C89, it
   measures processor time rather than wall time, and processor time is
   what a complexity claim is about. */
static double now_ms(void) {
    /* Through a variable rather than casting the call: the warning set
       has -Wbad-function-cast on, which is there to catch exactly the
       case where a cast hides what a function actually returned. */
    clock_t t = clock();
    return (double)t * 1000.0 / (double)CLOCKS_PER_SEC;
}

#define SMALL 1000
#define LARGE 4000 /* four times, so the classes are far apart */
#define REPEATS 3

/* Keys resolved per measurement, at either size: SMALL keys built
   WORK / SMALL times against LARGE keys built WORK / LARGE times. */
#define WORK 32000

static char g_key[32];

/* Built without width specifiers, because cf_fmt has none - which is
   how this test failed on its first run, the same way the digest did
   before it. The keys still need a fixed width so that they sort the
   way the numbers do, so the digits are placed by hand. */
static const char* key_of(size_t i) {
    size_t a = i / 100;
    size_t b = i % 100;

    g_key[0] = 's';
    g_key[1] = (char)('0' + (a / 100) % 10);
    g_key[2] = (char)('0' + (a / 10) % 10);
    g_key[3] = (char)('0' + a % 10);
    g_key[4] = '.';
    g_key[5] = 'k';
    g_key[6] = (char)('0' + (b / 10) % 10);
    g_key[7] = (char)('0' + b % 10);
    g_key[8] = '\0';
    return g_key;
}

/* Build and resolve a key space of `n` keys with three candidates each,
   returning the arena it took in KiB, or 0 if it failed. */
static size_t build_once(size_t n) {
    cf_arena*   ar = cf_arena_new(NULL, 0);
    cf_keyspace ks;
    cf_value    v;
    size_t      i;
    size_t      kib;

    if (ar == NULL || !cf_ks_init(&ks, ar, 64)) {
        cf_arena_free(ar);
        return 0;
    }
    for (i = 0; i < n; ++i) {
        int rank;
        for (rank = 0; rank < 3; ++rank) {
            const char* k = key_of(i);
            cf_value_int(&v, (cf_i64)(i + (size_t)rank));
            if (!cf_ks_add(&ks, k, strlen(k), rank * 10, &v, NULL)) {
                cf_arena_free(ar);
                return 0;
            }
        }
    }
    if (!cf_ks_resolve(&ks)) {
        cf_arena_free(ar);
        return 0;
    }
    kib = cf_arena_used(ar) / 1024 + 1;
    cf_arena_free(ar);
    return kib;
}

/* Build key spaces of `n` keys until WORK keys have been resolved, and
   time it, returning the best of several runs: the fastest is the one
   least polluted by whatever else the machine was doing. */
static double time_resolve(size_t n, size_t* arena_kib) {
    double best = 0.0;
    int    r;

    for (r = 0; r < REPEATS; ++r) {
        double t0 = now_ms();
        double t1;
        size_t k;

        for (k = 0; k < WORK / n; ++k) {
            *arena_kib = build_once(n);
            if (*arena_kib == 0) {
                return -1.0;
            }
        }
        t1 = now_ms();
        if (r == 0 || t1 - t0 < best) {
            best = t1 - t0;
        }
    }
    return best;
}

/* Time a fixed number of lookups over a key space of `n` keys. Fixed,
   so the only thing that can change the result is the cost of one
   lookup. */
#define LOOKUPS 200000

static double time_lookup(size_t n) {
    cf_arena*   ar = cf_arena_new(NULL, 0);
    cf_keyspace ks;
    cf_value    v;
    size_t      i;
    double      best = 0.0;
    int         r;

    if (ar == NULL || !cf_ks_init(&ks, ar, 64)) {
        return -1.0;
    }
    for (i = 0; i < n; ++i) {
        const char* k = key_of(i);
        cf_value_int(&v, (cf_i64)i);
        if (!cf_ks_add(&ks, k, strlen(k), 0, &v, NULL)) {
            cf_arena_free(ar);
            return -1.0;
        }
    }
    if (!cf_ks_resolve(&ks)) {
        cf_arena_free(ar);
        return -1.0;
    }

    for (r = 0; r < REPEATS; ++r) {
        double t0 = now_ms();
        double t1;
        size_t found = 0;
        for (i = 0; i < LOOKUPS; ++i) {
            const char* k = key_of(i % n);
            if (cf_ks_get(&ks, k, strlen(k)) != NULL) {
                ++found;
            }
        }
        t1 = now_ms();
        if (found != LOOKUPS) {
            cf_arena_free(ar);
            return -1.0;
        }
        if (r == 0 || t1 - t0 < best) {
            best = t1 - t0;
        }
    }
    cf_arena_free(ar);
    return best;
}

/* ---------------------------------------------------------------- */

static void test_resolution_is_not_quadratic(void) {
    size_t small_kib = 0, large_kib = 0;
    double small = time_resolve(SMALL, &small_kib);
    double large = time_resolve(LARGE, &large_kib);

    check(small >= 0.0 && large >= 0.0, "both sizes resolve");
    if (small < 0.0 || large < 0.0) {
        return;
    }
    printf("  %d keys resolved as %d x %d: %.1f ms (%u KiB each), as %d x %d: %.1f ms "
           "(%u KiB each)\n",
           WORK, WORK / SMALL, SMALL, small, (unsigned)small_kib, WORK / LARGE, LARGE,
           large, (unsigned)large_kib);

    if (small < 2.0) {
        /* Below a couple of milliseconds the ratio is measuring the
           clock, not the algorithm. Saying so beats asserting it. */
        printf("  (too fast to compare; ratio not checked)\n");
        return;
    }
    /* The same number of keys either way, in key spaces a quarter the
       size or four times as many of them. O(m log m) predicts about
       1.2x for the larger spaces, quadratic - in adding or in resolving
       - about 4x. Two and a half sits between them.

       Both sizes stay within a processor's cache (3 MiB at most), on
       purpose. Comparing one space of 4000 keys against one of 16000
       measured the cache instead: the small one fit a 12 MiB cache and
       the large one did not, and on a loaded machine correct code took 8
       to 12 times as long for four times the keys, which failed this
       check. Held to the same work and in cache, it took 0.8 to 1.7
       times at every size, build and load tried. */
    check(large < small * 2.5,
          "four times the keys per space cost far less than four times as "
          "much per key, so resolution did not become quadratic");

    /* Memory is linear in candidates, and a per-key allocation that
       crept back in would show up as superlinear growth here. */
    if (small_kib > 0) {
        check(large_kib < small_kib * 6,
              "and the arena grew in proportion, not faster");
    }
}

static void test_lookup_is_a_binary_search(void) {
    double small = time_lookup(SMALL);
    double large = time_lookup(LARGE);

    check(small >= 0.0 && large >= 0.0, "both sizes are searchable");
    if (small < 0.0 || large < 0.0) {
        return;
    }
    printf("  %d lookups over %d keys: %.1f ms, over %d keys: %.1f ms\n", LOOKUPS,
           SMALL, small, LARGE, large);

    if (small < 2.0) {
        printf("  (too fast to compare; ratio not checked)\n");
        return;
    }
    /* The same number of lookups over four times the keys. A binary
       search pays one more comparison per doubling, so about 1.2x; a
       scan would pay 4x. Two and a half separates them and leaves room
       for the cache behaving worse on the larger space. */
    check(large < small * 2.5,
          "four times the keys cost roughly the same per lookup, so the "
          "search is still logarithmic");
}

/* The claim that reading is cheap enough to do on a hot path, measured
   rather than asserted. */
static void test_read_cost(void) {
    static char  a0[]   = "prog";
    static char  a1[]   = "--server.port=9000";
    static char* argv[] = {a0, a1, NULL};
    cf_error     err    = CF_ERROR_INIT;
    cf_builder*  b      = cf_build(NULL);
    cf_config*   c;
    double       t0, t1;
    cf_i64       sum = 0;
    int          i;

    cf_builder_cli(b, "cli", CF_RANK_CLI, 2, argv);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    c = cf_builder_create(b, &err);
    if (c == NULL) {
        check(0, "builds");
        return;
    }
    t0 = now_ms();
    for (i = 0; i < 1000000; ++i) {
        sum += cf_get_int(c, "server.port", 0);
    }
    t1 = now_ms();
    check(sum == (cf_i64)9000 * 1000000, "a million reads, all correct");
    /* A million reads in n milliseconds is n nanoseconds each, and the
       first version of this line multiplied by a thousand and printed a
       number three orders out. A wrong number in output is the thing
       this suite spends most of its effort catching elsewhere.

       Unoptimised, too: the default preset builds without -O, so this
       is the pessimistic figure rather than the one to quote. */
    printf("  a million reads: %.1f ms, %.1f ns each, unoptimised\n", t1 - t0, t1 - t0);
    cf_config_free(c);
}

int main(void) {
    test_resolution_is_not_quadratic();
    test_lookup_is_a_binary_search();
    test_read_cost();

    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
