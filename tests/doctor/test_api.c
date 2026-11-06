/*
 * test_api.c - Doctor as a library (cf_doctor.h): a program linting its
 * own configuration, with a baseline, a severity, a diff and the rule
 * catalogue, in-process and without a report on disk.
 */

#include <configforge/cf_doctor.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../unit/cf_test.h"

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

/* An optional secret with no default, offered only where its policy
   refuses it, is a key nothing won: its report entry says "selected":
   null. Doctor used to find keys by their winner, so this one was in no
   rule's sight - the secret on the command line went unreported - and a
   report with no other key was no report at all. */
static cf_config* build_unset(int with_port) {
    static const char* const only_env[] = {"env", NULL};
    static char              a0[]       = "prog";
    static char              a1[]       = "--license.code=ab12cd";
    static char*             argv[]     = {a0, a1, NULL};
    static char*             noenv[]    = {NULL};
    cf_error                 err        = CF_ERROR_INIT;
    cf_builder*              b          = cf_build(NULL);
    cf_def                   d;

    cf_builder_cli(b, "cli", CF_RANK_CLI, 2, argv);
    cf_builder_env(b, "env", CF_RANK_ENV, "APP_", noenv);
    if (with_port) {
        cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    }
    cf_def_init(&d, CF_TYPE_TEXT, "license.code");
    d.flags  = CF_SENSITIVE;
    d.policy = only_env;
    cf_builder_define(b, &d);
    return cf_builder_create(b, &err);
}

static void test_unselected_key(void) {
    int with_port;

    for (with_port = 0; with_port < 2; ++with_port) {
        cf_error          err = CF_ERROR_INIT;
        cf_config*        c   = build_unset(with_port);
        cf_doctor*        d   = cf_doctor_new();
        const cf_finding* f;

        check(c != NULL && d != NULL, "a configuration with a key nothing won");
        if (c == NULL || d == NULL) {
            cf_config_free(c);
            cf_doctor_free(d);
            continue;
        }
        check(cf_doctor_lint(d, c, NULL, &err),
              with_port ? "is linted, among other keys" : "is linted, as its only key");
        f = find(d, "CFD016", "license.code");
        check(f != NULL && strcmp(f->level, "error") == 0,
              with_port ? "and the secret it refused is found, among other keys"
                        : "and the secret it refused is found, as its only key");
        check(f != NULL && strstr(f->message, "policy:") != NULL &&
                  strstr(f->message, "ab12cd") == NULL,
              "naming the policy and not the secret");
        check(find(d, "CFD007", "license.code") == NULL,
              "once: what policy refused is not reported again for being on the "
              "command line");
        cf_doctor_free(d);
        cf_config_free(c);
    }
}

/* The report as a program writes it, and as one the library could not:
   it will not build a configuration with a required key unset, so a
   required key that nothing won comes from another producer's report. */
static void test_required(void) {
    static const char text[] =
        "{\"configforge_report\":1,\"keys\":{"
        "\"db.host\":{\"definition\":{\"type\":\"text\",\"required\":true},"
        "\"candidates\":[],\"selected\":null},"
        "\"db.port\":{\"definition\":{\"type\":\"int\",\"required\":true},"
        "\"candidates\":[{\"source\":\"file\",\"value\":\"5432\"}],"
        "\"selected\":{\"source\":\"file\"}}}}";
    cf_error          err = CF_ERROR_INIT;
    cf_doctor*        d   = cf_doctor_new();
    const cf_finding* f;

    check(cf_doctor_lint_report(d, text, strlen(text), "required.json", &err),
          "a report is linted");
    f = find(d, "CFD004", "db.host");
    check(f != NULL && strcmp(f->level, "error") == 0,
          "a required key with no value is an error");
    check(find(d, "CFD004", "db.port") == NULL, "and one with a value is not reported");
    cf_doctor_free(d);
}

/* The smallest report: one key both sides have, and one only this side. */
static size_t keys_report(char* buf, size_t cap, const char* only) {
    return cf_fmt(buf, cap,
                  "{\"configforge_report\":1,\"keys\":{"
                  "\"shared\":{\"candidates\":[{\"source\":\"file\",\"value\":\"1\"}],"
                  "\"selected\":{\"source\":\"file\"}},"
                  "\"%s\":{\"candidates\":[{\"source\":\"file\",\"value\":\"2\"}],"
                  "\"selected\":{\"source\":\"file\"}}}}",
                  only);
}

static void test_parity(void) {
    static char       ra[512], rb[512];
    cf_error          err = CF_ERROR_INIT;
    cf_doctor*        d   = cf_doctor_new();
    size_t            an  = keys_report(ra, sizeof ra, "only.a");
    size_t            bn  = keys_report(rb, sizeof rb, "only.b");
    const cf_finding* f;

    check(cf_doctor_diff_report(d, ra, an, "a.json", rb, bn, "b.json", &err),
          "two reports are compared");
    f = find(d, "CFD012", "only.a");
    check(f != NULL &&
              strstr(f->message, "present in a.json, absent in b.json") != NULL,
          "a key only the first has is found, with where it is and where it is not");
    f = find(d, "CFD012", "only.b");
    check(f != NULL &&
              strstr(f->message, "present in b.json, absent in a.json") != NULL,
          "as is one only the second has: parity runs both ways");
    check(find(d, "CFD012", "shared") == NULL, "and a key both have is not found");
    cf_doctor_free(d);
}

/* An array longer than the one that won: what is beyond its length is
   discarded, and the report says so with "truncated". */
static void test_truncated(void) {
    static const char text[] =
        "{\"configforge_report\":1,\"keys\":{"
        "\"hosts.0\":{\"candidates\":[{\"source\":\"file\",\"value\":\"a\"}],"
        "\"selected\":{\"source\":\"file\"}},"
        "\"hosts.1\":{\"candidates\":[{\"source\":\"file\",\"value\":\"b\","
        "\"at\":\"app.ini:3\",\"outcome\":\"truncated\"}],\"selected\":null}}}";
    cf_error          err = CF_ERROR_INIT;
    cf_doctor*        d   = cf_doctor_new();
    const cf_finding* f;

    check(cf_doctor_lint_report(d, text, strlen(text), "truncated.json", &err),
          "a report is linted");
    f = find(d, "CFD015", "hosts.1");
    check(f != NULL && strcmp(f->level, "warning") == 0 && f->at != NULL &&
              strcmp(f->at, "app.ini:3") == 0,
          "an element beyond the winning length is found, and where it was set");
    check(find(d, "CFD015", "hosts.0") == NULL, "and one within it, that won, is not");
    check(f != NULL &&
              strcmp(f->message, "discarded: beyond the winning array length") == 0,
          "a report that does not say why gets the general sentence");
    cf_doctor_free(d);
}

/* The same finding from a configuration the library built, which is how
   a program that lints itself meets it: the file lists three hosts and
   says there are three, and the command line says there is one. The
   library keeps what it cut in its report, with the count that cut it.
   Only the elements beyond the length are found. The one inside it is
   not, and neither is the count the file lost, which is shadowed and not
   cut; nothing else takes a key that has no value for a key that is
   wrong. */
static void test_cut_by_the_library(void) {
    static const cf_pair file[] = {
        {"hosts.0", "a"}, {"hosts.1", "b"}, {"hosts.2", "c"}, {"hosts.count", "3"}};
    static char       a0[]   = "prog";
    static char       a1[]   = "--hosts.count=1";
    static char*      argv[] = {a0, a1, NULL};
    cf_error          err    = CF_ERROR_INIT;
    cf_builder*       b      = cf_build(NULL);
    cf_config*        c;
    cf_doctor*        d = cf_doctor_new();
    const cf_finding* f;

    cf_builder_memory(b, "file", CF_RANK_USER_FILE, file, 4);
    cf_builder_cli(b, "cli", CF_RANK_CLI, 2, argv);
    c = cf_builder_create(b, &err);
    check(c != NULL && d != NULL,
          "a configuration with a file's three hosts and a count");
    if (c == NULL || d == NULL) {
        cf_config_free(c);
        cf_doctor_free(d);
        return;
    }
    check(cf_doctor_lint(d, c, NULL, &err), "is linted");
    f = find(d, "CFD015", "hosts.1");
    check(f != NULL && strcmp(f->level, "warning") == 0 &&
              strcmp(f->message,
                     "discarded: past the end: hosts.count is 1, from cli") == 0,
          "the second host is found, with the count that cut it and whose it is");
    check(find(d, "CFD015", "hosts.2") != NULL, "and so is the third");
    check(find(d, "CFD015", "hosts.0") == NULL &&
              find(d, "CFD015", "hosts.count") == NULL,
          "but not the host inside the length, or the count");
    check(cf_doctor_count(d) == 2, "and nothing else is said about the two it cut");
    cf_doctor_free(d);
    cf_config_free(c);
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

static void test_synthesized_list_count(void) {
    static char  a0[] = "app", a1[] = "--tokens=list-canary";
    static char* argv[] = {a0, a1};
    cf_error     err    = CF_ERROR_INIT;
    cf_builder*  b      = cf_build(NULL);
    cf_config*   c;
    cf_doctor*   doctor = cf_doctor_new();
    cf_def       def;
    cf_value     value;
    cf_def_init(&def, CF_TYPE_TEXT, "tokens");
    def.list_mode   = CF_LIST_APPEND;
    def.flags       = CF_SENSITIVE;
    def.has_default = CF_TRUE;
    cf_value_text(&value, "default-canary", 14);
    def.list_defaults      = &value;
    def.list_default_count = 1;
    cf_builder_define(b, &def);
    cf_builder_cli(b, "cli", CF_RANK_CLI, 2, argv);
    c = cf_builder_create(b, &err);
    check(c != NULL && doctor != NULL, "build a secret appended list and Doctor");
    if (c != NULL && doctor != NULL) {
        size_t i, count_findings = 0, item_findings = 0;
        check(cf_doctor_lint(doctor, c, "production", &err), err.message);
        check(find(doctor, "CFD001", "tokens.count") == NULL,
              "a synthesized aggregate count is not an overwritten assignment");
        for (i = 0; i < cf_doctor_count(doctor); ++i) {
            const cf_finding* f = cf_doctor_finding(doctor, i);
            if (strcmp(f->rule, "CFD007") == 0) {
                if (strcmp(f->key, "tokens.count") == 0) {
                    ++count_findings;
                } else if (strcmp(f->key, "tokens.1") == 0) {
                    ++item_findings;
                }
                check(strstr(f->message, "list-canary") == NULL &&
                          strstr(f->message, "default-canary") == NULL,
                      "list exposure findings never repeat secret contents");
            }
        }
        check(count_findings == 1 && item_findings == 1,
              "only original count and item exposures are reported, not the aggregate");
    }
    cf_doctor_free(doctor);
    cf_config_free(c);
}

/* A report file of a sequence: `keys` is the body of its "keys". */
static int seq_write(const char* path, const char* keys) {
    char   text[1024];
    size_t n =
        cf_fmt(text, sizeof text, "{\"configforge_report\":1,\"keys\":{%s}}", keys);
    FILE* f = fopen(path, "wb");
    int   ok;

    if (f == NULL || n >= sizeof text) {
        if (f != NULL) {
            fclose(f);
        }
        return 0;
    }
    ok = fwrite(text, 1, n, f) == n;
    return (fclose(f) == 0) && ok;
}

/* clang-format off */
#define SEQ_KEY(name, src, cand) \
    "\"" name "\":{\"candidates\":[{\"source\":\"" src "\"," cand "}]," \
    "\"selected\":{\"source\":\"" src "\"}}"
/* clang-format on */

static size_t count_rule(cf_doctor* d, const char* rule, const char* key) {
    size_t i, n = 0;
    for (i = 0; i < cf_doctor_count(d); ++i) {
        const cf_finding* f = cf_doctor_finding(d, i);
        if (strcmp(f->rule, rule) == 0 && (key == NULL || strcmp(f->key, key) == 0)) {
            ++n;
        }
    }
    return n;
}

/* --only-changed through the library: two keys present but empty
   (CFD011), one of which the change emptied; only that one is a finding,
   and the other is counted as left alone. A base that cannot be read is
   an error, not a filter that drops nothing - or everything. */
static void test_only_changed(void) {
    cf_doctor* d   = cf_doctor_new();
    cf_error   err = CF_ERROR_INIT;

    check(seq_write("cf_only_base.json",
                    SEQ_KEY("a", "user-file", "\"value\":\"\"") "," SEQ_KEY(
                        "b", "user-file", "\"value\":\"x\"")) &&
              seq_write("cf_only_head.json",
                        SEQ_KEY("a", "user-file", "\"value\":\"\"") "," SEQ_KEY(
                            "b", "user-file", "\"value\":\"\"")),
          "only-changed: reports written");
    check(cf_doctor_lint_file(d, "cf_only_head.json", &err) &&
              count_rule(d, "CFD011", "a") == 1 && count_rule(d, "CFD011", "b") == 1,
          "a plain lint finds both empty keys");
    cf_doctor_free(d);

    d = cf_doctor_new();
    check(cf_doctor_only_changed_file(d, "cf_only_base.json", &err) &&
              cf_doctor_lint_file(d, "cf_only_head.json", &err),
          "a lint against a base");
    check(count_rule(d, "CFD011", "b") == 1 && count_rule(d, "CFD011", "a") == 0 &&
              cf_doctor_unchanged_count(d) == 1,
          "finds only what the change emptied, and counts what it left alone");
    check(!cf_doctor_only_changed_file(d, "cf_only_none.json", &err) &&
              err.message[0] != '\0',
          "a base that cannot be read is an error");
    cf_doctor_free(d);
    remove("cf_only_base.json");
    remove("cf_only_head.json");
}

/* Four deployments of one service. The port moves from a file to the
   environment with its value, then changes value; the token, a secret,
   changes by digest and is then shown where it was digested, which
   nothing can compare; one key is dropped and another added. */
static void test_history(void) {
    static const char* const seq[] = {"cf_seq_1.json", "cf_seq_2.json", "cf_seq_3.json",
                                      "cf_seq_4.json"};
    static const char* const gap[] = {"cf_seq_1.json", "cf_seq_none.json",
                                      "cf_seq_3.json"};
    cf_error                 err   = CF_ERROR_INIT;
    cf_doctor*               d     = cf_doctor_new();
    const cf_finding*        f;
    size_t                   i;
    int                      leaked = 0;

    check(
        d != NULL &&
            seq_write(
                seq[0],
                SEQ_KEY("port", "file", "\"value\":\"80\",\"at\":\"app.ini:2:1\"") "," SEQ_KEY(
                    "token", "env",
                    "\"digest\":\"aaaa\"") "," SEQ_KEY("old.key", "file",
                                                       "\"value\":\"x\"")) &&
            seq_write(seq[1],
                      SEQ_KEY("port", "env", "\"value\":\"80\"") "," SEQ_KEY(
                          "token", "env",
                          "\"digest\":\"aaaa\"") "," SEQ_KEY("new.key", "file",
                                                             "\"value\":\"y\"")) &&
            seq_write(
                seq[2],
                SEQ_KEY("port", "env", "\"value\":\"81\",\"at\":\"deploy.ini:5:1\"") "," SEQ_KEY(
                    "token", "env",
                    "\"digest\":\"bbbb\"") "," SEQ_KEY("new.key", "file",
                                                       "\"value\":\"y\"")) &&
            seq_write(
                seq[3],
                SEQ_KEY("port", "env", "\"value\":\"81\",\"at\":\"deploy.ini:5:1\"") "," SEQ_KEY(
                    "token", "env",
                    "\"value\":\"secret-canary\"") "," SEQ_KEY("new.key", "file",
                                                               "\"value\":\"y\"")),
        "four reports of one service are written");
    if (d == NULL) {
        return;
    }

    check(cf_doctor_history_files(d, seq, 4, NULL, &err), err.message);
    check(count_rule(d, "HISTORY", NULL) == 6 && cf_doctor_count(d) == 6,
          "a history is one finding a change, and nothing for what did not");
    f = find(d, "HISTORY", "port");
    check(f != NULL && strstr(f->message, "cf_seq_2.json: same value, now from 'env' "
                                          "rather than 'file'") != NULL,
          "a value that moved to another source, at the report it moved in");
    check(count_rule(d, "HISTORY", "port") == 2, "then changed once, and only once");
    f = find(d, "HISTORY", "old.key");
    check(f != NULL && strstr(f->message, "removed") != NULL, "a key dropped");
    f = find(d, "HISTORY", "new.key");
    check(f != NULL && strstr(f->message, "added, from 'file'") != NULL,
          "and a key added");
    for (i = 0; i < cf_doctor_count(d); ++i) {
        f = cf_doctor_finding(d, i);
        if (strcmp(f->key, "port") == 0 &&
            strstr(f->message, "value changed") != NULL) {
            check(strstr(f->message, "cf_seq_3.json") != NULL &&
                      strcmp(f->at, "deploy.ini:5:1") == 0,
                  "a changed value, placed where the later report says it was set");
        }
        if (strcmp(f->key, "token") == 0 &&
            strstr(f->message, "cf_seq_3.json") != NULL) {
            check(strstr(f->message, "value changed (by digest), still from 'env'") !=
                      NULL,
                  "a secret that changed is told by its digest");
        }
        if (strcmp(f->key, "token") == 0 &&
            strstr(f->message, "cf_seq_4.json") != NULL) {
            check(strstr(f->message, "cannot be told") != NULL,
                  "and a step nothing can compare is said to be one");
        }
        if (strstr(f->message, "secret-canary") != NULL ||
            strstr(f->message, "aaaa") != NULL || strstr(f->message, "bbbb") != NULL ||
            strstr(f->message, "81") != NULL) {
            ++leaked;
        }
    }
    check(leaked == 0, "no value or digest is in any message");
    cf_doctor_free(d);

    d = cf_doctor_new();
    check(d != NULL && cf_doctor_history_files(d, seq, 4, "port", &err) &&
              cf_doctor_count(d) == 2,
          "one key's history is that key's alone");
    cf_doctor_free(d);

    d = cf_doctor_new();
    check(d != NULL && cf_doctor_blame_files(d, seq, 4, "port", &err), err.message);
    f = (d != NULL) ? find(d, "BLAME", "port") : NULL;
    check(f != NULL && cf_doctor_count(d) == 1 &&
              strstr(f->message, "from 'env' since cf_seq_3.json: value changed") !=
                  NULL &&
              strcmp(f->at, "deploy.ini:5:1") == 0,
          "blame names the report a value dates from, and where it set it");
    check(cf_doctor_blame_files(d, seq, 4, "token", &err) &&
              (f = find(d, "BLAME", "token")) != NULL &&
              strstr(f->message, "since cf_seq_4.json at least") != NULL,
          "and says 'at least' where an earlier step cannot be compared");
    check(cf_doctor_blame_files(d, seq, 4, "old.key", &err) &&
              (f = find(d, "BLAME", "old.key")) != NULL &&
              strstr(f->message, "absent since cf_seq_2.json: removed") != NULL,
          "a key that is gone is blamed on the report that dropped it");
    check(cf_doctor_blame_files(d, seq, 1, "new.key", &err) == CF_FALSE &&
              err.code == CF_E_KEY,
          "a key in none of the reports is an error, not an empty answer");
    cf_doctor_free(d);

    d = cf_doctor_new();
    check(d != NULL && cf_doctor_blame_files(d, seq, 1, "port", &err) &&
              (f = find(d, "BLAME", "port")) != NULL &&
              strstr(f->message, "from 'file' since cf_seq_1.json, the first of 1") !=
                  NULL,
          "unchanged throughout dates from the first report");
    check(!cf_doctor_history_files(d, gap, 3, NULL, &err) &&
              strstr(err.message, "cf_seq_none.json") != NULL &&
              cf_doctor_count(d) == 1,
          "a sequence with an unreadable report records nothing from the rest");
    check(!cf_doctor_history_files(d, seq, 4, "no.such.key", &err) &&
              err.code == CF_E_KEY && cf_doctor_count(d) == 1,
          "nor does one asked about a key none of them has");
    check(!cf_doctor_history_files(d, seq, 0, NULL, &err) &&
              !cf_doctor_blame_files(d, seq, 4, NULL, &err) && cf_doctor_count(d) == 1,
          "and no reports, or no key to blame, is refused");
    cf_doctor_free(d);

    for (i = 0; i < 4; ++i) {
        remove(seq[i]);
    }
}

/* A program's blind report: a port it defaults to `port`, a token, a
   secret, it defaults to `token`, and the command line `arg` if any. */
static size_t default_report(cf_i64 port, const char* token, const char* arg, char* buf,
                             size_t cap) {
    static char    prog[] = "prog";
    char           a1[64];
    char*          list[3];
    cf_error       err = CF_ERROR_INIT;
    cf_builder*    b   = cf_build(NULL);
    cf_config*     c;
    cf_report_info info = CF_REPORT_INFO_INIT;
    size_t         n;
    int            argc = 1;

    list[0] = prog;
    if (arg != NULL) {
        cf_fmt(a1, sizeof a1, "%s", arg);
        list[argc++] = a1;
    }
    list[argc] = NULL;
    cf_builder_cli(b, "cli", CF_RANK_CLI, argc, list);
    cf_builder_int(b, "port", port, 1, 65535, 0);
    cf_builder_text(b, "token", token, CF_SENSITIVE);
    c = cf_builder_create(b, &err);
    if (c == NULL) {
        return 0;
    }
    info.mode = CF_REPORT_BLIND;
    n         = cf_report(c, &info, buf, cap);
    cf_config_free(c);
    return n < cap ? n : 0;
}

static cf_doctor* default_compare(const char* a, size_t an, const char* b, size_t bn) {
    cf_error   err = CF_ERROR_INIT;
    cf_doctor* d   = cf_doctor_new();

    check(d != NULL && an > 0 && bn > 0 &&
              cf_doctor_diff_report(d, a, an, "<a>", b, bn, "<b>", &err),
          "two reports are compared");
    return d;
}

/* CFD017. Today's deployment and an upgrade's: the same value from
   different defaults is invisible to a value diff and reaches every
   deployment that never set the key. */
static void test_default_drift(void) {
    static char       ra[16384], rb[16384], rc[16384];
    size_t            an = default_report(8080, "alpha", NULL, ra, sizeof ra);
    size_t            bn = default_report(9090, "bravo", "--port=8080", rb, sizeof rb);
    size_t            cn = default_report(9090, "alpha", NULL, rc, sizeof rc);
    cf_doctor*        d;
    const cf_finding* f;

    d = default_compare(ra, an, rb, bn);
    f = find(d, "CFD017", "port");
    check(f != NULL && strcmp(f->level, "warning") == 0 &&
              strstr(f->message, "takes its default here, '8080'; there the default "
                                 "is '9090'") != NULL,
          "a value one side defaults to and the other sets: the defaults differ");
    check(find(d, "VALUE", "port") == NULL, "which no value diff shows");
    f = find(d, "CFD017", "token");
    check(f != NULL && strstr(f->message, "takes its default in both; the defaults "
                                          "differ (by digest)") != NULL,
          "a secret's defaults are compared by digest");
    check(find(d, "CFD014", "token") == NULL, "and said once, as a default");
    cf_doctor_free(d);

    d = default_compare(ra, an, rc, cn);
    f = find(d, "CFD017", "port");
    check(f != NULL &&
              strstr(f->message, "takes its default in both, '8080' here and '9090' "
                                 "there") != NULL,
          "two defaults that differ, both taken");
    check(find(d, "VALUE", "port") == NULL, "are one finding, not a value diff too");
    check(find(d, "CFD017", "token") == NULL, "the same default is no drift");
    cf_doctor_free(d);

    d = default_compare(ra, an, ra, an);
    check(count_rule(d, "CFD017", NULL) == 0, "and a report against itself has none");
    cf_doctor_free(d);
}

/* What the candidate says decides; a report without the mark is read by
   its sources, a kind "default" or a name, as it was before the mark. */
static void test_default_marks(void) {
    /* clang-format off */
    static const char marked[] =
        "{\"configforge_report\":1,\"keys\":{"
        SEQ_KEY("k", "app", "\"value\":\"1\",\"declared_default\":true") "}}";
    static const char by_kind[] =
        "{\"configforge_report\":1,\"sources\":[{\"name\":\"base\","
        "\"kind\":\"default\",\"rank\":0}],\"keys\":{"
        SEQ_KEY("k", "base", "\"value\":\"2\"") "}}";
    static const char unmarked[] =
        "{\"configforge_report\":1,\"keys\":{"
        SEQ_KEY("k", "app", "\"value\":\"1\"") "}}";
    static const char by_name[] =
        "{\"configforge_report\":1,\"keys\":{"
        SEQ_KEY("k", "default", "\"value\":\"2\"") "}}";
    /* clang-format on */
    cf_doctor*        d;
    const cf_finding* f;

    d = default_compare(marked, sizeof marked - 1, by_kind, sizeof by_kind - 1);
    f = find(d, "CFD017", "k");
    check(f != NULL && strstr(f->message, "in both, '1' here and '2' there") != NULL,
          "a marked default, and one a source of the kind 'default' supplied");
    cf_doctor_free(d);

    d = default_compare(marked, sizeof marked - 1, unmarked, sizeof unmarked - 1);
    f = find(d, "CFD017", "k");
    check(f != NULL &&
              strstr(f->message, "takes its default here; there it has none") != NULL,
          "the mark, not the source's name, makes a default");
    cf_doctor_free(d);

    d = default_compare(unmarked, sizeof unmarked - 1, by_name, sizeof by_name - 1);
    f = find(d, "CFD017", "k");
    check(f != NULL &&
              strstr(f->message, "takes its default there; here it has none") != NULL,
          "and without a table of sources, a source named 'default' is one");
    cf_doctor_free(d);
}

/* CFD018. A program that tracks its reads: what a source set and the
   program never read is a setting that changes nothing. One that does
   not track says nothing either way. */
static void test_unread(void) {
    static char prog[] = "prog", port[] = "--port=9000", host[] = "--host=web1";
    char*       argv[4];
    int         track;

    argv[0] = prog;
    argv[1] = port;
    argv[2] = host;
    argv[3] = NULL;
    for (track = 1; track >= 0; --track) {
        cf_error          err = CF_ERROR_INIT;
        cf_builder*       b   = cf_build(NULL);
        cf_doctor*        d   = cf_doctor_new();
        cf_config*        c;
        const cf_finding* f;

        cf_builder_cli(b, "cli", CF_RANK_CLI, 3, argv);
        cf_builder_int(b, "port", 8080, 1, 65535, 0);
        cf_builder_text(b, "host", "localhost", 0);
        cf_builder_bool(b, "tls", CF_FALSE, 0);
        if (track) {
            cf_builder_track_reads(b);
        }
        c = cf_builder_create(b, &err);
        check(c != NULL && d != NULL, err.message);
        if (c == NULL || d == NULL) {
            cf_config_free(c);
            cf_doctor_free(d);
            return;
        }
        (void)cf_get_int(c, "port", 0);
        (void)cf_get_bool(c, "tls", CF_FALSE);
        check(cf_doctor_lint(d, c, NULL, &err),
              "a configuration that tracks is linted");
        f = find(d, "CFD018", "host");
        if (track) {
            check(f != NULL && strcmp(f->level, "info") == 0 && f->at != NULL &&
                      strcmp(f->at, "argv[2]") == 0 &&
                      strstr(f->message, "set by cli and never read") != NULL,
                  "a value the command line set and nothing read");
            check(find(d, "CFD018", "port") == NULL, "a value read is not one");
            check(find(d, "CFD018", "tls") == NULL, "nor is a default never read");
        } else {
            check(count_rule(d, "CFD018", NULL) == 0,
                  "without tracking there is no finding");
        }
        cf_doctor_free(d);
        cf_config_free(c);
    }
}

#if CF_WITH_REPORT && CF_WITH_JSON
/* A blind report of `n` keys, "k<i>" = i + bump on even i, in `out`. */
static char* sized_report(size_t n, int bump, size_t* len) {
    static char    keys[16384][16], values[16384][16];
    static cf_pair pairs[16384]; /* off the stack: the suite keeps a budget */
    cf_builder*    b   = cf_build(NULL);
    cf_error       err = CF_ERROR_INIT;
    cf_config*     c;
    char*          buf = NULL;
    size_t         i, need;

    for (i = 0; i < n && i < 16384; ++i) {
        sprintf(keys[i], "k%lu", (unsigned long)i);
        sprintf(values[i], "%lu",
                (unsigned long)(i + (size_t)((i % 2 == 0) ? bump : 0)));
        pairs[i].key   = keys[i];
        pairs[i].value = values[i];
        cf_builder_int(b, keys[i], 0, 0, 0, 0);
    }
    cf_builder_memory(b, "mem", CF_RANK_USER_FILE, pairs, n);
    c = cf_builder_create(b, &err);
    if (c == NULL) {
        return NULL;
    }
    need = cf_report(c, NULL, NULL, 0);
    buf  = (char*)malloc(need + 1);
    if (buf != NULL) {
        *len = cf_report(c, NULL, buf, need + 1);
    }
    cf_config_free(c);
    return buf;
}

/* Milliseconds for the best of two diffs of two n-key reports. */
static double time_diff(size_t n) {
    size_t la = 0, lb = 0;
    char*  a    = sized_report(n, 0, &la);
    char*  b    = sized_report(n, 1, &lb);
    double best = -1.0;
    int    r;

    for (r = 0; a != NULL && b != NULL && r < 2; ++r) {
        cf_doctor* d   = cf_doctor_new();
        cf_error   err = CF_ERROR_INIT;
        clock_t    t0  = clock();
        double     ms;
        cf_bool    ok = cf_doctor_diff_report(d, a, la, "a", b, lb, "b", &err);
        ms            = (double)(clock() - t0) * 1000.0 / (double)CLOCKS_PER_SEC;
        cf_doctor_free(d);
        if (!ok) {
            best = -1.0;
            break;
        }
        if (best < 0.0 || ms < best) {
            best = ms;
        }
    }
    free(a);
    free(b);
    return best;
}

/* diff and history look every key of one report up in the other; a
   scan per look-up made them quadratic, and two 20,000-key reports took
   seven seconds (bench/). Sixteen times the keys must cost about sixteen
   times as long - 19 measured, the log of a lookup - and a scan made it
   56, so the bound is twice linear, 32.
   Four times was too small a step: below a few thousand keys the cost
   of reading a report hides the scan, and the old code passed. */
static void test_diff_scales(void) {
    double small = time_diff(1000);
    double large = time_diff(16000);

    check(small >= 0.0 && large >= 0.0, "diffs of 1000 and 16000 keys");
    printf("  doctor diff: 1000 keys %.1f ms, 16000 keys %.1f ms\n", small, large);
    if (small < 5.0) {
        printf("  (too fast to compare; ratio not checked)\n");
        return;
    }
    check(large < small * 32.0,
          "a diff of sixteen times the keys costs about sixteen times");
}
#endif

int main(void) {
#if CF_WITH_REPORT && CF_WITH_JSON
    test_lint();
    test_gate();
    test_baseline_and_severity();
    test_diff();
    test_origin_drift();
    test_unselected_key();
    test_required();
    test_parity();
    test_truncated();
    test_cut_by_the_library();
    test_inputs();
    test_synthesized_list_count();
    test_history();
    test_only_changed();
    test_default_drift();
    test_default_marks();
    test_unread();
    test_diff_scales();
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
    return cf_test_done();
}
