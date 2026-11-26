/* EVA harness for cf_arena_fixed and the arena it builds in a caller's
   buffer (card L-3b, alloc.c). Run by scripts/check/eva.cmake; never built
   into the library. The buffer is aligned for any object, its size covers
   0..256, a request 0..300. Own stop number: 30 alarms. */
#include "__fc_builtin.h"
#include <stddef.h>

#include "configforge/cf_alloc.h"
#include "configforge/cf_port.h"

#define STR 16

int main(void) {
    char text[STR];
    union {
        double      d;
        long double ld;
        void*       vp;
        char        c[256];
    } fixed_u;
    cf_arena* ar;
    char*     p;
    size_t    n = (size_t)Frama_C_interval(0, STR);

    Frama_C_make_unknown(text, STR);
    /* the fixed arena, over an aligned buffer of unknown size */
    ar = cf_arena_fixed(fixed_u.c, (size_t)Frama_C_interval(0, 256));
    if (ar != NULL) {
        p = (char*)cf_arena_alloc(ar, (size_t)Frama_C_interval(0, 300));
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
