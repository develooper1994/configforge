/*
 * test_live.c - cf_live: the three reload modes, a check that refuses,
 * a build that fails and leaves the running snapshot in place, a
 * CF_RESTART key kept at its running value and explained, and a reader
 * that keeps the snapshot it holds across a swap; and, where there are
 * symbolic links, a mounted Kubernetes ConfigMap updated the way the
 * kubelet updates one.
 */

/* symlink(2) and rename(2) for the ConfigMap test; CMake defines
   CF_TEST_SYMLINKS only where they exist. */
#if defined(CF_TEST_SYMLINKS)
#define _POSIX_C_SOURCE 200112L
#include <sys/stat.h>
#include <unistd.h>
#endif

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

/* What the next build reads. Memory sources keep a pointer to their
   pairs, so each set is static and the build is pointed at one. */
static const cf_pair  g_info[]  = {{"server.port", "8080"}, {"log.level", "info"}};
static const cf_pair  g_debug[] = {{"server.port", "9443"}, {"log.level", "debug"}};
static const cf_pair  g_trace[] = {{"server.port", "8080"}, {"log.level", "trace"}};
static const cf_pair  g_bad[]   = {{"server.port", "99999"}, {"log.level", "warn"}};
static const cf_pair* g_pairs   = g_info;
static const char*    g_file    = NULL;

static cf_builder* build(void* user) {
    cf_builder* b = cf_build(NULL);
    (void)user;
    if (g_file != NULL) {
        cf_builder_file(b, "app.ini", CF_RANK_USER_FILE, g_file, NULL, 0);
    } else {
        cf_builder_memory(b, "mem", CF_RANK_USER_FILE, g_pairs, 2);
    }
    cf_builder_int(b, "server.port", 8080, 1, 65535, CF_RESTART);
    cf_builder_text(b, "log.level", "info", 0);
    return b;
}

static cf_builder* no_builder(void* user) {
    (void)user;
    return NULL;
}

static int     g_checked = 0;
static cf_bool no_trace(void* user, const cf_config* running, const cf_config* next,
                        cf_error* err) {
    (void)user;
    (void)running;
    ++g_checked;
    if (strcmp(cf_get_text(next, "log.level", ""), "trace") == 0) {
        return cf_error_set(err, CF_E_POLICY, "log.level", NULL,
                            "trace logs every request body; not in production");
    }
    return CF_TRUE;
}

static cf_bool refuse_quietly(void* user, const cf_config* running,
                              const cf_config* next, cf_error* err) {
    (void)user;
    (void)next;
    (void)err;
    return running == NULL; /* the first, and nothing after it */
}

static const char* level(cf_live* l, char* out, size_t cap) {
    cf_config* c = cf_live_get(l);
    strncpy(out, cf_get_text(c, "log.level", "?"), cap - 1);
    out[cap - 1] = '\0';
    cf_config_free(c);
    return out;
}

static cf_i64 port(cf_live* l) {
    cf_config* c = cf_live_get(l);
    cf_i64     p = cf_get_int(c, "server.port", 0);
    cf_config_free(c);
    return p;
}

static void test_auto_and_manual(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_live*   l;
    cf_config* held;
    char       buf[256];

    g_pairs = g_info;
    l       = cf_live_open(CF_LIVE_MANUAL, build, no_trace, NULL, NULL, &err);
    check(l != NULL && g_checked == 1, "opens, checking the first snapshot");
    if (l == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(cf_live_generation(l) == 1, "generation 1");
    held    = cf_live_get(l);
    g_pairs = g_debug;
    check(cf_live_poll(l, &err) == CF_LIVE_UNCHANGED, "manual: a poll does nothing");
    check(cf_live_reload(l, &err) == CF_LIVE_SWAPPED, "manual: a reload swaps");
    check(strcmp(level(l, buf, sizeof buf), "debug") == 0, "a new reader sees the new");
    check(strcmp(cf_get_text(held, "log.level", ""), "info") == 0,
          "a reader holding the old keeps it");
    cf_config_free(held);
    check(cf_live_generation(l) == 2, "generation 2");

    check(port(l) == 8080, "a CF_RESTART key keeps its running value");
    check(cf_live_restarts(l, buf, sizeof buf) == 12 &&
              strcmp(buf, "server.port\n") == 0,
          "and is listed as waiting for a restart");
    held = cf_live_get(l);
    (void)cf_explain(held, "server.port", buf, sizeof buf);
    check(strstr(buf, "running") != NULL && strstr(buf, "9443") != NULL,
          "explain() shows the running value and the new one");
    cf_config_free(held);

    g_pairs = g_bad;
    check(cf_live_reload(l, &err) == CF_LIVE_FAILED &&
              strstr(err.message, "65535") != NULL,
          "an invalid change fails, saying why");
    check(strcmp(level(l, buf, sizeof buf), "debug") == 0 && cf_live_generation(l) == 2,
          "and the running snapshot stays");

    g_pairs        = g_trace;
    err.message[0] = '\0';
    check(cf_live_reload(l, &err) == CF_LIVE_FAILED &&
              strstr(err.message, "trace") != NULL,
          "a check refuses, saying why");
    check(strcmp(level(l, buf, sizeof buf), "debug") == 0, "and the running one stays");

    g_pairs = g_info;
    check(cf_live_reload(l, &err) == CF_LIVE_SWAPPED, "back to the first");
    check(cf_live_restarts(l, buf, sizeof buf) == 0 && buf[0] == '\0',
          "nothing waits for a restart when the value is back");
    check(cf_live_reload(l, &err) == CF_LIVE_UNCHANGED && cf_live_generation(l) == 3,
          "a reload that changes nothing is not a new generation");
    cf_live_close(l);
}

static void test_approve(void) {
    cf_error err = CF_ERROR_INIT;
    cf_live* l;
    char     buf[64];

    g_pairs = g_info;
    l       = cf_live_open(CF_LIVE_APPROVE, build, NULL, NULL, NULL, &err);
    check(l != NULL, "approve: opens");
    if (l == NULL) {
        return;
    }
    check(cf_live_approve(l, &err) == CF_LIVE_UNCHANGED, "nothing to approve yet");
    g_pairs = g_debug;
    check(cf_live_reload(l, &err) == CF_LIVE_PENDING, "a change waits");
    check(strcmp(level(l, buf, sizeof buf), "info") == 0, "readers do not see it yet");
    check(cf_live_pending(l) != NULL &&
              strcmp(cf_get_text(cf_live_pending(l), "log.level", ""), "debug") == 0,
          "it can be looked at");
    cf_live_reject(l);
    check(cf_live_pending(l) == NULL && strcmp(level(l, buf, sizeof buf), "info") == 0,
          "rejected, it is gone");
    check(cf_live_reload(l, &err) == CF_LIVE_PENDING &&
              cf_live_approve(l, &err) == CF_LIVE_SWAPPED,
          "approved, it is swapped in");
    check(strcmp(level(l, buf, sizeof buf), "debug") == 0 && cf_live_generation(l) == 2,
          "and readers see it");
    cf_live_close(l);

    g_pairs = g_info;
    l       = cf_live_open(CF_LIVE_AUTO, build, refuse_quietly, NULL, NULL, &err);
    g_pairs = g_debug;
    err.message[0] = '\0';
    check(l != NULL && cf_live_reload(l, &err) == CF_LIVE_FAILED &&
              err.message[0] != '\0',
          "a check that refuses without a reason still gives one");
    cf_live_close(l);
}

static cf_bool spill(const char* path, const char* text) {
    FILE* f = fopen(path, "wb");
    if (f == NULL) {
        return CF_FALSE;
    }
    fputs(text, f);
    fclose(f);
    return CF_TRUE;
}

/* A poll notices a file that changed; each write has a new length, so
   a change within one clock tick is still seen. */
static void test_poll(void) {
    cf_error err = CF_ERROR_INIT;
    cf_live* l;
    char     buf[64];

    if (!cf_has_feature("ini")) {
        return;
    }
    check(spill("cf_live.ini", "[log]\nlevel = info\n"), "wrote the file");
    g_file = "cf_live.ini";
    l      = cf_live_open(CF_LIVE_AUTO, build, NULL, NULL, NULL, &err);
    check(l != NULL, "auto: opens on a file");
    if (l == NULL) {
        g_file = NULL;
        return;
    }
    check(cf_live_poll(l, &err) == CF_LIVE_UNCHANGED, "nothing changed yet");
    check(spill("cf_live.ini", "[log]\nlevel = debug\n"), "changed the file");
    check(cf_live_poll(l, &err) == CF_LIVE_SWAPPED, "a poll swaps");
    check(strcmp(level(l, buf, sizeof buf), "debug") == 0, "to the file's new value");
    check(spill("cf_live.ini", "; touched\n[log]\nlevel = debug\n"),
          "touched the file");
    check(cf_live_wait(l, 0, &err) == CF_LIVE_UNCHANGED && cf_live_generation(l) == 2,
          "the same values again are not a new generation");
    check(cf_live_poll(l, &err) == CF_LIVE_UNCHANGED, "and are not rebuilt twice");
    check(spill("cf_live.ini", "[log]\nlevel = nonsense = x\n[\n"), "broke the file");
    check(cf_live_poll(l, &err) == CF_LIVE_FAILED, "a broken file fails");
    check(strcmp(level(l, buf, sizeof buf), "debug") == 0, "and changes nothing");
    cf_live_close(l);
    g_file = NULL;
    remove("cf_live.ini");
}

#if defined(CF_TEST_SYMLINKS)
/* The kubelet writes a new ConfigMap into a fresh directory, points a
   temporary link at it and renames that over `..data`; the file a
   program opens, `app.ini`, is a link to `..data/app.ini` and is never
   written at all. A reload that watched the file's own inode, or
   trusted the link's stamp, would never see an update. */
static void test_configmap(void) {
    cf_error err = CF_ERROR_INIT;
    cf_live* l;
    char     buf[64];

    if (!cf_has_feature("ini")) {
        return;
    }
    (void)mkdir("cf_cm", 0755);
    (void)mkdir("cf_cm/..2026_10_29_1", 0755);
    check(spill("cf_cm/..2026_10_29_1/app.ini", "[log]\nlevel = info\n"),
          "the first version");
    (void)remove("cf_cm/..data");
    (void)remove("cf_cm/app.ini");
    check(symlink("..2026_10_29_1", "cf_cm/..data") == 0 &&
              symlink("..data/app.ini", "cf_cm/app.ini") == 0,
          "mounted as the kubelet mounts it");
    g_file = "cf_cm/app.ini";
    l      = cf_live_open(CF_LIVE_AUTO, build, NULL, NULL, NULL, &err);
    check(l != NULL && strcmp(level(l, buf, sizeof buf), "info") == 0,
          "read through the links");
    if (l == NULL) {
        g_file = NULL;
        return;
    }
    (void)mkdir("cf_cm/..2026_10_29_2", 0755);
    check(spill("cf_cm/..2026_10_29_2/app.ini", "[log]\nlevel = debug\n"),
          "the second version");
    check(symlink("..2026_10_29_2", "cf_cm/..data_tmp") == 0 &&
              rename("cf_cm/..data_tmp", "cf_cm/..data") == 0,
          "swapped in one rename");
    check(cf_live_wait(l, 2000, &err) == CF_LIVE_SWAPPED, "the update is noticed");
    check(strcmp(level(l, buf, sizeof buf), "debug") == 0, "and read");
    cf_live_close(l);
    g_file = NULL;
    (void)remove("cf_cm/app.ini");
    (void)remove("cf_cm/..data");
    (void)remove("cf_cm/..2026_10_29_1/app.ini");
    (void)remove("cf_cm/..2026_10_29_2/app.ini");
    (void)rmdir("cf_cm/..2026_10_29_1");
    (void)rmdir("cf_cm/..2026_10_29_2");
    (void)rmdir("cf_cm");
}
#endif

static void test_nothing(void) {
    cf_error err = CF_ERROR_INIT;
    char     buf[8];

    check(cf_live_open(CF_LIVE_AUTO, NULL, NULL, NULL, NULL, &err) == NULL &&
              err.message[0] != '\0',
          "no build function");
    check(cf_live_open((cf_live_mode)7, build, NULL, NULL, NULL, &err) == NULL,
          "no such mode");
    check(cf_live_open(CF_LIVE_AUTO, no_builder, NULL, NULL, NULL, &err) == NULL &&
              strstr(err.message, "builder") != NULL,
          "a build function that makes no builder");
    check(cf_live_get(NULL) == NULL && cf_live_pending(NULL) == NULL &&
              cf_live_generation(NULL) == 0,
          "reads of nothing");
    check(cf_live_poll(NULL, &err) == CF_LIVE_FAILED &&
              cf_live_reload(NULL, &err) == CF_LIVE_FAILED &&
              cf_live_approve(NULL, &err) == CF_LIVE_FAILED &&
              cf_live_wait(NULL, 0, &err) == CF_LIVE_FAILED,
          "and changes of nothing");
    check(cf_live_restarts(NULL, buf, sizeof buf) == 0 && buf[0] == '\0',
          "no restarts of nothing");
    cf_live_reject(NULL);
    cf_live_close(NULL);
}

/* Not in a single-threaded build, which compiles the lock's atomic out
   and promises no thread anything. */
#if defined(CF_TEST_PTHREADS) && !defined(CF_SINGLE_THREADED)
#include <pthread.h>

/* Readers take and release snapshots while the main thread swaps them:
   under ThreadSanitizer this is the test of the lock, and anywhere it is
   the test that no reader is handed a snapshot already freed. */
static cf_live* g_shared;
static int      g_bad_reads;

static void* reader(void* arg) {
    int i;
    (void)arg;
    for (i = 0; i < 20000; ++i) {
        cf_config*  c = cf_live_get(g_shared);
        const char* v = cf_get_text(c, "log.level", "?");
        if (strcmp(v, "info") != 0 && strcmp(v, "debug") != 0) {
            ++g_bad_reads; /* racy on purpose only if the test fails */
        }
        cf_config_free(c);
    }
    return NULL;
}

static void test_threads(void) {
    cf_error  err = CF_ERROR_INIT;
    pthread_t t[4];
    int       i, started = 0;

    g_pairs  = g_info;
    g_shared = cf_live_open(CF_LIVE_MANUAL, build, NULL, NULL, NULL, &err);
    check(g_shared != NULL, "threads: opens");
    if (g_shared == NULL) {
        return;
    }
    for (i = 0; i < 4; ++i) {
        started += pthread_create(&t[i], NULL, reader, NULL) == 0 ? 1 : 0;
    }
    for (i = 0; i < 400; ++i) {
        g_pairs = (i % 2 == 0) ? g_debug : g_info;
        (void)cf_live_reload(g_shared, &err);
    }
    for (i = 0; i < started; ++i) {
        pthread_join(t[i], NULL);
    }
    check(started == 4 && g_bad_reads == 0, "every reader saw a whole snapshot");
    check(cf_live_generation(g_shared) == 401, "and every swap counted");
    cf_live_close(g_shared);
}
#endif

int main(void) {
    test_auto_and_manual();
    test_approve();
    test_poll();
#if defined(CF_TEST_SYMLINKS)
    test_configmap();
#endif
    test_nothing();
#if defined(CF_TEST_PTHREADS) && !defined(CF_SINGLE_THREADED)
    test_threads();
#endif
    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
