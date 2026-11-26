/* EVA harness for lib/port (card L-3b). Run by scripts/check/eva.cmake;
   never built into the library. Every integer argument covers its whole
   range; a buffer is 64 bytes with a length in 0..64; a string is
   NUL-terminated at an unknown place. */
#include "__fc_builtin.h"
#include <stddef.h>

#include "configforge/cf_port.h"

#define BUF 64
#define STR 16

/* A string of unknown content, NUL-terminated at an unknown place. */
static void unknown_string(char* s) {
    Frama_C_make_unknown(s, STR - 1);
    s[STR - 1] = '\0';
}

int main(void) {
    char        buf[BUF];
    char        a[STR];
    char        b[STR];
    char        s[STR];
    cf_refcount rc;
    cf_u64      u = Frama_C_unsigned_long_long_interval(0, 18446744073709551615ULL);
    cf_i64      i =
        Frama_C_long_long_interval(-9223372036854775807LL - 1, 9223372036854775807LL);
    size_t cap  = (size_t)Frama_C_interval(0, BUF);
    size_t alen = (size_t)Frama_C_interval(0, STR);
    size_t blen = (size_t)Frama_C_interval(0, STR);
    size_t at   = (size_t)Frama_C_interval(0, BUF + 8);
    int    c    = Frama_C_interval(-2147483647 - 1, 2147483647);
    int    d    = Frama_C_interval(-2147483647 - 1, 2147483647);

    /* refcount */
    cf_ref_init(&rc, Frama_C_interval(0, 1000));
    (void)cf_ref_inc(&rc);
    (void)cf_ref_dec(&rc);
    (void)cf_ref_get(&rc);

    /* character classes */
    (void)cf_is_digit(c);
    (void)cf_is_upper(c);
    (void)cf_is_lower(c);
    (void)cf_is_alpha(c);
    (void)cf_is_alnum(c);
    (void)cf_is_space(c);
    (void)cf_to_lower(c);
    (void)cf_to_upper(c);

    /* comparison */
    Frama_C_make_unknown(a, STR);
    Frama_C_make_unknown(b, STR);
    (void)cf_cmp_bytes(a, alen, b, blen);
    (void)cf_cmp_ascii_ci(a, alen, b, blen);

    /* integers */
    (void)cf_u64_to_text(u, buf, cap);
    (void)cf_i64_to_text(i, buf, cap);

    /* formatting: every conversion, matching arguments */
    unknown_string(s);
    (void)cf_fmt(buf, cap, "<%s|%S|%c|%%|%q|%", s, a, alen, c);
    (void)cf_fmt(buf, cap, "%d", c);
    (void)cf_fmt(buf, cap, "%i", d);
    (void)cf_fmt(buf, cap, "%u", (unsigned int)d);
    (void)cf_fmt(buf, cap, "%zu", alen);
    (void)cf_fmt(buf, cap, "%I", i);
    (void)cf_fmt(buf, cap, "%U", u);
    (void)cf_fmt(buf, cap, "%x", (unsigned int)c);
    (void)cf_fmt(NULL, 0, "%s%d", s, c);
    /* EVA is not relational: at < cap <= BUF, so split at. */
    //@ split at;
    (void)cf_fmt_append(buf, cap, at, "%s %U", s, u);

    (void)cf_port_description();
    return 0;
}
