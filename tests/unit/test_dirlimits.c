/*
 * test_dirlimits.c - a drop-in or secrets directory at the edges of what
 * it can list: more files than it holds, a name too long to carry, an
 * entry that is not a file, and a watch on a drive that is not there.
 *
 * Each is a place where a file used to be dropped without a word, or
 * where the call never returned. The test runs in a directory of its
 * own, and the build makes the directories it fills, since C89 has no
 * way to make one (tests/unit/CMakeLists.txt).
 */

#if defined(CF_TEST_FIFO)
/* mkfifo and symlink are POSIX, which glibc hides in strict C89. */
#define _POSIX_C_SOURCE 200112L
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include <configforge/configforge.h>

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#endif

#include "cf_test.h"

#if CF_WITH_DIR

/* One file a key, numbered from 1000 so that their order is their
   number: cf_fmt has no zero padding. */
static int fill(const char* dir, const char* stem, int from, int to) {
    char path[128];
    char text[64];
    int  i;

    for (i = from; i < to; ++i) {
        cf_fmt(path, sizeof path, "%s/%s%d", dir, stem, 1000 + i);
        cf_fmt(text, sizeof text, "k%d = %d\n", 1000 + i, i);
        if (!spill(path, text)) {
            return 0;
        }
    }
    return 1;
}

static void empty(const char* dir, const char* stem, int to) {
    char path[128];
    int  i;

    for (i = 0; i < to; ++i) {
        cf_fmt(path, sizeof path, "%s/%s%d", dir, stem, 1000 + i);
        remove(path);
    }
}

/* 256 files is the most a directory may hold, and 257 is an error that
   names the directory and the limit: the listing used to stop at 256
   and hand back whichever 256 the file system offered first, so an
   override could vanish depending on the order the disk keeps. */
static void test_too_many_files(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b;
    cf_config*  c;

    check(fill("cf_many.d", "f", 0, 256), "wrote 256 drop-in files");
    b = cf_build(NULL);
    cf_builder_dir(b, "conf.d", CF_RANK_SYS_FILE, "cf_many.d", "ini", 0);
    c = cf_builder_create(b, &err);
    check(c != NULL, "256 files load");
    if (c == NULL) {
        printf("  %s\n", err.message);
    } else {
        check(cf_get_int(c, "k1000", -1) == 0 && cf_get_int(c, "k1255", -1) == 255,
              "every one of them");
        cf_config_free(c);
    }

    check(fill("cf_many.d", "f", 256, 257), "and one more");
    b = cf_build(NULL);
    cf_builder_dir(b, "conf.d", CF_RANK_SYS_FILE, "cf_many.d", "ini", 0);
    c = cf_builder_create(b, &err);
    check(c == NULL, "257 files are an error, not 256 of them");
    has(err.message, "cf_many.d", "naming the directory");
    has(err.message, "256", "and the limit");
    cf_config_free(c);

    /* Optional covers a directory that is not there, never one that
       cannot be read whole. */
    b = cf_build(NULL);
    cf_builder_dir(b, "conf.d", CF_RANK_SYS_FILE, "cf_many.d", "ini", CF_OPTIONAL);
    c = cf_builder_create(b, &err);
    check(c == NULL, "optional or not");
    cf_config_free(c);
    empty("cf_many.d", "f", 257);
}

static void test_too_many_secrets(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b;
    cf_config*  c;

    check(fill("cf_many_secrets", "s", 0, 256), "wrote 256 secrets");
    b = cf_build(NULL);
    cf_builder_secrets(b, "secrets", CF_RANK_ENV, "cf_many_secrets", NULL, 0);
    c = cf_builder_create(b, &err);
    check(c != NULL, "256 secrets load");
    if (c == NULL) {
        printf("  %s\n", err.message);
    } else {
        check(cf_count(c) == 256, "every one of them");
        cf_config_free(c);
    }

    check(fill("cf_many_secrets", "s", 256, 257), "and one more");
    b = cf_build(NULL);
    cf_builder_secrets(b, "secrets", CF_RANK_ENV, "cf_many_secrets", NULL, CF_OPTIONAL);
    c = cf_builder_create(b, &err);
    check(c == NULL, "257 secrets are an error, optional or not");
    has(err.message, "cf_many_secrets", "naming the directory");
    has(err.message, "256", "and the limit");
    cf_config_free(c);
    empty("cf_many_secrets", "s", 257);
}

#if defined(_WIN32)
/* A name NTFS allows and this library cannot carry. 169 CJK characters
   are 507 bytes of UTF-8: the name fits a listing's slot, and with the
   directory in front the path does not fit (dir.c's own check). 171 are
   513 bytes, and the name itself does not fit (the listing's). Both
   used to be skipped; both are errors now, naming the directory. */
static void test_long_name(int chars, const char* what) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b;
    cf_config*  c;
    wchar_t     wpath[256];
    FILE*       f;
    int         i;

    memcpy(wpath, L"cf_l\\", 6 * sizeof(wchar_t));
    for (i = 0; i < chars; ++i) {
        wpath[5 + i] = (wchar_t)0x4E2D;
    }
    wpath[5 + chars] = L'\0';
    f                = _wfopen(wpath, L"wb");
    if (f == NULL) {
        printf("note: no file with a name of %d characters here; %s is not tested\n",
               chars, what);
        return;
    }
    fputs("k = 1\n", f);
    fclose(f);

    b = cf_build(NULL);
    cf_builder_dir(b, "conf.d", CF_RANK_SYS_FILE, "cf_l", "ini", 0);
    c = cf_builder_create(b, &err);
    check(c == NULL, what);
    has(err.message, "cf_l", "the error names the directory");
    cf_config_free(c);
    _wremove(wpath);
}

/* A drive-relative path on a drive that is not there reduced to "Q:",
   which the walk up the tree took for a step up and tried again, for
   ever. Either answer is the contract's; returning is the test. */
static void test_missing_drive(void) {
    DWORD             drives = GetLogicalDrives();
    cf_error          err    = CF_ERROR_INIT;
    char              drive[4], rel[16], abs[32];
    const char*       paths[1];
    cf_platform_wake* w;
    int               i;

    for (i = 25; i >= 3; --i) {
        if ((drives & ((DWORD)1 << i)) == 0) {
            break;
        }
    }
    if (i < 3) {
        printf("note: every drive letter is taken; the missing drive is not tested\n");
        return;
    }
    cf_fmt(drive, sizeof drive, "%c:", 'A' + i);
    cf_fmt(rel, sizeof rel, "%c:app.ini", 'A' + i);
    cf_fmt(abs, sizeof abs, "%c:\\nonexistent\\app.ini", 'A' + i);

    paths[0] = rel;
    w        = cf_platform_wake_open(paths, 1, cf_default_allocator(), &err);
    check(w == NULL, "a watch on a missing drive, drive-relative, returns and fails");
    has(err.message, drive, "naming the directory it would watch");
    cf_platform_wake_close(w);

    paths[0] = abs;
    w        = cf_platform_wake_open(paths, 1, cf_default_allocator(), &err);
    check(w == NULL, "and so does one with a full path");
    cf_platform_wake_close(w);
}
#endif /* _WIN32 */

#if defined(CF_TEST_FIFO)
/* A FIFO opened for reading waits for a writer, so one in conf.d hung
   cf_create and every reload. It is named and refused, as anything in
   the directory that is not a file is; a subdirectory is skipped, as it
   always was on Windows; and a link to a file is read through, because
   that is what a Kubernetes ConfigMap mount is. */
static void test_not_a_file(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b;
    cf_config*  c;

    check(spill("cf_fifo.d/10-base.ini", "port = 1\n"), "wrote a drop-in file");
    check(spill("cf_target.ini", "host = linked\n"), "and a file elsewhere");
    check(symlink("../cf_target.ini", "cf_fifo.d/20-link.ini") == 0,
          "linked into the directory");
    check(mkdir("cf_fifo.d/30-sub.ini", 0700) == 0, "made a subdirectory");
    check(mkfifo("cf_fifo.d/zz.ini", 0600) == 0, "made a FIFO");

    b = cf_build(NULL);
    cf_builder_dir(b, "conf.d", CF_RANK_SYS_FILE, "cf_fifo.d", NULL, 0);
    c = cf_builder_create(b, &err);
    check(c == NULL, "a FIFO is refused, not waited on");
    has(err.message, "zz.ini", "and named");
    cf_config_free(c);

    remove("cf_fifo.d/zz.ini");
    b = cf_build(NULL);
    cf_builder_dir(b, "conf.d", CF_RANK_SYS_FILE, "cf_fifo.d", NULL, 0);
    c = cf_builder_create(b, &err);
    check(c != NULL, "a subdirectory is skipped");
    if (c == NULL) {
        printf("  %s\n", err.message);
    } else {
        check(cf_get_int(c, "port", 0) == 1, "the file is read");
        check(strcmp(cf_get_text(c, "host", ""), "linked") == 0,
              "and so is the link, through to its file");
        cf_config_free(c);
    }

    check(mkfifo("cf_fifo_secrets/db.password", 0600) == 0, "made a FIFO of secrets");
    b = cf_build(NULL);
    cf_builder_secrets(b, "secrets", CF_RANK_ENV, "cf_fifo_secrets", NULL, 0);
    c = cf_builder_create(b, &err);
    check(c == NULL, "a FIFO among secrets is refused too");
    has(err.message, "db.password", "and named");
    cf_config_free(c);

    remove("cf_fifo_secrets/db.password");
    remove("cf_fifo.d/10-base.ini");
    remove("cf_fifo.d/20-link.ini");
    check(rmdir("cf_fifo.d/30-sub.ini") == 0, "removed the subdirectory");
    remove("cf_target.ini");
}
#endif /* CF_TEST_FIFO */

#endif /* CF_WITH_DIR */

int main(void) {
#if CF_WITH_DIR
    test_too_many_files();
    test_too_many_secrets();
#if defined(_WIN32)
    test_long_name(169, "a path too long for dir.c is an error");
    test_long_name(171, "a name too long for the listing is an error");
    /* Defined with the rest, under CF_WITH_DIR: the minimal build has no
       directory source, and MSVC refused the call to an undeclared name. */
    test_missing_drive();
#endif
#if defined(CF_TEST_FIFO)
    test_not_a_file();
#endif
#endif
    return cf_test_done();
}
