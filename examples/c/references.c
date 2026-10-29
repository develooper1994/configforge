/*
 * references.c - one value made from others, ${key} (D-10).
 *
 * A service URL is a host and a port that are configured anyway, and
 * writing it out a second time is how the two drift apart. A reference
 * says it once:
 *
 *     [server]
 *     host = web1
 *     port = 9443
 *     url  = https://${server.host}:${server.port}/
 *
 * Precedence chooses every key first, so a --server.host flag moves the
 * URL with it; the references are replaced after, and explain() says
 * what the value was made from. Then the one mistake references add: a
 * cycle, refused by name.
 *
 *     ./references
 *
 * The suite checks its output.
 */

#include <configforge/configforge.h>

#include <stdio.h>

#define FILE_NAME "app.ini"

static int write_file(const char* text) {
    FILE* f = fopen(FILE_NAME, "wb");
    if (f == NULL) {
        return 0;
    }
    fputs(text, f);
    fclose(f);
    return 1;
}

static cf_config* load(int argc, char** argv, cf_error* err) {
    cf_builder* b = cf_build(NULL);

    cf_builder_file(b, "app.ini", CF_RANK_USER_FILE, FILE_NAME, NULL, 0);
    cf_builder_cli(b, "cli", CF_RANK_CLI, argc, argv);
    cf_builder_text(b, "server.host", "localhost", 0);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_builder_text(b, "server.url", NULL, 0);
    return cf_builder_create(b, err);
}

int main(void) {
    static char  why[2048];
    static char  a0[]   = "references";
    static char  a1[]   = "--server.host=web2";
    static char* argv[] = {a0, a1, NULL};
    cf_error     err    = CF_ERROR_INIT;
    cf_config*   cfg;

    if (!write_file("[server]\nhost = web1\nport = 9443\n"
                    "url = https://${server.host}:${server.port}/\n")) {
        return 1;
    }
    cfg = load(1, argv, &err);
    if (cfg == NULL) {
        fprintf(stderr, "references: %s\n", err.message);
        return 1;
    }
    printf("url: %s\n", cf_get_text(cfg, "server.url", "?"));
    cf_explain(cfg, "server.url", why, sizeof why);
    fputs(why, stdout);
    cf_config_free(cfg);

    /* A flag for the host moves the URL with it. */
    cfg = load(2, argv, &err);
    if (cfg == NULL) {
        fprintf(stderr, "references: %s\n", err.message);
        return 1;
    }
    printf("\nwith --server.host=web2, url: %s\n", cf_get_text(cfg, "server.url", "?"));
    cf_config_free(cfg);

    /* Two values each made from the other. */
    write_file("[server]\nhost = ${server.url}\nurl = https://${server.host}/\n");
    cfg = load(1, argv, &err);
    printf("\na cycle: %s\n", (cfg == NULL) ? err.message : "accepted");
    cf_config_free(cfg);

    remove(FILE_NAME);
    return 0;
}
