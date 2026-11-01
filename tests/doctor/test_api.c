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

/* One side of a comparison: where two keys came from - "level", and
   "license", which is sensitive - and how its report was written. */
typedef struct drift_side {
    const char*    kind; /* "cli" or "env"            */
    const char*    name; /* what the source is called */
    const char*    level;
    const char*    license;
    cf_report_mode mode;
} drift_side;

/* The report of that side, written by the library as a program writes
   one. A builder reads the arguments or the environment it was given at
   cf_builder_create, not when it is told of them, so what it was given
   lives to that call. */
static size_t side_report(const drift_side* s, char* buf, size_t cap) {
    static char    prog[] = "prog";
    char           a1[64], a2[64];
    char*          list[4];
    cf_error       err = CF_ERROR_INIT;
    cf_builder*    b   = cf_build(NULL);
    cf_def         d;
    cf_config*     c;
    cf_report_info info = CF_REPORT_INFO_INIT;
    size_t         n;

    if (strcmp(s->kind, "cli") == 0) {
        cf_fmt(a1, sizeof a1, "--level=%s", s->level);
        cf_fmt(a2, sizeof a2, "--license=%s", s->license);
        list[0] = prog;
        list[1] = a1;
        list[2] = a2;
        list[3] = NULL;
        cf_builder_cli(b, s->name, CF_RANK_CLI, 3, list);
    } else {
        cf_fmt(a1, sizeof a1, "APP_LEVEL=%s", s->level);
        cf_fmt(a2, sizeof a2, "APP_LICENSE=%s", s->license);
        list[0] = a1;
        list[1] = a2;
        list[2] = NULL;
        cf_builder_env(b, s->name, CF_RANK_ENV, "APP_", list);
    }
    cf_builder_text(b, "level", "info", 0);
    cf_def_init(&d, CF_TYPE_TEXT, "license");
    d.flags = CF_SENSITIVE;
    cf_builder_define(b, &d);
    c = cf_builder_create(b, &err);
    if (c == NULL) {
        return 0;
    }
    info.mode = s->mode;
    n         = cf_report(c, &info, buf, cap);
    cf_config_free(c);
    /* A report longer than the buffer was cut short: not one to compare. */
    return n < cap ? n : 0;
}

static cf_doctor* drift_compare(const drift_side* a, const drift_side* b) {
    static char ra[16384], rb[16384];
    cf_error    err = CF_ERROR_INIT;
    cf_doctor*  d   = cf_doctor_new();
    size_t      an  = side_report(a, ra, sizeof ra);
    size_t      bn  = side_report(b, rb, sizeof rb);

    check(an > 0 && bn > 0 && d != NULL, "two reports are written");
    if (an > 0 && bn > 0 && d != NULL) {
        check(cf_doctor_diff_report(d, ra, an, "<a>", rb, bn, "<b>", &err),
              "and compared");
    }
    return d;
}

/* CFD002 is the same value from two kinds of source, which a value diff
   shows nothing of. It is not a different value from another source,
   which it shows, and the rule used to say both: every key whose source
   differed was drift. */
static void test_origin_drift(void) {
    drift_side cli     = {"cli", "cli", "info", "alpha", CF_REPORT_BLIND};
    drift_side env     = {"env", "env", "info", "alpha", CF_REPORT_BLIND};
    drift_side args    = {"cli", "args", "info", "alpha", CF_REPORT_BLIND};
    drift_side other   = {"env", "env", "debug", "bravo", CF_REPORT_BLIND};
    drift_side env_all = {"env", "env", "info", "alpha", CF_REPORT_FULL};
    cf_doctor* d;

    d = drift_compare(&cli, &env);
    {
        const cf_finding* f = find(d, "CFD002", "level");
        check(f != NULL && strstr(f->message, "'cli'") != NULL &&
                  strstr(f->message, "'env'") != NULL,
              "the same value from the command line and from the environment is "
              "origin drift, and names both");
    }
    check(find(d, "CFD002", "license") != NULL,
          "a secret is the same when its digests are, and drifts as well");
    check(find(d, "VALUE", "level") == NULL && find(d, "CFD014", "license") == NULL,
          "and is no difference in value");
    cf_doctor_free(d);

    d = drift_compare(&cli, &other);
    check(find(d, "VALUE", "level") != NULL && find(d, "CFD014", "license") != NULL,
          "values that differ are reported as differing");
    check(find(d, "CFD002", "level") == NULL && find(d, "CFD002", "license") == NULL,
          "and not as origin drift besides");
    cf_doctor_free(d);

    d = drift_compare(&cli, &args);
    check(find(d, "CFD002", "level") == NULL && find(d, "CFD002", "license") == NULL,
          "two names for one kind of source is no drift");
    cf_doctor_free(d);

    d = drift_compare(&cli, &env_all);
    check(find(d, "CFD002", "level") != NULL,
          "an ordinary value is compared whichever way its report was written");
    check(find(d, "CFD002", "license") == NULL &&
              find(d, "CFD014", "license") == NULL &&
              find(d, "VALUE", "license") == NULL,
          "a secret one report digests and the other shows is not compared: it "
          "neither agrees nor differs");
    cf_doctor_free(d);
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

/* The password moves to the command line in the second snapshot: the
   gate refuses it with the finding, and the first keeps serving. */
static int g_bad = 0;

static cf_builder* live_build(void* user) {
    cf_builder* b = cf_build(NULL);
    cf_def      d;

    (void)user;
    if (g_bad) {
        static char  a0[]   = "prog";
        static char  a1[]   = "--db.password=hunter2";
        static char* argv[] = {a0, a1, NULL};
        cf_builder_cli(b, "cli", CF_RANK_CLI, 2, argv);
    } else {
        static const cf_pair env[] = {{"db.password", "hunter2"}};
        cf_builder_memory(b, "vault", CF_RANK_ENV, env, 1);
    }
    cf_def_init(&d, CF_TYPE_TEXT, "db.password");
    d.flags = CF_SENSITIVE;
    cf_builder_define(b, &d);
    return b;
}

/* Whether `gate` passes the clean snapshot and refuses, naming the rule,
   the one that has a password on the command line. */
static int gate_works(void* gate) {
    cf_error err = CF_ERROR_INIT;
    cf_live* l;
    int      refused;

    g_bad = 0;
    l     = cf_live_open(CF_LIVE_MANUAL, live_build, cf_doctor_live_check, gate, NULL,
                         &err);
    if (l == NULL) {
        return 0;
    }
    g_bad   = 1;
    refused = cf_live_reload(l, &err) == CF_LIVE_FAILED &&
              strstr(err.message, "CFD007") != NULL;
    cf_live_close(l);
    g_bad = 0;
    return refused;
}

static void test_gate(void) {
    static cf_doctor_gate gate  = CF_DOCTOR_GATE_INIT;
    static cf_doctor_gate wrong = CF_DOCTOR_GATE_INIT;
    cf_error              err   = CF_ERROR_INIT;
    cf_live*              l;
    cf_config*            c;

    gate.profile  = "production";
    gate.fail_on  = "error";
    wrong.fail_on = "sometimes";
    g_bad         = 0;
    l = cf_live_open(CF_LIVE_MANUAL, live_build, cf_doctor_live_check, (void*)&gate,
                     NULL, &err);
    check(l != NULL, "a clean configuration passes the gate");
    if (l == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    g_bad = 1;
    check(cf_live_reload(l, &err) == CF_LIVE_FAILED, "the reload is refused");
    check(strstr(err.message, "CFD007") != NULL && strcmp(err.key, "db.password") == 0,
          "naming the rule and the key");
    c = cf_live_get(l);
    {
        char why[512];
        (void)cf_explain(c, "db.password", why, sizeof why);
        check(strstr(why, "vault") != NULL && cf_live_generation(l) == 1,
              "and the clean one keeps serving");
    }
    cf_config_free(c);
    cf_live_close(l);

    l = cf_live_open(CF_LIVE_MANUAL, live_build, cf_doctor_live_check, (void*)&wrong,
                     NULL, &err);
    check(l == NULL && strstr(err.message, "sometimes") != NULL,
          "a gate that fails on no level refuses itself");
    g_bad = 0;

    /* D-51: a gate carries its size. One whose size was never set is
       refused, by name; one longer than 1.0's, as a later release's
       would be, is read as far as 1.0 reaches; and no gate at all is the
       defaults. */
    {
        static cf_doctor_gate bare; /* size 0: not from CF_DOCTOR_GATE_INIT */
        cf_doctor_gate        init = CF_DOCTOR_GATE_INIT;
        union {
            cf_doctor_gate gate;
            unsigned char  bytes[sizeof(cf_doctor_gate) + 16];
        } bigger;

        l = cf_live_open(CF_LIVE_MANUAL, live_build, cf_doctor_live_check, &bare, NULL,
                         &err);
        check(l == NULL && strstr(err.message, "CF_DOCTOR_GATE_INIT") != NULL,
              "a gate that did not start from CF_DOCTOR_GATE_INIT is refused, by name");
        bare.size = sizeof bare - 1;
        l = cf_live_open(CF_LIVE_MANUAL, live_build, cf_doctor_live_check, &bare, NULL,
                         &err);
        check(l == NULL, "and so is one a byte short of 1.0's");

        memset(bigger.bytes, 0xA5, sizeof bigger.bytes);
        bigger.gate      = init;
        bigger.gate.size = sizeof bigger;
        check(gate_works(&init),
              "a gate with only its size set works as the defaults do");
        check(gate_works(&bigger.gate),
              "a gate longer than 1.0's is read as far as 1.0 reaches");
        check(gate_works(NULL), "and no gate at all is the defaults too");
    }
}

int main(void) {
#if CF_WITH_REPORT && CF_WITH_JSON
    test_lint();
    test_gate();
    test_baseline_and_severity();
    test_diff();
    test_origin_drift();
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
