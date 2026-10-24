/*
 * settings.c - the whole lifecycle of a setting.
 *
 * The first example shows reading a configuration and asking why a
 * value is what it is. This one shows what happens next, which is most
 * of what a program with a settings screen actually does:
 *
 *     ./settings                       read, and print
 *     ./settings --explain server.port why is it that
 *     ./settings --set server.port 9000  change it and save
 *     ./settings --watch               notice the file changing
 *
 * Every line of its output is checked by the test suite, so nothing
 * here can drift away from what the library does.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <string.h>

#define SETTINGS_FILE "settings.json"

/* One recipe, called twice: once at startup and again after a reload.
   A reload is a rebuild, so the recipe has to be a function rather than
   a sequence of statements in main - which is the shape any program
   that reloads ends up with anyway. */
static cf_config* load(int argc, char** argv, cf_error* err) {
    cf_builder* b = cf_build(NULL);

    cf_builder_cli(b, "cli", CF_RANK_CLI, argc, argv);
    cf_builder_env(b, "env", CF_RANK_ENV, "SETTINGS_", cf_environ());
    /* Optional: the first run has no settings file, and that is not an
       error. It still shows up in explain() as a source that supplied
       nothing, rather than as silence. */
    cf_builder_file(b, "settings", CF_RANK_USER_FILE, SETTINGS_FILE, NULL,
                    CF_FILE_OPTIONAL);

    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_builder_text(b, "server.host", "0.0.0.0", 0);
    cf_builder_text(b, "log.level", "info", 0);
    cf_builder_bool(b, "tls.enabled", CF_FALSE, 0);

    /* Commands, not settings. Saving a file that recorded --explain
       would make every later run explain something nobody asked about,
       which is what CF_TRANSIENT is for. */
    cf_builder_text(b, "explain", NULL, CF_TRANSIENT);
    cf_builder_text(b, "set", NULL, CF_TRANSIENT);
    cf_builder_bool(b, "watch", CF_FALSE, CF_TRANSIENT);
    cf_builder_unknown(b, CF_UNKNOWN_REJECT);

    return cf_builder_create(b, err);
}

static int save(const cf_config* cfg, cf_error* err) {
    static char buf[8192];
    FILE*       f;
    size_t      n;

    /* Only what somebody configured. Writing the defaults too would
       freeze them into the file, and a later release could never move
       one for a user who had never touched it. */
    n = cf_emit(cfg, "json", CF_EMIT_CONFIGURED, 0, buf, sizeof buf, err);
    if (n == 0) {
        return 0;
    }
    f = fopen(SETTINGS_FILE, "wb");
    if (f == NULL) {
        cf_error_set(err, CF_E_SOURCE, NULL, SETTINGS_FILE, "cannot write '%s'",
                     SETTINGS_FILE);
        return 0;
    }
    fputs(buf, f);
    fclose(f);
    return 1;
}

static void print(const cf_config* cfg) {
    printf("%s:%d  log=%s tls=%s\n", cf_get_text(cfg, "server.host", ""),
           (int)cf_get_int(cfg, "server.port", 0), cf_get_text(cfg, "log.level", ""),
           cf_get_bool(cfg, "tls.enabled", CF_FALSE) ? "on" : "off");
}

int main(int argc, char** argv) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* cfg = load(argc, argv, &err);

    if (cfg == NULL) {
        fprintf(stderr, "settings: %s\n", err.message);
        if (err.source[0] != '\0') {
            fprintf(stderr, "  set at: %s\n", err.source);
        }
        return 1;
    }

    if (cf_has(cfg, "explain")) {
        static char buf[4096];
        cf_explain(cfg, cf_get_text(cfg, "explain", ""), buf, sizeof buf);
        fputs(buf, stdout);
        cf_config_free(cfg);
        return 0;
    }

    /* Changing a setting derives a new configuration rather than
       altering this one. The old one stays valid, which matters here
       only because it costs nothing - and matters a great deal in a
       program where another thread is reading it. */
    if (cf_has(cfg, "set")) {
        cf_pair     change;
        cf_config*  next;
        const char* spec = cf_get_text(cfg, "set", "");
        const char* eq   = strchr(spec, '=');

        if (eq == NULL) {
            fprintf(stderr, "settings: --set wants key=value\n");
            cf_config_free(cfg);
            return 1;
        }
        {
            static char key[CF_KEY_MAX];
            size_t      n = (size_t)(eq - spec);
            if (n + 1 > sizeof key) {
                fprintf(stderr, "settings: that key is too long\n");
                cf_config_free(cfg);
                return 1;
            }
            memcpy(key, spec, n);
            key[n]       = '\0';
            change.key   = key;
            change.value = eq + 1;
        }

        next = cf_derive(cfg, "runtime", &change, 1, &err);
        if (next == NULL) {
            /* The contract travelled with it, so a value the original
               would have refused is refused here too - before anything
               is written. */
            fprintf(stderr, "settings: %s\n", err.message);
            cf_config_free(cfg);
            return 1;
        }
        cf_config_free(cfg);
        cfg = next;

        if (!save(cfg, &err)) {
            fprintf(stderr, "settings: %s\n", err.message);
            cf_config_free(cfg);
            return 1;
        }
        printf("saved %s\n", SETTINGS_FILE);
        print(cfg);
        cf_config_free(cfg);
        return 0;
    }

    if (cf_get_bool(cfg, "watch", CF_FALSE)) {
        size_t i;

        printf("watching %u file(s)\n", (unsigned)cf_watch_count(cfg));
        for (i = 0; i < cf_watch_count(cfg); ++i) {
            printf("  %s\n", cf_watch_path(cfg, i));
        }
        printf("changed: %s\n", cf_changed(cfg) ? "yes" : "no");

        /* Standing in for somebody editing the file while this program
           runs. A one-shot process can never observe a change on its
           own - it stamps the file as it builds - so the whole point of
           watching only appears inside a process that outlives an edit,
           and that is what this imitates. */
        {
            FILE* f = fopen(SETTINGS_FILE, "ab");
            if (f != NULL) {
                fputs("\n", f);
                fclose(f);
            }
        }
        printf("after an edit, changed: %s\n", cf_changed(cfg) ? "yes" : "no");

        /* The reload. Nothing is mutated: a new snapshot is built and
           the old one is released when this program is done with it. A
           reader still holding the old one keeps a configuration that
           does not move underneath them, which is why watching needs no
           lock and changes no contract. */
        {
            cf_config* next = load(argc, argv, &err);
            if (next == NULL) {
                fprintf(stderr, "settings: reload failed: %s\n", err.message);
                cf_config_free(cfg);
                return 1;
            }
            printf("reloaded, changed: %s\n", cf_changed(next) ? "yes" : "no");
            cf_config_free(cfg);
            cfg = next;
        }
        print(cfg);
        cf_config_free(cfg);
        return 0;
    }

    print(cfg);
    cf_config_free(cfg);
    return 0;
}
