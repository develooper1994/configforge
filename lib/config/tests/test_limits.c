/*
 * test_limits.c - docs/Limits.md says what the build does (A-13).
 *
 * The page's numbers are generated from the sources and compared by
 * gate.limits; this is the other half: the page's rows against the
 * values the library was compiled with. It reads the page named by
 * argv[1], and
 *
 *   - in a build that keeps the default limits, checks the five rows of
 *     "Build limits" against the compiled values and against
 *     cf_build_limit();
 *   - in any other build (the `minimal` preset) says so and does not
 *     compare those rows;
 *   - always checks the decode budget's constants and the depth rule.
 *
 * The behaviour at a limit is held by the tests named on the page.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cf_test.h"

#include "configforge/cf_build_options.h"
#include "configforge/cf_flatten.h"

static char* page = NULL;

static int load(const char* path) {
    FILE*  f = fopen(path, "rb");
    long   n;
    size_t got;

    if (f == NULL) {
        return 0;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    page = (char*)malloc((size_t)n + 1);
    if (page == NULL) {
        fclose(f);
        return 0;
    }
    got = fread(page, 1, (size_t)n, f);
    fclose(f);
    page[got] = '\0';
    return 1;
}

/* The number in the first cell after "| `name` | " in the page; -1 when
   the row is not there. */
static long row(const char* name) {
    char        key[64];
    const char* at;

    sprintf(key, "| `%s` | ", name);
    at = strstr(page, key);
    if (at == NULL) {
        return -1;
    }
    return strtol(at + strlen(key), NULL, 10);
}

static void same(const char* name, long compiled) {
    char msg[96];
    long stated = row(name);

    sprintf(msg, "limits: the page's row %s says %ld, the build has %ld", name, stated,
            compiled);
    check(stated == compiled, msg);
}

static void test_build_rows(void) {
    long key = row("CF_KEY_MAX"), message = row("CF_MESSAGE_MAX"),
         path = row("CF_PATH_MAX"), depth = row("CF_MAX_DEPTH");

    check(key > 0 && message > 0 && path > 0 && depth > 0,
          "limits: the page has the four build-limit rows");
    check(row("CF_STACK_BUDGET") >= 0, "limits: the page has the stack-budget row");
#ifndef CF_LIMITS_DEFAULT
    printf("limits: not a default build, page not compared\n");
    return;
#else
    same("CF_KEY_MAX", (long)cf_build_limit("key"));
    same("CF_MESSAGE_MAX", (long)cf_build_limit("message"));
    same("CF_PATH_MAX", (long)cf_build_limit("path"));
    same("CF_MAX_DEPTH", (long)cf_build_limit("depth"));
    same("CF_SOURCE_MAX", (long)cf_build_limit("source"));
#endif
}

static void test_fixed(void) {
    cf_error err = CF_ERROR_INIT;

    same("CF_SOURCE_MAX", (long)CF_SOURCE_MAX);
    check(sizeof(err.source) == (size_t)CF_SOURCE_MAX,
          "limits: CF_SOURCE_MAX is the size of cf_error's source");
    same("CF_MAX_SOURCES", (long)CF_MAX_SOURCES);
    same("CF_MAX_OPEN_PREFIXES", (long)CF_MAX_OPEN_PREFIXES);
    same("CF_EXPAND_MAX", (long)CF_EXPAND_MAX);
    same("CF_EXPAND_TOTAL_MAX", (long)CF_EXPAND_TOTAL_MAX);
}

static void test_budget(void) {
    char want[128];

    same("CF_DECODE_FACTOR", (long)CF_DECODE_FACTOR);
    same("CF_DECODE_FLOOR", (long)CF_DECODE_FLOOR);
    same("CF_DECODE_KEY_COST", (long)CF_DECODE_KEY_COST);
    check(cf_decode_budget(0) == (size_t)CF_DECODE_FLOOR,
          "limits: an empty document may make the floor");
    check(cf_decode_budget(1048576) == (size_t)64 * 1048576 + 1048576,
          "limits: 1 MiB may make 64 x 1 MiB and the floor");
    sprintf(want, "= %lu bytes", (unsigned long)cf_decode_budget(1048576));
    has(page, want, "limits: the page's 1 MiB row is the budget of 1 MiB");
    has(page, "| 1 MiB | 65 MiB |", "limits: the page's 1 MiB row says 65 MiB");
}

static void test_depth(void) {
    char want[96];

    if (row("CF_MAX_DEPTH") != (long)CF_MAX_DEPTH) {
        return; /* another build's depth: the page states the default */
    }
    sprintf(want, "`CF_MAX_DEPTH` %lu allows %lu levels",
            (unsigned long)cf_build_limit("depth"),
            (unsigned long)cf_build_limit("depth") - 1);
    has(page, want, "limits: the depth rule, as cf_build_limit(\"depth\") has it");
}

int main(int argc, char** argv) {
    if (argc < 2 || !load(argv[1])) {
        printf("limits: the page (docs/Limits.md) is the one argument\n");
        return 1;
    }
    test_build_rows();
    test_fixed();
    test_budget();
    test_depth();
    free(page);
    return cf_test_done();
}
