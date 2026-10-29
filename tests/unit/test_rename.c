/*
 * test_rename.c - a renamed key (CF_DEPRECATED with replaced_by): the
 * old spelling still configures the new key, below the new spelling in
 * the same source and by ordinary precedence across sources, and
 * explain() says it was written under the old name.
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

static int count(const char* hay, const char* needle) {
    int         n = 0;
    const char* p = hay;
    while ((p = strstr(p, needle)) != NULL) {
        ++n;
        p += strlen(needle);
    }
    return n;
}

static void contract(cf_builder* b) {
    cf_def d;

    cf_builder_text(b, "database.host", "localhost", 0);
    cf_def_init(&d, CF_TYPE_TEXT, "db.host");
    d.flags       = CF_DEPRECATED;
    d.replaced_by = "database.host";
    cf_builder_define(b, &d);
    cf_builder_unknown(b, CF_UNKNOWN_REJECT);
}

static cf_config* build(const cf_pair* file, size_t n, const cf_pair* cli, size_t m,
                        cf_error* err) {
    cf_builder* b = cf_build(NULL);
    cf_builder_memory(b, "file", CF_RANK_USER_FILE, file, n);
    if (m > 0) {
        cf_builder_memory(b, "cli", CF_RANK_CLI, cli, m);
    }
    contract(b);
    return cf_builder_create(b, err);
}

static void test_old_name_still_works(void) {
    static const cf_pair file[] = {{"db.host", "db1"}};
    static char          ex[2048];
    cf_error             err = CF_ERROR_INIT;
    cf_config*           c   = build(file, 1, NULL, 0, &err);

    check(c != NULL, "a renamed key builds");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(strcmp(cf_get_text(c, "database.host", ""), "db1") == 0,
          "a value under the old name configures the new key");
    check(strcmp(cf_get_text(c, "db.host", ""), "db1") == 0,
          "and the old key still reads, for code that has not moved");
    cf_explain(c, "database.host", ex, sizeof ex);
    has(ex, "(as db.host)", "explain() says it was written under the old name");
    cf_config_free(c);
}

/* In one source the new spelling wins, wherever each is written. */
static void test_new_name_wins(void) {
    static const cf_pair file[] = {{"database.host", "db2"}, {"db.host", "db1"}};
    cf_error             err    = CF_ERROR_INIT;
    cf_config*           c      = build(file, 2, NULL, 0, &err);

    check(c != NULL && strcmp(cf_get_text(c, "database.host", ""), "db2") == 0,
          "the new spelling beats the old in the same source, even written first");
    cf_config_free(c);
}

/* Across sources, precedence as ever: the old name on the command line
   still beats the new one in a file. */
static void test_precedence(void) {
    static const cf_pair file[] = {{"database.host", "db2"}};
    static const cf_pair cli[]  = {{"db.host", "db9"}};
    cf_error             err    = CF_ERROR_INIT;
    cf_config*           c      = build(file, 1, cli, 1, &err);

    check(c != NULL && strcmp(cf_get_text(c, "database.host", ""), "db9") == 0,
          "precedence between sources is unchanged by the spelling");
    cf_config_free(c);
}

/* A derived configuration makes the copies again, and only once. */
static void test_derived(void) {
    static const cf_pair file[] = {{"db.host", "db1"}};
    static char          ex[2048];
    cf_error             err = CF_ERROR_INIT;
    cf_config*           c   = build(file, 1, NULL, 0, &err);
    cf_config*           d;
    cf_pair              p;

    if (c == NULL) {
        check(0, "the derive test builds");
        return;
    }
    p.key   = "db.host";
    p.value = "db5";
    d       = cf_derive(c, "admin", &p, 1, &err);
    check(d != NULL && strcmp(cf_get_text(d, "database.host", ""), "db5") == 0,
          "a change to the old name at run time reaches the new key");
    if (d != NULL) {
        cf_explain(d, "database.host", ex, sizeof ex);
        check(count(ex, "(as db.host)") == 2, "one copy for each value, not two");
    }
    cf_config_free(d);
    cf_config_free(c);
}

static void test_definitions(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_def      d;
    cf_config*  c;

    cf_def_init(&d, CF_TYPE_TEXT, "db.host");
    d.replaced_by = "database.host"; /* not marked deprecated */
    cf_builder_define(b, &d);
    c = cf_builder_create(b, &err);
    check(c == NULL && err.code == CF_E_DEFINITION,
          "a replacement needs CF_DEPRECATED");
    cf_config_free(c);

    b = cf_build(NULL);
    cf_def_init(&d, CF_TYPE_TEXT, "db.host");
    d.flags       = CF_DEPRECATED;
    d.replaced_by = "db.host";
    cf_builder_define(b, &d);
    c = cf_builder_create(b, &err);
    check(c == NULL && err.code == CF_E_DEFINITION, "and must name another key");
    cf_config_free(c);
}

int main(void) {
    test_old_name_still_works();
    test_new_name_wins();
    test_precedence();
    test_derived();
    test_definitions();
    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
