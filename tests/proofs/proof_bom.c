/*
 * proof_bom.c - CBMC: for every blob of up to N bytes, cf_blob_skip_bom -
 * which every text reader calls before it reads its first byte - reads
 * nothing outside the blob and returns 0 or 3, never more than the
 * blob's length.
 *
 * cbmc-sources: lib/serialize/src/blob.c lib/port/src/port.c
 */

#include <configforge/cf_serialize.h>

#include <assert.h>

#ifndef N
#define N 4
#endif

size_t nondet_size(void);
char   nondet_char(void);

int main(void) {
    char    s[N];
    size_t  len = nondet_size();
    size_t  i;
    size_t  skip;
    cf_blob b;

    __CPROVER_assume(len <= N);
    for (i = 0; i < N; ++i) {
        s[i] = nondet_char();
    }

    cf_blob_mem(&b, s, len, NULL);
    assert(b.p == s && b.n == len && b.name != NULL);

    skip = cf_blob_skip_bom(&b);
    assert(skip == 0 || skip == 3);
    assert(skip <= len);
    assert((skip == 3) ==
           (len >= 3 && (unsigned char)s[0] == 0xEFu && (unsigned char)s[1] == 0xBBu &&
            (unsigned char)s[2] == 0xBFu));

    assert(cf_blob_skip_bom(NULL) == 0);
    cf_blob_mem(NULL, s, len, NULL);
    return 0;
}
