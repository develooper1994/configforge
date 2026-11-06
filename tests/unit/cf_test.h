/*
 * cf_test.h - what every test program here shares, written once.
 *
 *     #include "cf_test.h"
 *     ...
 *     int main(void) { ...; return cf_test_done(); }
 *
 * A check counts and goes on: one run lists every failure, not the
 * first. ctest runs each program and reads its exit status, so a test
 * framework would add a second runner and nothing the suite lacks; this
 * is the part each program used to define for itself (sixty of them,
 * in five spellings). C89 and C++, every compiler in the matrix.
 *
 * Static, so each program has its own counts; cf_test_done() names every
 * helper, so a program that uses only some of them still compiles
 * without an unused-function warning (-Wall, MSVC's C4505).
 */

#ifndef CONFIGFORGE_TESTS_CF_TEST_H
#define CONFIGFORGE_TESTS_CF_TEST_H

#include <stdio.h>
#include <string.h>

#include "cf_fixture.h" /* spill() */

static int g_fail = 0, g_run = 0;

/* A condition, and what it says when false. */
static void check(int c, const char* what) {
    ++g_run;
    if (!c) {
        ++g_fail;
        printf("FAIL: %s\n", what);
    }
}

/* Text that must be exactly this, both shown when it is not. */
static void check_text(const char* got, const char* want, const char* what) {
    ++g_run;
    if (got == NULL || strcmp(got, want) != 0) {
        ++g_fail;
        printf("FAIL: %s  got=\"%s\" want=\"%s\"\n", what,
               got != NULL ? got : "(null)", want);
    }
}

/* Text that must hold a needle; the text is shown when it does not. */
static void has(const char* hay, const char* needle, const char* what) {
    ++g_run;
    if (hay == NULL || strstr(hay, needle) == NULL) {
        ++g_fail;
        printf("FAIL: %s  missing \"%s\" in:\n%s\n", what, needle,
               hay != NULL ? hay : "(null)");
    }
}

/* Text that must not hold a needle - a secret, a refused construct. */
static void lacks(const char* hay, const char* needle, const char* what) {
    ++g_run;
    if (hay != NULL && strstr(hay, needle) != NULL) {
        ++g_fail;
        printf("FAIL: %s  found \"%s\" in:\n%s\n", what, needle, hay);
    }
}

/* The summary line and the exit status: 0 when nothing failed. */
static int cf_test_done(void) {
    (void)&check;
    (void)&check_text;
    (void)&has;
    (void)&lacks;
    (void)&spill;
    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}

#endif /* CONFIGFORGE_TESTS_CF_TEST_H */
