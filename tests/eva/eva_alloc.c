/* EVA harness for lib/base/src/alloc.c (card L-3b). Run by
   scripts/check/eva.cmake; never built into the library. The arena is
   built by the harness: a growing one over the default allocator. The fixed arena
   is tests/eva/eva_arena_fixed.c. Sizes cover their
   whole range up to a bound; text is 16 unknown bytes. */
#include "__fc_builtin.h"
#include <stddef.h>

#include "configforge/cf_alloc.h"
#include "configforge/cf_port.h"

#define STR 16

int main(void) {
    char      text[STR];
    cf_arena* ar;
    char*     p;
    size_t    n = (size_t)Frama_C_interval(0, STR);

    Frama_C_make_unknown(text, STR);

    (void)cf_default_allocator();
    ar = cf_arena_new(cf_default_allocator(), (size_t)Frama_C_interval(0, 4096));
    if (ar != NULL) {
        p = (char*)cf_arena_alloc(ar, (size_t)Frama_C_interval(0, 5000));
        if (p != NULL) {
            p[0] = 'x';
        }
        (void)cf_arena_calloc(ar, n);
        (void)cf_arena_dup(ar, text, n);
        (void)cf_arena_used(ar);
        (void)cf_arena_capacity(ar);
        (void)cf_arena_is_fixed(ar);
        (void)cf_arena_allocator(ar);
        cf_arena_wipe(ar);
        cf_arena_free(ar);
    }

    return 0;
}
