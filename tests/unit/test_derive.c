/*
 * test_derive.c - changing a value without mutating a snapshot.
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
        printf("FAIL: %s  missing \"%s\"\n", w, needle);
    }
}

static char  a0[]     = "prog";
static char  a1[]     = "--server.port=8080";
static char* g_argv[] = {a0, a1, NULL};
static char  e0[]     = "MYAPP_SERVER__HOST=10.0.0.1";
static char* g_env[]  = {e0, NULL};

static cf_config* build(cf_error* err) {
    cf_builder* b = cf_build(NULL);

    cf_builder_cli(b, "cli", CF_RANK_CLI, 2, g_argv);
    cf_builder_env(b, "env", CF_RANK_ENV, "MYAPP_", g_env);
    cf_builder_int(b, "server.port", 1234, 1, 65535, 0);
    cf_builder_text(b, "server.host", "0.0.0.0", 0);
    cf_builder_text(b, "log.level", "info", 0);
    return cf_builder_create(b, err);
}

static void test_derive(void) {
    static const cf_pair change[] = {{"server.port", "9000"}};
    cf_error             err      = CF_ERROR_INIT;
    cf_config*           base     = build(&err);
    cf_config*           next;
    static char          buf[4096];

    check(base != NULL, "builds");
    if (base == NULL) {
        printf("  %s\n", err.message);
        return;
    }

    next = cf_derive(base, "runtime", change, 1, &err);
    check(next != NULL, "derives");
    if (next == NULL) {
        printf("  %s\n", err.message);
        cf_config_free(base);
        return;
    }

    check(cf_get_int(next, "server.port", 0) == 9000, "the new value");
    /* The original is unchanged, which is the whole reason a snapshot is
       immutable: a reader holding it does not have it move underneath
       them. */
    check(cf_get_int(base, "server.port", 0) == 8080, "and the old one is");
    check(cf_digest(base) != cf_digest(next), "two different configurations");

    /* Everything nobody touched came across, at the rank it had. */
    check(strcmp(cf_get_text(next, "server.host", ""), "10.0.0.1") == 0,
          "the environment's value survived");
    check(strcmp(cf_get_text(next, "log.level", ""), "info") == 0,
          "and so did the default");
    check(cf_count(next) == cf_count(base), "same keys, no more and no fewer");

    /* Provenance survives, which is the question people ask right after
       changing a setting: what was it before, and where did that come
       from. */
    cf_explain(next, "server.port", buf, sizeof buf);
    has(buf, "runtime", "the new value names its source");
    has(buf, "9000", "and its value");
    has(buf, "8080", "the displaced value is still in the chain");
    has(buf, "argv[1]", "with where it originally came from");
    has(buf, "shadowed", "marked as having lost");

    /* Independent: freeing one does not disturb the other. */
    cf_config_free(base);
    check(cf_get_int(next, "server.port", 0) == 9000,
          "the derived configuration outlives the one it came from");
    cf_explain(next, "server.host", buf, sizeof buf);
    has(buf, "10.0.0.1", "including its inherited text");
    cf_config_free(next);
}

/* Deriving keeps the contract, or it would accept what the original
   refused. */
static void test_contract_travels(void) {
    static const cf_pair bad[]     = {{"server.port", "99999"}};
    static const cf_pair unknown[] = {{"nonesuch.key", "1"}};
    cf_error             err       = CF_ERROR_INIT;
    cf_config*           base      = build(&err);
    cf_config*           next;

    if (base == NULL) {
        check(0, "builds");
        return;
    }
    next = cf_derive(base, "runtime", bad, 1, &err);
    check(next == NULL, "a value outside the range is refused");
    check(err.code == CF_E_VALIDATE, "as a validation error");
    has(err.message, "65535", "naming the bound");

    /* The unknown-key policy travels too. The base ignores them, so the
       derived one must, or the same program would behave differently
       after a setting changed. */
    next = cf_derive(base, "runtime", unknown, 1, &err);
    check(next != NULL, "an unknown key follows the inherited policy");
    if (next != NULL) {
        cf_config_free(next);
    }
    cf_config_free(base);
}

static void test_derive_chain(void) {
    static const cf_pair one[] = {{"server.port", "9000"}};
    static const cf_pair two[] = {{"server.port", "9001"}};
    cf_error             err   = CF_ERROR_INIT;
    cf_config*           a     = build(&err);
    cf_config*           b;
    cf_config*           c;
    static char          buf[4096];

    if (a == NULL) {
        check(0, "builds");
        return;
    }
    b = cf_derive(a, "first", one, 1, &err);
    c = (b != NULL) ? cf_derive(b, "second", two, 1, &err) : NULL;
    check(c != NULL, "deriving from a derived configuration");
    if (c == NULL) {
        cf_config_free(a);
        return;
    }
    check(cf_get_int(c, "server.port", 0) == 9001, "the latest value wins");

    /* The chain grows rather than collapsing: three answers to "where
       did this come from" are better than one. */
    cf_explain(c, "server.port", buf, sizeof buf);
    has(buf, "9001", "the newest");
    has(buf, "9000", "the one before it");
    has(buf, "8080", "and the one the command line gave");

    cf_config_free(a);
    cf_config_free(b);
    cf_config_free(c);
}

/* A configuration built in a caller's buffer has no allocator to
   borrow, and falling back to malloc is the one thing a target with no
   heap cannot survive. */
static void test_fixed_arena(void) {
    static const cf_pair change[] = {{"server.port", "9000"}};
    static char          buf1[1 << 16];
    static char          buf2[1 << 16];
    cf_arena*            ar  = cf_arena_fixed(buf1, sizeof buf1);
    cf_builder*          b   = cf_build_in(ar);
    cf_error             err = CF_ERROR_INIT;
    cf_config*           base;
    cf_config*           next;

    cf_builder_cli(b, "cli", CF_RANK_CLI, 2, g_argv);
    cf_builder_int(b, "server.port", 1234, 1, 65535, 0);
    base = cf_builder_create(b, &err);
    if (base == NULL) {
        check(0, "builds in a fixed arena");
        return;
    }

    check(cf_derive(base, "runtime", change, 1, &err) == NULL,
          "deriving without an arena is refused");
    check(err.code == CF_E_CAPABILITY, "as a capability limit");
    has(err.message, "cf_derive_in", "pointing at the call that works");

    next = cf_derive_in(base, cf_arena_fixed(buf2, sizeof buf2), "runtime", change, 1,
                        &err);
    check(next != NULL, "and deriving into one works");
    if (next != NULL) {
        check(cf_get_int(next, "server.port", 0) == 9000, "with the new value");
        cf_config_free(next);
    }
    cf_config_free(base);
}

int main(void) {
    test_derive();
    test_contract_travels();
    test_derive_chain();
    test_fixed_arena();

    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
