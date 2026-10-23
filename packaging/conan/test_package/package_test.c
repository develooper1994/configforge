/*
 * package_test.c - the packaged library, used as a package manager's
 * consumer would: one header, one call into each tier, and an answer to
 * the question the library exists for.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <string.h>

int main(void) {
    static const cf_pair pairs[] = {{"server.port", "9443"}};
    cf_error             err     = CF_ERROR_INIT;
    cf_builder*          b       = cf_build(NULL);
    cf_config*           c;
    char                 why[1024];

    cf_builder_memory(b, "memory", CF_RANK_USER_FILE, pairs, 1);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    c = cf_builder_create(b, &err);
    if (c == NULL) {
        fprintf(stderr, "package_test: %s\n", err.message);
        return 1;
    }
    cf_explain(c, "server.port", why, sizeof why);
    printf("ConfigForge %s\n%s", cf_version(), why);
    if (cf_get_int(c, "server.port", 0) != 9443 || strstr(why, "memory") == NULL) {
        cf_config_free(c);
        return 1;
    }
    cf_config_free(c);
    return 0;
}
