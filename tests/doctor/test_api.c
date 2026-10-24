/*
 * test_api.c - Doctor as a library (cf_doctor.h): a program linting its
 * own configuration, with a baseline, a severity, a diff and the rule
 * catalogue, in-process and without a report on disk.
 */

#include <configforge/cf_doctor.h>

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

/* A configuration with two things wrong: a password on the command line
   (CFD007) and a key nobody defined (CFD005). */
static cf_config* build(const char* port_arg) {
    static char a0[] = "prog";
    static char a1[] = "--db.password=hunter2";
    static char a2[] = "--extra.thing=1";
    char*       argv[5];
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_def      d;
    int         argc = 3;
    static char a3[32];

    argv[0] = a0;
    argv[1] = a1;
    argv[2] = a2;
    if (port_arg != NULL) {
        cf_fmt(a3, sizeof a3, "%s", port_arg);
        argv[argc++] = a3;
    }
    argv[argc] = NULL;
    cf_builder_cli(b, "cli", CF_RANK_CLI, argc, argv);
    cf_def_init(&d, CF_TYPE_TEXT, "db.password");
    d.flags = CF_SENSITIVE;
    cf_builder_define(b, &d);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    return cf_builder_create(b, &err);
}

static const cf_finding* find(cf_doctor* d, const char* rule, const char* key) {
    size_t i;
    for (i = 0; i < cf_doctor_count(d); ++i) {
        const cf_finding* f = cf_doctor_finding(d, i);
        if (strcmp(f->rule, rule) == 0 && strcmp(f->key, key) == 0) {
            return f;
        }
    }
    return NULL;
}

static void test_lint(void) {
    cf_error          err = CF_ERROR_INIT;
    cf_config*        c   = build(NULL);
    cf_doctor*        d   = cf_doctor_new();
    const cf_finding* f;

    check(c != NULL && d != NULL, "a configuration and a context");
    if (c == NULL || d == NULL) {
        cf_config_free(c);
        cf_doctor_free(d);
        return;
    }
    check(cf_doctor_lint(d, c, "production", &err),
          "a configuration is linted in-process");
    f = find(d, "CFD007", "db.password");
    check(f != NULL, "a password on the command line is found");
    check(f != NULL && strcmp(f->level, "error") == 0, "as an error");
    check(f != NULL && f->at != NULL && strcmp(f->at, "argv[1]") == 0,
          "and where it was set is known");
    check(f != NULL && strstr(f->message, "hunter2") == NULL,
          "and the message does not carry the secret");
    check(find(d, "CFD005", "extra.thing") != NULL, "an undefined key is found");
    check(cf_doctor_count_at(d, "error") >= 1 && cf_doctor_count_at(d, "warning") >= 1,
          "and counted by level");
    cf_doctor_free(d);
    cf_config_free(c);
}

static void test_baseline_and_severity(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_config*  c   = build(NULL);
    cf_doctor*  d   = cf_doctor_new();
    const char* rule;
    const char* key;

    if (c == NULL || d == NULL) {
        check(0, "built");
        cf_config_free(c);
        cf_doctor_free(d);
        return;
    }
    check(cf_doctor_accept(d, "CFD007", "db.password"), "an accepted finding");
    check(cf_doctor_accept(d, "CFD999", "no.such"), "and one that will not match");
    check(cf_doctor_severity(d, "CFD005", "off", NULL, &err), "a rule switched off");
    check(!cf_doctor_severity(d, "CFD005", "loud", NULL, &err),
          "a level that is not one");
    cf_doctor_lint(d, c, NULL, &err);
    check(find(d, "CFD007", "db.password") == NULL,
          "the accepted finding is not reported");
    check(cf_doctor_count_at(d, "accepted") == 1, "but counted");
    check(find(d, "CFD005", "extra.thing") == NULL,
          "the rule switched off reports nothing");
    check(cf_doctor_stale_count(d) == 1 && cf_doctor_stale(d, 0, &rule, &key) &&
              strcmp(rule, "CFD999") == 0 && strcmp(key, "no.such") == 0,
          "and the entry that matched nothing is stale");
    cf_doctor_free(d);
    cf_config_free(c);
}

static void test_diff(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* a   = build("--server.port=8080");
    cf_config* b   = build("--server.port=9443");
    cf_doctor* d   = cf_doctor_new();

    check(a != NULL && b != NULL && d != NULL, "two configurations");
    if (a != NULL && b != NULL && d != NULL) {
        check(cf_doctor_diff(d, a, b, &err), "are compared in-process");
        check(find(d, "VALUE", "server.port") != NULL,
              "and the value that differs is found");
    }
    cf_doctor_free(d);
    cf_config_free(a);
    cf_config_free(b);
}

static void test_inputs(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_doctor* d   = cf_doctor_new();

    check(!cf_doctor_lint_report(d, "{}", 2, "empty.json", &err),
          "a document that is no report");
    check(strstr(err.message, "not a provenance report") != NULL,
          "is refused, and said why");
    check(cf_doctor_count(d) == 0, "and leaves nothing found");
    check(cf_doctor_rule_count() >= 16, "the catalogue");
    check(cf_doctor_rule_text("CFD010") != NULL && cf_doctor_rule_text("NOPE") == NULL,
          "names its rules");
    cf_doctor_free(d);
}

int main(void) {
#if CF_WITH_REPORT && CF_WITH_JSON
    test_lint();
    test_baseline_and_severity();
    test_diff();
    test_inputs();
#else
    /* A configuration's report needs both; without them the library
       says so. */
    {
        cf_error   err = CF_ERROR_INIT;
        cf_doctor* d   = cf_doctor_new();
        cf_config* c   = build(NULL);
        check(!cf_doctor_lint(d, c, NULL, &err),
              "linting a configuration needs a report");
        cf_doctor_free(d);
        cf_config_free(c);
    }
#endif
    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
