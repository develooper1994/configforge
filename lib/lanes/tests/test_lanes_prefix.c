/*
 * test_lanes_prefix.c - lanes built under LANES_PREFIX_CF, as ConfigForge
 * builds it, beside an unprefixed lanes in the same program.
 *
 * This program is linked with its own copy of the sources, compiled with
 * -DLANES_PREFIX_CF, whose symbols are cf_lanes_*, and with the unprefixed
 * library, whose are lanes_*. That it links is the proof that a program
 * which also links a lanes of its own sees no clash; that the two tables
 * differ shows the calls went to two copies; that they answer alike shows
 * the prefix changed nothing but the names.
 */

#include <stdio.h>
#include <string.h>

#include "lanes/lanes.h"

/* The header renamed lanes_scalar to cf_lanes_scalar; the unprefixed one
   is reached by taking the rename away and declaring it again. */
#ifndef LANES_PREFIX_CF
#error "this program is compiled with LANES_PREFIX_CF"
#endif
static const lanes_ops* (*prefixed_scalar)(void)     = cf_lanes_scalar;
static unsigned (*prefixed_cpu)(void)                = cf_lanes_cpu;
static unsigned (*prefixed_built)(void)              = cf_lanes_built;
static const lanes_ops* (*prefixed_best)(void)       = cf_lanes_best;
static const lanes_ops* (*prefixed_by_isa)(unsigned) = cf_lanes_by_isa;
static void (*prefixed_set_init)(lanes_set*, const unsigned char*,
                                 size_t)             = cf_lanes_set_init;
#undef lanes_scalar
#undef lanes_by_isa
#undef lanes_best
#undef lanes_cpu
#undef lanes_built
#undef lanes_set_init
const lanes_ops* lanes_scalar(void);
const lanes_ops* lanes_by_isa(unsigned isa);
const lanes_ops* lanes_best(void);
unsigned         lanes_cpu(void);
unsigned         lanes_built(void);
void             lanes_set_init(lanes_set* s, const unsigned char* bytes, size_t n);

static int g_fail = 0, g_run = 0;

static void check(int c, const char* what) {
    ++g_run;
    if (!c) {
        ++g_fail;
        printf("FAIL: %s\n", what);
    }
}

int main(void) {
    const lanes_ops*           a      = prefixed_scalar();
    const lanes_ops*           b      = lanes_scalar();
    static const unsigned char text[] = "key=value, 12345678; \xC3\xA9\xE2\x82";
    unsigned char              set_bytes[2];
    lanes_set                  sa, sb;
    unsigned long              va = 0, vb = 0;
    const size_t               n = sizeof text - 1;

    check(a != NULL && b != NULL, "both copies have a scalar table");
    if (a == NULL || b == NULL) {
        return 1; /* check() goes on; the rest would read through NULL */
    }
    check(a != b, "the prefixed copy is another copy, not the unprefixed one");
    check(prefixed_by_isa(LANES_ISA_SCALAR) == a,
          "the prefixed by_isa finds the prefixed table");
    check(lanes_by_isa(LANES_ISA_SCALAR) == b,
          "the unprefixed by_isa finds the unprefixed one");
    check(prefixed_best() == a && lanes_best() == b, "each best is its own scalar");
    check(prefixed_cpu() == lanes_cpu(), "both detect the same processor");
    check(prefixed_built() == lanes_built(), "both built the same backends");
    check(strcmp(a->name, b->name) == 0 && a->isa == b->isa && a->size == b->size,
          "the tables describe the same backend");

    set_bytes[0] = ';';
    set_bytes[1] = '=';
    prefixed_set_init(&sa, set_bytes, 2);
    lanes_set_init(&sb, set_bytes, 2);
    check(memcmp(sa.bits, sb.bits, sizeof sa.bits) == 0, "both make the same set");

    check(a->find_byte(text, n, ',') == b->find_byte(text, n, ','), "find_byte alike");
    check(a->find_any(text, n, &sa) == b->find_any(text, n, &sb), "find_any alike");
    check(a->find_any(text, n, &sa) == 3, "find_any finds the '='");
    check(a->find_lt(text, n, 'a') == b->find_lt(text, n, 'a'), "find_lt alike");
    check(a->ascii_run(text, n) == b->ascii_run(text, n), "ascii_run alike");
    check(a->count_byte(text, n, 'e') == b->count_byte(text, n, 'e'),
          "count_byte alike");
    check(a->utf8_valid(text, n) == b->utf8_valid(text, n), "utf8_valid alike");
    check(a->utf8_valid(text, n) == n - 2, "the cut sequence is not in the prefix");
    check(a->digits8(text + 11, &va) == b->digits8(text + 11, &vb) && va == vb &&
              va == 12345678UL,
          "digits8 alike");

    printf("test_lanes_prefix: %d checks, %d failed\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
