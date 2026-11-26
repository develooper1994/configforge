/* EVA harness for lib/base: key.c, error.c, value.c (card L-3b; alloc.c is
   eva_alloc.c). Run by scripts/check/eva.cmake; never built into the library. Integers
   cover their whole range; a buffer is 64 bytes with a length in 0..64; a string is
   NUL-terminated at an unknown place. The real type is left out with the float half of
   numeric.c (docs/Roadmap.md): no cf_value_real, no CF_TYPE_REAL, and
   cf_value_infer (which can read a real) is not called. */
#include "__fc_builtin.h"
#include <stddef.h>

#include "configforge/cf_error.h"
#include "configforge/cf_port.h"
#include "configforge/cf_value.h"

#define BUF 64
#define STR 16

static void unknown_string(char* s) {
    Frama_C_make_unknown(s, STR - 1);
    s[STR - 1] = '\0';
}

int main(void) {
    char     buf[BUF];
    char     text[STR];
    char     s[STR];
    cf_error e  = CF_ERROR_INIT;
    cf_error e2 = CF_ERROR_INIT;
    cf_value va;
    cf_value vb;
    cf_value out;
    size_t   n   = (size_t)Frama_C_interval(0, STR);
    size_t   cap = (size_t)Frama_C_interval(0, BUF);
    int      any = Frama_C_interval(-2147483647 - 1, 2147483647);

    Frama_C_make_unknown(text, STR);
    unknown_string(s);
    /* A text value is NUL-terminated as well as length-carrying (cf_text). */
    n    = (size_t)Frama_C_interval(0, STR - 1);
    s[n] = '\0';

    /* keys */
    (void)cf_key_valid(text, n);

    /* errors */
    (void)cf_error_set(&e, (cf_code)any, s, s, "<%s|%d>", s, any);
    cf_error_place(&e, any, (unsigned int)Frama_C_interval(0, 2147483647), 3U);
    cf_error_copy(&e2, &e);
    cf_error_clear(&e2);
    (void)cf_code_name((cf_code)any);
    (void)cf_code_description((cf_code)any);
    (void)cf_reason_name(any);
    (void)cf_reason_description(any);
    (void)cf_reason_severity(any);
    (void)cf_reason_weakness(any);

    /* values (no real) */
    (void)cf_type_name((cf_type)any);
    (void)cf_type_json_name((cf_type)any);
    cf_value_null(&va);
    cf_value_bool(&vb, (cf_bool)Frama_C_interval(0, 1));
    (void)cf_value_to_text(&va, buf, cap);
    (void)cf_value_to_text(&vb, buf, cap);
    (void)cf_value_cmp(&va, &vb);
    (void)cf_value_equal(&va, &vb);
    cf_value_int(&va, Frama_C_long_long_interval(-9223372036854775807LL - 1,
                                                 9223372036854775807LL));
    cf_value_int(&vb, Frama_C_long_long_interval(-9223372036854775807LL - 1,
                                                 9223372036854775807LL));
    (void)cf_value_to_text(&va, buf, cap);
    (void)cf_value_cmp(&va, &vb);
    (void)cf_value_equal(&va, &vb);
    cf_value_text(&va, s, n);
    cf_value_text(&vb, text, n);
    (void)cf_value_to_text(&va, buf, cap);
    (void)cf_value_cmp(&va, &vb);
    (void)cf_value_equal(&va, &vb);
    cf_value_null(&vb);
    (void)cf_value_cmp(&va, &vb);
    (void)cf_value_cmp(&vb, &va);
    (void)cf_value_from_text(CF_TYPE_NULL, text, n, &out);
    (void)cf_value_from_text(CF_TYPE_BOOL, text, n, &out);
    (void)cf_value_from_text(CF_TYPE_INT, text, n, &out);
    return 0;
}
