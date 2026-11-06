/*
 * cf_fixture.h - writing the files a test reads, once.
 *
 * Apart from cf_test.h, which includes it, for a program that writes
 * fixtures and checks nothing (tests/doctor/fixtures.c). C89 and C++.
 */

#ifndef CONFIGFORGE_TESTS_CF_FIXTURE_H
#define CONFIGFORGE_TESTS_CF_FIXTURE_H

#include <stdio.h>

/* A file with exactly this text; false when it could not be written. */
static int spill(const char* path, const char* text) {
    FILE* f = fopen(path, "wb");
    if (f == NULL) {
        return 0;
    }
    fputs(text, f);
    fclose(f);
    return 1;
}

#endif /* CONFIGFORGE_TESTS_CF_FIXTURE_H */
