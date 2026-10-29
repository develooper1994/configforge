/*
 * test_reserved.c - the syntax D-10 reserves, ${...}.
 *
 * References to other values are not supported yet, and a value that
 * looks like one is refused rather than taken literally - so that the
 * day they arrive, no configuration that works today changes meaning.
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

static cf_bool spill(const char* path, const char* text) {
    FILE* f = fopen(path, "wb");
    if (f == NULL) {
        return CF_FALSE;
    }
    fputs(text, f);
    fclose(f);
    return CF_TRUE;
}

static char a0[] = "prog";

/* The case it exists for: somebody writes what would be a reference in
   another tool, and is told at construction, with the place to fix. */
static void test_refused_where_written(void) {
    static char  a1[]   = "--cache.dir=${HOME}/cache";
    static char* argv[] = {a0, a1, NULL};
    cf_error     err    = CF_ERROR_INIT;
    cf_builder*  b      = cf_build(NULL);
    cf_config*   c;

    cf_builder_cli(b, "cli", CF_RANK_CLI, 2, argv);
    cf_builder_text(b, "cache.dir", "/tmp", 0);
    c = cf_builder_create(b, &err);
    check(c == NULL, "a value containing ${ fails construction");
    check(err.code == CF_E_VALIDATE, "as a validation error, like any invalid value");
    has(err.message, "cache.dir", "naming the key");
    has(err.message, "$${", "and saying how to write it literally");
    has(err.source, "argv[1]", "and where it was set");
    cf_config_free(c);
}

/* $${ is a literal ${, and the program reads what was meant. */
static void test_escape(void) {
    static char  a1[]   = "--prompt=$${USER} costs $$5";
    static char* argv[] = {a0, a1, NULL};
    cf_error     err    = CF_ERROR_INIT;
    cf_builder*  b      = cf_build(NULL);
    cf_config*   c;

    cf_builder_cli(b, "cli", CF_RANK_CLI, 2, argv);
    cf_builder_text(b, "prompt", "", 0);
    c = cf_builder_create(b, &err);
    check(c != NULL, "an escaped ${ builds");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    /* Only the pair before a brace is an escape; "$$5" is two dollars. */
    check(strcmp(cf_get_text(c, "prompt", ""), "${USER} costs $$5") == 0,
          "and reads back as a literal ${, with other dollars untouched");
    cf_config_free(c);
}

/* A parameter that asked for fallback gets it: the value is refused,
   stays in the chain with its reason, and the next candidate wins. */
static void test_fallback(void) {
    static char  e0[]  = "MYAPP_CACHE__DIR=${XDG_CACHE_HOME}/app";
    static char* env[] = {e0, NULL};
    cf_error     err   = CF_ERROR_INIT;
    cf_builder*  b     = cf_build(NULL);
    cf_config*   c;
    cf_def       d;
    static char  ex[4096];

    cf_builder_env(b, "env", CF_RANK_ENV, "MYAPP_", env);
    cf_def_init(&d, CF_TYPE_TEXT, "cache.dir");
    d.on_invalid  = CF_INVALID_FALLBACK;
    d.has_default = CF_TRUE;
    cf_value_text(&d.default_value, "/var/cache/app", 14);
    cf_builder_define(b, &d);
    c = cf_builder_create(b, &err);
    check(c != NULL, "with fallback, construction succeeds");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(strcmp(cf_get_text(c, "cache.dir", ""), "/var/cache/app") == 0,
          "and the default is the answer");
    cf_explain(c, "cache.dir", ex, sizeof ex);
    has(ex, "rejected", "explain() says the environment's value was refused");
    has(ex, "refers to no key", "and why");
    cf_config_free(c);
}

/* Tier 0 has no definition to ask for fallback, so the default applies:
   strict, the same as for any invalid value. Built by hand rather than
   with cf_app, which on Windows reads the process's real command line
   and would never see this argv. */
static void test_tier0(void) {
    static char  a1[]   = "--log.file=${LOGDIR}/app.log";
    static char* argv[] = {a0, a1, NULL};
    cf_error     err    = CF_ERROR_INIT;
    cf_builder*  b      = cf_build(NULL);
    cf_config*   c;

    cf_builder_cli(b, "cli", CF_RANK_CLI, 2, argv);
    c = cf_builder_create(b, &err);
    check(c == NULL, "Tier 0 refuses it too");
    has(err.message, "log.file", "naming the key");
    cf_config_free(c);
}

/* A file is written by somebody too, typed or not. */
static void test_files(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b;
    cf_config*  c;

    check(spill("cf_res.json", "{ \"cache\": { \"dir\": \"${HOME}\" } }\n"),
          "wrote a JSON file");
    b = cf_build(NULL);
    cf_builder_file(b, "file", CF_RANK_USER_FILE, "cf_res.json", NULL, 0);
    cf_builder_text(b, "cache.dir", "/tmp", 0);
    c = cf_builder_create(b, &err);
    check(c == NULL, "a JSON string is refused like raw text");
    has(err.source, "cf_res.json", "and the file is named");
    cf_config_free(c);
    remove("cf_res.json");

    check(spill("cf_res.ini", "[cache]\ndir = ${HOME}\n"), "wrote an INI file");
    b = cf_build(NULL);
    cf_builder_file(b, "file", CF_RANK_USER_FILE, "cf_res.ini", NULL, 0);
    cf_builder_text(b, "cache.dir", "/tmp", 0);
    c = cf_builder_create(b, &err);
    check(c == NULL, "and so is an INI value");
    has(err.source, "cf_res.ini:2", "with its line");
    cf_config_free(c);
    remove("cf_res.ini");
}

/* The program's own default is code, not configuration, and changes
   with the program; it is not refused. */
static void test_default_is_code(void) {
    static char* argv[] = {a0, NULL};
    cf_error     err    = CF_ERROR_INIT;
    cf_builder*  b      = cf_build(NULL);
    cf_config*   c;

    cf_builder_cli(b, "cli", CF_RANK_CLI, 1, argv);
    cf_builder_text(b, "template", "${name}", 0);
    c = cf_builder_create(b, &err);
    check(c != NULL, "a default containing ${ is the program's business");
    if (c != NULL) {
        check(strcmp(cf_get_text(c, "template", ""), "${name}") == 0, "and kept as is");
    }
    cf_config_free(c);
}

#if CF_WITH_REPORT
/* An escaped value is unescaped once. Replayed from a full report it is
   already what was meant, and must not be refused the second time. */
static void test_replay(void) {
    static char  a1[]    = "--prompt=$${USER}";
    static char* argv[]  = {a0, a1, NULL};
    static char* plain[] = {a0, NULL};
    static char  buf[16384];
    cf_error     err = CF_ERROR_INIT;
    cf_builder*  b   = cf_build(NULL);
    cf_config*   c;
    cf_config*   back;

    cf_report_info info;

    cf_builder_cli(b, "cli", CF_RANK_CLI, 2, argv);
    cf_builder_text(b, "prompt", "", 0);
    c = cf_builder_create(b, &err);
    if (c == NULL) {
        check(0, "builds");
        return;
    }
    memset(&info, 0, sizeof info);
    info.mode = CF_REPORT_FULL;
    cf_report(c, &info, buf, sizeof buf);
    check(spill("cf_res_rep.json", buf), "report written");

    b = cf_build(NULL);
    cf_builder_cli(b, "cli", CF_RANK_CLI, 1, plain);
    cf_builder_replay(b, "replay", CF_RANK_ENV, "cf_res_rep.json");
    cf_builder_text(b, "prompt", "", 0);
    back = cf_builder_create(b, &err);
    check(back != NULL, "the replay builds");
    if (back != NULL) {
        check(strcmp(cf_get_text(back, "prompt", ""), "${USER}") == 0,
              "and has the literal the original meant");
        check(cf_digest(back) == cf_digest(c), "the same configuration");
    } else {
        printf("  %s\n", err.message);
    }
    cf_config_free(back);
    cf_config_free(c);
    remove("cf_res_rep.json");
}
#endif

int main(void) {
    test_refused_where_written();
    test_escape();
    test_fallback();
    test_tier0();
    test_files();
    test_default_is_code();
#if CF_WITH_REPORT
    test_replay();
#endif

    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
