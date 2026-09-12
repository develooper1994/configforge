/* test_explain.c - the feature the library exists for. */
#include "configforge/cf_explain.h"
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
    if (strstr(hay, needle) == NULL) {
        ++g_fail;
        printf("FAIL: %s  missing \"%s\" in:\n%s\n", w, needle, hay);
    }
}

int main(void) {
    static cf_pair defaults[] = {{"server.port", "8080"}, {"server.host", "0.0.0.0"}};
    static char    e0[]       = "MYAPP_SERVER__PORT=9090";
    static char    a0[]       = "myapp";
    static char*   env[]      = {e0, NULL};
    static char*   argv[]     = {a0, NULL};

    cf_arena*        ar = cf_arena_new(NULL, 0);
    cf_source        def, e, c;
    const cf_source* list[3];
    cf_source_view   sv;
    cf_keyspace      ks;
    static char      buf[2048];
    size_t           n;

    cf_ks_init(&ks, ar, 8);
    cf_source_cli(&c, "cli", CF_RANK_CLI, 1, argv);
    cf_source_env(&e, "env", CF_RANK_ENV, "MYAPP_", env);
    cf_source_memory(&def, "default", CF_RANK_DEFAULT, defaults, 2);
    cf_source_load(&c, &ks);
    cf_source_load(&e, &ks);
    cf_source_load(&def, &ks);
    cf_ks_resolve(&ks);
    cf_ks_infer_untyped(&ks);

    list[0]  = &c;
    list[1]  = &e;
    list[2]  = &def;
    sv.items = list;
    sv.count = 3;

    n = cf_ks_explain(&ks, &sv, "server.port", 11, buf, sizeof buf);
    check(n > 0, "explain produces output");
    printf("%s", buf);

    has(buf, "server.port = 9090", "shows the resolved value");
    has(buf, "(int)", "shows the type");
    has(buf, "env", "names the winning source");
    has(buf, "selected", "marks the winner");
    has(buf, "MYAPP_SERVER__PORT", "shows where to change it");
    has(buf, "8080", "keeps the losing candidate");
    has(buf, "shadowed", "says why it lost");

    /* Sizing by calling with no buffer must agree with the real run. */
    check(cf_ks_explain(&ks, &sv, "server.port", 11, NULL, 0) == n,
          "length is reported without a buffer");

    check(cf_ks_explain(&ks, &sv, "no.such.key", 11, buf, sizeof buf) == 0,
          "absent key explains nothing");

    /* Truncation must terminate rather than run over. */
    {
        char   small[16];
        size_t big = cf_ks_explain(&ks, &sv, "server.port", 11, small, sizeof small);
        check(big == n, "truncated call still reports full length");
        check(small[sizeof small - 1] == '\0', "truncated output terminates");
    }

    n = cf_ks_inspect(&ks, &sv, buf, sizeof buf);
    check(n > 0, "inspect produces output");
    has(buf, "server.host", "inspect covers every key");
    /* The cli source is configured and supplied nothing. That is
       usually benign - a command line with no options - but it is also
       what a misspelled prefix looks like, so it gets a quiet line. */
    has(buf, "source 'cli' supplied nothing", "empty source is reported");

    /* The dangerous case is different and must not be conflated with
       it: a source that supplied values and won none of them means
       someone is editing something that has no effect. */
    {
        static cf_pair   dead_pairs[] = {{"server.port", "1"}, {"server.host", "x"}};
        cf_arena*        a2           = cf_arena_new(NULL, 0);
        cf_keyspace      k2;
        cf_source        hi, lo;
        const cf_source* l2[2];
        cf_source_view   v2;
        static char      b2[2048];

        cf_ks_init(&k2, a2, 8);
        cf_source_memory(&hi, "override", 0, defaults, 2);
        cf_source_memory(&lo, "user-file", 20, dead_pairs, 2);
        cf_source_load(&hi, &k2);
        cf_source_load(&lo, &k2);
        cf_ks_resolve(&k2);
        cf_ks_infer_untyped(&k2);

        l2[0]    = &hi;
        l2[1]    = &lo;
        v2.items = l2;
        v2.count = 2;
        cf_ks_inspect(&k2, &v2, b2, sizeof b2);

        has(b2, "user-file", "shadowed source named");
        has(b2, "supplied 2 values and won none", "shadowed source reported");
        has(b2, "every one is shadowed", "and why it matters");
        cf_arena_free(a2);
    }

    cf_arena_free(ar);
    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
