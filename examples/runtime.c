/*
 * runtime.c - changing a setting while the program runs.
 *
 * An operator opens the admin page of a running service and turns the
 * log level up to debug, to see why requests are failing. Three things
 * have to be true for that to be safe, and they are what this shows:
 *
 *   1. A request already in flight keeps the configuration it started
 *      with. A snapshot never changes; a change makes a new one
 *      (cf_derive), so nothing a reader holds moves underneath it and
 *      no lock is needed to read.
 *   2. The change obeys the same contract as the file did. An operator
 *      typing 0 workers gets the same refusal a file would.
 *   3. Afterwards, explain() says who changed it and what it was
 *      before - which is the first thing anyone asks the next morning.
 *
 *     ./runtime
 *
 * The suite checks its output.
 */

#include <configforge/configforge.h>

#include <stdio.h>

#define FILE_NAME "service.ini"

int main(void) {
    static char text[2048];
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  live;
    cf_config*  in_flight;
    cf_config*  next;
    cf_config*  refused;
    cf_pair     debug[1];
    cf_pair     zero[1];
    FILE*       f = fopen(FILE_NAME, "wb");

    if (f == NULL) {
        return 1;
    }
    fputs("[log]\nlevel = info\n[server]\nworkers = 4\n", f);
    fclose(f);

    cf_builder_file(b, "service.ini", CF_RANK_SYS_FILE, FILE_NAME, NULL, 0);
    cf_builder_text(b, "log.level", "warn", 0);
    cf_builder_int(b, "server.workers", 1, 1, 64, 0);
    live = cf_builder_create(b, &err);
    if (live == NULL) {
        fprintf(stderr, "runtime: %s\n", err.message);
        return 1;
    }
    printf("started: log level %s, %d workers\n", cf_get_text(live, "log.level", "?"),
           (int)cf_get_int(live, "server.workers", 0));

    /* A request starts and takes the configuration as it is now. In a
       threaded server this is the reader; the snapshot is reference
       counted, so it stays valid for as long as the request holds it. */
    in_flight = live;

    /* The operator's change: a new snapshot from the old one, with the
       change as a source of its own named "admin". */
    debug[0].key   = "log.level";
    debug[0].value = "debug";
    next           = cf_derive(live, "admin", debug, 1, &err);
    if (next == NULL) {
        fprintf(stderr, "runtime: %s\n", err.message);
        return 1;
    }
    printf("\nthe request in flight still sees: %s\n",
           cf_get_text(in_flight, "log.level", "?"));
    printf("a new request sees:               %s\n",
           cf_get_text(next, "log.level", "?"));

    /* What changed, as a log line would put it. */
    cf_diff_text(live, next, text, sizeof text);
    printf("\nchanged:\n%s", text);

    /* The morning after: why is the log level debug? */
    cf_explain(next, "log.level", text, sizeof text);
    printf("\n%s", text);

    /* A mistake typed into the admin page meets the contract, and the
       running configuration is left as it was. */
    zero[0].key   = "server.workers";
    zero[0].value = "0";
    refused       = cf_derive(next, "admin", zero, 1, &err);
    printf("\n0 workers: %s\n", (refused == NULL) ? err.message : "accepted");
    printf("still running with %d workers\n",
           (int)cf_get_int(next, "server.workers", 0));

    /* The request finishes; the old snapshot goes when its last holder
       lets go. */
    cf_config_free(refused);
    cf_config_free(live);
    cf_config_free(next);
    remove(FILE_NAME);
    return 0;
}
