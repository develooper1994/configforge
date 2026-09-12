/*
 * test_report.c - the provenance report and Report Replay.
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
        printf("FAIL: %s  missing \"%s\"\n", w, needle);
    }
}
static void lacks(const char* hay, const char* needle, const char* w) {
    ++g_run;
    if (hay != NULL && strstr(hay, needle) != NULL) {
        ++g_fail;
        printf("FAIL: %s  found \"%s\" when it must not be there\n", w, needle);
    }
}

static char  a0[]   = "prog";
static char  a1[]   = "--server.port=7000";
static char* argv[] = {a0, a1, NULL};
static char  e0[]   = "MYAPP_DB__PASSWORD=hunter2";
static char  e1[]   = "MYAPP_SERVER__PORT=9090";
static char* env[]  = {e0, e1, NULL};

static cf_config* build(cf_error* err) {
    cf_builder* b = cf_build(NULL);

    cf_builder_cli(b, "cli", CF_RANK_CLI, 2, argv);
    cf_builder_env(b, "env", CF_RANK_ENV, "MYAPP_", env);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_builder_text(b, "server.host", "0.0.0.0", 0);
    cf_builder_text(b, "db.password", NULL, CF_REQUIRED | CF_SENSITIVE);
    return cf_builder_create(b, err);
}

static size_t report(cf_config* c, cf_report_mode mode, char* buf, size_t cap) {
    cf_report_info info;

    memset(&info, 0, sizeof info);
    info.mode        = mode;
    info.profile     = "production";
    info.application = "myapp";
    info.app_version = "2.3.1";
    info.generated   = "2026-09-12T00:00:00Z";
    return cf_report(c, &info, buf, cap);
}

/* ---------------------------------------------------------------- */

static void test_blind_report(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_config*  cfg = build(&err);
    static char buf[16384];
    size_t      n;

    check(cfg != NULL, "builds");
    if (cfg == NULL) {
        printf("  %s\n", err.message);
        return;
    }

    n = report(cfg, CF_REPORT_BLIND, buf, sizeof buf);
    check(n > 0 && n < sizeof buf, "blind report fits");

    has(buf, "\"configforge_report\": 1", "schema version at the top");
    has(buf, "\"mode\": \"blind\"", "mode is structural, not inferred");
    has(buf, "\"profile\": \"production\"", "profile label");
    has(buf, "\"features\":", "compiled-in format set recorded");

    /* The chain is the artifact: a shadowed candidate is the whole
       reason the report is worth keeping. */
    has(buf, "\"server.port\"", "key present");
    has(buf, "7000", "the winner");
    has(buf, "9090", "the shadowed candidate");
    has(buf, "\"outcome\": \"shadowed\"", "and why it lost");
    has(buf, "MYAPP_SERVER__PORT", "with where it came from");

    /* The secret must not be in an artifact meant to be safe to keep,
       and everything an audit needs must still be. */
    lacks(buf, "hunter2", "the secret is absent from a blind report");
    has(buf, "\"redacted\": true", "redaction is explicit");
    has(buf, "\"length\": 7", "length survives");
    has(buf, "\"digest\": \"fnv1a:", "and a digest, so a change is visible");
    /* Asserting the prefix alone once let a broken digest through for
       a whole session: the format string had a width specifier the
       library's formatter does not support, so every report carried
       the literal "%08x%08x". The shape is checked now, and
       test_digest_distinguishes checks that it actually distinguishes. */
    {
        const char* d = strstr(buf, "\"digest\": \"fnv1a:");
        size_t      k;
        check(d != NULL, "digest present");
        if (d != NULL) {
            d += strlen("\"digest\": \"fnv1a:");
            for (k = 0; k < 16; ++k) {
                if (!((d[k] >= '0' && d[k] <= '9') || (d[k] >= 'a' && d[k] <= 'f'))) {
                    break;
                }
            }
            check(k == 16 && d[16] == '"', "digest is 16 hex digits");
        }
    }
    has(buf, "\"sensitive\": true", "the definition says why");

    /* Measure-or-write. */
    check(report(cfg, CF_REPORT_BLIND, NULL, 0) == n,
          "the report can be measured first");

    cf_config_free(cfg);
}

static void test_full_report(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_config*  cfg = build(&err);
    static char buf[16384];

    if (cfg == NULL) {
        check(0, "builds");
        return;
    }
    report(cfg, CF_REPORT_FULL, buf, sizeof buf);

    has(buf, "\"mode\": \"full\"", "a full report says so");
    has(buf, "hunter2", "a full report carries the values");
    lacks(buf, "\"redacted\": true", "and does not redact");
    cf_config_free(cfg);
}

static void test_report_is_valid_json(cf_arena* ar) {
    cf_error        err = CF_ERROR_INIT;
    cf_config*      cfg = build(&err);
    static char     buf[16384];
    const cf_codec* json = cf_codec_by_name("json");
    cf_keyspace     ks;
    cf_flatten      fl;
    cf_blob         blob;

    if (cfg == NULL) {
        check(0, "builds");
        return;
    }
    report(cfg, CF_REPORT_FULL, buf, sizeof buf);
    cf_config_free(cfg);

    /* Written by hand rather than through the event encoder, so it
       gets checked against the library's own reader rather than
       trusted. */
    cf_blob_mem(&blob, buf, strlen(buf), "report");
    cf_ks_init(&ks, ar, 256);
    cf_flatten_init(&fl, &ks, 0);
    check(json->decode(&blob, cf_flatten_sink(&fl), &err), "the report parses as JSON");
    if (err.message[0] != '\0') {
        printf("  %s\n", err.message);
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

static void test_replay(void) {
    cf_error     err = CF_ERROR_INIT;
    cf_config*   cfg = build(&err);
    cf_config*   back;
    cf_builder*  b;
    static char  buf[16384];
    static char  a2[]    = "prog";
    static char* plain[] = {a2, NULL};

    if (cfg == NULL) {
        check(0, "builds");
        return;
    }
    report(cfg, CF_REPORT_FULL, buf, sizeof buf);
    check(spill("cf_rep.json", buf), "report written to disk");

    /* No command line, no environment: everything comes from the
       report. This is the point of replay - reproducing what a process
       was running, not reconstructing it from files that may have
       changed since. */
    b = cf_build(NULL);
    cf_builder_cli(b, "cli", CF_RANK_CLI, 1, plain);
    cf_builder_replay(b, "replay", CF_RANK_ENV, "cf_rep.json");
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_builder_text(b, "server.host", "0.0.0.0", 0);
    cf_builder_text(b, "db.password", NULL, CF_REQUIRED | CF_SENSITIVE);
    back = cf_builder_create(b, &err);

    check(back != NULL, "a full report replays");
    if (back == NULL) {
        printf("  %s\n", err.message);
        cf_config_free(cfg);
        return;
    }

    check(cf_get_int(back, "server.port", 0) == 7000,
          "the value the process actually had");
    check(strcmp(cf_get_text(back, "db.password", ""), "hunter2") == 0,
          "including the secret, which is why a full report is one");
    check(cf_digest(back) == cf_digest(cfg),
          "the replayed configuration is the same configuration");

    /* A replayed value must never be mistakable for one that came from
       the environment. */
    {
        static char ex[4096];
        cf_explain(back, "server.port", ex, sizeof ex);
        has(ex, "replay", "replay is a distinct source in explain()");
        has(ex, "cf_rep.json", "and names the artifact");
    }

    cf_config_free(back);
    cf_config_free(cfg);
    remove("cf_rep.json");
}

static void test_blind_report_refuses_replay(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_config*  cfg = build(&err);
    cf_builder* b;
    static char buf[16384];

    if (cfg == NULL) {
        check(0, "builds");
        return;
    }
    report(cfg, CF_REPORT_BLIND, buf, sizeof buf);
    cf_config_free(cfg);
    check(spill("cf_blind.json", buf), "blind report written");

    b = cf_build(NULL);
    cf_builder_replay(b, "replay", CF_RANK_ENV, "cf_blind.json");
    check(cf_builder_create(b, &err) == NULL, "a blind report cannot be replayed");
    /* Half a configuration would look like a whole one, so refusing
       outright is the only honest answer. */
    has(err.message, "value-blind", "and says exactly why");
    check(err.code == CF_E_CAPABILITY, "reported as a capability limit");
    remove("cf_blind.json");
}

static void test_replay_rejects_other_json(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b;

    check(spill("cf_notrep.json", "{\"server\":{\"port\":1}}"), "wrote a doc");
    b = cf_build(NULL);
    cf_builder_replay(b, "replay", CF_RANK_ENV, "cf_notrep.json");
    check(cf_builder_create(b, &err) == NULL, "an ordinary document is not a report");
    has(err.message, "not a provenance report", "and says so");
    remove("cf_notrep.json");

    check(spill("cf_v99.json", "{\"configforge_report\":99,\"mode\":\"full\"}"),
          "wrote a future report");
    b = cf_build(NULL);
    cf_builder_replay(b, "replay", CF_RANK_ENV, "cf_v99.json");
    check(cf_builder_create(b, &err) == NULL,
          "a schema this build does not speak is refused");
    has(err.message, "schema", "naming the mismatch");
    remove("cf_v99.json");
}

/* The point of a digest is that it distinguishes. A test that only
   looks for the prefix proves nothing. */
static void test_digest_distinguishes(void) {
    static char  e2[]   = "MYAPP_DB__PASSWORD=other!!";
    static char* env2[] = {e2, e1, NULL};
    cf_error     err    = CF_ERROR_INIT;
    cf_builder*  b;
    cf_config*   c1;
    cf_config*   c2;
    static char  b1[16384];
    static char  b2[16384];
    const char*  d1;
    const char*  d2;

    c1 = build(&err);
    if (c1 == NULL) {
        check(0, "builds");
        return;
    }
    report(c1, CF_REPORT_BLIND, b1, sizeof b1);
    cf_config_free(c1);

    b = cf_build(NULL);
    cf_builder_cli(b, "cli", CF_RANK_CLI, 2, argv);
    cf_builder_env(b, "env", CF_RANK_ENV, "MYAPP_", env2);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_builder_text(b, "server.host", "0.0.0.0", 0);
    cf_builder_text(b, "db.password", NULL, CF_REQUIRED | CF_SENSITIVE);
    c2 = cf_builder_create(b, &err);
    if (c2 == NULL) {
        check(0, "second build");
        return;
    }
    report(c2, CF_REPORT_BLIND, b2, sizeof b2);
    cf_config_free(c2);

    d1 = strstr(b1, "fnv1a:");
    d2 = strstr(b2, "fnv1a:");
    check(d1 != NULL && d2 != NULL, "both reports carry a digest");
    if (d1 != NULL && d2 != NULL) {
        check(strncmp(d1, d2, 22) != 0, "a different secret gives a different digest");
    }
    /* And the same secret gives the same one, or the digest would be
       useless in the other direction. */
    {
        cf_config*  c3 = build(&err);
        static char b3[16384];
        const char* d3;
        if (c3 == NULL) {
            return;
        }
        report(c3, CF_REPORT_BLIND, b3, sizeof b3);
        cf_config_free(c3);
        d3 = strstr(b3, "fnv1a:");
        check(d3 != NULL && strncmp(d1, d3, 22) == 0,
              "the same secret gives the same digest");
    }
}

int main(void) {
    cf_arena* ar = cf_arena_new(NULL, 1 << 16);

    test_blind_report();
    test_digest_distinguishes();
    test_full_report();
    test_report_is_valid_json(ar);
    test_replay();
    test_blind_report_refuses_replay();
    test_replay_rejects_other_json();

    cf_arena_free(ar);
    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
