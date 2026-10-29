/*
 * test_interpolate.c - references, ${key} (D-10): precedence first,
 * references after, the result typed by the contract, and every
 * refusal naming the key - a cycle, a key with no value, a secret a
 * plain key would show.
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
static void has(const char* hay, const char* needle, const char* w) {
    ++g_run;
    if (hay == NULL || strstr(hay, needle) == NULL) {
        ++g_fail;
        printf("FAIL: %s  missing \"%s\" in \"%s\"\n", w, needle, hay ? hay : "(null)");
    }
}

/* A configuration from `n` pairs in a file-like source and, above it,
   `m` more as if typed on a command line. */
static cf_config* build(const cf_pair* file, size_t n, const cf_pair* cli, size_t m,
                        cf_error* err) {
    cf_builder* b = cf_build(NULL);

    cf_builder_memory(b, "file", CF_RANK_USER_FILE, file, n);
    if (m > 0) {
        cf_builder_memory(b, "cli", CF_RANK_CLI, cli, m);
    }
    cf_builder_text(b, "server.host", "localhost", 0);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_builder_text(b, "server.url", NULL, 0);
    cf_builder_text(b, "db.password", NULL, CF_SENSITIVE);
    cf_builder_text(b, "db.dsn", NULL, CF_SENSITIVE);
    cf_builder_text(b, "db.leak", NULL, 0);
    cf_builder_int(b, "admin.port", 1, 1, 65535, 0);
    return cf_builder_create(b, err);
}

static void test_basic(void) {
    static const cf_pair file[] = {
        {"server.host", "web1"},
        {"server.port", "9443"},
        {"server.url", "https://${server.host}:${server.port}/"},
        {"admin.port", "${server.port}"}};
    static char ex[2048];
    cf_error    err = CF_ERROR_INIT;
    cf_config*  c   = build(file, 4, NULL, 0, &err);

    check(c != NULL, "references build");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    has(cf_get_text(c, "server.url", ""), "https://web1:9443/",
        "each ${key} is its value");
    check(cf_get_int(c, "admin.port", 0) == 9443,
          "and the result is typed by the contract");
    cf_explain(c, "server.url", ex, sizeof ex);
    has(ex, "from \"https://${server.host}:${server.port}/\"",
        "explain() shows the template");
    cf_config_free(c);
}

/* Precedence chooses each key first; a reference reads the winner. */
static void test_precedence_first(void) {
    static const cf_pair file[] = {{"server.host", "web1"},
                                   {"server.url", "http://${server.host}"}};
    static const cf_pair cli[]  = {{"server.host", "web2"}};
    cf_error             err    = CF_ERROR_INIT;
    cf_config*           c      = build(file, 2, cli, 1, &err);

    check(c != NULL && strcmp(cf_get_text(c, "server.url", ""), "http://web2") == 0,
          "a reference reads what a program reading that key would");
    cf_config_free(c);
}

static void test_chain_and_escape(void) {
    static const cf_pair file[] = {
        {"a.one", "${a.two}"}, {"a.two", "${a.three}-$${literal}"}, {"a.three", "x"}};
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = build(file, 3, NULL, 0, &err);

    check(c != NULL, "a reference to a reference builds");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    has(cf_get_text(c, "a.one", ""), "x-${literal}", "each finished before it is read");
    check(strcmp(cf_get_text(c, "a.two", ""), "x-${literal}") == 0,
          "and $${ stays a literal");
    cf_config_free(c);
}

static void test_refusals(void) {
    static const cf_pair cycle[] = {{"a.one", "${a.two}"}, {"a.two", "<${a.one}>"}};
    static const cf_pair none[]  = {{"server.url", "http://${server.hots}"}};
    static const cf_pair leak[]  = {{"db.password", "hunter2"},
                                    {"db.leak", "pw=${db.password}"}};
    static const cf_pair dsn[]   = {{"db.password", "hunter2"},
                                    {"db.dsn", "postgres://u:${db.password}@db"}};
    static const cf_pair bad[]   = {{"server.url", "${HOME}/x"}};
    static char          ex[2048];
    cf_error             err = CF_ERROR_INIT;
    cf_config*           c;

    c = build(cycle, 2, NULL, 0, &err);
    check(c == NULL, "a cycle is refused");
    has(err.message, "lead back to it", "and called one");
    cf_config_free(c);

    c = build(none, 1, NULL, 0, &err);
    check(c == NULL, "a reference to a key with no value is refused");
    has(err.message, "${server.hots} names a key with no value", "naming it");
    cf_config_free(c);

    c = build(leak, 2, NULL, 0, &err);
    check(c == NULL, "a plain key may not show a secret");
    has(err.message, "would show the secret", "and says why");
    cf_config_free(c);

    c = build(dsn, 2, NULL, 0, &err);
    check(c != NULL, "a sensitive key may refer to one");
    if (c != NULL) {
        cf_explain(c, "db.dsn", ex, sizeof ex);
        check(strstr(ex, "hunter2") == NULL && strstr(ex, "${db.password}") == NULL,
              "and neither its value nor its template is explained");
    }
    cf_config_free(c);

    c = build(bad, 1, NULL, 0, &err);
    check(c == NULL, "a reference naming no key is refused");
    has(err.message, "$${", "saying how to write a literal");
    cf_config_free(c);
}

/* A result the contract refuses falls back like any refused value. */
static void test_fallback(void) {
    static const cf_pair file[] = {{"server.host", "web1"},
                                   {"workers", "${server.host}"}};
    static char          ex[2048];
    cf_error             err = CF_ERROR_INIT;
    cf_builder*          b   = cf_build(NULL);
    cf_config*           c;
    cf_def               d;

    cf_builder_memory(b, "file", CF_RANK_USER_FILE, file, 2);
    cf_builder_text(b, "server.host", NULL, 0);
    cf_def_init(&d, CF_TYPE_INT, "workers");
    d.has_default = CF_TRUE;
    cf_value_int(&d.default_value, 4);
    d.on_invalid = CF_INVALID_FALLBACK;
    cf_builder_define(b, &d);
    c = cf_builder_create(b, &err);
    check(c != NULL && cf_get_int(c, "workers", 0) == 4,
          "an expanded value of the wrong type hands over to the default");
    if (c != NULL) {
        cf_explain(c, "workers", ex, sizeof ex);
        has(ex, "\"web1\" is not a valid int", "with the reason in the chain");
    }
    cf_config_free(c);
}

/* Tier 0: no contract, so the result is inferred, as any text is. */
static void test_tier0(void) {
    static const cf_pair file[] = {{"base", "40"}, {"twice", "${base}"}};
    cf_error             err    = CF_ERROR_INIT;
    cf_builder*          b      = cf_build(NULL);
    cf_config*           c;

    cf_builder_memory(b, "file", CF_RANK_USER_FILE, file, 2);
    c = cf_builder_create(b, &err);
    check(c != NULL && cf_type_of(c, "twice") == CF_TYPE_INT &&
              cf_get_int(c, "twice", 0) == 40,
          "an undeclared result is inferred");
    cf_config_free(c);
}

/* The report records the template, and a replay is the same
   configuration without reading it again. */
static void test_report(void) {
    static const cf_pair file[] = {{"server.host", "web1"},
                                   {"server.url", "http://${server.host}"}};
    static char          rep[16384];
    cf_error             err = CF_ERROR_INIT;
    cf_config*           c   = build(file, 2, NULL, 0, &err);
    cf_config*           back;
    cf_builder*          b;
    cf_report_info       info;
    FILE*                f;

    if (c == NULL) {
        check(0, "the report test builds");
        return;
    }
    if (!cf_has_feature("report") || !cf_has_feature("json")) {
        cf_config_free(c); /* a build without the report or its reader */
        return;
    }
    memset(&info, 0, sizeof info);
    info.mode = CF_REPORT_FULL;
    cf_report(c, &info, rep, sizeof rep);
    has(rep, "\"written\": \"http://${server.host}\"",
        "the report records the template");
    f = fopen("cf_interp_rep.json", "wb");
    if (f != NULL) {
        fputs(rep, f);
        fclose(f);
    }
    b = cf_build(NULL);
    cf_builder_replay(b, "replay", CF_RANK_USER_FILE, "cf_interp_rep.json");
    back = cf_builder_create(b, &err);
    check(back != NULL &&
              strcmp(cf_get_text(back, "server.url", ""), "http://web1") == 0,
          "a replay keeps the result and does not read it again");
    cf_config_free(back);
    cf_config_free(c);
    remove("cf_interp_rep.json");
}

int main(void) {
    test_basic();
    test_precedence_first();
    test_chain_and_escape();
    test_refusals();
    test_fallback();
    test_tier0();
    test_report();
    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
