/* test_alloc.c - arena behaviour, including clean exhaustion. */
#include "configforge/cf_alloc.h"
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

static void test_growing(void) {
    cf_arena* ar = cf_arena_new(NULL, 0);
    char*     a;
    char*     b;

    check(ar != NULL, "arena created");
    a = cf_arena_dup(ar, "hello", 5);
    check(a != NULL && strcmp(a, "hello") == 0, "dup copies and terminates");
    b = (char*)cf_arena_calloc(ar, 64);
    check(b != NULL && b[0] == 0 && b[63] == 0, "calloc zeroes");
    check(cf_arena_used(ar) >= 70, "used accounts for both");
    check(!cf_arena_is_fixed(ar), "growing arena is not fixed");

    /* Larger than a block: must still succeed by taking a bigger one. */
    check(cf_arena_alloc(ar, 100000) != NULL, "oversize request served");
    cf_arena_free(ar);
}

static void test_alignment(void) {
    cf_arena* ar = cf_arena_new(NULL, 0);
    int       i;
    for (i = 0; i < 8; ++i) {
        void*         p    = cf_arena_alloc(ar, (size_t)(i + 1));
        unsigned long addr = (unsigned long)(size_t)p;
        check(p != NULL && (addr % sizeof(void*)) == 0, "allocation aligned");
    }
    cf_arena_free(ar);
}

static void test_fixed(void) {
    static char buf[512];
    cf_arena*   ar     = cf_arena_fixed(buf, sizeof buf);
    int         served = 0;
    void*       p;

    check(ar != NULL, "fixed arena created");
    check(cf_arena_is_fixed(ar), "fixed arena reports fixed");

    /* Must fail cleanly rather than grow: this is the freestanding
       profile's whole contract. */
    for (;;) {
        p = cf_arena_alloc(ar, 32);
        if (p == NULL) {
            break;
        }
        ++served;
        if (served > 1000) {
            break;
        }
    }
    check(served > 0 && served < 100, "fixed arena exhausts and stops");
    /* A smaller request may still fit in the tail; a large one must
       not. Refusing is the contract, not refusing everything. */
    check(cf_arena_alloc(ar, 512) == NULL, "still refuses a large request");
    cf_arena_free(ar); /* must be a no-op, not a crash */
}

int main(void) {
    test_growing();
    test_alignment();
    test_fixed();
    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
