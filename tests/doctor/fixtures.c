/*
 * fixtures.c - reports for Doctor to be tested against.
 *
 * Written as a program rather than checked in as JSON, so the fixtures
 * cannot drift from the schema the library actually emits. A stale
 * golden report would make Doctor's tests pass against a format
 * nothing produces any more.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../unit/cf_fixture.h"

static char a0[] = "app";

static int emit(const char* out, const char* profile, int argc, char* const* argv,
                char* const* env, const char* file) {
    cf_error       err = CF_ERROR_INIT;
    cf_builder*    b   = cf_build(NULL);
    cf_config*     c;
    cf_report_info info = CF_REPORT_INFO_INIT;
    static char    buf[65536];

    cf_builder_cli(b, "cli", CF_RANK_CLI, argc, argv);
    cf_builder_env(b, "env", CF_RANK_ENV, "MYAPP_", env);
    if (file != NULL) {
        cf_builder_file(b, "user-file", CF_RANK_USER_FILE, file, NULL, 0);
    }
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_builder_text(b, "server.host", "0.0.0.0", 0);
    cf_builder_text(b, "log.level", "info", 0);
    {
        /* A secret that may only come from the environment. When a
           command line supplies it anyway the library rejects the
           candidate and Doctor is where that becomes loud. */
        static const char* const only_env[] = {"env", NULL};
        cf_def                   d;
        cf_def_init(&d, CF_TYPE_TEXT, "db.password");
        d.flags  = CF_REQUIRED | CF_SENSITIVE;
        d.policy = only_env;
        cf_builder_define(b, &d);
    }

    c = cf_builder_create(b, &err);
    if (c == NULL) {
        fprintf(stderr, "fixtures: %s: %s\n", out, err.message);
        return 1;
    }
    info.mode        = CF_REPORT_BLIND;
    info.profile     = profile;
    info.application = "myapp";
    info.app_version = "2.3.1";
    info.generated   = "2026-09-12T00:00:00Z";
    cf_report(c, &info, buf, sizeof buf);
    cf_config_free(c);
    return spill(out, buf) ? 0 : 1;
}

/* Replaces the first `from` in the report `buf` with `to`. Zero when it
   is not there or there is no room. */
static int swap(char* buf, size_t cap, const char* from, const char* to) {
    char*  at = strstr(buf, from);
    size_t fn = strlen(from), tn = strlen(to);

    if (at == NULL || strlen(buf) - fn + tn >= cap) {
        return 0;
    }
    memmove(at + tn, at + fn, strlen(at + fn) + 1);
    memcpy(at, to, tn);
    return 1;
}

/* One side of a pair of full reports - every value written, secrets
   included - with four secrets, each a marker no output may carry: a
   password the two sides set differently (CFD014), a PIN its own
   contract refuses (CFD006), a salt both sides default to differently
   and a seed one side defaults to and the other sets (CFD017). Doctor
   reads full reports too, and its documentation promises it shows no
   secret from any of them. The PIN's refusal is given the reason a
   library of 1.4.0 or before wrote for a malformed reference, which
   quoted the start of the value, and none of the kind that a library
   since writes beside a reason: fixing the library does not rewrite
   the reports already written. */
static int emit_secret(const char* out, char* const* env, const char* salt,
                       const char* seed) {
    cf_error       err = CF_ERROR_INIT;
    cf_builder*    b   = cf_build(NULL);
    cf_config*     c;
    cf_report_info info = CF_REPORT_INFO_INIT;
    static char    buf[65536];
    cf_def         d;

    cf_builder_env(b, "env", CF_RANK_ENV, "MYAPP_", env);
    cf_def_init(&d, CF_TYPE_TEXT, "db.password");
    d.flags = CF_SENSITIVE;
    cf_builder_define(b, &d);
    cf_def_init(&d, CF_TYPE_TEXT, "api.pin");
    d.flags      = CF_SENSITIVE;
    d.max_len    = 4;
    d.on_invalid = CF_INVALID_FALLBACK;
    cf_builder_define(b, &d);
    cf_def_init(&d, CF_TYPE_TEXT, "api.salt");
    d.flags       = CF_SENSITIVE;
    d.has_default = CF_TRUE;
    cf_value_text(&d.default_value, salt, strlen(salt));
    cf_builder_define(b, &d);
    cf_def_init(&d, CF_TYPE_TEXT, "api.seed");
    d.flags       = CF_SENSITIVE;
    d.has_default = CF_TRUE;
    cf_value_text(&d.default_value, seed, strlen(seed));
    cf_builder_define(b, &d);

    c = cf_builder_create(b, &err);
    if (c == NULL) {
        fprintf(stderr, "fixtures: %s: %s\n", out, err.message);
        return 1;
    }
    info.mode        = CF_REPORT_FULL;
    info.profile     = "production";
    info.application = "myapp";
    info.generated   = "2026-09-12T00:00:00Z";
    cf_report(c, &info, buf, sizeof buf);
    cf_config_free(c);
    if (!swap(buf, sizeof buf,
              "\"reason\": \"too long\", \"reason_kind\": \"too-long\"",
              "\"reason\": \"'${MARKER-GOLF-7731' refers to no key\"")) {
        fprintf(stderr, "fixtures: %s: no refusal to give a reason\n", out);
        return 1;
    }
    return spill(out, buf) ? 0 : 1;
}

/* A full report of this library with six sensitive keys, each refused for
   a different kind (cf_refusal), the value a marker none of the output
   may carry: not an integer, outside its range, not among the permitted
   values, under its least length, over its greatest, and with a "${" that
   names no key. A seventh kind, a reference to a key with no value, fails
   the whole construction, so it has no report: test_secrets.c has it. */
static int emit_kinds(const char* out) {
    static char              e0[]        = "MYAPP_K__TYPE=MARKER-s3cr3t-type";
    static char              e1[]        = "MYAPP_K__RANGE=987654321";
    static char              e2[]        = "MYAPP_K__ALLOWED=MARKER-s3cr3t-allowed";
    static char              e3[]        = "MYAPP_K__SHORT=MARKER-s3cr3t-short";
    static char              e4[]        = "MYAPP_K__LONG=MARKER-s3cr3t-long";
    static char              e5[]        = "MYAPP_K__REF=MARKER-s3cr3t-${";
    static char*             env[]       = {e0, e1, e2, e3, e4, e5, NULL};
    static const char* const permitted[] = {"alpha", "beta", NULL};
    cf_error                 err         = CF_ERROR_INIT;
    cf_builder*              b           = cf_build(NULL);
    cf_config*               c;
    cf_report_info           info = CF_REPORT_INFO_INIT;
    static char              buf[65536];
    cf_def                   d;

    cf_builder_env(b, "env", CF_RANK_ENV, "MYAPP_", env);
    cf_def_init(&d, CF_TYPE_INT, "k.type");
    d.flags      = CF_SENSITIVE;
    d.on_invalid = CF_INVALID_FALLBACK;
    cf_builder_define(b, &d);
    cf_def_init(&d, CF_TYPE_INT, "k.range");
    d.flags      = CF_SENSITIVE;
    d.on_invalid = CF_INVALID_FALLBACK;
    d.has_range  = CF_TRUE;
    d.imin       = 0;
    d.imax       = 9;
    cf_builder_define(b, &d);
    cf_def_init(&d, CF_TYPE_TEXT, "k.allowed");
    d.flags      = CF_SENSITIVE;
    d.on_invalid = CF_INVALID_FALLBACK;
    d.allowed    = permitted;
    cf_builder_define(b, &d);
    cf_def_init(&d, CF_TYPE_TEXT, "k.short");
    d.flags      = CF_SENSITIVE;
    d.on_invalid = CF_INVALID_FALLBACK;
    d.min_len    = 64;
    cf_builder_define(b, &d);
    cf_def_init(&d, CF_TYPE_TEXT, "k.long");
    d.flags      = CF_SENSITIVE;
    d.on_invalid = CF_INVALID_FALLBACK;
    d.max_len    = 4;
    cf_builder_define(b, &d);
    cf_def_init(&d, CF_TYPE_TEXT, "k.ref");
    d.flags      = CF_SENSITIVE;
    d.on_invalid = CF_INVALID_FALLBACK;
    cf_builder_define(b, &d);

    c = cf_builder_create(b, &err);
    if (c == NULL) {
        fprintf(stderr, "fixtures: %s: %s\n", out, err.message);
        return 1;
    }
    info.mode        = CF_REPORT_FULL;
    info.profile     = "production";
    info.application = "myapp";
    info.generated   = "2026-09-12T00:00:00Z";
    cf_report(c, &info, buf, sizeof buf);
    cf_config_free(c);
    return spill(out, buf) ? 0 : 1;
}

/* Ends the first string that follows `field` in the report `buf` with a
   line break and a GitHub workflow command, as a report written by
   hand, or by a library with a bug, could. Zero when the field is not
   there or there is no room. */
static int taint(char* buf, size_t cap, const char* field) {
    static const char add[] = "\\n::add-mask::injected";
    char*             at    = strstr(buf, field);
    size_t            n     = sizeof add - 1;
    size_t            len   = strlen(buf);

    if (at == NULL || len + n >= cap) {
        return 0;
    }
    at = strchr(at + strlen(field), '"');
    if (at == NULL) {
        return 0;
    }
    memmove(at + n, at, strlen(at) + 1);
    memcpy(at, add, n);
    return 1;
}

int main(void) {
    static char  pe0[]    = "MYAPP_SERVER__PORT=9090";
    static char  pe1[]    = "MYAPP_DB__PASSWORD=prodsecret";
    static char  pe2[]    = "MYAPP_SERVER__HOST=127.0.0.1";
    static char* penv[]   = {pe0, pe1, pe2, NULL};
    static char* noargs[] = {a0, NULL};

    static char  sa1[]   = "--server.port=5000";
    static char* sargv[] = {a0, sa1, NULL};

    /* A secret on the command line, in a fixture of its own so the
       clean case stays clean. The library rejects the candidate under
       the key's policy; Doctor is where that becomes loud. */
    static char  la1[]   = "--db.password=leaked-on-the-command-line";
    static char* largv[] = {a0, la1, NULL};
    static char  se0[]   = "MYAPP_DB__PASSWORD=devsecret";
    static char* senv[]  = {se0, NULL};

    static char  da1[]   = "--server.port=5000";
    static char  da2[]   = "--server.host=1.2.3.4";
    static char* dargv[] = {a0, da1, da2, NULL};

    if (!spill("cf_fx_staging.ini", "[server]\nport = 3000\nhost = 127.0.0.1\n")) {
        return 1;
    }
    if (!spill("cf_fx_dead.ini", "[server]\nport = 3000\nhost = 9.9.9.9\n")) {
        return 1;
    }

    /* Production: the environment wins the port and the host, nothing
       overridden twice. */
    if (emit("cf_fx_prod.json", "production", 1, noargs, penv, NULL)) {
        return 1;
    }
    /* Staging: the command line wins, a file supplies the host, and the
       secret differs. Every cross-environment rule has something to
       find between this and production: the host is the same value from
       another kind of source (CFD002), the port a different value from
       one (which is not). */
    if (emit("cf_fx_staging.json", "staging", 2, sargv, senv, "cf_fx_staging.ini")) {
        return 1;
    }
    if (emit("cf_fx_leak.json", "staging", 2, largv, senv, NULL)) {
        return 1;
    }

    /* A report with the three things a single-report lint can prove
       from the definitions alone: a key nobody declared, a deprecated
       one still being supplied, and one set to nothing at all. */
    {
        static char    ra1[]   = "--nonesuch.key=1";
        static char    ra2[]   = "--old.name=still-here";
        static char    ra3[]   = "--server.host=";
        static char*   rargv[] = {a0, ra1, ra2, ra3, NULL};
        cf_error       err     = CF_ERROR_INIT;
        cf_builder*    b       = cf_build(NULL);
        cf_config*     c;
        cf_report_info info = CF_REPORT_INFO_INIT;
        static char    buf[65536];
        cf_def         d;

        cf_builder_cli(b, "cli", CF_RANK_CLI, 4, rargv);
        cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
        cf_builder_text(b, "server.host", "0.0.0.0", 0);
        cf_def_init(&d, CF_TYPE_TEXT, "old.name");
        d.flags       = CF_DEPRECATED;
        d.has_default = CF_TRUE;
        cf_value_text(&d.default_value, "", 0);
        cf_builder_define(b, &d);

        c = cf_builder_create(b, &err);
        if (c == NULL) {
            fprintf(stderr, "fixtures: rules: %s\n", err.message);
            return 1;
        }
        info.mode        = CF_REPORT_BLIND;
        info.profile     = "staging";
        info.application = "myapp";
        info.generated   = "2026-09-12T00:00:00Z";
        cf_report(c, &info, buf, sizeof buf);
        cf_config_free(c);
        if (!spill("cf_fx_rules.json", buf)) {
            return 1;
        }
    }
    /* A file whose every key is overridden: the case CFD010 exists for. */
    if (emit("cf_fx_dead.json", "staging", 3, dargv, senv, "cf_fx_dead.ini")) {
        return 1;
    }

    /* A key set twice in one file. The second line wins wherever this
       file is read, so the first is unreachable - CFD001. The host is
       set once, and must not be reported. */
    if (!spill("cf_fx_twice.ini",
               "[server]\nport = 3000\nhost = 10.0.0.1\nport = 4000\n")) {
        return 1;
    }
    if (emit("cf_fx_twice.json", "staging", 1, noargs, senv, "cf_fx_twice.ini")) {
        return 1;
    }

    /* What the sources themselves say: a secret on the command line
       with no policy to refuse it (CFD007), and a policy that ranks the
       environment above the command line (CFD013). */
    {
        static const char* const env_first[] = {"env", "cli", NULL};
        static char              xa1[]       = "--api.token=typed-into-a-terminal";
        static char              xa2[]       = "--server.port=7000";
        static char*             xargv[]     = {a0, xa1, xa2, NULL};
        static char              xe0[]       = "MYAPP_SERVER__PORT=9090";
        static char*             xenv[]      = {xe0, NULL};
        cf_error                 err         = CF_ERROR_INIT;
        cf_builder*              b           = cf_build(NULL);
        cf_config*               c;
        cf_report_info           info = CF_REPORT_INFO_INIT;
        static char              buf[65536];
        cf_def                   d;

        cf_builder_cli(b, "cli", CF_RANK_CLI, 3, xargv);
        cf_builder_env(b, "env", CF_RANK_ENV, "MYAPP_", xenv);
        cf_def_init(&d, CF_TYPE_TEXT, "api.token");
        d.flags = CF_SENSITIVE;
        cf_builder_define(b, &d);
        cf_def_init(&d, CF_TYPE_INT, "server.port");
        d.policy = env_first;
        cf_builder_define(b, &d);

        c = cf_builder_create(b, &err);
        if (c == NULL) {
            fprintf(stderr, "fixtures: sources: %s\n", err.message);
            return 1;
        }
        info.mode        = CF_REPORT_BLIND;
        info.profile     = "staging";
        info.application = "myapp";
        info.generated   = "2026-09-12T00:00:00Z";
        cf_report(c, &info, buf, sizeof buf);
        cf_config_free(c);
        if (!spill("cf_fx_sources.json", buf)) {
            return 1;
        }
    }

    /* A key nothing won: an optional secret with no default, offered
       only on the command line, which its policy refuses. Its report
       entry says "selected": null. A Doctor that found keys by their
       winner never saw it, and the secret on the command line went
       unreported. */
    {
        static const char* const only_env[] = {"env", NULL};
        static char              ua1[]      = "--license.code=ab12cd";
        static char*             uargv[]    = {a0, ua1, NULL};
        static char*             noenv[]    = {NULL};
        cf_error                 err        = CF_ERROR_INIT;
        cf_builder*              b          = cf_build(NULL);
        cf_config*               c;
        cf_report_info           info = CF_REPORT_INFO_INIT;
        static char              buf[65536];
        cf_def                   d;

        cf_builder_cli(b, "cli", CF_RANK_CLI, 2, uargv);
        cf_builder_env(b, "env", CF_RANK_ENV, "MYAPP_", noenv);
        cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
        cf_def_init(&d, CF_TYPE_TEXT, "license.code");
        d.flags  = CF_SENSITIVE;
        d.policy = only_env;
        cf_builder_define(b, &d);

        c = cf_builder_create(b, &err);
        if (c == NULL) {
            fprintf(stderr, "fixtures: unset: %s\n", err.message);
            return 1;
        }
        info.mode        = CF_REPORT_BLIND;
        info.profile     = "production";
        info.application = "myapp";
        info.generated   = "2026-09-12T00:00:00Z";
        cf_report(c, &info, buf, sizeof buf);
        cf_config_free(c);
        if (!spill("cf_fx_unset.json", buf)) {
            return 1;
        }
    }

    /* Two secrets in two files, one that every user can read and one
       that only its owner can. The report records which is which where
       the platform can tell, and Doctor judges from the report alone. */
    {
        cf_error       err = CF_ERROR_INIT;
        cf_builder*    b;
        cf_config*     c;
        cf_report_info info = CF_REPORT_INFO_INIT;
        static char    buf[65536];
        cf_def         d;

        if (!spill("cf_fx_open.ini", "[api]\nkey = readable-by-all\n") ||
            !spill("cf_fx_closed.ini", "[db]\nkey = owner-only\n")) {
            return 1;
        }
#ifndef _WIN32
        /* fopen leaves the umask's 0644, which is the open case; the
           closed one is made so explicitly. */
        if (system("chmod 644 cf_fx_open.ini && chmod 600 cf_fx_closed.ini") != 0) {
            return 1;
        }
#endif
        b = cf_build(NULL);
        cf_builder_file(b, "user-file", CF_RANK_USER_FILE, "cf_fx_open.ini", NULL, 0);
        cf_builder_file(b, "sys-file", CF_RANK_SYS_FILE, "cf_fx_closed.ini", NULL, 0);
        cf_def_init(&d, CF_TYPE_TEXT, "api.key");
        d.flags = CF_SENSITIVE;
        cf_builder_define(b, &d);
        cf_def_init(&d, CF_TYPE_TEXT, "db.key");
        d.flags = CF_SENSITIVE;
        cf_builder_define(b, &d);
        c = cf_builder_create(b, &err);
        if (c == NULL) {
            fprintf(stderr, "fixtures: files: %s\n", err.message);
            return 1;
        }
        info.mode    = CF_REPORT_BLIND;
        info.profile = "production";
        cf_report(c, &info, buf, sizeof buf);
        cf_config_free(c);
        if (!spill("cf_fx_files.json", buf)) {
            return 1;
        }
    }

    /* Two documents, one whose format its content gave and one named,
       under a variable that wins: the report says what each was read as
       and how (D-73), and explain shows it beside the source. */
    {
        static char    e0[]   = "MYAPP_SERVER__PORT=9090";
        static char*   denv[] = {e0, NULL};
        const char*    first  = "{\"server\": {\"port\": 9443}}\n";
        const char*    second = "{\"server\": {\"port\": 9000}}\n";
        cf_error       err    = CF_ERROR_INIT;
        cf_builder*    b      = cf_build(NULL);
        cf_config*     c;
        cf_report_info info = CF_REPORT_INFO_INIT;
        static char    buf[65536];
        int            ok;

        cf_builder_env(b, "env", CF_RANK_ENV, "MYAPP_", denv);
        cf_builder_document(b, "remote", CF_RANK_USER_FILE, first, strlen(first), NULL,
                            0);
        cf_builder_document(b, "embedded", CF_RANK_SYS_FILE, second, strlen(second),
                            "json", 0);
        cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
        c = cf_builder_create(b, &err);
        if (c == NULL) {
            fprintf(stderr, "fixtures: documents: %s\n", err.message);
            return 1;
        }
        info.mode    = CF_REPORT_BLIND;
        info.profile = "production";
        cf_report(c, &info, buf, sizeof buf);
        cf_config_free(c);
        if (!spill("cf_fx_doc.json", buf)) {
            return 1;
        }
        /* The same report with every field dump and explain print
           beside the candidates carrying a workflow command. */
        ok = taint(buf, sizeof buf, "\"mode\": \"");
        ok = ok && taint(buf, sizeof buf, "\"profile\": \"");
        ok = ok && taint(buf, sizeof buf, "\"fingerprint\": \"");
        ok = ok && taint(buf, sizeof buf, "\"format\": \"");
        ok = ok && taint(buf, sizeof buf, "\"format_how\": \"");
        ok = ok && taint(buf, sizeof buf, "\"type\": \"");
        if (!ok) {
            fprintf(stderr, "fixtures: fields: a field to taint is missing\n");
            return 1;
        }
        if (!spill("cf_fx_fields.json", buf)) {
            return 1;
        }
    }

    /* The full pair (emit_secret). */
    {
        static char  ka0[]   = "MYAPP_DB__PASSWORD=MARKER-ALPHA-7731";
        static char  ka1[]   = "MYAPP_API__PIN=MARKER-CHARLIE-7731";
        static char* kaenv[] = {ka0, ka1, NULL};
        static char  kb0[]   = "MYAPP_DB__PASSWORD=MARKER-BRAVO-7731";
        static char  kb1[]   = "MYAPP_API__PIN=MARKER-CHARLIE-7731";
        static char  kb2[]   = "MYAPP_API__SEED=MARKER-FOXTROT-7731";
        static char* kbenv[] = {kb0, kb1, kb2, NULL};

        if (emit_secret("cf_fx_secret_a.json", kaenv, "MARKER-DELTA-7731",
                        "MARKER-DELTA-7731") ||
            emit_secret("cf_fx_secret_b.json", kbenv, "MARKER-ECHO-7731",
                        "MARKER-ECHO-7731")) {
            return 1;
        }
    }

    if (emit_kinds("cf_fx_kinds.json")) {
        return 1;
    }

    /* Text from a report reaches a CI log and a pull-request comment.
       The permitted spelling here holds a line break and a GitHub
       workflow command, which the refusal's reason quotes; the value
       refused holds a team mention and a link. */
    {
        static const char* const modes[] = {"safe\n::error::injected", NULL};
        static char              ia1[]   = "--app.mode=@org/team https://x.example";
        static char*             iargv[] = {a0, ia1, NULL};
        cf_error                 err     = CF_ERROR_INIT;
        cf_builder*              b       = cf_build(NULL);
        cf_config*               c;
        cf_report_info           info = CF_REPORT_INFO_INIT;
        static char              buf[65536];
        cf_def                   d;

        cf_builder_cli(b, "cli", CF_RANK_CLI, 2, iargv);
        cf_def_init(&d, CF_TYPE_TEXT, "app.mode");
        d.allowed    = modes;
        d.on_invalid = CF_INVALID_FALLBACK;
        cf_builder_define(b, &d);
        c = cf_builder_create(b, &err);
        if (c == NULL) {
            fprintf(stderr, "fixtures: inject: %s\n", err.message);
            return 1;
        }
        info.mode    = CF_REPORT_BLIND;
        info.profile = "staging";
        cf_report(c, &info, buf, sizeof buf);
        cf_config_free(c);
        if (!spill("cf_fx_inject.json", buf)) {
            return 1;
        }
    }

    /* Configuration files, not reports, for `doctor check` against
       contract.schema.json: one that holds to it, a misspelt key, and a
       port out of range. */
    if (!spill("cf_fx_contract_ok.json", "{\"server\": {\"port\": 9000, \"token\": "
                                         "\"t\"}, \"plugins\": {\"x\": 1}}\n") ||
        !spill("cf_fx_contract_typo.json",
               "{\"server\": {\"prot\": 9000, \"token\": \"t\"}}\n") ||
        !spill("cf_fx_contract_range.json",
               "{\"server\": {\"port\": 70000, \"token\": \"t\"}}\n")) {
        return 1;
    }
    return 0;
}
