/*
 * nodiscard_c23.c - ten failures, each checked,, built as C23 with -Werror.
 *
 * Under C23 CF_NODISCARD is [[nodiscard]]. This file compiles only when
 * every one of these results is used; nodiscard_ignored.c is its twin that
 * drops one, and must not compile (cmake/gates/ConsumerNodiscard.cmake).
 */

#include <configforge/configforge.h>

#include <stdio.h>

int main(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_arena*   arena;
    char*       copy;
    cf_builder* b;
    cf_config*  cfg;
    cf_i64      port    = 0;
    cf_bool     verbose = CF_FALSE;
    const char* name    = NULL;
    size_t      len     = 0;

    arena = cf_arena_new(NULL, 0);
    if (arena == NULL) {
        return 1;
    }
    copy = cf_arena_dup(arena, "demo", 4);
    if (copy == NULL) {
        cf_arena_free(arena);
        return 1;
    }
    b = cf_build(NULL);
    if (b == NULL) {
        cf_arena_free(arena);
        return 1;
    }
    /* A definition block is not checked call by call: the first failure
       comes back from cf_builder_create (scripts/check/nodiscard.txt). */
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_builder_bool(b, "server.verbose", CF_FALSE, 0);
    cf_builder_text(b, "server.name", copy, 0);
    cfg = cf_builder_create(b, &err);
    if (cfg == NULL) {
        fprintf(stderr, "nodiscard_c23: %s\n", err.message);
        cf_arena_free(arena);
        return 1;
    }
    if (cf_try_int(cfg, "server.port", &port) != CF_OK || port != 8080 ||
        cf_try_bool(cfg, "server.verbose", &verbose) != CF_OK ||
        cf_try_text(cfg, "server.name", &name, &len) != CF_OK || len != 4) {
        cf_config_free(cfg);
        cf_arena_free(arena);
        return 1;
    }
    if (cf_arena_is_fixed(arena) || !cf_key_valid("server.port", 11) ||
        !cf_has(cfg, "server.port")) {
        cf_config_free(cfg);
        cf_arena_free(arena);
        return 1;
    }
    cf_config_free(cfg);
    cf_arena_free(arena);
    printf("nodiscard_c23 ok\n");
    return 0;
}
