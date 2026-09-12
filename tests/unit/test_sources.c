/* test_sources.c - memory, environment and command line. */
#include "configforge/cf_source.h"
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

static void key_is(const char* prefix, const char* var, const char* want,
                   const char* what) {
    char   out[CF_KEY_MAX];
    size_t n = cf_env_key(prefix, var, strlen(var), out, sizeof out);
    ++g_run;
    if (want == NULL) {
        if (n != 0) {
            ++g_fail;
            printf("FAIL: %s expected skip, got \"%s\"\n", what, out);
        }
        return;
    }
    if (n == 0 || strcmp(out, want) != 0) {
        ++g_fail;
        printf("FAIL: %s got=\"%s\" want=\"%s\"\n", what, (n ? out : "<skip>"), want);
    }
}

static void test_env_key(void) {
    /* A double underscore is the separator; a single one is literal.
       Mapping a single underscore to a dot cannot be reversed. */
    key_is("MYAPP_", "MYAPP_SERVER__PORT", "server.port", "double underscore is a dot");
    key_is("MYAPP_", "MYAPP_TLS_MIN_VERSION", "tls_min_version",
           "single underscore is literal");
    key_is("MYAPP_", "MYAPP_A__B__C", "a.b.c", "three segments");
    key_is("MYAPP_", "MYAPP_PORT", "port", "single segment");

    key_is("MYAPP_", "OTHER_PORT", NULL, "wrong prefix skipped");
    key_is("MYAPP_", "MYAPP_", NULL, "prefix alone skipped");
    key_is("MYAPP_", "MYAPP_A B", NULL, "space skipped");
    key_is("MYAPP_", "MYAPP___X", NULL, "leading dot skipped");
    key_is(NULL, "PATH", "path", "no prefix takes everything");
}

static void test_env_source(cf_arena* ar) {
    /* Mutable arrays rather than literals: the source takes the same
       char* const* that the platform environment is, so that callers
       never have to cast. The tests absorb the awkwardness instead. */
    static char     e0[]  = "MYAPP_SERVER__PORT=9090";
    static char     e1[]  = "MYAPP_SERVER__HOST=0.0.0.0";
    static char     e2[]  = "MYAPP_TLS__ENABLED=true";
    static char     e3[]  = "PATH=/usr/bin";   /* wrong prefix: skipped */
    static char     e4[]  = "MYAPP_BAD KEY=1"; /* invalid key: skipped  */
    static char     e5[]  = "MYAPP_NOEQUALS";  /* malformed: skipped    */
    static char*    env[] = {e0, e1, e2, e3, e4, e5, NULL};
    cf_source       s;
    cf_keyspace     ks;
    const cf_value* v;
    const cf_entry* e;

    cf_ks_init(&ks, ar, 8);
    check(cf_source_env(&s, "env", CF_RANK_ENV, "MYAPP_", env), "env source built");
    check(cf_source_load(&s, &ks), "env source loads");
    cf_ks_resolve(&ks);
    cf_ks_infer_untyped(&ks);

    check(cf_ks_count(&ks) == 3, "three usable variables");

    v = cf_ks_get(&ks, "server.port", 11);
    check(v != NULL && v->type == CF_TYPE_INT && v->u.i == 9090,
          "port inferred as int");

    v = cf_ks_get(&ks, "tls.enabled", 11);
    check(v != NULL && v->type == CF_TYPE_BOOL && v->u.b, "bool inferred");

    v = cf_ks_get(&ks, "server.host", 11);
    check(v != NULL && v->type == CF_TYPE_TEXT, "dotted address stays text");

    /* The location must be the spelling the reader would have to
       change, not the word "environment". */
    e = cf_ks_find(&ks, "server.port", 11);
    check(e->chain->at != NULL && strcmp(e->chain->at, "MYAPP_SERVER__PORT") == 0,
          "location is the variable name");
}

static void test_cli_source(cf_arena* ar) {
    static char     a0[]   = "myapp";
    static char     a1[]   = "--server.port=8080";
    static char     a2[]   = "--server.host=127.0.0.1";
    static char     a3[]   = "ignored";
    static char     a4[]   = "--tls.enabled";
    static char     a5[]   = "positional";
    static char     a6[]   = "--";
    static char     a7[]   = "--not.parsed=1";
    static char*    argv[] = {a0, a1, a2, a3, a4, a5, a6, a7, NULL};
    cf_source       s;
    cf_keyspace     ks;
    const cf_value* v;
    const cf_entry* e;

    cf_ks_init(&ks, ar, 8);
    check(cf_source_cli(&s, "cli", CF_RANK_CLI, 8, argv), "cli source built");
    check(cf_source_load(&s, &ks), "cli source loads");
    cf_ks_resolve(&ks);
    cf_ks_infer_untyped(&ks);

    v = cf_ks_get(&ks, "server.port", 11);
    check(v != NULL && v->u.i == 8080, "equals form");
    v = cf_ks_get(&ks, "server.host", 11);
    check(v != NULL && v->type == CF_TYPE_TEXT, "equals form with text");
    v = cf_ks_get(&ks, "tls.enabled", 11);
    check(v != NULL && v->type == CF_TYPE_BOOL && v->u.b, "bare flag is true");

    check(cf_ks_get(&ks, "not.parsed", 10) == NULL, "-- ends parsing");
    check(cf_ks_count(&ks) == 3, "positional arguments ignored");

    e = cf_ks_find(&ks, "server.port", 11);
    check(e->chain->at != NULL && strcmp(e->chain->at, "argv[1]") == 0,
          "location is the argument index");
}

static void test_cli_rejects_bad_option(cf_arena* ar) {
    static char  b0[]   = "myapp";
    static char  b1[]   = "--Server.Port=1";
    static char* argv[] = {b0, b1, NULL};
    cf_source    s;
    cf_keyspace  ks;

    cf_ks_init(&ks, ar, 4);
    cf_source_cli(&s, "cli", CF_RANK_CLI, 2, argv);
    /* A misspelled option is the user's typo. Ignoring it silently is
       how a wrong flag becomes an hour of confusion. */
    check(!cf_source_load(&s, &ks), "invalid option is rejected, not ignored");
}

static void test_precedence_across_sources(cf_arena* ar) {
    static cf_pair defaults[] = {
        {"server.port", "8080"}, {"server.host", "0.0.0.0"}, {"log.level", "info"}};
    static char  pe[]   = "MYAPP_SERVER__PORT=9090";
    static char  pa0[]  = "myapp";
    static char  pa1[]  = "--log.level=debug";
    static char* env[]  = {pe, NULL};
    static char* argv[] = {pa0, pa1, NULL};

    cf_source       def, e, c;
    cf_keyspace     ks;
    const cf_value* v;

    cf_ks_init(&ks, ar, 8);
    cf_source_memory(&def, "default", CF_RANK_DEFAULT, defaults, 3);
    cf_source_env(&e, "env", CF_RANK_ENV, "MYAPP_", env);
    cf_source_cli(&c, "cli", CF_RANK_CLI, 2, argv);

    /* Loaded in the wrong order on purpose: precedence is a rank, not
       an arrival order. */
    check(cf_source_load(&def, &ks), "defaults load");
    check(cf_source_load(&c, &ks), "cli loads");
    check(cf_source_load(&e, &ks), "env loads");
    cf_ks_resolve(&ks);
    cf_ks_infer_untyped(&ks);

    v = cf_ks_get(&ks, "server.port", 11);
    check(v != NULL && v->u.i == 9090, "env beats default");
    v = cf_ks_get(&ks, "log.level", 9);
    check(v != NULL && v->type == CF_TYPE_TEXT && strcmp(v->u.t.p, "debug") == 0,
          "cli beats default");
    v = cf_ks_get(&ks, "server.host", 11);
    check(v != NULL && strcmp(v->u.t.p, "0.0.0.0") == 0, "default survives");

    check(cf_ks_is_sorted(&ks), "space stays sorted across sources");
}

int main(void) {
    cf_arena* ar = cf_arena_new(NULL, 0);

    test_env_key();
    test_env_source(ar);
    test_cli_source(ar);
    test_cli_rejects_bad_option(ar);
    test_precedence_across_sources(ar);

    cf_arena_free(ar);
    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
