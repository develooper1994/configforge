/*
 * test_lock.c - a lock that plugs in (D-227, D-187): cf_lock_none takes
 * nothing, cf_lock_sidecar excludes a second take of one path (in one
 * process too), and a locker the program fills in is called as a built-in
 * one is.
 */

#ifndef _WIN32
#define _POSIX_C_SOURCE 200112L /* system */
#endif

#include <configforge/cf_lock.h>
#include <configforge/cf_platform.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cf_test.h"

static int exists(const char* path) {
    FILE* f = fopen(path, "rb");

    if (f == NULL) {
        return 0;
    }
    fclose(f);
    return 1;
}

/* A locker of the program's own: busy when its counter is odd. */
static cf_bool mine_take(const cf_locker* self, cf_lock* held, const char* path,
                         unsigned long wait_ms, cf_error* err) {
    unsigned* n = (unsigned*)self->user;

    (void)path;
    (void)wait_ms;
    ++*n;
    if ((*n & 1u) != 0u) {
        return cf_error_set(err, CF_E_BUSY, NULL, path, "held by the test");
    }
    held->handle = 5;
    return CF_TRUE;
}

static unsigned g_given = 0;
static void     mine_give(const cf_locker* self, cf_lock* held) {
    (void)self;
    ++g_given;
    held->handle = -1;
}

static void test_none(void) {
    const cf_locker* n = cf_lock_none();
    cf_lock          a = {-1, NULL}, b = {-1, NULL};
    static cf_error  err = CF_ERROR_INIT;

    check(strcmp(n->name, "none") == 0, "none: its name");
    check(n->suggested_wait_ms == 0ul, "none: no suggested wait");
    check(n->take(n, &a, "cf_lk_none.txt", 0, &err), "none: the first take is 1");
    check(n->take(n, &b, "cf_lk_none.txt", 0, &err), "none: the same path again is 1");
    n->give(n, &a);
    n->give(n, &b);
    check(!exists(".cf_lk_none.txt.cf-lock") && !exists("cf_lk_none.txt"),
          "none: no file is made");
}

static void test_plug(void) {
    unsigned        count = 0;
    cf_locker       mine  = {"mine", 7ul, mine_take, mine_give, NULL};
    cf_lock         h     = {-1, NULL};
    static cf_error err   = CF_ERROR_INIT;

    mine.user = &count;
    check(mine.suggested_wait_ms == 7ul, "plug: suggested_wait_ms reads back");
    check(!mine.take(&mine, &h, "x", 0, &err), "plug: the first take is refused (odd)");
    check(err.code == CF_E_BUSY, "plug: CF_E_BUSY");
    check(strstr(err.message, "held by the test") != NULL, "plug: its message");
    check(mine.take(&mine, &h, "x", 0, &err), "plug: the second take succeeds");
    check(count == 2u && h.handle == 5, "plug: self->user reached it; handle set");
    mine.give(&mine, &h);
    check(g_given == 1u && h.handle == -1, "plug: give called through its pointer");
}

static void test_codes(void) {
    volatile int memory = (int)CF_E_MEMORY; /* read at run time: C6326 */

    check(strcmp(cf_code_name(CF_E_BUSY), "busy") == 0, "code: CF_E_BUSY is busy");
    check(memory == 9, "code: CF_E_MEMORY is still 9");
}

static void test_sidecar(void) {
    const cf_locker* s = cf_lock_sidecar();
    cf_lock          a = {-1, NULL}, b = {-1, NULL}, never = {-1, NULL};
    static cf_error  err = CF_ERROR_INIT, err2 = CF_ERROR_INIT;
    time_t           t0, t1;

    (void)remove(".cf_lk.txt.cf-lock");
    check(strcmp(s->name, "sidecar") == 0, "sidecar: its name");
    check(s->take(s, &a, "cf_lk.txt", 0, &err), "sidecar: the first take is 1");
    check(exists(".cf_lk.txt.cf-lock"), "sidecar: the lock file exists");

    check(!s->take(s, &b, "cf_lk.txt", 0, &err2), "sidecar: a second take is 0");
    check(err2.code == CF_E_BUSY, "sidecar: CF_E_BUSY");
    check(strstr(err2.message, "is busy") != NULL, "sidecar: 'is busy'");

    t0 = time(NULL);
    check(!s->take(s, &b, "cf_lk.txt", 1500ul, &err2), "sidecar: the wait ends busy");
    t1 = time(NULL);
    check(t1 - t0 >= 1, "sidecar: a 1500 ms wait took at least a second");

    s->give(s, &a);
    check(s->take(s, &b, "cf_lk.txt", 0, &err),
          "sidecar: after give, the take succeeds");
    s->give(s, &b);
    check(exists(".cf_lk.txt.cf-lock"), "sidecar: the lock file is kept");
    check(!exists("cf_lk.txt"), "sidecar: the file itself is not made");

    s->give(s, &never);
    s->give(s, &never);
    check(never.handle == -1, "sidecar: give of a lock never taken is harmless");

#if CF_PLATFORM_POSIX
    (void)remove(".cf_lk.txt.cf-lock");
    if (system("ln -s cf_other .cf_lk.txt.cf-lock") == 0) {
        check(!s->take(s, &a, "cf_lk.txt", 0, &err2), "link: the take is refused");
        check(strstr(err2.message, "is a symbolic link") != NULL, "link: its message");
        check(!exists("cf_other"), "link: the target is not created");
        (void)remove(".cf_lk.txt.cf-lock");
    }
#endif
    (void)remove(".cf_lk.txt.cf-lock");
}

int main(void) {
    cf_lock         h   = {-1, NULL};
    static cf_error err = CF_ERROR_INIT;

    test_none();
    test_plug();
    test_codes();
    /* A platform with nothing to lock (none, WASI) says so by the sidecar's take. */
    if (!cf_lock_sidecar()->take(cf_lock_sidecar(), &h, "cf_lk.txt", 0, &err) &&
        err.code == CF_E_CAPABILITY) {
        printf("skipped: no files to lock\n");
        check(err.code == CF_E_CAPABILITY, "none: CF_E_CAPABILITY");
        return cf_test_done();
    }
    cf_lock_sidecar()->give(cf_lock_sidecar(), &h);
    test_sidecar();
    return cf_test_done();
}
