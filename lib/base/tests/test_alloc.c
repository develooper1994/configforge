/* test_alloc.c - arena behaviour, including clean exhaustion. */
#include "configforge/cf_alloc.h"
#include <stdio.h>
#include <string.h>

#include "cf_test.h"

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

/* cf_arena_fixed aligns the caller's buffer itself: from every start offset
   1..15 each allocation is aligned for a double and lies inside the buffer
   (the alignment arithmetic is otherwise argued only in a comment, and
   EVA, which cannot follow it, records its alarms as accepted). */
static void test_fixed_unaligned(void) {
    union {
        double d;
        char   c[512];
    } store;
    size_t off;

    for (off = 1; off < 16; ++off) {
        char*     start = store.c + off;
        size_t    size  = sizeof store.c - off;
        cf_arena* ar    = cf_arena_fixed(start, size);
        int       i;
        int       ok = 1;

        check(ar != NULL, "unaligned fixed arena created");
        if (ar == NULL) {
            continue;
        }
        check((char*)ar >= start && (char*)ar < start + size,
              "arena struct inside buffer");
        for (i = 0; i < 8; ++i) {
            char* p = (char*)cf_arena_alloc(ar, 24);
            if (p == NULL) {
                break;
            }
            if (((size_t)p % sizeof(double)) != 0 || p < start ||
                p + 24 > start + size) {
                ok = 0;
            }
        }
        check(ok, "every allocation aligned and inside the buffer");
    }
}

/* A request that cannot be sized is refused, never served short (K-9d): the
   rounding up to the alignment and the block header must not wrap. */
static void test_oversize_refused(void) {
    static char buf[512];
    cf_arena*   ar  = cf_arena_new(NULL, 0);
    cf_arena*   fx  = cf_arena_fixed(buf, sizeof buf);
    size_t      max = (size_t)-1;
    size_t      used;

    check(ar != NULL && fx != NULL, "arenas created");
    if (ar == NULL || fx == NULL) {
        cf_arena_free(ar);
        return;
    }
    used = cf_arena_used(ar);
    check(cf_arena_alloc(ar, max) == NULL, "a request of SIZE_MAX is refused");
    check(cf_arena_alloc(ar, max - 7) == NULL,
          "a request that wraps on rounding is refused");
    check(cf_arena_alloc(ar, max - 20) == NULL,
          "a request that wraps with the block header is refused");
    check(cf_arena_alloc(fx, max) == NULL, "a fixed arena refuses SIZE_MAX");
    check(cf_arena_calloc(ar, max) == NULL, "calloc of SIZE_MAX is refused");
    check(cf_arena_dup(ar, "x", max) == NULL, "dup of SIZE_MAX bytes is refused");
    check(cf_arena_used(ar) == used, "a refusal leaves the arena as it was");
    check(cf_arena_alloc(ar, 8) != NULL, "the arena still serves");
    cf_arena_free(ar);
}

int main(void) {
    test_growing();
    test_alignment();
    test_fixed();
    test_fixed_unaligned();
    test_oversize_refused();
    return cf_test_done();
}
