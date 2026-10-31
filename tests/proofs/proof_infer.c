/*
 * proof_infer.c - CBMC: for every input of up to N bytes, cf_infer_scalar
 * - which every untyped value from every file passes through - reads
 * nothing outside the input and returns one of the four kinds.
 *
 * cbmc-sources: src/core/numeric.c
 */

#include <configforge/cf_numeric.h>

#include <assert.h>

#ifndef N
#define N 12
#endif

size_t nondet_size(void);
char   nondet_char(void);

int main(void) {
    char           s[N];
    size_t         len = nondet_size();
    size_t         i;
    cf_scalar_kind k;

    __CPROVER_assume(len <= N);
    for (i = 0; i < N; ++i) {
        s[i] = nondet_char();
    }
    k = cf_infer_scalar(s, len);
    assert(k == CF_SCALAR_TEXT || k == CF_SCALAR_BOOL || k == CF_SCALAR_INT ||
           k == CF_SCALAR_REAL);
    return 0;
}
