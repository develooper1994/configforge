/*
 * explain_demo.c - the thing no other C configuration library does.
 *
 *   ./explain_demo
 *   ./explain_demo --server.port=7000
 *   MYAPP_SERVER__PORT=9090 ./explain_demo
 */

#include "configforge/cf_explain.h"

#include <stdio.h>

int main(int argc, char** argv) {
    static const cf_pair defaults[] = {{"server.port", "8080"},
                                       {"server.host", "0.0.0.0"},
                                       {"log.level", "info"},
                                       {"tls.enabled", "false"}};

    cf_arena*        ar = cf_arena_new(NULL, 0);
    cf_keyspace      ks;
    cf_source        cli, env, def;
    const cf_source* list[3];
    cf_source_view   sv;
    static char      buf[4096]; /* static, not stack: the library runs on targets
                                   where 4 KB of stack is the whole budget */

    cf_ks_init(&ks, ar, 16);

    cf_source_cli(&cli, "cli", CF_RANK_CLI, argc, argv);
    cf_source_env(&env, "env", CF_RANK_ENV, "MYAPP_", cf_environ());
    cf_source_memory(&def, "default", CF_RANK_DEFAULT, defaults, 4);

    if (!cf_source_load(&cli, &ks)) {
        fputs("bad command line\n", stderr);
        return 1;
    }
    cf_source_load(&env, &ks);
    cf_source_load(&def, &ks);
    cf_ks_resolve(&ks);
    cf_ks_infer_untyped(&ks);

    list[0]  = &cli;
    list[1]  = &env;
    list[2]  = &def;
    sv.items = list;
    sv.count = 3;

    cf_ks_inspect(&ks, &sv, buf, sizeof buf);
    fputs(buf, stdout);

    printf("\ndigest %08lx%08lx\n", (unsigned long)(cf_ks_digest(&ks) >> 32),
           (unsigned long)(cf_ks_digest(&ks) & (cf_u64)0xFFFFFFFF));

    cf_arena_free(ar);
    return 0;
}
