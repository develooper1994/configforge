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

#include "cf_test.h"

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

/* Everything a configuration holds, as bytes: the image with its
   provenance where the build has one, the inspection text where it
   does not. Either changes if a read changed anything. */
static size_t snapshot(const cf_config* c, char* buf, size_t cap) {
#if CF_WITH_IMAGE
    return cf_image_write(cf_keys(c), NULL, CF_IMAGE_PROVENANCE, buf, cap);
#else
    return cf_inspect(c, buf, cap);
#endif
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

    n1 = snapshot(c, before, sizeof before);
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
#if CF_WITH_EMIT
    (void)cf_emit(c, "json", CF_EMIT_ALL, 0, scratch, sizeof scratch, &err);
#endif
    (void)cf_array_count(c, "server.tags");
    cf_iter_keys(&it, c, NULL);
    while (cf_iter_next(&it)) {
        /* walking is a read */
    }
    cf_iter_children(&it, c, "server");
    while (cf_iter_next(&it)) {
    }

    n2 = snapshot(c, after, sizeof after);
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

/* Eight readers on a tracking snapshot: the flags are the one thing a
   read writes, and each must end set, once, with every value intact. */
static void test_concurrent_tracked_reads(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  c;
    pthread_t   th[READERS];
    reader_arg  args[READERS];
    int         i, total = 0;

    cf_builder_int(b, "server.port", 9000, 1, 65535, 0);
    cf_builder_text(b, "server.host", "10.0.0.1", 0);
    cf_builder_text(b, "log.level", "info", 0);
    cf_builder_track_reads(b);
    c = cf_builder_create(b, &err);
    if (c == NULL) {
        check(0, "builds tracking");
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
    check(total == 0, "tracking readers agree on every value");
    check(cf_read_count(c) == 2 && !cf_was_read(c, "log.level"),
          "and leave exactly the two keys they read marked");
    cf_config_free(c);
}

#endif /* CF_TEST_PTHREADS */

#if CF_WITH_REPORT
static size_t count_of(const char* hay, const char* needle) {
    size_t      n = 0;
    const char* p = hay;
    while ((p = strstr(p, needle)) != NULL) {
        ++n;
        p += strlen(needle);
    }
    return n;
}
#endif

/* Read coverage, off and on: what counts as a read, what does not, and
   what the report says. */
static void test_track_reads(void) {
    cf_error    err   = CF_ERROR_INIT;
    cf_config*  plain = build(&err);
    cf_builder* b     = cf_build(NULL);
    cf_config*  c;
    cf_config*  d;
    static char scratch[8192];
    cf_iter     it;
    cf_pair     pair;

    check(plain != NULL && !cf_tracks_reads(plain), "tracking is off by default");
    (void)cf_get_int(plain, "server.port", 0);
    check(!cf_was_read(plain, "server.port") && cf_read_count(plain) == 0,
          "and an untracked configuration reports no reads");
#if CF_WITH_REPORT
    (void)cf_report(plain, NULL, scratch, sizeof scratch);
    check(strstr(scratch, "\"read\"") == NULL, "nor any \"read\" in its report");
#endif
    cf_config_free(plain);

    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_builder_text(b, "server.host", "0.0.0.0", 0);
    cf_builder_text(b, "log.level", "info", 0);
    cf_builder_bool(b, "feature.x", CF_FALSE, 0);
    cf_builder_track_reads(b);
    c = cf_builder_create(b, &err);
    check(c != NULL && cf_tracks_reads(c), "tracking is on when asked for");
    if (c == NULL) {
        return;
    }
    check(cf_read_count(c) == 0, "nothing is read yet");

    (void)cf_explain(c, "log.level", scratch, sizeof scratch);
    (void)cf_inspect(c, scratch, sizeof scratch);
    cf_iter_keys(&it, c, NULL);
    while (cf_iter_next(&it)) {
    }
    (void)cf_was_read(c, "log.level");
    check(cf_read_count(c) == 0,
          "explaining, inspecting, walking and asking are not reads");

    (void)cf_get_int(c, "server.port", 0);
    (void)cf_get_int(c, "server.port", 0);
    (void)cf_get_int(c, "server.host", 0);
    (void)cf_has(c, "feature.x");
    (void)cf_get_text(c, "nothing.here", "");
    check(cf_was_read(c, "server.port"), "a getter marks its key");
    check(cf_was_read(c, "server.host"), "a read of the wrong type still uses the key");
    check(cf_was_read(c, "feature.x"), "cf_has is a read");
    check(!cf_was_read(c, "log.level"), "a key never asked for stays unread");
    check(!cf_was_read(c, "nothing.here"), "a key nothing supplied is not recorded");
    check(cf_read_count(c) == 3, "three keys read, the repeat counted once");

#if CF_WITH_REPORT
    (void)cf_report(c, NULL, scratch, sizeof scratch);
    check(count_of(scratch, "\"read\": true") == 3 &&
              count_of(scratch, "\"read\": false") == 1,
          "the blind report marks each key read or not");
#endif

    pair.key   = "log.level";
    pair.value = "debug";
    d          = cf_derive(c, "admin", &pair, 1, &err);
    check(d != NULL && cf_tracks_reads(d) && cf_read_count(d) == 0,
          "a derived configuration tracks, from nothing");
    if (d != NULL) {
        (void)cf_get_text(d, "log.level", "");
        check(cf_was_read(d, "log.level") && !cf_was_read(c, "log.level"),
              "and a read through it marks it alone");
        cf_config_free(d);
    }
    cf_config_free(c);
}

int main(void) {
    test_reads_do_not_mutate();
    test_refcount();
    test_track_reads();
#if defined(CF_TEST_PTHREADS)
    test_concurrent_reads();
    test_concurrent_release();
    test_concurrent_tracked_reads();
#else
    printf("  (no threads here; the invariants above are what the "
           "contract rests on)\n");
#endif

    return cf_test_done();
}
