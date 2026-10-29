/*
 * test_edges.c - the core's small functions at their edges: every code,
 * type and outcome named; values written into buffers too short for
 * them and compared across types; the definitions cf_define refuses as
 * contradicting themselves; and a diff that adds, removes, and is told
 * to stop. Each is a branch the larger tests pass by on the common side.
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

static void test_names(void) {
    int c;

    for (c = CF_OK; c <= CF_E_MEMORY; ++c) {
        check(cf_code_name((cf_code)c) != NULL && cf_code_name((cf_code)c)[0] != '\0',
              "every code has a name");
        check(cf_code_description((cf_code)c) != NULL, "and a description");
    }
    check(cf_code_name((cf_code)999) != NULL, "an unknown code still has a name");
    for (c = CF_TYPE_NULL; c <= CF_TYPE_TEXT; ++c) {
        check(cf_type_name((cf_type)c) != NULL && cf_type_json_name((cf_type)c) != NULL,
              "every type has both names");
    }
    check(cf_type_name((cf_type)99) != NULL, "an unknown type too");
    for (c = CF_OUT_PENDING; c <= CF_OUT_TRUNCATED; ++c) {
        check(cf_outcome_name((cf_outcome)c) != NULL, "every outcome has a name");
    }
}

static void test_values(void) {
    cf_value a, b;
    char     buf[16];

    cf_value_null(&a);
    check(cf_value_to_text(&a, buf, 2) == 0 && buf[0] == 0,
          "null into a short buffer is 0, and empty");
    cf_value_bool(&a, CF_TRUE);
    check(cf_value_to_text(&a, buf, 3) == 0, "true too");
    cf_value_bool(&a, CF_FALSE);
    check(cf_value_to_text(&a, buf, 5) == 0, "false too");
    check(cf_value_to_text(NULL, buf, sizeof buf) == 0 &&
              cf_value_to_text(&a, NULL, 4) == 0,
          "nothing to write, or nowhere");

    cf_value_int(&a, 1);
    cf_value_text(&b, "1", 1);
    check(cf_value_cmp(&a, &b) != 0 && !cf_value_equal(&a, &b),
          "an int is not its text");
    cf_value_bool(&a, CF_TRUE);
    cf_value_bool(&b, CF_TRUE);
    check(cf_value_equal(&a, &b), "two trues are equal");
    cf_value_bool(&b, CF_FALSE);
    check(cf_value_cmp(&a, &b) != 0, "true is not false");
    cf_value_null(&a);
    cf_value_null(&b);
    check(cf_value_equal(&a, &b), "two nulls are equal");
#if CF_WITH_REAL
    cf_value_real(&a, 1.5);
    cf_value_real(&b, 1.5);
    check(cf_value_equal(&a, &b), "two equal reals");
    cf_value_real(&b, 2.5);
    check(cf_value_cmp(&a, &b) < 0, "and ordered");
#endif

    check(cf_value_from_text(CF_TYPE_NULL, "null", 4, &a) && a.type == CF_TYPE_NULL,
          "null from its text");
    check(!cf_value_from_text(CF_TYPE_NULL, "nil", 3, &a), "and not from another");
    check(cf_value_from_text(CF_TYPE_BOOL, "false", 5, &a) && !a.u.b,
          "false from its text");
    check(!cf_value_from_text(CF_TYPE_BOOL, "fals", 4, &a), "and not from a near miss");
    check(!cf_value_from_text(CF_TYPE_INT, NULL, 0, &a), "no text is no value");
    check(!cf_value_from_text((cf_type)99, "1", 1, &a),
          "an unknown type converts nothing");
    check(!cf_value_infer(NULL, 0, &a), "nothing to infer");
}

/* What cf_define refuses, each named. */
static void test_definitions(void) {
    static const char* const words[] = {"a", NULL};
    cf_arena*                ar      = cf_arena_new(NULL, 0);
    cf_defset                ds;
    cf_def                   d;
    cf_error                 err  = CF_ERROR_INIT;
    size_t                   dist = 0;
    int                      i;

    check(!cf_defset_init(NULL, ar, 4) && !cf_defset_init(&ds, NULL, 4),
          "init with nothing");
    check(cf_defset_init(&ds, ar, 4), "init");
    check(!cf_define(&ds, NULL, &err), "no definition");
    cf_def_init(&d, CF_TYPE_INT, "Bad Key");
    check(!cf_define(&ds, &d, &err), "a key that is not one");
    cf_def_init(&d, CF_TYPE_INT, "a.int");
    d.size = 0;
    check(!cf_define(&ds, &d, &err), "a descriptor never initialised");
    cf_def_init(&d, CF_TYPE_INT, "a.int");
    d.allowed = words;
    check(!cf_define(&ds, &d, &err), "allowed spellings on an int");
    cf_def_init(&d, CF_TYPE_TEXT, "a.text");
    d.min_len = 5;
    d.max_len = 2;
    check(!cf_define(&ds, &d, &err), "a length range upside down");
    cf_def_init(&d, CF_TYPE_INT, "a.int");
    d.has_default = CF_TRUE;
    cf_value_text(&d.default_value, "x", 1);
    check(!cf_define(&ds, &d, &err), "a default of another type");
    cf_def_init(&d, CF_TYPE_INT, "a.int");
    d.has_range   = CF_TRUE;
    d.imin        = 1;
    d.imax        = 9;
    d.has_default = CF_TRUE;
    cf_value_int(&d.default_value, 10);
    check(!cf_define(&ds, &d, &err), "a default outside its own range");
#if CF_WITH_REAL
    cf_def_init(&d, CF_TYPE_REAL, "a.real");
    d.has_range   = CF_TRUE;
    d.rmin        = 0.0;
    d.rmax        = 1.0;
    d.has_default = CF_TRUE;
    cf_value_real(&d.default_value, 2.0);
    check(!cf_define(&ds, &d, &err), "a real default outside its range");
#endif
    cf_def_init(&d, CF_TYPE_TEXT, "a.old");
    d.flags       = CF_DEPRECATED;
    d.replaced_by = "Not A Key";
    check(!cf_define(&ds, &d, &err), "a replacement that is not a key");

    check(!cf_defset_open(NULL, "p", &err) && !cf_defset_open(&ds, NULL, &err) &&
              !cf_defset_open(&ds, "", &err),
          "an open prefix needs a set and a name");
    for (i = 0; i < CF_MAX_OPEN_PREFIXES; ++i) {
        char p[16];
        cf_fmt(p, sizeof p, "p%d", i);
        (void)cf_defset_open(&ds, cf_arena_dup(ar, p, strlen(p)), &err);
    }
    check(!cf_defset_open(&ds, "one.more", &err), "and there is a limit to them");
    check(!cf_defset_is_open(NULL, "p0.x", 4) && !cf_defset_is_open(&ds, NULL, 0),
          "is_open of nothing");

    cf_def_text(&ds, "server.port", NULL, 0, &err);
    cf_defset_seal(&ds);
    cf_def_init(&d, CF_TYPE_TEXT, "late.key");
    check(!cf_define(&ds, &d, &err), "nothing is added to a sealed set");
    check(cf_defset_find(NULL, "a", 1) == NULL && cf_defset_find(&ds, NULL, 0) == NULL,
          "find in nothing");
    check(cf_defset_count(NULL) == 0 && cf_defset_at(NULL, 0) == NULL &&
              cf_defset_at(&ds, 99) == NULL,
          "count and at of nothing");
    check(cf_defset_nearest(NULL, "a", 1, &dist) == NULL &&
              cf_defset_nearest(&ds, NULL, 0, &dist) == NULL,
          "nearest to nothing");
    check(cf_defset_nearest(&ds, "server.pot", 10, NULL) != NULL,
          "nearest without asking the distance");
    {
        char longkey[200];
        memset(longkey, 'a', sizeof longkey - 1);
        longkey[sizeof longkey - 1] = '\0';
        (void)cf_defset_nearest(&ds, longkey, strlen(longkey), &dist);
        check(1, "a key too long to measure answers");
    }
    cf_arena_free(ar);
}

/* A diff: a key added, a key removed, and a callback that stops it. */
static int     g_seen = 0;
static cf_bool stop_at_first(void* ctx, const char* key, size_t klen, cf_change kind,
                             const cf_candidate* before, const cf_candidate* after) {
    (void)ctx;
    (void)key;
    (void)klen;
    (void)kind;
    (void)before;
    (void)after;
    ++g_seen;
    return CF_FALSE;
}

static void test_diff(void) {
    static const cf_pair one[] = {{"a", "1"}, {"gone", "x"}};
    static const cf_pair two[] = {{"a", "2"}, {"new", "y"}};
    static char          buf[1024];
    cf_error             err = CF_ERROR_INIT;
    cf_builder*          b1  = cf_build(NULL);
    cf_builder*          b2  = cf_build(NULL);
    cf_config*           x;
    cf_config*           y;

    cf_builder_memory(b1, "m", 0, one, 2);
    cf_builder_memory(b2, "m", 0, two, 2);
    x = cf_builder_create(b1, &err);
    y = cf_builder_create(b2, &err);
    check(x != NULL && y != NULL, "two configurations");
    if (x != NULL && y != NULL) {
        cf_diff_text(x, y, buf, sizeof buf);
        check(strstr(buf, "gone") != NULL && strstr(buf, "new") != NULL,
              "a removed key and an added one are both named");
        g_seen = 0;
        (void)cf_diff(x, y, stop_at_first, NULL);
        check(g_seen == 1, "a callback that says stop is not called again");
        check(cf_diff_text(NULL, y, buf, sizeof buf) == 0, "a diff with nothing");
        check(cf_diff_text(x, y, buf, 1) > 1, "measured past a one-byte buffer");
    }
    cf_config_free(x);
    cf_config_free(y);
}

int main(void) {
    test_names();
    test_values();
    test_definitions();
    test_diff();
    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
