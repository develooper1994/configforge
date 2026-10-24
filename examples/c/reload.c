/*
 * reload.c - notice a file changing, rebuild, and say what changed.
 *
 * The loop a long-running program keeps: wait on the watcher (or poll
 * cf_changed where the platform has nothing to wait on), rebuild from
 * the same recipe, log what the reload did with cf_diff_text, and swap.
 * The old configuration stays valid until it is freed, so a thread that
 * is still reading it never sees half of each.
 *
 * To run unattended, the program makes the edit itself; in a real one,
 * a person or a deployment does.
 *
 *     ./reload
 */

#include <configforge/configforge.h>

#include <stdio.h>

#define FILE_NAME "reload.toml"

/* The recipe, called once at startup and once per reload. */
static cf_config* load(cf_error* err) {
    cf_builder* b = cf_build(NULL);

    cf_builder_file(b, "file", CF_RANK_USER_FILE, FILE_NAME, NULL, 0);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_builder_int(b, "pool.size", 4, 1, 256, 0);
    cf_builder_text(b, "db.password", NULL, CF_SENSITIVE);
    return cf_builder_create(b, err);
}

static int write_file(const char* text) {
    FILE* f = fopen(FILE_NAME, "wb");
    if (f == NULL) {
        return 0;
    }
    fputs(text, f);
    fclose(f);
    return 1;
}

int main(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_config*  cfg;
    cf_config*  next;
    cf_watcher* w;
    int         woke;

    if (!write_file("[server]\nport = 8080\n[db]\npassword = \"first\"\n")) {
        return 1;
    }
    cfg = load(&err);
    if (cfg == NULL) {
        fprintf(stderr, "reload: %s\n", err.message);
        return 1;
    }
    printf("started on port %d\n", (int)cf_get_int(cfg, "server.port", 0));

    /* NULL where the platform has no handle to wait on; the program then
       asks cf_changed when it likes, which works everywhere. */
    w = cf_watcher_open(cfg, NULL, &err);

    /* The edit, as a deployment would make it. */
    if (!write_file("[server]\nport = 9443\n[pool]\nsize = 16\n"
                    "[db]\npassword = \"second\"\n")) {
        return 1;
    }

    woke = (w != NULL) ? cf_watcher_wait(w, 5000) : 0;
    if (!cf_changed(cfg)) {
        fprintf(stderr, "reload: the edit was not noticed (woke=%d)\n", woke);
        return 1;
    }

    next = load(&err);
    if (next == NULL) {
        /* A broken edit leaves the running configuration in place: the
           reload fails, the program does not. */
        fprintf(stderr, "reload: keeping the old configuration: %s\n", err.message);
    } else {
        static char changes[2048];
        cf_diff_text(cfg, next, changes, sizeof changes);
        printf("reloaded:\n%s", changes);
        cf_config_free(cfg);
        cfg = next;
    }
    printf("now on port %d\n", (int)cf_get_int(cfg, "server.port", 0));

    cf_watcher_close(w);
    cf_config_free(cfg);
    remove(FILE_NAME);
    return 0;
}
