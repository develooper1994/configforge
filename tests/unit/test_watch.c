/*
 * test_watch.c - change notification, D-35.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <string.h>

static int  g_fail = 0, g_run = 0;
static void check(int c, const char* w) {
    ++g_run;
    if (!c) {
        ++g_fail;
        printf("FAIL: %s\n", w);
    }
}

static cf_bool spill(const char* path, const char* text) {
    FILE* f = fopen(path, "wb");
    if (f == NULL) {
        return CF_FALSE;
    }
    fputs(text, f);
    fclose(f);
    return CF_TRUE;
}

static cf_config* build(cf_error* err) {
    cf_builder* b = cf_build(NULL);

    cf_builder_file(b, "file", CF_RANK_USER_FILE, "cf_w.ini", NULL, 0);
    cf_builder_file(b, "maybe", CF_RANK_SYS_FILE, "cf_w_absent.ini", NULL,
                    CF_FILE_OPTIONAL);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    return cf_builder_create(b, err);
}

static void test_watch(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c;
    size_t     i;
    int        found = 0;

    check(spill("cf_w.ini", "[server]\nport = 3000\n"), "wrote a file");
    c = build(&err);
    check(c != NULL, "builds");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }

    /* The files it was read from, so a caller need not keep the paths
       itself. Both of them: an optional file that is absent is still
       watched, because its arrival is a change. */
    check(cf_watch_count(c) == 2, "two files are watched");
    for (i = 0; i < cf_watch_count(c); ++i) {
        if (strcmp(cf_watch_path(c, i), "cf_w.ini") == 0) {
            found = 1;
        }
    }
    check(found, "and the one that exists is named");

    check(!cf_changed(c), "nothing has changed yet");

    /* A different length is a change no clock granularity can hide,
       which is what keeps this test from depending on timing. */
    check(spill("cf_w.ini", "[server]\nport = 3000\n# and a comment\n"),
          "the file is rewritten");
    check(cf_changed(c), "and the change is seen");

    /* Reload is a rebuild, and the old snapshot stays valid: that is
       what makes watching free of any contract change. */
    {
        const char* before = cf_get_text(c, "server.port", "");
        cf_config*  next   = build(&err);
        check(next != NULL, "rebuilding works");
        if (next != NULL) {
            check(!cf_changed(next), "and the new one is up to date");
            check(strcmp(cf_get_text(c, "server.port", ""), before) == 0,
                  "while the old one is untouched, pointers included");
            cf_config_free(next);
        }
    }
    cf_config_free(c);
}

/* Appearing and disappearing are both changes. An optional file that
   was missing and now is not is exactly the case somebody wants to be
   told about. */
static void test_appearance(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c;

    check(spill("cf_w.ini", "[server]\nport = 3000\n"), "wrote a file");
    remove("cf_w_absent.ini");
    c = build(&err);
    if (c == NULL) {
        check(0, "builds");
        return;
    }
    check(!cf_changed(c), "quiet to begin with");

    check(spill("cf_w_absent.ini", "[server]\nport = 4000\n"),
          "the optional file appears");
    check(cf_changed(c), "and that is a change");
    cf_config_free(c);

    c = build(&err);
    if (c != NULL) {
        check(!cf_changed(c), "quiet again after rebuilding");
        remove("cf_w_absent.ini");
        check(cf_changed(c), "and its disappearance is a change too");
        cf_config_free(c);
    }
}

/* A configuration with no files has nothing to watch and says so
   rather than reporting a change forever. */
static void test_nothing_to_watch(void) {
    static char  a0[]   = "prog";
    static char  a1[]   = "--server.port=1";
    static char* argv[] = {a0, a1, NULL};
    cf_error     err    = CF_ERROR_INIT;
    cf_builder*  b      = cf_build(NULL);
    cf_config*   c;

    cf_builder_cli(b, "cli", CF_RANK_CLI, 2, argv);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    c = cf_builder_create(b, &err);
    if (c == NULL) {
        check(0, "builds");
        return;
    }
    check(cf_watch_count(c) == 0, "a command line is not a file");
    check(!cf_changed(c), "and never changes under you");
    cf_config_free(c);
}

int main(void) {
    test_watch();
    test_appearance();
    test_nothing_to_watch();

    remove("cf_w.ini");
    remove("cf_w_absent.ini");
    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
