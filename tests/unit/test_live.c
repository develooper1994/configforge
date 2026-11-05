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

#include <stddef.h>
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
    {
        cf_live_status st;
        memset(&st, 0, sizeof st);
        check(!cf_live_status_get(l, &st), "a status of an unknown size is refused");
        st.size = sizeof st;
        check(cf_live_status_get(l, &st), "the status");
        check(st.generation == 3 && st.reloads == 5 && st.failures == 2,
              "counts the generations, the rebuilds and the failures");
        check(st.last == CF_LIVE_UNCHANGED && !st.pending,
              "says what the last one did");
        {
            cf_error last = CF_ERROR_INIT;
            check(cf_live_last_error(l, &last) && strstr(last.message, "trace") != NULL,
                  "and why the last failure failed");
        }
        check(!cf_live_status_get(NULL, &st) && !cf_live_status_get(l, NULL),
              "and nothing of nothing");
        {
            /* A caller built against a later, larger struct: this library
               fills what it has and leaves the rest alone. */
            static unsigned char bigger[sizeof(cf_live_status) + 16];
            cf_live_status*      big = (cf_live_status*)(void*)bigger;
            memset(bigger, 0xA5, sizeof bigger);
            big->size = sizeof bigger;
            check(cf_live_status_get(l, big) && big->generation == 3 &&
                      bigger[sizeof(cf_live_status)] == 0xA5 &&
                      bigger[sizeof bigger - 1] == 0xA5,
                  "a larger struct is filled as far as this library's reaches");
        }
    }
    {
        cf_live* fresh;
        cf_error none = CF_ERROR_INIT;
        g_pairs       = g_info;
        fresh         = cf_live_open(CF_LIVE_MANUAL, build, NULL, NULL, NULL, &err);
        check(fresh != NULL && !cf_live_last_error(fresh, &none) && none.code == CF_OK,
              "no failure, no error");
        cf_live_close(fresh);
    }
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
    {
        cf_live_status st;
        st.size = sizeof st;
        g_pairs = g_trace;
        check(cf_live_reload(l, &err) == CF_LIVE_PENDING, "another waits");
        check(cf_live_status_get(l, &st) && st.pending && st.last == CF_LIVE_PENDING,
              "and the status says so");
        cf_live_reject(l);
        check(cf_live_status_get(l, &st) && !st.pending, "until it is rejected");
    }
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

/* cf_live_open_with: each function gets its own `user`. */
static int g_build_seen, g_check_seen;

static cf_builder* build_with(void* user) {
    g_build_seen = (user == (void*)&g_build_seen) ? 1 : -1;
    return build(NULL);
}

static cf_bool check_with(void* user, const cf_config* running, const cf_config* next,
                          cf_error* err) {
    (void)running;
    (void)next;
    (void)err;
    g_check_seen = (user == (void*)&g_check_seen) ? 1 : -1;
    return CF_TRUE;
}

static void test_options(void) {
    cf_error        err = CF_ERROR_INIT;
    cf_live_options o   = CF_LIVE_OPTIONS_INIT;
    cf_live*        l;

    g_pairs      = g_info;
    o.build      = build_with;
    o.build_user = &g_build_seen;
    o.check      = check_with;
    o.check_user = &g_check_seen;
    l            = cf_live_open_with(&o, &err);
    check(l != NULL && g_build_seen == 1 && g_check_seen == 1,
          "options: build and check each get their own user");
    cf_live_close(l);
    o.size = 0;
    check(cf_live_open_with(&o, &err) == NULL &&
              strstr(err.message, "OPTIONS_INIT") != NULL,
          "options without a size are refused, by name");
    check(cf_live_open_with(NULL, &err) == NULL, "and no options");
}

static cf_builder* live_literal_build(void* user) {
    const cf_pair* const* pairs = (const cf_pair* const*)user;
    cf_builder*           b     = cf_build(NULL);
    cf_builder_text(b, "literal", NULL, CF_RESTART);
    cf_builder_text(b, "level", "old", 0);
    cf_builder_memory(b, "mem", CF_RANK_USER_FILE, *pairs, 2);
    return b;
}

static void test_literal_pin(void) {
    static const cf_pair before[] = {{"literal", "$${kept}"}, {"level", "old"}};
    static const cf_pair after[]  = {{"literal", "changed"}, {"level", "new"}};
    const cf_pair*       pairs    = before;
    cf_error             err      = CF_ERROR_INIT;
    cf_live*             live =
        cf_live_open(CF_LIVE_MANUAL, live_literal_build, NULL, &pairs, NULL, &err);
    cf_config* c;
    check(live != NULL, "open a scalar containing a literal reference");
    if (live == NULL) {
        return;
    }
    pairs = after;
    check(cf_live_reload(live, &err) == CF_LIVE_SWAPPED,
          "keeping text does not reparse it as an authored template");
    c = cf_live_get(live);
    check(strcmp(cf_get_text(c, "literal", ""), "${kept}") == 0 &&
              strcmp(cf_get_text(c, "level", ""), "new") == 0,
          "literal scalar text is held while unrelated settings change");
    cf_config_free(c);
    cf_live_close(live);
}

/* Probation: what the program says about the snapshot it runs on. */
static int     g_healthy = 1;
static int     g_asked   = 0;
static char    g_last_event[64];
static char    g_last_message[CF_MESSAGE_MAX];
static cf_bool health(void* user, const cf_config* running, cf_error* err) {
    (void)user;
    ++g_asked;
    if (!g_healthy) {
        return cf_error_set(err, CF_E_POLICY, NULL, NULL, "upstream answered 503 at %s",
                            cf_get_text(running, "log.level", "?"));
    }
    return CF_TRUE;
}
static cf_u64 g_fp_previous, g_fp_now;
static void   remember(void* user, const cf_log_event* e) {
    (void)user;
    if (e->size >= sizeof *e) {
        g_fp_previous = e->previous;
        g_fp_now      = e->fingerprint;
    }
    if (strncmp(e->event, "reload-", 7) == 0 && strcmp(e->event, "reload") != 0) {
        const char* m = (e->message != NULL) ? e->message : "";
        strncpy(g_last_event, e->event, sizeof g_last_event - 1);
        strncpy(g_last_message, m, sizeof g_last_message - 1);
    }
}

static cf_live* open_probation(cf_live_mode mode, unsigned polls, cf_live_health_fn h) {
    cf_error        err = CF_ERROR_INIT;
    cf_live_options o   = CF_LIVE_OPTIONS_INIT;
    o.mode              = mode;
    o.build             = build;
    o.log               = remember;
    o.log_min           = CF_LOG_DEBUG;
    o.health            = h;
    o.probation         = polls;
    return cf_live_open_with(&o, &err);
}

static cf_live_status status_of(cf_live* l) {
    cf_live_status st;
    memset(&st, 0, sizeof st);
    st.size = sizeof st;
    (void)cf_live_status_get(l, &st);
    return st;
}

static void test_probation(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_live*   l;
    char       buf[64];
    cf_config* held;
    cf_u64     fp_info, fp_debug;

    g_pairs   = g_info;
    g_healthy = 1;
    l         = open_probation(CF_LIVE_MANUAL, 3, health);
    check(l != NULL, "probation: opens");
    if (l == NULL) {
        return;
    }
    check(!status_of(l).probation && !cf_live_confirm(l) &&
              cf_live_rollback(l, NULL, &err) == CF_LIVE_FAILED &&
              err.code == CF_E_CAPABILITY,
          "the first snapshot is not on probation: nothing to roll back to");

    held    = cf_live_get(l);
    fp_info = cf_fingerprint(held);
    cf_config_free(held);
    g_pairs = g_debug;
    check(cf_live_reload(l, &err) == CF_LIVE_SWAPPED && status_of(l).probation,
          "a swap starts a probation");
    held     = cf_live_get(l);
    fp_debug = cf_fingerprint(held);
    cf_config_free(held);
    check(fp_info != fp_debug && g_fp_previous == fp_info && g_fp_now == fp_debug,
          "the reload's log event names the fingerprints before and after");
    check(cf_live_rollback(l, "the pool did not connect", &err) == CF_LIVE_ROLLED_BACK,
          "a rollback");
    check(g_fp_previous == fp_debug && g_fp_now == fp_info,
          "and a rollback's, the other way round");
    check(strcmp(level(l, buf, sizeof buf), "info") == 0 && cf_live_generation(l) == 3,
          "readers have the snapshot before, as a new generation");
    check(strcmp(g_last_event, "reload-rolled-back") == 0 &&
              strstr(g_last_message, "the pool did not connect") != NULL,
          "logged, with the reason");
    {
        cf_live_status st = status_of(l);
        check(st.rollbacks == 1 && !st.probation && st.last == CF_LIVE_ROLLED_BACK &&
                  st.failures == 0,
              "the status counts it, and it is no failure");
    }
    check(cf_live_rollback(l, NULL, &err) == CF_LIVE_FAILED &&
              status_of(l).rollbacks == 1,
          "a second rollback has nothing to go back to");

    check(cf_live_reload(l, &err) == CF_LIVE_SWAPPED &&
              strcmp(level(l, buf, sizeof buf), "debug") == 0,
          "a reload asked for is a retry, and may swap the same values in again");
    g_asked = 0;
    check(cf_live_poll(l, &err) == CF_LIVE_UNCHANGED && status_of(l).probation &&
              cf_live_poll(l, &err) == CF_LIVE_UNCHANGED && status_of(l).probation,
          "healthy polls keep it on probation until the count is done");
    check(cf_live_poll(l, &err) == CF_LIVE_UNCHANGED && !status_of(l).probation &&
              g_asked == 3 && strcmp(g_last_event, "reload-confirmed") == 0,
          "the last of them confirms it, a MANUAL program's polls too");
    check(cf_live_rollback(l, NULL, &err) == CF_LIVE_FAILED,
          "and a confirmed snapshot is not rolled back");

    g_pairs   = g_trace;
    g_healthy = 0;
    check(cf_live_reload(l, &err) == CF_LIVE_SWAPPED && g_fp_previous == fp_debug,
          "another swap, whose event names the one it replaced");
    check(cf_live_poll(l, &err) == CF_LIVE_ROLLED_BACK &&
              strcmp(level(l, buf, sizeof buf), "debug") == 0,
          "a health function that says no rolls it back at the next poll");
    check(strstr(g_last_message, "503 at trace") != NULL,
          "with the health function's reason, about the snapshot it ran on");
    g_healthy = 1;

    g_pairs = g_info;
    check(cf_live_reload(l, &err) == CF_LIVE_SWAPPED && g_fp_previous == fp_debug,
          "a swap on probation, from the snapshot the health check kept");
    g_pairs = g_trace;
    check(cf_live_reload(l, &err) == CF_LIVE_SWAPPED, "and another within it");
    check(cf_live_rollback(l, NULL, &err) == CF_LIVE_ROLLED_BACK &&
              strcmp(level(l, buf, sizeof buf), "debug") == 0,
          "rolls back to the last confirmed, not the one in between");

    g_pairs = g_info;
    check(cf_live_reload(l, &err) == CF_LIVE_SWAPPED && cf_live_confirm(l) &&
              !status_of(l).probation && strcmp(g_last_event, "reload-confirmed") == 0,
          "cf_live_confirm ends it early");
    cf_live_close(l);

    /* Off unless asked for, and an options struct from 1.0 never is. */
    g_pairs = g_info;
    l       = open_probation(CF_LIVE_MANUAL, 0, health);
    g_pairs = g_debug;
    check(l != NULL && cf_live_reload(l, &err) == CF_LIVE_SWAPPED &&
              !status_of(l).probation &&
              cf_live_rollback(l, NULL, &err) == CF_LIVE_FAILED,
          "probation 0 keeps nothing");
    cf_live_close(l);
    {
        cf_live_options o = CF_LIVE_OPTIONS_INIT;
        o.build           = build;
        o.probation       = 5;
        o.size            = offsetof(cf_live_options, health);
        g_pairs           = g_info;
        l                 = cf_live_open_with(&o, &err);
        g_pairs           = g_debug;
        check(l != NULL && cf_live_reload(l, &err) == CF_LIVE_SWAPPED &&
                  !status_of(l).probation,
              "options of 1.0's size: the members after them are not read");
        cf_live_close(l);
    }

    /* APPROVE: the approval is the swap that goes on probation. */
    g_pairs = g_info;
    l       = open_probation(CF_LIVE_APPROVE, 1, NULL);
    g_pairs = g_debug;
    check(l != NULL && cf_live_reload(l, &err) == CF_LIVE_PENDING &&
              !status_of(l).probation && cf_live_approve(l, &err) == CF_LIVE_SWAPPED &&
              status_of(l).probation,
          "approve: the approved swap is on probation");
    check(cf_live_rollback(l, NULL, &err) == CF_LIVE_ROLLED_BACK &&
              strcmp(level(l, buf, sizeof buf), "info") == 0,
          "and can be rolled back");
    {
        static unsigned char short_st[sizeof(cf_live_status) + 8];
        cf_live_status*      st = (cf_live_status*)(void*)short_st;
        memset(short_st, 0xA5, sizeof short_st);
        st->size = offsetof(cf_live_status, rollbacks);
        check(cf_live_status_get(l, st) && st->generation == 3 &&
                  short_st[offsetof(cf_live_status, rollbacks)] == 0xA5,
              "a status of 1.0's size gets 1.0's members and nothing past them");
    }
    cf_live_close(l);
}

/* After a rollback the file still holds what was rolled back from: a
   poll must not build it and swap it in again. */
static void test_probation_file(void) {
    cf_error err = CF_ERROR_INIT;
    cf_live* l;
    char     buf[64];

    if (!cf_has_feature("ini")) {
        return;
    }
    check(spill("cf_probation.ini", "[log]\nlevel = info\n"), "wrote the file");
    g_file = "cf_probation.ini";
    l      = open_probation(CF_LIVE_AUTO, 2, NULL);
    check(l != NULL, "probation: auto opens on a file");
    if (l == NULL) {
        g_file = NULL;
        return;
    }
    check(spill("cf_probation.ini", "[log]\nlevel = debug\n"), "changed the file");
    check(cf_live_poll(l, &err) == CF_LIVE_SWAPPED, "a poll swaps");
    check(cf_live_rollback(l, "errors doubled", &err) == CF_LIVE_ROLLED_BACK,
          "rolled back");
    check(cf_live_poll(l, &err) == CF_LIVE_UNCHANGED &&
              strcmp(level(l, buf, sizeof buf), "info") == 0,
          "the unchanged file does not bring it back");
    check(spill("cf_probation.ini", "; touched\n[log]\nlevel = debug\n"),
          "touched it, the same values");
    check(cf_live_poll(l, &err) == CF_LIVE_UNCHANGED &&
              strcmp(level(l, buf, sizeof buf), "info") == 0 &&
              cf_live_generation(l) == 3,
          "nor do the same values written again");
    check(spill("cf_probation.ini", "[log]\nlevel = warn\n"), "a real change");
    check(cf_live_poll(l, &err) == CF_LIVE_SWAPPED &&
              strcmp(level(l, buf, sizeof buf), "warn") == 0,
          "is swapped in, on probation again");
    cf_live_close(l);
    g_file = NULL;
    remove("cf_probation.ini");
}

int main(void) {
    test_probation();
    test_probation_file();
    test_options();
    test_auto_and_manual();
    test_literal_pin();
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
