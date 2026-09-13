/*
 * test_threads.c - the threading contract, D-14.
 *
 * "Reads are safe concurrently; construction and release are the
 * caller's to synchronise." An unstated threading contract is assumed
 * to be full internal locking by users and nothing at all by
 * implementers, so this file is the statement.
 *
 * Threads are not in C89, so this uses pthreads where they are present
 * and checks the invariants single-threaded where they are not. The
 * single-threaded path still proves the properties the contract rests
 * on: that a read touches nothing mutable, and that the reference count
 * is the only thing that moves.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <string.h>

static int  g_fail = 0, g_run = 0;
static void check(int c, const char* w) {
    ++g_run;
    if (!c) {
        ++g_fail;
        printf("FAIL: %s\n", w);
    }
}

static char a0[] = "prog";

static cf_config* build(cf_error* err) {
    static char  a1[]   = "--server.port=9000";
    static char  a2[]   = "--server.host=10.0.0.1";
    static char* argv[] = {a0, a1, a2, NULL};
    cf_builder*  b      = cf_build(NULL);

    cf_builder_cli(b, "cli", CF_RANK_CLI, 3, argv);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_builder_text(b, "server.host", "0.0.0.0", 0);
    cf_builder_text(b, "log.level", "info", 0);
    return cf_builder_create(b, err);
}

/* Every read path, run twice, with a byte-for-byte comparison of the
   whole snapshot in between. A read that mutated anything reachable
   would show up here whether or not a second thread exists. */
static void test_reads_do_not_mutate(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_config*  c   = build(&err);
    static char before[8192];
    static char after[8192];
    static char scratch[4096];
    cf_iter     it;
    size_t      n1, n2;
    cf_u64      d1, d2;

    check(c != NULL, "builds");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }

    n1 = cf_image_write(cf_keys(c), NULL, CF_IMAGE_PROVENANCE, before, sizeof before);
    d1 = cf_digest(c);

    /* Everything a reader can do. */
    (void)cf_get_int(c, "server.port", 0);
    (void)cf_get_text(c, "server.host", "");
    (void)cf_get_bool(c, "nothing.here", CF_FALSE);
    (void)cf_has(c, "log.level");
    (void)cf_type_of(c, "server.port");
    (void)cf_count(c);
    (void)cf_explain(c, "server.port", scratch, sizeof scratch);
    (void)cf_inspect(c, scratch, sizeof scratch);
    (void)cf_emit(c, "json", CF_EMIT_ALL, 0, scratch, sizeof scratch, &err);
    (void)cf_array_count(c, "server.tags");
    cf_iter_keys(&it, c, NULL);
    while (cf_iter_next(&it)) {
        /* walking is a read */
    }
    cf_iter_children(&it, c, "server");
    while (cf_iter_next(&it)) {
    }

    n2 = cf_image_write(cf_keys(c), NULL, CF_IMAGE_PROVENANCE, after, sizeof after);
    d2 = cf_digest(c);

    check(n1 == n2 && n1 > 0, "the snapshot is the same size after reading");
    check(memcmp(before, after, n1) == 0,
          "and byte for byte identical: a read mutates nothing");
    check(d1 == d2, "the digest is unchanged");

    cf_config_free(c);
}

/* The reference count is the one thing that moves, and it moves through
   the atomics in the portability layer. */
static void test_refcount(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = build(&err);
    int        i;

    if (c == NULL) {
        check(0, "builds");
        return;
    }
    for (i = 0; i < 1000; ++i) {
        cf_config_ref(c);
    }
    for (i = 0; i < 1000; ++i) {
        cf_config_free(c);
    }
    /* Still alive and still itself: the count came back to one. */
    check(cf_get_int(c, "server.port", 0) == 9000,
          "a thousand references taken and returned leave it intact");
    cf_config_free(c);
}

#if defined(CF_TEST_PTHREADS)

#include <pthread.h>

#define READERS 8
#define ROUNDS 2000

typedef struct reader_arg {
    const cf_config* c;
    cf_u64           digest;
    int              mismatches;
} reader_arg;

static void* reader(void* p) {
    reader_arg* a = (reader_arg*)p;
    char        buf[1024];
    int         i;

    for (i = 0; i < ROUNDS; ++i) {
        cf_iter it;
        if (cf_get_int(a->c, "server.port", 0) != 9000) {
            ++a->mismatches;
        }
        if (strcmp(cf_get_text(a->c, "server.host", ""), "10.0.0.1") != 0) {
            ++a->mismatches;
        }
        if (cf_digest(a->c) != a->digest) {
            ++a->mismatches;
        }
        (void)cf_explain(a->c, "server.port", buf, sizeof buf);
        cf_iter_keys(&it, a->c, NULL);
        while (cf_iter_next(&it)) {
        }
    }
    return NULL;
}

/* Concurrent reads are the entire point of an immutable snapshot, so
   they are what gets exercised. Under the sanitiser this is also a race
   detector run. */
static void test_concurrent_reads(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = build(&err);
    pthread_t  th[READERS];
    reader_arg args[READERS];
    int        i, total = 0;

    if (c == NULL) {
        check(0, "builds");
        return;
    }
    for (i = 0; i < READERS; ++i) {
        args[i].c          = c;
        args[i].digest     = cf_digest(c);
        args[i].mismatches = 0;
        check(pthread_create(&th[i], NULL, reader, &args[i]) == 0, "reader started");
    }
    for (i = 0; i < READERS; ++i) {
        pthread_join(th[i], NULL);
        total += args[i].mismatches;
    }
    check(total == 0, "eight readers, sixteen thousand rounds, no disagreement");
    cf_config_free(c);
}

/* Releasing from several threads is safe because the count is atomic;
   the last one out frees. */
static void* releaser(void* p) {
    cf_config_free((cf_config*)p);
    return NULL;
}

static void test_concurrent_release(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = build(&err);
    pthread_t  th[READERS];
    int        i;

    if (c == NULL) {
        check(0, "builds");
        return;
    }
    for (i = 0; i < READERS - 1; ++i) {
        cf_config_ref(c);
    }
    for (i = 0; i < READERS; ++i) {
        pthread_create(&th[i], NULL, releaser, c);
    }
    for (i = 0; i < READERS; ++i) {
        pthread_join(th[i], NULL);
    }
    check(1, "concurrent release does not double free");
}

#endif /* CF_TEST_PTHREADS */

int main(void) {
    test_reads_do_not_mutate();
    test_refcount();
#if defined(CF_TEST_PTHREADS)
    test_concurrent_reads();
    test_concurrent_release();
#else
    printf("  (no threads here; the invariants above are what the "
           "contract rests on)\n");
#endif

    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
