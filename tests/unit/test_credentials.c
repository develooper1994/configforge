/*
 * test_credentials.c - systemd's credentials as a source: the directory
 * $CREDENTIALS_DIRECTORY names, read as a directory of secrets is, each
 * value saying it came from a systemd credential; the variable unset,
 * with CF_OPTIONAL and without; a variable naming no directory; and the
 * build that left directories out.
 *
 * Runs in a working directory of its own, where the build made cf_creds
 * (tests/unit/CMakeLists.txt): the test sets the process's environment,
 * and a relative path keeps it from depending on where the tree is.
 */

#ifndef _WIN32
#define _POSIX_C_SOURCE 200112L /* setenv, unsetenv */
#endif

#include <configforge/configforge.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cf_test.h"

/* NULL unsets it. Windows' _putenv_s removes a variable set to "", so
   there an empty value and an unset one are the same case. */
static void creds_set(const char* value) {
#if defined(_WIN32)
    _putenv_s("CREDENTIALS_DIRECTORY", (value != NULL) ? value : "");
#else
    if (value == NULL) {
        unsetenv("CREDENTIALS_DIRECTORY");
    } else {
        setenv("CREDENTIALS_DIRECTORY", value, 1);
    }
#endif
}

static void test_null(void) {
    check(!cf_builder_credentials(NULL, "c", 0, NULL, 0), "no builder, no source");
}

#if CF_WITH_DIR

static void test_reads(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  c;
    cf_def      d;
    static char ex[4096];

    check(spill("cf_creds/db.password", "M4RK3R horse\n"),
          "a credential named as its key");
    check(spill("cf_creds/MYAPP_API__TOKEN", "t0ken"), "one named as its variable");
    creds_set("cf_creds");

    cf_builder_credentials(b, NULL, CF_RANK_ENV, "MYAPP_", 0);
    cf_def_init(&d, CF_TYPE_TEXT, "db.password");
    d.flags = CF_SENSITIVE;
    cf_builder_define(b, &d);
    c = cf_builder_create(b, &err);
    check(c != NULL, "systemd's credentials build");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(strcmp(cf_get_text(c, "db.password", ""), "M4RK3R horse") == 0,
          "the credential's id is the key, its trailing newline not part of the value");
    check(
        strcmp(cf_get_text(c, "api.token", ""), "t0ken") == 0,
        "an id spelled as a variable maps as the environment's does, with the prefix");
    check(cf_count(c) == 2, "two credentials, two keys");

    cf_explain(c, "db.password", ex, sizeof ex);
    has(ex, "systemd credential db.password",
        "explain() says a systemd credential, by id");
    has(ex, "cf_creds/db.password", "and names its file");
    has(ex, "credentials", "and the source is called what it is");
    lacks(ex, "M4RK3R", "and, for a sensitive key, not the secret");
    cf_explain(c, "api.token", ex, sizeof ex);
    has(ex, "systemd credential MYAPP_API__TOKEN",
        "the id as systemd has it, not the key");

    if (cf_has_feature("report")) {
        cf_report_info info = CF_REPORT_INFO_INIT;
        info.mode           = CF_REPORT_BLIND;
        cf_report(c, &info, ex, sizeof ex);
        has(ex, "systemd credential db.password",
            "a blind report says where it came from");
        lacks(ex, "M4RK3R", "and never what it is");
    }
    cf_config_free(c);
    remove("cf_creds/db.password");
    remove("cf_creds/MYAPP_API__TOKEN");
}

/* The later of two sources at one rank wins, as for any two sources. */
static void test_precedence(void) {
    static const cf_pair mem[] = {{"db.password", "from-memory"}};
    cf_error             err   = CF_ERROR_INIT;
    cf_builder*          b;
    cf_config*           c;

    check(spill("cf_creds/db.password", "from-credential"), "a credential");
    creds_set("cf_creds");

    b = cf_build(NULL);
    cf_builder_memory(b, "mem", CF_RANK_ENV, mem, 1);
    cf_builder_credentials(b, "creds", CF_RANK_ENV, NULL, 0);
    c = cf_builder_create(b, &err);
    check(c != NULL &&
              strcmp(cf_get_text(c, "db.password", ""), "from-credential") == 0,
          "added after the memory source, the credential wins");
    cf_config_free(c);

    b = cf_build(NULL);
    cf_builder_credentials(b, "creds", CF_RANK_ENV, NULL, 0);
    cf_builder_memory(b, "mem", CF_RANK_ENV, mem, 1);
    c = cf_builder_create(b, &err);
    check(c != NULL && strcmp(cf_get_text(c, "db.password", ""), "from-memory") == 0,
          "added before it, the credential loses");
    cf_config_free(c);
    remove("cf_creds/db.password");
}

static void test_unset(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b;
    cf_config*  c;

    creds_set(NULL);
    b = cf_build(NULL);
    cf_builder_credentials(b, NULL, CF_RANK_ENV, NULL, CF_OPTIONAL);
    c = cf_builder_create(b, &err);
    check(c != NULL, "not run with credentials, and optional: it builds");
    if (c != NULL) {
        check(cf_count(c) == 0, "and the credentials contribute nothing");
    }
    cf_config_free(c);

    creds_set("");
    b = cf_build(NULL);
    cf_builder_credentials(b, NULL, CF_RANK_ENV, NULL, CF_OPTIONAL);
    c = cf_builder_create(b, &err);
    check(c != NULL, "an empty variable is an unset one");
    cf_config_free(c);

    creds_set(NULL);
    b = cf_build(NULL);
    cf_builder_credentials(b, NULL, CF_RANK_ENV, NULL, 0);
    c = cf_builder_create(b, &err);
    check(c == NULL, "not optional, it is an error");
    has(err.message, "CREDENTIALS_DIRECTORY", "that names the variable");
    cf_config_free(c);
}

static void test_refuses(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b;
    cf_config*  c;

    /* systemd made the directory it names; one that is not there is a
       mistake CF_OPTIONAL does not cover. */
    creds_set("cf_no_such_creds");
    b = cf_build(NULL);
    cf_builder_credentials(b, NULL, CF_RANK_ENV, NULL, CF_OPTIONAL);
    c = cf_builder_create(b, &err);
    check(c == NULL, "a variable naming no directory is an error, optional or not");
    has(err.message, "cf_no_such_creds", "naming the directory");
    cf_config_free(c);

    check(spill("cf_creds/Not A Key", "x"), "a credential that names nothing");
    creds_set("cf_creds");
    b = cf_build(NULL);
    cf_builder_credentials(b, NULL, CF_RANK_ENV, NULL, 0);
    c = cf_builder_create(b, &err);
    check(c == NULL, "is an error, not a skip");
    has(err.message, "Not A Key", "and is named");
    has(err.message, "credential", "as a credential");
    cf_config_free(c);
    remove("cf_creds/Not A Key");
}

#else

static void test_disabled(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  c;

    creds_set("cf_creds");
    cf_builder_credentials(b, NULL, CF_RANK_ENV, NULL, CF_OPTIONAL);
    c = cf_builder_create(b, &err);
    check(c == NULL, "systemd's credentials are refused without CF_WITH_DIR");
    has(err.message, "CF_WITH_DIR", "and the error names the option");
    cf_config_free(c);
}

#endif /* CF_WITH_DIR */

int main(void) {
    test_null();
#if CF_WITH_DIR
    test_reads();
    test_precedence();
    test_unset();
    test_refuses();
#else
    test_disabled();
#endif
    creds_set(NULL);
    return cf_test_done();
}
