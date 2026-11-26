/* EVA harness for lib/numeric (card L-3b). Run by scripts/check/eva.cmake;
   never built into the library. Every integer argument covers its whole
   range; a text is 64 unknown bytes with a length in 0..64 and need not
   be NUL-terminated (the API takes a length). The integer half only: the
   float half of numeric.c is out of scope (docs/Roadmap.md). */
#include "__fc_builtin.h"
#include <stddef.h>

#include "configforge/cf_numeric.h"
#include "configforge/cf_port.h"

#define BUF 64

int main(void) {
    char   text[BUF];
    size_t len = (size_t)Frama_C_interval(0, BUF);
    size_t end;
    cf_i64 i;
    cf_u64 u;

    Frama_C_make_unknown(text, BUF);

    (void)cf_text_to_i64(text, len, &i, &end);
    (void)cf_text_to_i64(text, len, &i, NULL);
    (void)cf_text_to_u64(text, len, &u, &end);
    (void)cf_text_to_u64(text, len, &u, NULL);

    (void)cf_infer_scalar(text, len);
    return 0;
}
