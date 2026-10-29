/*
 * preflight.c - a service that lints its own configuration before it
 * serves.
 *
 * A container starts with a configuration somebody assembled from a
 * shipped file, the environment and a command line typed into a
 * deployment manifest. Most mistakes there are not invalid values -
 * the contract catches those - but valid ones in the wrong place: the
 * database password typed as a flag, where every `ps` on the node and
 * every orchestrator log can read it. Doctor knows those rules, and as
 * a library (cf_doctor.h) it can say so before the first request, with
 * the production profile's severities, instead of in a CI job after.
 *
 * Twice: started the wrong way, it refuses; started the right way - the
 * password from the environment, where the orchestrator's secret store
 * puts it - it serves.
 *
 *     ./preflight
 *
 * The suite checks its output.
 */

#include <configforge/cf_doctor.h>

#include <stdio.h>

#define FILE_NAME "service.ini"

static cf_config* load(int argc, char** argv, char** env, cf_error* err) {
    cf_builder* b = cf_build(NULL);
    cf_def      d;

    cf_builder_file(b, "service.ini", CF_RANK_SYS_FILE, FILE_NAME, NULL, 0);
    cf_builder_env(b, "env", CF_RANK_ENV, "SHOP_", env);
    cf_builder_cli(b, "cli", CF_RANK_CLI, argc, argv);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_builder_text(b, "db.host", "localhost", 0);
    cf_def_init(&d, CF_TYPE_TEXT, "db.password");
    d.flags = CF_SENSITIVE | CF_REQUIRED;
    cf_builder_define(b, &d);
    return cf_builder_create(b, err);
}

/* The preflight: Doctor's rules over this configuration, as production
   would judge it. 0 when it may serve. */
static int preflight(const cf_config* cfg) {
    cf_error   err = CF_ERROR_INIT;
    cf_doctor* doc = cf_doctor_new();
    size_t     i, errors;

    if (doc == NULL || !cf_doctor_lint(doc, cfg, "production", &err)) {
        printf("preflight: could not run: %s\n", err.message);
        cf_doctor_free(doc);
        return 1;
    }
    for (i = 0; i < cf_doctor_count(doc); ++i) {
        const cf_finding* f = cf_doctor_finding(doc, i);
        printf("preflight: %s %s %s at %s: %s\n", f->rule, f->level, f->key,
               (f->at != NULL) ? f->at : "?", f->message);
    }
    errors = cf_doctor_count_at(doc, "error");
    cf_doctor_free(doc);
    return (errors > 0) ? 1 : 0;
}

static void start(const char* title, int argc, char** argv, char** env) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* cfg = load(argc, argv, env, &err);

    printf("%s\n", title);
    if (cfg == NULL) {
        printf("  refused by the contract: %s\n\n", err.message);
        return;
    }
    if (preflight(cfg) != 0) {
        printf("  refusing to serve\n\n");
    } else {
        printf("  preflight clean: serving on :%d, database %s\n\n",
               (int)cf_get_int(cfg, "server.port", 0),
               cf_get_text(cfg, "db.host", "?"));
    }
    cf_config_free(cfg);
}

int main(void) {
    static char  a0[]       = "shop";
    static char  a1[]       = "--db.password=hunter2";
    static char  a2[]       = "--server.port=9443";
    static char* typed[]    = {a0, a1, a2, NULL};
    static char* flagless[] = {a0, a2, NULL};
    static char  e0[]       = "SHOP_DB__PASSWORD=hunter2";
    static char* secret[]   = {e0, NULL};
    static char* none[]     = {NULL};
    FILE*        f          = fopen(FILE_NAME, "wb");

    if (f == NULL) {
        return 1;
    }
    fputs("[db]\nhost = db.internal\n", f);
    fclose(f);

    start("1. the password typed into the manifest as a flag", 3, typed, none);
    start("2. the password from the environment, as the secret store sets it", 2,
          flagless, secret);
    remove(FILE_NAME);
    return 0;
}
