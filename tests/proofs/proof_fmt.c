/*
 * proof_fmt.c - CBMC: cf_fmt, through which every error message and
 * every explain() line is written, never writes past `cap`, always
 * terminates when cap > 0, and returns the length the whole text needs -
 * for every capacity, every string argument of up to N bytes, every
 * slice of one (%S), and every int64 (%I).
 *
 * cbmc-sources: src/core/port.c src/core/numeric.c
 */

#include <configforge/cf_port.h>

#include <assert.h>
#include <string.h>

#ifndef N
#define N 6
#endif

size_t nondet_size(void);
char   nondet_char(void);
cf_i64 nondet_i64(void);

int main(void) {
    char   arg[N + 1];
    char   buf[2 * N + 24];
    size_t cap = nondet_size();
    size_t i, n, len = nondet_size(), want;
    cf_i64 v = nondet_i64();

    __CPROVER_assume(cap <= sizeof buf);
    __CPROVER_assume(len <= N);
    for (i = 0; i < N; ++i) {
        arg[i] = nondet_char();
        __CPROVER_assume(arg[i] != '\0');
    }
    arg[N] = '\0';

    n = cf_fmt(buf, cap, "[%s]", arg);
    assert(n == N + 2);
    if (cap > 0) {
        want = (n < cap) ? n : cap - 1;
        assert(buf[want] == '\0');
    }

    n = cf_fmt(buf, cap, "%S=%I", arg, len, v);
    if (cap > 0) {
        assert(strlen(buf) < cap);
    }
    assert(n >= len + 2);
    return 0;
}
