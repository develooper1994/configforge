/*
 * live.c - a service that reloads itself, under the mode its operators
 * chose (cf_live).
 *
 * Three scenes from one running gateway:
 *
 *   1. AUTO. An edit turns on debug logging and moves the port. The log
 *      level changes at once; the port cannot change under a bound
 *      socket, so it is kept, explained, and listed for the restart.
 *   2. APPROVE. A rate-limit change waits until an operator has read
 *      what it changes and said yes.
 *   3. A broken edit. The reload fails and says why; the gateway keeps
 *      serving what it had.
 *
 * To run unattended, the program makes the edits itself.
 *
 *     ./live
 */

#include <configforge/configforge.h>

#include <stdio.h>

#define FILE_NAME "gateway.ini"

static cf_builder* build(void* user) {
    cf_builder* b = cf_build(NULL);
    (void)user;
    cf_builder_file(b, "gateway.ini", CF_RANK_USER_FILE, FILE_NAME, NULL, 0);
    cf_builder_int(b, "server.port", 8080, 1, 65535, CF_RESTART);
    cf_builder_text(b, "log.level", "info", 0);
    cf_builder_int(b, "limit.per_minute", 600, 1, 100000, 0);
    return b;
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

/* What a request handler does: take the snapshot, read, let it go. */
static void serve(cf_live* live) {
    cf_config* cfg = cf_live_get(live);
    printf("serving on :%d, log %s, %d requests a minute\n",
           (int)cf_get_int(cfg, "server.port", 0), cf_get_text(cfg, "log.level", "?"),
           (int)cf_get_int(cfg, "limit.per_minute", 0));
    cf_config_free(cfg);
}

int main(void) {
    static char    text[2048];
    cf_error       err = CF_ERROR_INIT;
    cf_live*       live;
    cf_config*     cfg;
    cf_live_result r;

    if (!write_file("[server]\nport = 8080\n")) {
        return 1;
    }

    /* 1. AUTO */
    live = cf_live_open(CF_LIVE_AUTO, build, NULL, NULL, NULL, &err);
    if (live == NULL) {
        fprintf(stderr, "live: %s\n", err.message);
        return 1;
    }
    serve(live);
    write_file("[server]\nport = 9443\n[log]\nlevel = debug\n");
    r = cf_live_wait(live, 2000, &err);
    printf("1. reload: %s\n", r == CF_LIVE_SWAPPED ? "swapped" : "not swapped");
    serve(live);
    cf_live_restarts(live, text, sizeof text);
    printf("waiting for a restart: %s", text);
    cfg = cf_live_get(live);
    cf_explain(cfg, "server.port", text, sizeof text);
    printf("%s", text);
    cf_config_free(cfg);
    cf_live_close(live);

    /* 2. APPROVE */
    write_file("[server]\nport = 8080\n");
    live = cf_live_open(CF_LIVE_APPROVE, build, NULL, NULL, NULL, &err);
    if (live == NULL) {
        fprintf(stderr, "live: %s\n", err.message);
        return 1;
    }
    write_file("[server]\nport = 8080\n[limit]\nper_minute = 60\n");
    r = cf_live_wait(live, 2000, &err);
    if (r == CF_LIVE_PENDING) {
        cfg = cf_live_get(live);
        cf_diff_text(cfg, cf_live_pending(live), text, sizeof text);
        printf("2. waiting for approval:\n%s", text);
        cf_config_free(cfg);
        serve(live); /* still the old limit */
        printf("operator: approved\n");
        cf_live_approve(live, &err);
    }
    serve(live);

    /* 3. A broken edit */
    write_file("[server]\nport = 8080\n[limit]\nper_minute = lots\n");
    r = cf_live_wait(live, 2000, &err);
    printf("3. reload %s: %s\n", r == CF_LIVE_FAILED ? "refused" : "accepted?",
           err.message);
    serve(live);

    cf_live_close(live);
    remove(FILE_NAME);
    return 0;
}
