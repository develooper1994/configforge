/*
 * proof_text_to_int.c - CBMC: for every input of up to N bytes, every
 * byte any value, cf_text_to_i64 and cf_text_to_u64 read nothing outside
 * the input, overflow nothing, and report a consumed length within it.
 *
 * Not a test: a proof, over all inputs at once, by bounded model
 * checking. Run by verify -DEXTRA=proofs.
 *
 * cbmc-sources: src/core/numeric.c
 */

#include <configforge/cf_numeric.h>

#include <assert.h>

#ifndef N
#define N 22 /* one more than the longest int64, sign included */
#endif

size_t nondet_size(void);
char   nondet_char(void);
int    nondet_int(void);

int main(void) {
    char    s[N];
    size_t  len = nondet_size();
    size_t  end = 0;
    size_t  i;
    cf_i64  v;
    cf_u64  u;
    size_t* endp = nondet_int() ? &end : (size_t*)0;

    __CPROVER_assume(len <= N);
    for (i = 0; i < N; ++i) {
        s[i] = nondet_char();
    }

    if (cf_text_to_i64(s, len, &v, endp)) {
        assert(endp == 0 || end <= len);
        (void)v;
    }
    if (cf_text_to_u64(s, len, &u, endp)) {
        assert(endp == 0 || end <= len);
    }
    return 0;
}
