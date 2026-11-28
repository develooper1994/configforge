/*
 * test_migrate.c - a value migrated, not only renamed (D-122): the
 * definition of a deprecated key carries a function that turns the value
 * written under the old name into the value of the key that replaced it.
 */

#include <configforge/configforge.h>

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "cf_test.h"

static int count(const char* hay, const char* needle) {
    int         n = 0;
    const char* p = hay;
    while ((p = strstr(p, needle)) != NULL) {
        ++n;
        p += strlen(needle);
    }
    return n;
}

static cf_bool s_to_ms(const cf_value* in, cf_value* out, void* user, char* why,
                       size_t cap) {
    (void)user;
    if (in->u.i < 0) {
        strncpy(why, "negative", cap - 1);
        why[cap - 1] = '\0';
        return CF_FALSE;
    }
    cf_value_int(out, in->u.i * 1000);
    return CF_TRUE;
}

static cf_bool s_text(const cf_value* in, cf_value* out, void* user, char* why,
                      size_t cap) {
    (void)in;
    (void)user;
    (void)why;
    (void)cap;
    cf_value_text(out, "x", 1);
    return CF_TRUE;
}

/* A reason that holds the value, as a careless function's would. */
static cf_bool s_leak(const cf_value* in, cf_value* out, void* user, char* why,
                      size_t cap) {
    (void)out;
    (void)user;
    (void)cap;
    sprintf(why, "the value %ld is no good", (long)in->u.i);
    return CF_FALSE;
}

static char s_buf[64];

static cf_bool s_upper(const cf_value* in, cf_value* out, void* user, char* why,
                       size_t cap) {
    size_t i, n = in->u.t.n;
    (void)user;
    (void)why;
    (void)cap;
    if (n >= sizeof s_buf) {
        n = sizeof s_buf - 1;
    }
    for (i = 0; i < n; ++i) {
        char ch  = in->u.t.p[i];
        s_buf[i] = (ch >= 'a' && ch <= 'z') ? (char)(ch - 'a' + 'A') : ch;
    }
    s_buf[n] = '\0';
    cf_value_text(out, s_buf, n);
    return CF_TRUE;
}

static void contract(cf_builder* b, cf_migrate_fn fn, unsigned flags) {
    cf_def d;

    cf_builder_int(b, "database.timeout_ms", 5000, 1, 3600000, 0);
    cf_def_init(&d, CF_TYPE_INT, "timeout_s");
    d.flags       = CF_DEPRECATED | flags;
    d.replaced_by = "database.timeout_ms";
    d.migrate     = fn;
    cf_builder_define(b, &d);
    cf_builder_unknown(b, CF_UNKNOWN_REJECT);
}

static cf_config* build(const cf_pair* file, size_t n, const cf_pair* cli, size_t m,
                        cf_migrate_fn fn, unsigned flags, cf_error* err) {
    cf_builder* b = cf_build(NULL);
    cf_builder_memory(b, "file", CF_RANK_USER_FILE, file, n);
    if (m > 0) {
        cf_builder_memory(b, "cli", CF_RANK_CLI, cli, m);
    }
    contract(b, fn, flags);
    return cf_builder_create(b, err);
}

static void test_converted(void) {
    static const cf_pair file[] = {{"timeout_s", "30"}};
    static char          ex[2048];
    cf_error             err = CF_ERROR_INIT;
    cf_config*           c   = build(file, 1, NULL, 0, s_to_ms, 0, &err);

    check(c != NULL, "a migrated key builds");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(cf_get_int(c, "database.timeout_ms", 0) == 30000,
          "a value in the old unit reaches the new key converted");
    cf_explain(c, "database.timeout_ms", ex, sizeof ex);
    has(ex, "(from timeout_s, converted)", "explain() says it was converted");
    lacks(ex, "(as timeout_s)", "and not merely renamed");
    check(cf_get_int(c, "timeout_s", 0) == 30, "the old key keeps its own value");
    cf_config_free(c);
}

static void test_plain_rename(void) {
    static const cf_pair file[] = {{"timeout_s", "30"}};
    static char          ex[2048];
    cf_error             err = CF_ERROR_INIT;
    cf_config*           c   = build(file, 1, NULL, 0, NULL, 0, &err);

    check(c != NULL && cf_get_int(c, "database.timeout_ms", 0) == 30,
          "without a function the value is moved as it was");
    if (c != NULL) {
        cf_explain(c, "database.timeout_ms", ex, sizeof ex);
        has(ex, "(as timeout_s)", "and the place says it was only renamed");
    }
    cf_config_free(c);
}

static void test_precedence(void) {
    static const cf_pair same[] = {{"timeout_s", "5"}, {"database.timeout_ms", "7000"}};
    static const cf_pair file9[] = {{"database.timeout_ms", "9000"}};
    static const cf_pair cli5[]  = {{"timeout_s", "5"}};
    static const cf_pair file7[] = {{"timeout_s", "7"}};
    static const cf_pair cli9[]  = {{"database.timeout_ms", "9000"}};
    cf_error             err     = CF_ERROR_INIT;
    cf_config*           c;

    c = build(same, 2, NULL, 0, s_to_ms, 0, &err);
    check(c != NULL && cf_get_int(c, "database.timeout_ms", 0) == 7000,
          "the new spelling beats the converted copy in the same source");
    cf_config_free(c);

    c = build(file9, 1, cli5, 1, s_to_ms, 0, &err);
    check(c != NULL && cf_get_int(c, "database.timeout_ms", 0) == 5000,
          "a converted command line beats a file");
    cf_config_free(c);

    c = build(file7, 1, cli9, 1, s_to_ms, 0, &err);
    check(c != NULL && cf_get_int(c, "database.timeout_ms", 0) == 9000,
          "a converted file loses to a command line");
    cf_config_free(c);
}

static void test_range(void) {
    static const cf_pair file[] = {{"timeout_s", "99999999"}};
    cf_error             err    = CF_ERROR_INIT;
    cf_config*           c      = build(file, 1, NULL, 0, s_to_ms, 0, &err);

    check(c == NULL, "a converted value outside the new range is refused");
    has(err.message, "database.timeout_ms: 99999999000 is outside 1..3600000",
        "by the new key's own range");
    cf_config_free(c);
}

static void test_failure(void) {
    static const cf_pair file[] = {{"timeout_s", "-1"}};
    cf_error             err    = CF_ERROR_INIT;
    cf_config*           c      = build(file, 1, NULL, 0, s_to_ms, 0, &err);

    check(c == NULL && err.code == CF_E_VALIDATE,
          "a refused migration fails the build");
    has(err.message, "timeout_s: the migration to database.timeout_ms failed: negative",
        "with the old key and the function's reason");
    cf_config_free(c);
}

static void test_wrong_type(void) {
    static const cf_pair file[] = {{"timeout_s", "3"}};
    cf_error             err    = CF_ERROR_INIT;
    cf_config*           c      = build(file, 1, NULL, 0, s_text, 0, &err);

    check(c == NULL, "a result of the wrong type is refused");
    has(err.message, "produced text where int was declared", "and says which");
    cf_config_free(c);
}

static void test_sensitive(void) {
    static const cf_pair file[] = {{"timeout_s", "424242"}};
    cf_error             err    = CF_ERROR_INIT;
    cf_config*           c      = build(file, 1, NULL, 0, s_leak, CF_SENSITIVE, &err);

    check(c == NULL, "a sensitive migration that fails fails the build");
    has(err.message, "the migration to database.timeout_ms failed", "naming the key");
    lacks(err.message, "424242", "and never the value");
    cf_config_free(c);
}

static void test_derived(void) {
    static const cf_pair file[] = {{"timeout_s", "30"}};
    static char          ex[2048];
    cf_error             err = CF_ERROR_INIT;
    cf_config*           c   = build(file, 1, NULL, 0, s_to_ms, 0, &err);
    cf_config*           d;
    cf_pair              p;

    if (c == NULL) {
        check(0, "the derive test builds");
        return;
    }
    d = cf_derive(c, "admin", NULL, 0, &err);
    check(d != NULL && cf_get_int(d, "database.timeout_ms", 0) == 30000,
          "a derive keeps the converted value");
    if (d != NULL) {
        cf_explain(d, "database.timeout_ms", ex, sizeof ex);
        check(count(ex, "(from timeout_s, converted)") == 1,
              "and converts it once, not twice");
    }
    cf_config_free(d);
    p.key   = "timeout_s";
    p.value = "10";
    d       = cf_derive(c, "admin", &p, 1, &err);
    check(d != NULL && cf_get_int(d, "database.timeout_ms", 0) == 10000,
          "a new value under the old name is converted too");
    cf_config_free(d);
    cf_config_free(c);
}

static void test_reference(void) {
    static const cf_pair file[] = {{"timeout_s", "${t}"}};
    cf_error             err    = CF_ERROR_INIT;
    cf_builder*          b      = cf_build(NULL);
    cf_config*           c;

    cf_builder_memory(b, "file", CF_RANK_USER_FILE, file, 1);
    cf_builder_text(b, "t", "3", 0);
    contract(b, s_to_ms, 0);
    c = cf_builder_create(b, &err);
    check(c == NULL, "a value that holds a reference is not migrated");
    has(err.message, "a value that holds a reference cannot be migrated",
        "and the message says why");
    cf_config_free(c);
}

static void definition_refused(cf_def* d, const char* what) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  c;

    cf_builder_define(b, d);
    c = cf_builder_create(b, &err);
    check(c == NULL, what);
    has(err.message, "migrate needs CF_DEPRECATED, replaced_by and a scalar key", what);
    cf_config_free(c);
}

static void test_definitions(void) {
    cf_def d;

    cf_def_init(&d, CF_TYPE_INT, "timeout_s");
    d.replaced_by = "database.timeout_ms";
    d.migrate     = s_to_ms;
    definition_refused(&d, "a migration needs CF_DEPRECATED");

    cf_def_init(&d, CF_TYPE_INT, "timeout_s");
    d.flags   = CF_DEPRECATED;
    d.migrate = s_to_ms;
    definition_refused(&d, "a migration needs a replacement");

    cf_def_init(&d, CF_TYPE_INT, "timeout_s");
    d.flags       = CF_DEPRECATED;
    d.replaced_by = "database.timeout_ms";
    d.migrate     = s_to_ms;
    d.list_mode   = CF_LIST_REPLACE;
    definition_refused(&d, "a migration needs a scalar key");
}

static void test_old_descriptor(void) {
    static const cf_pair file[] = {{"timeout_s", "30"}};
    cf_error             err    = CF_ERROR_INIT;
    cf_builder*          b      = cf_build(NULL);
    cf_config*           c;
    cf_def               d;

    cf_builder_memory(b, "file", CF_RANK_USER_FILE, file, 1);
    cf_builder_int(b, "database.timeout_ms", 5000, 1, 3600000, 0);
    cf_def_init(&d, CF_TYPE_INT, "timeout_s");
    d.flags       = CF_DEPRECATED;
    d.replaced_by = "database.timeout_ms";
    d.migrate     = s_to_ms;
    d.size        = offsetof(cf_def, list_default_count) + sizeof d.list_default_count;
    cf_builder_define(b, &d);
    c = cf_builder_create(b, &err);
    check(c != NULL && cf_get_int(c, "database.timeout_ms", 0) == 30,
          "a descriptor from before the member is not asked to run a function");
    cf_config_free(c);
}

static void test_text(void) {
    static const cf_pair file[] = {{"old.name", "abc"}};
    cf_error             err    = CF_ERROR_INIT;
    cf_builder*          b      = cf_build(NULL);
    cf_config*           c;
    cf_def               d;

    cf_builder_memory(b, "file", CF_RANK_USER_FILE, file, 1);
    cf_builder_text(b, "new.name", "", 0);
    cf_def_init(&d, CF_TYPE_TEXT, "old.name");
    d.flags       = CF_DEPRECATED;
    d.replaced_by = "new.name";
    d.migrate     = s_upper;
    cf_builder_define(b, &d);
    c = cf_builder_create(b, &err);
    check(c != NULL && strcmp(cf_get_text(c, "new.name", ""), "ABC") == 0,
          "a text is migrated to a text");
    memset(s_buf, 'z', sizeof s_buf - 1);
    check(c != NULL && strcmp(cf_get_text(c, "new.name", ""), "ABC") == 0,
          "and the library's copy outlives the function's buffer");
    cf_config_free(c);
}

int main(void) {
    test_converted();
    test_plain_rename();
    test_precedence();
    test_range();
    test_failure();
    test_wrong_type();
    test_sensitive();
    test_derived();
    test_reference();
    test_definitions();
    test_old_descriptor();
    test_text();
    return cf_test_done();
}
