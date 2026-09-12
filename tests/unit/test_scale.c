/*
 * test_scale.c - the key space at a size where the algorithm matters.
 *
 * Ten thousand keys across three sources. The earlier sorted-on-insert
 * key space was quadratic here; this exists so that a regression to it
 * shows up as a hang rather than as a slightly slower build.
 */

#include "configforge/cf_keyspace.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define N 10000

static int  g_fail = 0, g_run = 0;
static void check(int c, const char* w) {
    ++g_run;
    if (!c) {
        ++g_fail;
        printf("FAIL: %s\n", w);
    }
}

int main(void) {
    cf_arena*   ar = cf_arena_new(NULL, 1 << 20);
    cf_keyspace ks;
    char        key[64];
    cf_value    v;
    int         i;
    clock_t     t0, t1;
    cf_u64      d1, d2;

    check(cf_ks_init(&ks, ar, 64), "init");

    t0 = clock();
    /* Inserted in an order chosen to be bad for anything that keeps
       the array sorted as it goes: descending, so every insert would
       land at the front. */
    for (i = N - 1; i >= 0; --i) {
        size_t n = cf_fmt(key, sizeof key, "section.%d.value", i);
        cf_value_int(&v, (cf_i64)i);
        if (!cf_ks_add(&ks, key, n, 40, &v, NULL)) {
            break;
        }
    }
    for (i = 0; i < N; i += 2) {
        size_t n = cf_fmt(key, sizeof key, "section.%d.value", i);
        cf_value_int(&v, (cf_i64)(i + 1000000));
        cf_ks_add(&ks, key, n, 10, &v, "ENV");
    }
    for (i = 0; i < N; i += 3) {
        size_t n = cf_fmt(key, sizeof key, "section.%d.value", i);
        cf_value_int(&v, (cf_i64)(i + 2000000));
        cf_ks_add(&ks, key, n, 0, &v, "argv");
    }
    cf_ks_resolve(&ks);
    cf_ks_infer_untyped(&ks);
    t1 = clock();

    check(cf_ks_count(&ks) == N, "every key present exactly once");
    check(cf_ks_is_sorted(&ks), "sorted invariant holds at scale");

    /* Precedence must still hold for every key, not just the ones a
       small test happened to look at. */
    {
        int wrong = 0;
        for (i = 0; i < N; ++i) {
            size_t          n   = cf_fmt(key, sizeof key, "section.%d.value", i);
            const cf_value* got = cf_ks_get(&ks, key, n);
            cf_i64          want;
            if (i % 3 == 0) {
                want = i + 2000000;
            } else if (i % 2 == 0) {
                want = i + 1000000;
            } else {
                want = i;
            }
            if (got == NULL || got->u.i != want) {
                ++wrong;
            }
        }
        check(wrong == 0, "precedence correct for every key");
    }

    /* The chain length must match how many sources supplied the key. */
    {
        size_t              n   = cf_fmt(key, sizeof key, "section.%d.value", 0);
        const cf_entry*     e   = cf_ks_find(&ks, key, n);
        int                 len = 0;
        const cf_candidate* c;
        for (c = e->chain; c != NULL; c = c->next) {
            ++len;
        }
        check(len == 3, "all three candidates retained");
        check(e->chain->source == 0, "chain ordered by precedence");
    }

    d1 = cf_ks_digest(&ks);

    /* Same content, opposite insertion order: the digest is over the
       resolved configuration, so it must not move. */
    {
        cf_keyspace ks2;
        cf_ks_init(&ks2, ar, 64);
        for (i = 0; i < N; i += 3) {
            size_t n = cf_fmt(key, sizeof key, "section.%d.value", i);
            cf_value_int(&v, (cf_i64)(i + 2000000));
            cf_ks_add(&ks2, key, n, 0, &v, "argv");
        }
        for (i = 0; i < N; ++i) {
            size_t n = cf_fmt(key, sizeof key, "section.%d.value", i);
            cf_value_int(&v, (cf_i64)i);
            cf_ks_add(&ks2, key, n, 40, &v, NULL);
        }
        for (i = N - 2; i >= 0; i -= 2) {
            size_t n = cf_fmt(key, sizeof key, "section.%d.value", i);
            cf_value_int(&v, (cf_i64)(i + 1000000));
            cf_ks_add(&ks2, key, n, 10, &v, "ENV");
        }
        cf_ks_resolve(&ks2);
        cf_ks_infer_untyped(&ks2);
        d2 = cf_ks_digest(&ks2);
        check(d1 == d2, "digest independent of insertion order at scale");
    }

    printf("%d keys, %d candidates, build+resolve %.0f ms, arena %lu KiB\n", N,
           N + N / 2 + N / 3, 1000.0 * (double)(t1 - t0) / (double)CLOCKS_PER_SEC,
           (unsigned long)(cf_arena_used(ar) / 1024));

    cf_arena_free(ar);
    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
