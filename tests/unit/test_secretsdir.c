/*
 * test_secretsdir.c - a directory of secrets, one file a key, as
 * containers mount them: the names it maps, the newline it drops, the
 * names it refuses, and the build that left directories out.
 *
 * The directories cf_secrets and cf_secrets_bad are made by the build
 * (tests/unit/CMakeLists.txt), since C89 has no way to make one.
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

#if CF_WITH_DIR

static cf_bool spill(const char* path, const char* text) {
    FILE* f = fopen(path, "wb");
    if (f == NULL) {
        return CF_FALSE;
    }
    fputs(text, f);
    fclose(f);
    return CF_TRUE;
}

static void test_reads(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  c;
    cf_def      d;
    static char ex[2048];

    check(spill("cf_secrets/db.password", "correct horse\n"),
          "a secret named as its key");
    check(spill("cf_secrets/MYAPP_API__TOKEN", "t0ken"), "one named as its variable");
    check(spill("cf_secrets/.hidden", "x"), "and a dotfile");

    cf_builder_secrets(b, "secrets", CF_RANK_ENV, "cf_secrets", "MYAPP_", 0);
    cf_def_init(&d, CF_TYPE_TEXT, "db.password");
    d.flags = CF_SENSITIVE;
    cf_builder_define(b, &d);
    c = cf_builder_create(b, &err);
    check(c != NULL, "a secrets directory builds");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(strcmp(cf_get_text(c, "db.password", ""), "correct horse") == 0,
          "the file's name is the key, its trailing newline not part of the value");
    check(strcmp(cf_get_text(c, "api.token", ""), "t0ken") == 0,
          "a variable's name maps as the environment's does, with the prefix");
    check(cf_count(c) == 2, "and a dotfile is no secret");

    cf_explain(c, "db.password", ex, sizeof ex);
    has(ex, "cf_secrets/db.password", "explain() names the file");
    check(strstr(ex, "correct horse") == NULL,
          "and, for a sensitive key, not the secret");
    cf_config_free(c);
    remove("cf_secrets/db.password");
    remove("cf_secrets/MYAPP_API__TOKEN");
    remove("cf_secrets/.hidden");
}

static void test_refuses(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  c;

    check(spill("cf_secrets_bad/Not A Key", "x"), "a file that names nothing");
    cf_builder_secrets(b, "secrets", CF_RANK_ENV, "cf_secrets_bad", NULL, 0);
    c = cf_builder_create(b, &err);
    check(c == NULL, "is an error, not a skip");
    has(err.message, "Not A Key", "and is named");
    cf_config_free(c);
    remove("cf_secrets_bad/Not A Key");
}

static void test_missing(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  c;

    cf_builder_secrets(b, "secrets", CF_RANK_ENV, "cf_no_such_dir", NULL,
                       CF_FILE_OPTIONAL);
    c = cf_builder_create(b, &err);
    check(c != NULL, "an optional directory that is not there is fine");
    cf_config_free(c);

    b = cf_build(NULL);
    cf_builder_secrets(b, "secrets", CF_RANK_ENV, "cf_no_such_dir", NULL, 0);
    c = cf_builder_create(b, &err);
    check(c == NULL, "a required one is not");
    cf_config_free(c);
}

#else

static void test_disabled(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  c;

    cf_builder_secrets(b, "secrets", CF_RANK_ENV, "cf_secrets", NULL, CF_FILE_OPTIONAL);
    c = cf_builder_create(b, &err);
    check(c == NULL, "a secrets directory is refused without CF_WITH_DIR");
    has(err.message, "CF_WITH_DIR", "and the error names the option");
    cf_config_free(c);
}

#endif /* CF_WITH_DIR */

int main(void) {
#if CF_WITH_DIR
    test_reads();
    test_refuses();
    test_missing();
#else
    test_disabled();
#endif
    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
