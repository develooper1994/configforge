/*
 * explain_demo.c - the thing no other C configuration library does.
 *
 *   ./explain_demo
 *   ./explain_demo --server.port=7000
 *   MYAPP_SERVER__PORT=9090 ./explain_demo
 */

#include "configforge/configforge.h"

#include <stdio.h>

int main(int argc, char** argv) {
    static const cf_pair defaults[] = {{"server.port", "8080"},
                                       {"server.host", "0.0.0.0"},
                                       {"log.level", "info"},
                                       {"tls.enabled", "false"}};

    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  cfg;
    static char buf[4096]; /* static, not stack: the library runs on targets
                              where 4 KB of stack is the whole budget */

    cf_builder_cli(b, "cli", CF_RANK_CLI, argc, argv);
    cf_builder_env(b, "env", CF_RANK_ENV, "MYAPP_", cf_environ());
    cf_builder_memory(b, "default", CF_RANK_DEFAULT, defaults, 4);

    cfg = cf_builder_create(b, &err);
    if (cfg == NULL) {
        fprintf(stderr, "%s\n", err.message);
        return 1;
    }

    cf_inspect(cfg, buf, sizeof buf);
    fputs(buf, stdout);

    printf("\ndigest %08lx%08lx\n", (unsigned long)(cf_digest(cfg) >> 32),
           (unsigned long)(cf_digest(cfg) & (cf_u64)0xFFFFFFFF));

    cf_config_free(cfg);
    return 0;
}
