/*
 * test_trusted.c - a secrets or credentials directory, and a drop-in
 * directory that asks, read only when nobody but its owner and root can
 * change what it hands over (D-83): a directory others may write to, a
 * file in it they may write to, a sticky directory's entries, a group
 * that is the owner's own, CF_ANY_WRITER and CF_TRUSTED_DIR.
 *
 * Runs in a working directory of its own (lib/config/tests/CMakeLists.txt),
 * makes its directories and sets their modes itself, and removes what
 * it made. On Windows the platform cannot say who may write, and a
 * drop-in directory that asks is refused for it.
 */

#if defined(CF_TEST_POSIX)
/* mkdir, chmod, chown, umask, getpwuid: POSIX, which glibc hides in
   strict C89. */
#define _POSIX_C_SOURCE 200112L
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#endif

#include <configforge/configforge.h>

#include <stdio.h>
#include <string.h>

#include "cf_test.h"

#if CF_WITH_DIR

/* A flag no file source knows is refused by the message every source
   has for one, wherever the test runs. */
static void test_not_a_file_flag(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b;
    cf_config*  c;

    b = cf_build(NULL);
    cf_builder_file(b, "f", CF_RANK_USER_FILE, "x.ini", NULL, CF_TRUSTED_DIR);
    c = cf_builder_create(b, &err);
    check(c == NULL, "CF_TRUSTED_DIR on a file source fails the builder");
    has(err.message, "are not source flags",
        "by the message for a flag it does not know");
    cf_config_free(c);
}

#if defined(CF_TEST_POSIX)

/* A secrets directory `sec` of this mode holding db.password of that
   one, both made under umask 022 and then set. */
static int make_secrets(unsigned dir_mode, unsigned file_mode) {
    if (mkdir("sec", 0755) != 0 || !spill("sec/db.password", "s3cret\n")) {
        return 0;
    }
    return chmod("sec/db.password", (mode_t)file_mode) == 0 &&
           chmod("sec", (mode_t)dir_mode) == 0;
}

static void remove_secrets(void) {
    chmod("sec", 0755);
    remove("sec/db.password");
    rmdir("sec");
}

/* The secrets directory read with these flags: the configuration, or
   NULL with `err` set. */
static cf_config* read_secrets(unsigned flags, cf_error* err) {
    cf_builder* b = cf_build(NULL);

    cf_builder_secrets(b, "s", CF_RANK_ENV, "sec", NULL, flags);
    return cf_builder_create(b, err);
}

/* 1: Docker's shape, a 0755 directory of 0644 files. */
static void test_plain(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c;

    check(make_secrets(0755, 0644), "made sec 0755 holding db.password 0644");
    c = read_secrets(0, &err);
    check(c != NULL, "a directory only its owner can write to builds");
    if (c == NULL) {
        printf("  %s\n", err.message);
    } else {
        check_text(cf_get_text(c, "db.password", ""), "s3cret",
                   "and the value is read");
        cf_config_free(c);
    }
    remove_secrets();
}

/* 2: a directory everybody can write to, and CF_ANY_WRITER. */
static void test_world_writable(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c;

    check(make_secrets(0777, 0644), "made sec 0777");
    c = read_secrets(0, &err);
    check(c == NULL, "a secrets directory others can write to is refused");
    check(err.code == CF_E_SOURCE, "as a source error");
    has(err.message, "can be written by users other than its owner and root",
        "saying who can write to it");
    has(err.message, "0777", "and its mode");
    cf_config_free(c);

    cf_error_clear(&err);
    c = read_secrets(CF_ANY_WRITER, &err);
    check(c != NULL, "with CF_ANY_WRITER it builds");
    if (c == NULL) {
        printf("  %s\n", err.message);
    } else {
        check_text(cf_get_text(c, "db.password", ""), "s3cret", "and reads it");
        cf_config_free(c);
    }
    remove_secrets();
}

/* 3: sticky and world-writable, holding the program's own file - the
   shape of a Kubernetes mount. */
static void test_sticky_own(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c;

    check(make_secrets(01777, 0644), "made sec 01777 holding the program's own file");
    c = read_secrets(0, &err);
    check(c != NULL, "a sticky directory of the program's own files builds");
    if (c == NULL) {
        printf("  %s\n", err.message);
    } else {
        cf_config_free(c);
    }
    remove_secrets();
}

/* 4: a file in it that others can write to. */
static void test_file_writable(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c;

    check(make_secrets(0755, 0666), "made sec 0755 holding db.password 0666");
    c = read_secrets(0, &err);
    check(c == NULL, "a secret others can write to is refused");
    has(err.message, "sec/db.password", "naming the file");
    has(err.message, "0666", "and its mode");
    cf_config_free(c);
    remove_secrets();
}

/* 5: group-writable, by the owner's own group (user private groups,
   umask 002). */
static void test_own_group(void) {
    cf_error       err = CF_ERROR_INIT;
    cf_config*     c;
    struct stat    st;
    struct passwd* pw;

    check(make_secrets(0775, 0644), "made sec 0775");
    pw = getpwuid(geteuid());
    if (stat("sec", &st) != 0 || pw == NULL || pw->pw_gid != st.st_gid) {
        printf("skipped: sec's group is not this user's own group\n");
        remove_secrets();
        return;
    }
    c = read_secrets(0, &err);
    check(c != NULL, "a directory its owner's own group can write to builds");
    if (c == NULL) {
        printf("  %s\n", err.message);
    } else {
        cf_config_free(c);
    }
    remove_secrets();
}

/* 6: a drop-in directory others can write to is read unless it asks. */
static void test_dropin(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b;
    cf_config*  c;

    check(mkdir("conf.d", 0755) == 0 && spill("conf.d/a.ini", "port = 1\n") &&
              chmod("conf.d/a.ini", 0644) == 0 && chmod("conf.d", 0777) == 0,
          "made conf.d 0777 holding a.ini 0644");

    b = cf_build(NULL);
    cf_builder_dir(b, "d", CF_RANK_SYS_FILE, "conf.d", NULL, 0);
    c = cf_builder_create(b, &err);
    check(c != NULL, "a drop-in directory others can write to is read as before");
    if (c == NULL) {
        printf("  %s\n", err.message);
    } else {
        check(cf_get_int(c, "port", 0) == 1, "its file read");
        cf_config_free(c);
    }

    cf_error_clear(&err);
    b = cf_build(NULL);
    cf_builder_dir(b, "d", CF_RANK_SYS_FILE, "conf.d", NULL, CF_TRUSTED_DIR);
    c = cf_builder_create(b, &err);
    check(c == NULL, "with CF_TRUSTED_DIR it is refused");
    has(err.message, "can be written by users other than its owner and root",
        "by the directory's message");
    cf_config_free(c);

    cf_error_clear(&err);
    b = cf_build(NULL);
    cf_builder_dir(b, "d", CF_RANK_SYS_FILE, "conf.d", NULL,
                   CF_TRUSTED_DIR | CF_ANY_WRITER);
    c = cf_builder_create(b, &err);
    check(c == NULL, "CF_TRUSTED_DIR with CF_ANY_WRITER fails the builder");
    has(err.message, "contradict", "saying the two contradict each other");
    cf_config_free(c);

    chmod("conf.d", 0755);
    remove("conf.d/a.ini");
    rmdir("conf.d");
}

/* 8: in a sticky directory others can write to, a file of a user who
   is neither root, its owner nor the program's. Only root can give a
   file away. */
static void test_stranger(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c;

    if (geteuid() != 0) {
        printf("skipped: a file of another user needs root to make\n");
        return;
    }
    check(make_secrets(01777, 0644), "made sec 01777");
    check(chown("sec/db.password", (uid_t)65534, (gid_t)65534) == 0,
          "gave db.password to user 65534");
    c = read_secrets(0, &err);
    check(c == NULL, "another user's file in a shared directory is refused");
    has(err.message, "belongs to user 65534", "naming its owner");
    cf_config_free(c);
    remove_secrets();
}

#else

/* Windows and WASI cannot say, and a drop-in directory that asks is told so. */
static void test_cannot_say(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b;
    cf_config*  c;

    check(!cf_platform_has_trusted_dir(), "this platform cannot say who may write");
    b = cf_build(NULL);
    cf_builder_dir(b, "d", CF_RANK_SYS_FILE, ".", NULL, CF_TRUSTED_DIR);
    c = cf_builder_create(b, &err);
    check(c == NULL, "a drop-in directory with CF_TRUSTED_DIR is refused");
    check(err.code == CF_E_CAPABILITY, "as what the platform cannot do");
    has(err.message, "cannot say who may write", "and says so");
    cf_config_free(c);
}

#endif /* CF_TEST_POSIX */

#endif /* CF_WITH_DIR */

int main(void) {
#if CF_WITH_DIR
#if defined(CF_TEST_POSIX)
    umask(022);
    test_plain();
    test_world_writable();
    test_sticky_own();
    test_file_writable();
    test_own_group();
    test_dropin();
    test_stranger();
#else
    test_cannot_say();
#endif
    test_not_a_file_flag();
#endif
    return cf_test_done();
}
