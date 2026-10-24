/*
 * test_write.c - keeping a change: cf_write asks whether the source can
 * be written, whether the program granted it, and whether the contract
 * would accept each value from it, and writes nothing unless every
 * answer is yes. Through a writable source of the test's own, so that
 * it runs in every build; test_store writes a real store.
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

/* What the source was asked to write, last time. */
static size_t      g_written = 0;
static char        g_keys[256];
static const char* g_meta = NULL;

static cf_bool local_load(const cf_source* s, cf_keyspace* ks) {
    (void)s;
    (void)ks;
    return CF_TRUE;
}

static cf_bool local_write(const cf_source* s, const cf_store_item* items, size_t count,
                           const char* meta, cf_error* err) {
    size_t i;

    (void)s;
    (void)err;
    g_written = count;
    g_meta    = meta;
    g_keys[0] = '\0';
    for (i = 0; i < count; ++i) {
        if (strlen(g_keys) + strlen(items[i].key) + 2 < sizeof g_keys) {
            strcat(g_keys, items[i].key);
            strcat(g_keys, " ");
        }
    }
    return CF_TRUE;
}

static const cf_source_vtable local_vt = {"local", local_load, NULL, NULL, local_write};

static const char* const only_env[]  = {"env", NULL};
static const char* const local_too[] = {"local", "env", NULL};
static const char* const levels[]    = {"debug", "info", "warn", NULL};

static char*         g_env[] = {NULL};
static const cf_pair g_mem[] = {{"app.id", "orders"}};

/* A configuration with the test's source, the environment, and a
   contract; `grant` names the source to allow, or NULL. */
static cf_config* build(const char* grant, cf_unknown_policy unknown, cf_error* err) {
    cf_builder* b = cf_build(NULL);
    cf_source   s;
    cf_def      d;

    memset(&s, 0, sizeof s);
    s.vt   = &local_vt;
    s.name = "local";
    s.rank = CF_RANK_USER_FILE;
    cf_builder_source(b, &s);
    cf_builder_env(b, "env", CF_RANK_ENV, "APP_", g_env);
    cf_builder_memory(b, "mem", CF_RANK_SYS_FILE, g_mem, 1);

    /* Required, and supplied by the configuration: a write of other
       keys is not a configuration missing it. */
    cf_builder_text(b, "app.id", NULL, CF_REQUIRED);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_def_init(&d, CF_TYPE_TEXT, "log.level");
    d.allowed = levels;
    cf_builder_define(b, &d);
    cf_def_init(&d, CF_TYPE_TEXT, "db.password");
    d.policy = only_env;
    cf_builder_define(b, &d);
    cf_def_init(&d, CF_TYPE_TEXT, "db.host");
    d.policy = local_too;
    cf_builder_define(b, &d);
    cf_builder_bool(b, "cf.explain", CF_FALSE, CF_TRANSIENT);
    cf_builder_open(b, "plugins");
    cf_builder_unknown(b, unknown);
    if (grant != NULL) {
        cf_builder_writable(b, grant);
    }
    return cf_builder_create(b, err);
}

static cf_bool write_int(const cf_config* c, const char* key, cf_i64 v, cf_error* err) {
    cf_store_item one;
    one.key = key;
    cf_value_int(&one.value, v);
    g_written = 0;
    return cf_write(c, "local", &one, 1, "test", err);
}

static cf_bool write_text(const cf_config* c, const char* key, const char* v,
                          cf_error* err) {
    cf_store_item one;
    one.key = key;
    cf_value_text(&one.value, v, strlen(v));
    g_written = 0;
    return cf_write(c, "local", &one, 1, "test", err);
}

static void test_granted(void) {
    cf_store_item two[2];
    cf_error      err = CF_ERROR_INIT;
    cf_config*    c   = build("local", CF_UNKNOWN_REJECT, &err);

    check(c != NULL, "a grant to a writable source builds");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    two[0].key = "server.port";
    cf_value_int(&two[0].value, 9443);
    two[1].key = "log.level";
    cf_value_text(&two[1].value, "debug", 5);
    g_written = 0;
    check(cf_write(c, "local", two, 2, "admin: incident 42", &err), "a granted write");
    check(g_written == 2, "writes every item");
    has(g_keys, "server.port log.level", "in order");
    check(g_meta != NULL && strcmp(g_meta, "admin: incident 42") == 0,
          "with what the writer said");
    check(cf_get_int(c, "server.port", 0) == 8080,
          "and the snapshot does not change underneath its readers");
    cf_config_free(c);
}

static void test_not_granted(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = build(NULL, CF_UNKNOWN_REJECT, &err);

    check(c != NULL, "a writable source with no grant builds");
    check(!write_int(c, "server.port", 9443, &err),
          "and is not written: capability is not permission");
    check(err.code == CF_E_POLICY && g_written == 0,
          "a policy refusal, before the source");
    has(err.message, "cf_builder_writable", "and the message says what grants it");

    check(!cf_write(c, "nowhere", NULL, 0, NULL, &err), "a source that is not there");
    has(err.message, "no source named 'nowhere'", "is named");

    check(!cf_write(c, "env", NULL, 0, NULL, &err), "the environment");
    check(err.code == CF_E_CAPABILITY, "cannot be written at all");
    has(err.message, "env source, which cannot be written", "and says so");
    cf_config_free(c);
}

/* A grant is checked against the sources when the configuration is
   created, not discovered wrong at the first write. */
static void test_grant_checked(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = build("env", CF_UNKNOWN_IGNORE, &err);

    check(c == NULL && err.code == CF_E_CAPABILITY,
          "a grant to a source that cannot be written fails creation");
    has(err.message, "'env' is granted writing", "naming it");
    cf_config_free(c);

    c = build("locla", CF_UNKNOWN_IGNORE, &err);
    check(c == NULL && err.code == CF_E_POLICY, "and so does a grant to no source");
    has(err.message, "no source has that name", "as a typo");
    cf_config_free(c);
}

/* The contract decides, as it does when the value is read back. */
static void test_contract(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = build("local", CF_UNKNOWN_REJECT, &err);

    if (c == NULL) {
        check(0, "the contract test builds");
        return;
    }
    check(!write_int(c, "server.port", 99999, &err) && g_written == 0,
          "a value out of range is not written");
    check(err.code == CF_E_VALIDATE, "a validation refusal");
    has(err.message, "'server.port' is not written to 'local'",
        "naming the key and source");
    has(err.message, "99999 is outside 1..65535", "with the read side's reason");

    check(!write_text(c, "server.port", "9443", &err), "text where an int is declared");
    has(err.message, "source supplied text where int was declared",
        "is refused as reading it back would be");

    check(!write_text(c, "log.level", "verbose", &err), "a spelling not allowed");
    check(write_text(c, "log.level", "warn", &err) && g_written == 1, "one that is");

    check(!write_int(c, "server.prot", 1, &err), "an unknown key");
    has(err.message, "did you mean 'server.port'", "is refused with the suggestion");
    check(write_int(c, "plugins.cache.size", 64, &err), "unless its prefix is open");

    check(!write_int(c, "cf.explain", 1, &err), "a command");
    has(err.message, "is a command, not a setting", "is never written");
    cf_config_free(c);

    c = build("local", CF_UNKNOWN_IGNORE, &err);
    check(c != NULL && write_int(c, "server.prot", 1, &err),
          "where unknown keys are accepted, writing one is too");
    cf_config_free(c);
}

/* A key's policy is an operator's lock, and writing past it would lift
   it. */
static void test_policy(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = build("local", CF_UNKNOWN_REJECT, &err);

    if (c == NULL) {
        check(0, "the policy test builds");
        return;
    }
    check(!write_text(c, "db.password", "hunter2", &err) && g_written == 0,
          "a key that may only be set from the environment is not written");
    check(err.code == CF_E_POLICY, "a policy refusal");
    has(err.message, "may only be set from env", "with the policy");
    check(write_text(c, "db.host", "db1", &err),
          "a key whose policy names the source is");
    cf_config_free(c);
}

/* Every item or none. */
static void test_all_or_none(void) {
    cf_store_item two[2];
    cf_error      err = CF_ERROR_INIT;
    cf_config*    c   = build("local", CF_UNKNOWN_REJECT, &err);

    if (c == NULL) {
        check(0, "the all-or-none test builds");
        return;
    }
    two[0].key = "log.level";
    cf_value_text(&two[0].value, "info", 4);
    two[1].key = "server.port";
    cf_value_int(&two[1].value, 0);
    g_written = 0;
    check(!cf_write(c, "local", two, 2, NULL, &err) && g_written == 0,
          "one refused item refuses the lot before anything is written");
    two[1].key = "Bad Key";
    check(!cf_write(c, "local", two, 2, NULL, &err) && err.code == CF_E_KEY,
          "so does a key that is not valid");
    cf_config_free(c);
}

/* A change made at run time can be kept from the configuration that
   holds it. */
static void test_derived(void) {
    cf_pair    p;
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = build("local", CF_UNKNOWN_REJECT, &err);
    cf_config* d;

    if (c == NULL) {
        check(0, "the derive test builds");
        return;
    }
    p.key   = "server.port";
    p.value = "9443";
    d       = cf_derive(c, "admin", &p, 1, &err);
    check(d != NULL, "a derived configuration");
    check(d != NULL &&
              write_int(d, "server.port", cf_get_int(d, "server.port", 0), &err),
          "keeps the grant, and writes");
    check(g_written == 1, "through the original's source");
    check(d != NULL && !cf_write(d, "admin", NULL, 0, NULL, &err) &&
              err.code == CF_E_CAPABILITY,
          "and the change itself is not a source to write");
    cf_config_free(d);
    cf_config_free(c);
}

int main(void) {
    test_granted();
    test_not_granted();
    test_grant_checked();
    test_contract();
    test_policy();
    test_all_or_none();
    test_derived();
    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
