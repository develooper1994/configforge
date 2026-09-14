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
#include <string.h>

static char a0[] = "app";

static cf_bool spill(const char* path, const char* text) {
    FILE* f = fopen(path, "wb");
    if (f == NULL) {
        return CF_FALSE;
    }
    fputs(text, f);
    fclose(f);
    return CF_TRUE;
}

static int emit(const char* out, const char* profile, int argc, char* const* argv,
                char* const* env, const char* file) {
    cf_error       err = CF_ERROR_INIT;
    cf_builder*    b   = cf_build(NULL);
    cf_config*     c;
    cf_report_info info;
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
    memset(&info, 0, sizeof info);
    info.mode        = CF_REPORT_BLIND;
    info.profile     = profile;
    info.application = "myapp";
    info.app_version = "2.3.1";
    info.generated   = "2026-09-12T00:00:00Z";
    cf_report(c, &info, buf, sizeof buf);
    cf_config_free(c);
    return spill(out, buf) ? 0 : 1;
}

int main(void) {
    static char  pe0[]    = "MYAPP_SERVER__PORT=9090";
    static char  pe1[]    = "MYAPP_DB__PASSWORD=prodsecret";
    static char* penv[]   = {pe0, pe1, NULL};
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

    /* Production: environment wins the port, nothing overridden twice. */
    if (emit("cf_fx_prod.json", "production", 1, noargs, penv, NULL)) {
        return 1;
    }
    /* Staging: the command line wins, a file supplies the host, and the
       secret differs. Every cross-environment rule has something to
       find between this and production. */
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
        cf_report_info info;
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
        memset(&info, 0, sizeof info);
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
    /* A file whose every key is overridden: the case CFD001 exists for. */
    if (emit("cf_fx_dead.json", "staging", 3, dargv, senv, "cf_fx_dead.ini")) {
        return 1;
    }
    return 0;
}
