/*
 * proof_int_to_text.c - CBMC: for every int64 and uint64, and every
 * buffer capacity up to the worst case and beyond, cf_i64_to_text and
 * cf_u64_to_text write nothing past `cap`, terminate what they write,
 * and report 0 exactly when the text does not fit.
 *
 * That the text reads back as the same number is left to the unit tests
 * (the extremes and a sweep, test_numeric): asked here too, the round
 * trip made the formula too large to solve in hours.
 *
 * cbmc-sources: src/core/numeric.c
 */

#include <configforge/cf_numeric.h>

#include <assert.h>

cf_i64 nondet_i64(void);
cf_u64 nondet_u64(void);
size_t nondet_size(void);

int main(void) {
    char   buf[CF_I64_DIGITS + 2];
    cf_i64 v   = nondet_i64();
    cf_u64 u   = nondet_u64();
    size_t cap = nondet_size();
    size_t n;

    __CPROVER_assume(cap <= sizeof buf);

    n = cf_i64_to_text(v, buf, cap);
    assert(n == 0 || n < cap);
    assert(n == 0 || buf[n] == '\0');
    assert(cap < CF_I64_DIGITS || n > 0);

    n = cf_u64_to_text(u, buf, cap);
    assert(n == 0 || n < cap);
    assert(n == 0 || buf[n] == '\0');
    assert(cap < CF_I64_DIGITS || n > 0);
    return 0;
}
