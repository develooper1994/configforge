/*
 * test_report.c - the provenance report and Report Replay.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <string.h>

#if CF_WITH_REPORT

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
    cf_report_info info = CF_REPORT_INFO_INIT;

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

static void test_sensitive_defaults(void) {
    static char        setting[]  = "PRIVATE_SECRET__OVERRIDE=private-live-43";
    static char*       settings[] = {setting, NULL};
    static const char* keys[] = {"secret.untouched", "secret.override", "secret.pin"};
    static const char* defaults[] = {"\"default\": \"private-default-17\"",
                                     "\"default\": \"private-fallback-89\"",
                                     "\"default\": 817263"};
    cf_error           err        = CF_ERROR_INIT;
    cf_builder*        b          = cf_build(NULL);
    cf_config*         cfg;
    static char        buf[16384];
    char               short_buf[31];
    size_t             i, n;

    cf_builder_env(b, "env", CF_RANK_ENV, "PRIVATE_", settings);
    cf_builder_text(b, "secret.untouched", "private-default-17", CF_SENSITIVE);
    cf_builder_text(b, "secret.override", "private-fallback-89", CF_SENSITIVE);
    cf_builder_int(b, "secret.pin", 817263, 0, 0, CF_SENSITIVE);
    cf_builder_text(b, "ordinary.default", "public-value", 0);
    cfg = cf_builder_create(b, &err);
    check(cfg != NULL, "sensitive selected and shadowed defaults build");
    if (cfg == NULL) {
        printf("  %s\n", err.message);
        return;
    }

    n = report(cfg, CF_REPORT_BLIND, buf, sizeof buf);
    check(n > 0 && n < sizeof buf, "blind default report fits");
    lacks(buf, "private-default-17", "a selected secret default is absent");
    lacks(buf, "private-fallback-89", "a shadowed secret default is absent");
    lacks(buf, "private-live-43", "the explicit secret stays absent too");
    lacks(buf, "817263", "a numeric secret default is absent");
    has(buf, "\"default\": \"public-value\"", "public default metadata survives");
    check(report(cfg, CF_REPORT_BLIND, NULL, 0) == n,
          "redacting definition defaults preserves measurement");
    check(report(cfg, CF_REPORT_BLIND, short_buf, sizeof short_buf) == n &&
              memcmp(short_buf, buf, sizeof short_buf - 1) == 0 &&
              short_buf[sizeof short_buf - 1] == '\0',
          "a short blind report has the same length and safe prefix");

    for (i = 0; i < sizeof keys / sizeof keys[0]; ++i) {
        n = cf_explain_json(cfg, keys[i], CF_REPORT_BLIND, buf, sizeof buf);
        check(n > 0 && n < sizeof buf, "one sensitive default's report fits");
        lacks(buf, "\"default\":", "blind key metadata omits the secret default");
        has(buf, "\"redacted\": true", "default candidates are still redacted");
        has(buf, "\"digest\":", "default candidates retain change evidence");
        cf_explain_json(cfg, keys[i], CF_REPORT_FULL, buf, sizeof buf);
        has(buf, defaults[i], "explicit full key reports retain default metadata");
    }

    cf_report(cfg, NULL, buf, sizeof buf);
    lacks(buf, "private-default-17", "NULL report info is blind for defaults too");
    lacks(buf, "private-fallback-89", "NULL info also hides shadowed defaults");
    report(cfg, CF_REPORT_FULL, buf, sizeof buf);
    for (i = 0; i < sizeof defaults / sizeof defaults[0]; ++i) {
        has(buf, defaults[i], "explicit full reports retain default metadata");
    }
    has(buf, "private-live-43", "full reports retain the explicit value");
    cf_config_free(cfg);
}

/* Which candidate is the declared default is said by the candidate, and
   not by an appended list's count: that count is a result, even where a
   default is all that fed it. */
static void test_default_marks(void) {
    static char  p0[]   = "prog";
    static char  p1[]   = "--server.port=7000";
    static char* args[] = {p0, p1, NULL};
    cf_error     err    = CF_ERROR_INIT;
    cf_builder*  b      = cf_build(NULL);
    cf_config*   cfg;
    cf_def       def;
    cf_value     value;
    static char  buf[16384];
    const char*  mark;

    cf_builder_cli(b, "cli", CF_RANK_CLI, 2, args);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_def_init(&def, CF_TYPE_TEXT, "tags");
    def.list_mode   = CF_LIST_APPEND;
    def.has_default = CF_TRUE;
    cf_value_text(&value, "a", 1);
    def.list_defaults      = &value;
    def.list_default_count = 1;
    cf_builder_define(b, &def);
    cfg = cf_builder_create(b, &err);
    check(cfg != NULL, "a default, a value over one, and a defaulted list build");
    if (cfg == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    cf_explain_json(cfg, "server.port", CF_REPORT_FULL, buf, sizeof buf);
    has(buf, "\"value\": 8080, \"outcome\": \"shadowed\"",
        "the shadowed default is in the chain");
    mark = strstr(buf, "\"declared_default\": true");
    check(mark != NULL && strstr(mark + 1, "\"declared_default\"") == NULL,
          "and is the one candidate marked a default");
    report(cfg, CF_REPORT_FULL, buf, sizeof buf);
    has(buf, "\"synthesized\": true", "the list's count is synthesized");
    lacks(buf, "\"synthesized\": true, \"declared_default\"",
          "and is not marked a default");
    cf_config_free(cfg);
}

static void test_invalid_modes(void) {
    cf_error       err  = CF_ERROR_INIT;
    cf_config*     cfg  = build(&err);
    cf_report_info info = CF_REPORT_INFO_INIT;
    static char    buf[16384];
    int            mode;

    check(cfg != NULL, "invalid-mode fixture builds");
    if (cfg == NULL) {
        return;
    }
    for (mode = -1; mode <= 2; mode += 3) {
        info.mode = (cf_report_mode)mode;
        buf[0]    = 'x';
        check(cf_report(cfg, &info, buf, sizeof buf) == 0 && buf[0] == '\0',
              "an unsupported mode refuses the report instead of exposing values");
        check(cf_report(cfg, &info, NULL, 0) == 0,
              "an unsupported report mode measures zero");
        buf[0] = 'x';
        check(cf_explain_json(cfg, "db.password", info.mode, buf, sizeof buf) == 0 &&
                  buf[0] == '\0',
              "an unsupported mode refuses a single-key report too");
        check(cf_explain_json(cfg, "db.password", info.mode, NULL, 0) == 0,
              "an unsupported single-key mode measures zero");
    }
    cf_config_free(cfg);
}

/* D-51: cf_report_info carries its size. An info that never had it set
   is refused - cf_report has no error to give, so it writes nothing; one
   longer than 1.0's, as a later release's would be, is read as far as 1.0
   reaches; and CF_REPORT_INFO_INIT alone is the blind report a NULL info
   asks for. */
static void test_info_size(void) {
    cf_error       err  = CF_ERROR_INIT;
    cf_config*     cfg  = build(&err);
    cf_report_info init = CF_REPORT_INFO_INIT;
    cf_report_info info;
    static char    blind[16384], other[16384];
    union {
        cf_report_info info;
        unsigned char  bytes[sizeof(cf_report_info) + 16];
    } bigger;
    size_t n, m;

    if (cfg == NULL) {
        check(0, "builds");
        return;
    }
    n = cf_report(cfg, NULL, blind, sizeof blind);
    m = cf_report(cfg, &init, other, sizeof other);
    check(n > 0 && m == n && strcmp(blind, other) == 0,
          "CF_REPORT_INFO_INIT alone is the blind report that a NULL info gives");

    info      = init;
    info.size = 0;
    memset(other, 'x', sizeof other);
    check(cf_report(cfg, &info, other, sizeof other) == 0 && other[0] == '\0',
          "an info that never had its size set writes nothing");
    info.size = sizeof info - 1;
    memset(other, 'x', sizeof other);
    check(cf_report(cfg, &info, other, sizeof other) == 0 && other[0] == '\0',
          "nor does one a byte short of 1.0's");
    check(cf_report(cfg, &info, NULL, 0) == 0, "and measuring it says 0");

    memset(bigger.bytes, 0xA5, sizeof bigger.bytes);
    bigger.info      = init;
    bigger.info.size = sizeof bigger;
    memset(other, 'x', sizeof other);
    m = cf_report(cfg, &bigger.info, other, sizeof other);
    check(m == n && strcmp(blind, other) == 0,
          "an info longer than 1.0's is read as far as 1.0 reaches");
    cf_config_free(cfg);
}

/* explain() for a machine: one key, as the report holds it, valid JSON,
   redacted as the report is; and the report says which build wrote it. */
static void test_explain_json(cf_arena* ar) {
    cf_error        err = CF_ERROR_INIT;
    cf_config*      cfg = build(&err);
    static char     buf[4096];
    const cf_codec* json = cf_codec_by_name("json");
    cf_keyspace     ks;
    cf_flatten      fl;
    cf_blob         blob;
    size_t          n;

    if (cfg == NULL) {
        check(0, "builds");
        return;
    }
    n = cf_explain_json(cfg, "server.port", CF_REPORT_BLIND, buf, sizeof buf);
    check(n > 0 && n < sizeof buf, "one key's chain is written");
    has(buf, "\"configforge_key\": 1", "marked as one key of a schema-1 report");
    has(buf, "\"key\": \"server.port\"", "named");
    has(buf, "\"candidates\"", "with its candidates");
    has(buf, "\"source\": \"cli\"", "and the winner");
    check(cf_explain_json(cfg, "server.port", CF_REPORT_BLIND, NULL, 0) == n,
          "measured as it is written");
    cf_blob_mem(&blob, buf, strlen(buf), "explain");
    cf_ks_init(&ks, ar, 64);
    cf_flatten_init(&fl, &ks, 0);
    check(json->decode(&blob, cf_flatten_sink(&fl), &err), "and it is JSON");

    cf_explain_json(cfg, "db.password", CF_REPORT_BLIND, buf, sizeof buf);
    lacks(buf, "hunter2", "a sensitive key's value is not in a blind explanation");
    has(buf, "\"digest\"", "but its digest is");

    cf_explain_json(cfg, "no.such.key", CF_REPORT_BLIND, buf, sizeof buf);
    has(buf, "\"selected\": null", "a key nobody supplied has no winner");

    report(cfg, CF_REPORT_BLIND, buf, sizeof buf);
    has(buf, "\"build\": ", "the report names the build that wrote it");
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
    cf_builder_replay(b, "replay", CF_RANK_ENV, "cf_rep.json", 0);
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
    cf_builder_replay(b, "replay", CF_RANK_ENV, "cf_blind.json", 0);
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
    cf_builder_replay(b, "replay", CF_RANK_ENV, "cf_notrep.json", 0);
    check(cf_builder_create(b, &err) == NULL, "an ordinary document is not a report");
    has(err.message, "not a provenance report", "and says so");
    remove("cf_notrep.json");

    check(spill("cf_v99.json", "{\"configforge_report\":99,\"mode\":\"full\"}"),
          "wrote a future report");
    b = cf_build(NULL);
    cf_builder_replay(b, "replay", CF_RANK_ENV, "cf_v99.json", 0);
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
        check(d1 != NULL && d3 != NULL && strncmp(d1, d3, 22) == 0,
              "the same secret gives the same digest");
    }
}

static void test_digest_text_bytes(void) {
    static char before[4096], after[4096];
    char        text[64];
    cf_def      def;
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b;
    cf_config*  c;
    const char* first;
    const char* second;
    size_t      i;

    memset(text, 'a', sizeof text);
    for (i = 0; i < 4; ++i) {
        if (i == 1) {
            text[sizeof text - 1] = 'b';
        } else if (i == 2) {
            text[2] = '\0';
        } else if (i == 3) {
            memset(text, 'a', sizeof text);
        }
        cf_def_init(&def, CF_TYPE_TEXT, "secret");
        def.flags       = CF_SENSITIVE;
        def.has_default = CF_TRUE;
        cf_value_text(&def.default_value, text, sizeof text);
        b = cf_build(NULL);
        check(b != NULL, "a byte-length secret builder");
        if (b == NULL) {
            return;
        }
        check(cf_builder_define(b, &def), "a byte-length secret definition");
        c = cf_builder_create(b, &err);
        check(c != NULL, "a byte-length secret configuration");
        if (c == NULL) {
            return;
        }
        report(c, CF_REPORT_BLIND, i == 0 ? before : after, sizeof before);
        cf_config_free(c);
        has(i == 0 ? before : after, "\"length\": 64",
            "redacted text retains its actual byte length");
        first = strstr(before, "\"digest\": \"fnv1a:");
        check(first != NULL, "the long secret has a digest");
        if (i > 0 && i < 3) {
            second = strstr(after, "\"digest\": \"fnv1a:");
            check(first != NULL && second != NULL &&
                      strncmp(first, second, strlen("\"digest\": \"fnv1a:") + 16) != 0,
                  "changes past the numeric buffer and embedded NUL are hashed");
        } else if (i == 3) {
            second = strstr(after, "\"digest\": \"fnv1a:");
            check(first != NULL && second != NULL &&
                      strncmp(first, second, strlen("\"digest\": \"fnv1a:") + 16) == 0,
                  "identical long text has identical comparison evidence");
        }
    }
}

/* The fingerprint, worked out here from its definition: FNV-1a over
   key, type and value of each key in order, a secret entering as the
   digest a blind report shows - so this test, and not the code under
   it, says what the number is. */
static cf_u64 fnv(cf_u64 h, const char* p) {
    for (; *p != '\0'; ++p) {
        h ^= (cf_u64)(unsigned char)*p;
        h *= ((cf_u64)256U << 32) | (cf_u64)435U;
    }
    return h;
}
static cf_u64 fnv_basis(void) {
    return ((cf_u64)3421674724U << 32) | (cf_u64)2216829733U;
}
static cf_u64 entry(cf_u64 h, const char* key, const char* type, const char* value) {
    h = fnv(fnv(h, key), "\037");
    h = fnv(fnv(h, type), "\037");
    return fnv(fnv(h, value), "\036");
}

static void test_fingerprint(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_config*  c   = build(&err);
    cf_u64      fp;
    static char buf[8192]; /* off the stack: the suite keeps a budget */
    char        secret[24], text[24], expect[64];
    cf_u64      h;

    check(c != NULL, "fingerprint: builds");
    if (c == NULL) {
        return;
    }
    fp = cf_fingerprint(c);
    {
        /* hunter2's digest, as the blind report writes it */
        cf_value v;
        v.type  = CF_TYPE_TEXT;
        v.u.t.p = "hunter2";
        v.u.t.n = 7;
        (void)cf_fingerprint_format(cf_ks_value_digest(&v), secret, sizeof secret);
    }
    h = fnv_basis();
    h = entry(h, "db.password", "text", secret);
    h = entry(h, "server.host", "text", "0.0.0.0");
    h = entry(h, "server.port", "int", "7000");
    check(fp == h, "the fingerprint is the definition: a secret by its blind digest");
    check(fp != cf_digest(c), "and is not cf_digest, which hashes the secret itself");

    check(cf_fingerprint_format(fp, text, sizeof text) == 22 &&
              strncmp(text, "fnv1a:", 6) == 0 && strlen(text) == 22,
          "written as fnv1a: and sixteen hex digits");
    check(cf_fingerprint_format(fp, buf, 5) == 22 && strcmp(buf, "fnv1") == 0,
          "and as far as the buffer allows");
    cf_fmt(expect, sizeof expect, "\"fingerprint\": \"%s\"", text);
    (void)report(c, CF_REPORT_BLIND, buf, sizeof buf);
    has(buf, expect, "a blind report carries it");
    lacks(buf, "hunter2", "and still no secret");
    (void)report(c, CF_REPORT_FULL, buf, sizeof buf);
    has(buf, expect, "a full report carries the same one");
    cf_config_free(c);

    c = build(&err);
    check(c != NULL && cf_fingerprint(c) == fp,
          "the same configuration, the same fingerprint");
    cf_config_free(c);
    e0[19] = 'X'; /* MYAPP_DB__PASSWORD=Xunter2 */
    c      = build(&err);
    check(c != NULL && cf_fingerprint(c) != fp, "a changed secret changes it");
    cf_config_free(c);
    e0[19] = 'h';
    a1[16] = '1'; /* --server.port=7010 */
    c      = build(&err);
    check(c != NULL && cf_fingerprint(c) != fp, "and so does any other value");
    cf_config_free(c);
    a1[16] = '0';
}

int main(void) {
    cf_arena* ar = cf_arena_new(NULL, 1 << 16);

    test_fingerprint();
    test_blind_report();
    test_digest_distinguishes();
    test_digest_text_bytes();
    test_full_report();
    test_sensitive_defaults();
    test_default_marks();
    test_invalid_modes();
    test_info_size();
    test_report_is_valid_json(ar);
    test_explain_json(ar);
    test_replay();
    test_blind_report_refuses_replay();
    test_replay_rejects_other_json();

    cf_arena_free(ar);
    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}

#else /* !CF_WITH_REPORT */

int main(void) {
    printf("skipped: this build has no provenance report (CF_WITH_REPORT=OFF)\n");
    return 0;
}

#endif /* CF_WITH_REPORT */
