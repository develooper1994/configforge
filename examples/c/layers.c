/*
 * layers.c - where a value comes from, with every kind of source at once.
 *
 * A deployed program rarely reads one file. A vendor ships defaults, an
 * administrator drops a file into conf.d, a container passes a .env, the
 * environment says something, and somebody types a flag. This is that
 * program, with each layer above the one before it:
 *
 *     the definitions' defaults          (rank 40)
 *     app.toml, shipped with the program (35)
 *     conf.d/, every file in name order  (30)
 *     .env                               (15)
 *     the environment, LAYERS_*          (10)
 *     the command line                    (0)
 *
 * and then it asks, for one key from each layer, why the value is what
 * it is. The files are in examples/c/layers/; the suite runs this there.
 *
 *     cd examples/c/layers && ../../../build/examples/c/layers
 */

#include <configforge/configforge.h>

#include <stdio.h>

int main(void) {
    /* Fixed rather than the process's own, so that the output is the
       same on every machine; cf_environ() and argc/argv are what a real
       program passes. */
    static char  e0[]   = "LAYERS_LOG__LEVEL=warn";
    static char* env[]  = {e0, NULL};
    static char  a0[]   = "layers";
    static char  a1[]   = "--server.port=9443";
    static char* argv[] = {a0, a1, NULL};

    static const char* const keys[] = {"server.host", "server.port", "server.workers",
                                       "db.url",      "log.level",   "cache.size",
                                       NULL};
    static char              why[2048];
    cf_error                 err = CF_ERROR_INIT;
    cf_builder*              b   = cf_build(NULL);
    cf_config*               cfg;
    size_t                   i;

    cf_builder_file(b, "app.toml", 35, "app.toml", NULL, 0);
    cf_builder_dir(b, "conf.d", 30, "conf.d", NULL, CF_OPTIONAL);
    cf_builder_dotenv(b, ".env", 15, ".env", "LAYERS_", CF_OPTIONAL);
    cf_builder_env(b, "env", CF_RANK_ENV, "LAYERS_", env);
    cf_builder_cli(b, "cli", CF_RANK_CLI, 2, argv);

    /* The contract: what the program reads, with a default for each. */
    cf_builder_text(b, "server.host", "127.0.0.1", 0);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_builder_int(b, "server.workers", 2, 1, 64, 0);
    cf_builder_text(b, "db.url", "sqlite:dev.db", CF_SENSITIVE);
    cf_builder_text(b, "log.level", "info", 0);
    cf_builder_int(b, "cache.size", 64, 1, 4096, 0);
    cf_builder_unknown(b, CF_UNKNOWN_REJECT);

    cfg = cf_builder_create(b, &err);
    if (cfg == NULL) {
        fprintf(stderr, "layers: %s\n", err.message);
        return 1;
    }

    /* One answer per key: the winner, everything it beat, and where each
       was set. db.url is sensitive, so its values are not printed - only
       that they exist and where. */
    for (i = 0; keys[i] != NULL; ++i) {
        cf_explain(cfg, keys[i], why, sizeof why);
        fputs(why, stdout);
        fputs("\n", stdout);
    }
    cf_config_free(cfg);
    return 0;
}
