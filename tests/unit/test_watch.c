/*
 * test_watch.c - change notification, D-35.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <stdlib.h>
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
    {
        cf_error    e2 = CF_ERROR_INIT;
        cf_watcher* w  = cf_watcher_open(c, NULL, &e2);
        check(w == NULL && e2.code == CF_E_CAPABILITY,
              "a handle that could never fire is refused rather than handed out");
        cf_watcher_close(w);
    }
    cf_config_free(c);
}

/* Directories for the tests below. system() and the shell's mkdir,
   because C89 has no way to make a directory and test_file.c already
   pays for this the same way. */
static void make_dir(const char* posix, const char* win) {
    char cmd[256];
#ifdef _WIN32
    (void)posix;
    cf_fmt(cmd, sizeof cmd, "mkdir %s >nul 2>&1", win);
#else
    (void)win;
    cf_fmt(cmd, sizeof cmd, "mkdir -p %s", posix);
#endif
    check(system(cmd) >= 0, "made a directory");
}

static void drop_dir(const char* posix, const char* win) {
    char cmd[256];
#ifdef _WIN32
    (void)posix;
    cf_fmt(cmd, sizeof cmd, "rmdir /s /q %s >nul 2>&1", win);
#else
    (void)win;
    cf_fmt(cmd, sizeof cmd, "rm -rf %s", posix);
#endif
    check(system(cmd) >= 0, "removed a directory");
}

/* A drop-in directory edited in place. Its own modification time moves
   when a file is added or removed and not when one is rewritten, so a
   stamp of the directory alone saw nothing - and editing one file in
   conf.d is the commonest change such a directory ever gets. */
static void test_directory_edited_in_place(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b;
    cf_config*  c;

    drop_dir("cf_wconf", "cf_wconf");
    make_dir("cf_wconf", "cf_wconf");
    check(spill("cf_wconf/10-base.ini", "[server]\nport = 3000\n"), "wrote a drop-in");
    b = cf_build(NULL);
    cf_builder_dir(b, "conf.d", CF_RANK_SYS_FILE, "cf_wconf", NULL, 0);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    c = cf_builder_create(b, &err);
    if (c == NULL) {
        check(0, "a directory source builds");
        printf("  %s\n", err.message);
        drop_dir("cf_wconf", "cf_wconf");
        return;
    }
    check(!cf_changed(c), "quiet to begin with");
    check(spill("cf_wconf/10-base.ini", "[server]\nport = 30000\n"),
          "a file inside it is rewritten");
    check(cf_changed(c), "and the directory is seen to have changed");
    cf_config_free(c);
    drop_dir("cf_wconf", "cf_wconf");
}

/* Wait until nothing more arrives. A rewrite is several events, and on
   Windows a change made between a completion and the next read is
   buffered and completes that read at once - so one drain is not
   guaranteed to leave the handle quiet, and asserting that it does
   would be asserting something the design never promised. */
static cf_bool settle(cf_watcher* w) {
    int i;
    for (i = 0; i < 20; ++i) {
        if (cf_watcher_wait(w, 150) == 0) {
            return CF_TRUE;
        }
    }
    return CF_FALSE;
}

/* The handle an event loop waits on, D-35 option 3. */
static void test_wake(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_config*  c;
    cf_config*  next;
    cf_watcher* w;

    check(spill("cf_w.ini", "[server]\nport = 3000\n"), "wrote a file");
    remove("cf_w_absent.ini");
    remove("cf_w_neighbour.txt");
    c = build(&err);
    if (c == NULL) {
        check(0, "builds");
        return;
    }

    w = cf_watcher_open(c, NULL, &err);
    if (!cf_platform_has_wake()) {
        /* Saying so is the whole behaviour here: a loop handed a handle
           that never fires would wait forever and look healthy. */
        check(w == NULL && err.code == CF_E_CAPABILITY,
              "a platform without a handle says so");
        check(strstr(err.message, "cf_changed") != NULL,
              "and names what to do instead");
        cf_config_free(c);
        return;
    }
    check(w != NULL, "a watcher opens");
    if (w == NULL) {
        printf("  %s\n", err.message);
        cf_config_free(c);
        return;
    }
    check(cf_watcher_fd(w) >= 0 || cf_watcher_handle(w) != NULL,
          "and has something to wait on");

    check(settle(w), "a fresh watcher goes quiet");
    check(cf_watcher_wait(w, 0) == 0, "and waiting on it returns at once");

    check(spill("cf_w.ini", "[server]\nport = 3000\n# and a comment\n"),
          "the file is rewritten");
    check(cf_watcher_wait(w, 5000) == 1, "which wakes it");
    check(cf_changed(c), "and cf_changed agrees");

    /* The configuration is released before its watcher on purpose: the
       watcher copied what it needed, so the order is the caller's. */
    next = build(&err);
    cf_config_free(c);
    check(cf_watcher_wait(w, 0) >= 0, "a watcher outlives its configuration");
    cf_watcher_close(w);
    if (next == NULL) {
        check(0, "rebuilding works");
        return;
    }
    w = cf_watcher_open(next, NULL, &err);
    if (w == NULL) {
        check(0, "a watcher opens on the rebuilt configuration");
        cf_config_free(next);
        return;
    }
    check(settle(w), "quiet again after the swap");

    /* A wake-up means "ask now", not "it changed". */
    check(spill("cf_w_neighbour.txt", "not configuration\n"), "a neighbour is written");
    check(cf_watcher_wait(w, 5000) == 1,
          "and wakes it, because the directory is watched");
    check(!cf_changed(next), "while cf_changed, the authority, says nothing changed");
    check(settle(w), "and draining quiets it");

    check(spill("cf_w_absent.ini", "[server]\nport = 4000\n"),
          "the optional file appears");
    check(cf_watcher_wait(w, 5000) == 1, "which wakes it");
    check(cf_changed(next), "and is a change");

    cf_watcher_close(w);
    cf_config_free(next);
    remove("cf_w_absent.ini");
    remove("cf_w_neighbour.txt");
}

/* An optional file in a directory nobody has created yet. The watch
   starts on the nearest ancestor that exists and moves down as the
   missing levels appear, so the file still wakes the caller when it
   finally arrives. */
static void test_wake_missing_directory(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b;
    cf_config*  c;
    cf_watcher* w;

    if (!cf_platform_has_wake()) {
        return;
    }
#if defined(CF_TEST_LIBKQUEUE) && CF_TEST_LIBKQUEUE
    /* libkqueue, the Linux stand-in for kqueue that the test build uses,
       does not report a directory created inside a watched one; a real
       kqueue reports it as NOTE_WRITE and NOTE_LINK. Asserting it here
       would test the stand-in, not this code. The macOS job runs it. */
    printf("skipped: libkqueue does not report a new subdirectory\n");
    return;
#endif
    drop_dir("cf_wd", "cf_wd");
    b = cf_build(NULL);
    cf_builder_file(b, "late", CF_RANK_USER_FILE, "cf_wd/sub/late.ini", NULL,
                    CF_FILE_OPTIONAL);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    c = cf_builder_create(b, &err);
    if (c == NULL) {
        check(0, "an optional file in a missing directory builds");
        return;
    }
    w = cf_watcher_open(c, NULL, &err);
    check(w != NULL, "and can be watched although no part of its directory exists");
    if (w == NULL) {
        printf("  %s\n", err.message);
        cf_config_free(c);
        return;
    }
    check(settle(w), "quiet to begin with");

    make_dir("cf_wd", "cf_wd");
    check(cf_watcher_wait(w, 5000) == 1, "the first missing level wakes it");
    check(!cf_changed(c), "though nothing that matters has changed yet");
    check(settle(w), "and it re-arms one level down");

    make_dir("cf_wd/sub", "cf_wd\\sub");
    check(cf_watcher_wait(w, 5000) == 1, "the second level wakes it");
    check(settle(w), "and it re-arms on the directory itself");

    check(spill("cf_wd/sub/late.ini", "[server]\nport = 5000\n"), "the file arrives");
    check(cf_watcher_wait(w, 5000) == 1, "which wakes it");
    check(cf_changed(c), "and is the change");

    cf_watcher_close(w);
    cf_config_free(c);
    drop_dir("cf_wd", "cf_wd");
}

int main(void) {
    /* The feature list says what is true: "watch" exactly where there is
       a handle to wait on. It was once an option that switched nothing
       and reported the feature absent on every build. */
    check((cf_has_feature("watch") ? 1 : 0) == (cf_platform_has_wake() ? 1 : 0),
          "the feature list agrees with the platform about watching");
    test_watch();
    test_appearance();
    test_nothing_to_watch();
    test_directory_edited_in_place();
    test_wake();
    test_wake_missing_directory();

    remove("cf_w.ini");
    remove("cf_w_absent.ini");
    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
