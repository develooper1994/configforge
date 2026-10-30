/*
 * test_log.c - cf_log: what the library says that is not an error, and
 * to whom. Unknown keys under CF_UNKNOWN_WARN, deprecated keys in use,
 * values refused and fallen back from, a derived configuration logging
 * where its original did, the floor a sink asks for, and cf_live's
 * reloads - each event with its level, stable name, key and source.
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

/* What the sink was handed, kept: an event's strings live only for the
   call. */
#define MAX_SEEN 16
typedef struct seen {
    cf_log_level level;
    cf_code      code;
    char         event[32];
    char         key[64];
    char         message[CF_MESSAGE_MAX];
} seen;

static seen   g_seen[MAX_SEEN];
static size_t g_count;

static void sink(void* user, const cf_log_event* e) {
    seen* s;
    (void)user;
    if (g_count >= MAX_SEEN || e->size < sizeof *e) {
        return;
    }
    s        = &g_seen[g_count++];
    s->level = e->level;
    s->code  = e->code;
    cf_fmt(s->event, sizeof s->event, "%s", e->event);
    cf_fmt(s->key, sizeof s->key, "%s", (e->key != NULL) ? e->key : "");
    cf_fmt(s->message, sizeof s->message, "%s", e->message);
}

static const seen* find(const char* event, const char* key) {
    size_t i;
    for (i = 0; i < g_count; ++i) {
        if (strcmp(g_seen[i].event, event) == 0 &&
            (key == NULL || strcmp(g_seen[i].key, key) == 0)) {
            return &g_seen[i];
        }
    }
    return NULL;
}

static cf_config* build(cf_log_level min) {
    static const cf_pair pairs[] = {{"server.port", "8080"},
                                    {"log.levl", "debug"},
                                    {"old.host", "a"},
                                    {"pool.size", "999"},
                                    {"db.pin", "1234567"}};
    cf_error             err     = CF_ERROR_INIT;
    cf_builder*          b       = cf_build(NULL);
    cf_def               d;

    cf_builder_memory(b, "file", CF_RANK_USER_FILE, pairs, 5);
    cf_builder_int(b, "server.port", 80, 1, 65535, 0);
    cf_def_init(&d, CF_TYPE_TEXT, "old.host");
    d.flags = CF_DEPRECATED;
    cf_builder_define(b, &d);
    cf_def_init(&d, CF_TYPE_INT, "db.pin");
    d.flags              = CF_SENSITIVE;
    d.has_range          = CF_TRUE;
    d.imin               = 1;
    d.imax               = 9999;
    d.has_default        = CF_TRUE;
    d.default_value.type = CF_TYPE_INT;
    d.default_value.u.i  = 1;
    d.on_invalid         = CF_INVALID_FALLBACK;
    cf_builder_define(b, &d);
    cf_def_init(&d, CF_TYPE_INT, "pool.size");
    d.has_range          = CF_TRUE;
    d.imin               = 1;
    d.imax               = 64;
    d.has_default        = CF_TRUE;
    d.default_value.type = CF_TYPE_INT;
    d.default_value.u.i  = 4;
    d.on_invalid         = CF_INVALID_FALLBACK;
    cf_builder_define(b, &d);
    cf_builder_unknown(b, CF_UNKNOWN_WARN);
    cf_builder_log(b, sink, NULL, min);
    return cf_builder_create(b, &err);
}

static void test_create(void) {
    cf_config*           c;
    cf_config*           d;
    const seen*          s;
    cf_error             err    = CF_ERROR_INIT;
    static const cf_pair more[] = {{"another.typo", "1"}};

    g_count = 0;
    c       = build(CF_LOG_INFO);
    check(c != NULL, "created");
    s = find("unknown-key", "log.levl");
    check(s != NULL && s->level == CF_LOG_WARN && s->code == CF_E_KEY &&
              strstr(s->message, "file") != NULL,
          "an unknown key is a warning, naming its source");
    s = find("deprecated-key", "old.host");
    check(s != NULL && s->level == CF_LOG_WARN, "a deprecated key in use is one");
    s = find("fallback", "pool.size");
    check(s != NULL && strstr(s->message, "refused") != NULL,
          "and a value refused and fallen back from");
    s = find("fallback", "db.pin");
    check(s != NULL && strstr(s->message, "1234567") == NULL,
          "a secret's refused value never reaches the log");
    check(find("unknown-key", "server.port") == NULL, "a defined key is not unknown");

    g_count = 0;
    d       = cf_derive(c, "admin", more, 1, &err);
    check(d != NULL && find("unknown-key", "another.typo") != NULL,
          "a derived configuration logs where its original did");
    cf_config_free(d);
    cf_config_free(c);

    g_count = 0;
    c       = build(CF_LOG_ERROR);
    check(c != NULL && g_count == 0, "nothing under the floor the sink asked for");
    cf_config_free(c);
    check(strcmp(cf_log_level_name(CF_LOG_WARN), "warn") == 0, "levels have names");
}

/* cf_live: each reload's result, and the key waiting for a restart. */
static const cf_pair  g_v1[]  = {{"server.port", "8080"}, {"log.level", "info"}};
static const cf_pair  g_v2[]  = {{"server.port", "9443"}, {"log.level", "debug"}};
static const cf_pair  g_bad[] = {{"server.port", "0"}, {"log.level", "info"}};
static const cf_pair* g_pairs = g_v1;

static cf_builder* live_build(void* user) {
    cf_builder* b = cf_build(NULL);
    (void)user;
    cf_builder_memory(b, "mem", CF_RANK_USER_FILE, g_pairs, 2);
    cf_builder_int(b, "server.port", 8080, 1, 65535, CF_RESTART);
    cf_builder_text(b, "log.level", "info", 0);
    return b;
}

static void test_live(void) {
    cf_error        err = CF_ERROR_INIT;
    cf_live_options o   = CF_LIVE_OPTIONS_INIT;
    cf_live*        l;
    const seen*     s;

    g_pairs   = g_v1;
    o.build   = live_build;
    o.log     = sink;
    o.log_min = CF_LOG_DEBUG;
    l         = cf_live_open_with(&o, &err);
    check(l != NULL, "live: opens");
    if (l == NULL) {
        return;
    }
    g_count = 0;
    g_pairs = g_v2;
    (void)cf_live_reload(l, &err);
    s = find("reload", NULL);
    check(s != NULL && s->level == CF_LOG_INFO &&
              strstr(s->message, "generation 2") != NULL,
          "a swap is logged with its generation");
    s = find("restart-pending", "server.port");
    check(s != NULL && s->level == CF_LOG_WARN, "and the key waiting for a restart");
    g_count = 0;
    (void)cf_live_reload(l, &err);
    s = find("reload", NULL);
    check(s != NULL && s->level == CF_LOG_DEBUG, "nothing changed is debug");
    g_count = 0;
    g_pairs = g_bad;
    (void)cf_live_reload(l, &err);
    s = find("reload-failed", NULL);
    check(s != NULL && s->level == CF_LOG_ERROR && s->code == CF_E_VALIDATE,
          "a failed reload is an error, with its code");
    cf_live_close(l);
}

int main(void) {
    test_create();
    test_live();
    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
