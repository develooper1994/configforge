/*
 * test_iterate.c - walking a prefix.
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
    static char  a1[]   = "--server.tags.0=alpha";
    static char  a2[]   = "--server.tags.1=beta";
    static char  a3[]   = "--server.tags.count=2";
    static char  a4[]   = "--plugins.auth.enabled=true";
    static char  a5[]   = "--plugins.auth.timeout=30";
    static char  a6[]   = "--plugins.cache.size=64";
    static char  a7[]   = "--servername=decoy";
    static char  a8[]   = "--server.port=8080";
    static char* argv[] = {a0, a1, a2, a3, a4, a5, a6, a7, a8, NULL};
    cf_builder*  b      = cf_build(NULL);

    cf_builder_cli(b, "cli", CF_RANK_CLI, 9, argv);
    return cf_builder_create(b, err);
}

/* Collects the names an iterator yields, so a test asserts the sequence
   rather than one element of it. */
static size_t collect(cf_iter* it, char out[][64], size_t max) {
    size_t n = 0;
    while (n < max && cf_iter_next(it)) {
        size_t len = (it->namelen < 63) ? it->namelen : 63;
        memcpy(out[n], it->name, len);
        out[n][len] = '\0';
        ++n;
    }
    return n;
}

static void test_keys_under_a_prefix(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_config*  c   = build(&err);
    cf_iter     it;
    static char got[16][64];
    size_t      n;

    check(c != NULL, "builds");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }

    check(cf_iter_keys(&it, c, "plugins"), "iterator starts");
    n = collect(&it, got, 16);
    check(n == 3, "three keys under plugins");
    check(strcmp(got[0], "auth.enabled") == 0, "in order, first");
    check(strcmp(got[1], "auth.timeout") == 0, "second");
    check(strcmp(got[2], "cache.size") == 0, "third");

    /* A prefix matches at a segment boundary only. "servername" is the
       trap every hand-written prefix check falls into. */
    cf_iter_keys(&it, c, "server");
    n = collect(&it, got, 16);
    check(n == 4, "server has four keys, not five");
    {
        size_t i;
        int    leaked = 0;
        for (i = 0; i < n; ++i) {
            if (strstr(got[i], "decoy") != NULL) {
                leaked = 1;
            }
        }
        check(!leaked, "servername is not under server");
    }

    /* An empty prefix is the whole configuration. */
    cf_iter_keys(&it, c, NULL);
    n = collect(&it, got, 16);
    check(n == 8, "no prefix walks everything: four, three and the decoy");

    cf_iter_keys(&it, c, "no.such.prefix");
    check(!cf_iter_next(&it), "an absent prefix yields nothing");

    cf_config_free(c);
}

static void test_children(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_config*  c   = build(&err);
    cf_iter     it;
    static char got[16][64];
    size_t      n;

    if (c == NULL) {
        check(0, "builds");
        return;
    }

    /* How a program discovers what is configured rather than being told
       in advance, which is what plugin configuration needs. */
    check(cf_iter_children(&it, c, "plugins"), "children iterator starts");
    n = collect(&it, got, 16);
    check(n == 2, "two plugins, not three keys");
    check(strcmp(got[0], "auth") == 0, "auth");
    check(strcmp(got[1], "cache") == 0, "cache");

    /* A child that is a table has no value of its own; one that is a
       leaf reports it. */
    cf_iter_children(&it, c, "server");
    while (cf_iter_next(&it)) {
        if (it.namelen == 4 && memcmp(it.name, "port", 4) == 0) {
            check(it.value != NULL && it.value->type == CF_TYPE_INT,
                  "a leaf child carries its value");
        }
        if (it.namelen == 4 && memcmp(it.name, "tags", 4) == 0) {
            check(it.value == NULL, "a table child carries none");
        }
    }
    cf_config_free(c);
}

static void test_arrays(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = build(&err);
    char       key[CF_KEY_MAX];
    size_t     i, n;

    if (c == NULL) {
        check(0, "builds");
        return;
    }
    n = cf_array_count(c, "server.tags");
    check(n == 2, "the recorded length");

    for (i = 0; i < n; ++i) {
        cf_array_key(key, sizeof key, "server.tags", i);
        check(cf_has(c, key), "each element is reachable");
    }
    cf_array_key(key, sizeof key, "server.tags", 0);
    check(strcmp(cf_get_text(c, key, ""), "alpha") == 0, "element zero");

    check(cf_array_count(c, "no.such.array") == 0, "an absent array is empty");
    /* Not an array at all: a scalar has no length key, and reporting
       one would invent a structure that is not there. */
    check(cf_array_count(c, "server.port") == 0, "a scalar is not an array");

    cf_config_free(c);
}

static void test_shortened_array(void) {
    /* The reason the length is recorded rather than counted: a
       high-precedence source that shortens an array leaves the longer
       one's extra elements in the key space, and counting the index
       keys present would report the union of two arrays. */
    static char  a1[]   = "--t.0=x";
    static char  a2[]   = "--t.count=1";
    static char* argv[] = {a0, a1, a2, NULL};
    static char  p0[]   = "T__0=old";
    static char  p1[]   = "T__1=ghost";
    static char  p2[]   = "T__2=ghost";
    static char  p3[]   = "T__COUNT=3";
    static char* env[]  = {p0, p1, p2, p3, NULL};
    cf_error     err    = CF_ERROR_INIT;
    cf_builder*  b      = cf_build(NULL);
    cf_config*   c;

    cf_builder_cli(b, "cli", CF_RANK_CLI, 3, argv);
    cf_builder_env(b, "env", CF_RANK_ENV, "T", env);
    c = cf_builder_create(b, &err);
    if (c == NULL) {
        check(0, "builds");
        printf("  %s\n", err.message);
        return;
    }
    check(cf_array_count(c, "t") == 1,
          "the winning length wins, not the count of keys present");
    cf_config_free(c);
}

int main(void) {
    test_keys_under_a_prefix();
    test_children();
    test_arrays();
    test_shortened_array();

    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
