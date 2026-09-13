/*
 * consumer.c - a program that knows only the installed package.
 *
 * It includes the umbrella header, which pulls in everything: a header
 * left out of the install list breaks here and nowhere else.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <string.h>

int main(int argc, char** argv) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* cfg;
    char       buf[2048];
    cf_iter    it;

    cfg = cf_app("consumer", argc, argv, &err);
    if (cfg == NULL) {
        fprintf(stderr, "consumer: %s\n", err.message);
        return 1;
    }
    /* One call into each layer, so a target property that stopped
       exporting a symbol fails here rather than in somebody's
       project. */
    if (cf_version() == NULL || cf_platform_name() == NULL) {
        return 1;
    }
    (void)cf_get_int(cfg, "server.port", 8080);
    (void)cf_explain(cfg, "server.port", buf, sizeof buf);
    (void)cf_emit(cfg, "json", CF_EMIT_ALL, 0, buf, sizeof buf, &err);
    (void)cf_iter_keys(&it, cfg, NULL);

    printf("consumer ok: configforge %s on %s\n", cf_version(), cf_platform_name());
    cf_config_free(cfg);
    return 0;
}
