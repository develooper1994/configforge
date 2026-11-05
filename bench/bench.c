/*
 * bench.c - what the library's common work costs, as a function of how
 * much configuration there is. Not a test and not a gate: numbers to
 * start an optimisation from, so that one starts from a measurement.
 *
 *     ./cf_bench [N ...]        default: 100 1000 10000 20000
 *
 * For each N: a configuration of N keys built from memory with a
 * definition each; cf_get_int on every key; a blind report written; and
 * Doctor's diff of two N-key reports that differ in half their values.
 * Each is timed with clock(), C89's own, as CPU time per operation and
 * per key, so a row that grows faster than N says so at a glance.
 */

#include <configforge/configforge.h>
#include <configforge/cf_doctor.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double seconds_since(clock_t t0) {
    return (double)(clock() - t0) / (double)CLOCKS_PER_SEC;
}

/* N pairs "section<i/64>.key<i>" = "<i + bump>", in one allocation. */
static cf_pair* make_pairs(size_t n, int bump, char** storage) {
    cf_pair* p = (cf_pair*)malloc(n * sizeof *p);
    char*    s = (char*)malloc(n * 48);
    size_t   i;

    if (p == NULL || s == NULL) {
        free(p);
        free(s);
        return NULL;
    }
    for (i = 0; i < n; ++i) {
        char* k = s + i * 48;
        char* v = k + 32;
        sprintf(k, "section%lu.key%lu", (unsigned long)(i / 64), (unsigned long)i);
        sprintf(v, "%lu", (unsigned long)(i + (size_t)((i % 2 == 0) ? bump : 0)));
        p[i].key   = k;
        p[i].value = v;
    }
    *storage = s;
    return p;
}

static cf_config* build(const cf_pair* p, size_t n) {
    cf_builder* b   = cf_build(NULL);
    cf_error    err = CF_ERROR_INIT;
    cf_config*  c;
    size_t      i;

    for (i = 0; i < n; ++i) {
        cf_builder_int(b, p[i].key, 0, 0, 0, 0);
    }
    cf_builder_memory(b, "mem", CF_RANK_USER_FILE, p, n);
    c = cf_builder_create(b, &err);
    if (c == NULL) {
        fprintf(stderr, "bench: build failed: %s\n", err.message);
    }
    return c;
}

static char* report(const cf_config* c, size_t* len) {
    size_t need = cf_report(c, NULL, NULL, 0);
    char*  buf  = (char*)malloc(need + 1);
    if (buf != NULL) {
        *len = cf_report(c, NULL, buf, need + 1);
    }
    return buf;
}

static void row(const char* what, size_t n, double secs, size_t ops) {
    printf("%-14s %7lu  %10.3f ms  %9.1f ns/op  %8.1f ns/key\n", what, (unsigned long)n,
           secs * 1e3, secs * 1e9 / (double)ops, secs * 1e9 / (double)n);
}

static int run(size_t n) {
    char *     sa = NULL, *sb = NULL, *ra, *rb;
    cf_pair*   pa = make_pairs(n, 0, &sa);
    cf_pair*   pb = make_pairs(n, 1, &sb);
    cf_config *a, *b;
    clock_t    t0;
    size_t     i, la = 0, lb = 0;
    cf_i64     sum = 0;
    int        rounds;

    if (pa == NULL || pb == NULL) {
        return 1;
    }
    t0 = clock();
    a  = build(pa, n);
    row("build", n, seconds_since(t0), 1);
    b = build(pb, n);
    if (a == NULL || b == NULL) {
        return 1;
    }

    rounds = (n < 10000) ? 100 : 10;
    t0     = clock();
    while (rounds-- > 0) {
        for (i = 0; i < n; ++i) {
            sum += cf_get_int(a, pa[i].key, 0);
        }
    }
    row("cf_get_int", n, seconds_since(t0) / ((n < 10000) ? 100.0 : 10.0), n);

    t0 = clock();
    ra = report(a, &la);
    row("report", n, seconds_since(t0), 1);
    rb = report(b, &lb);

    if (ra != NULL && rb != NULL) {
        cf_doctor* d   = cf_doctor_new();
        cf_error   err = CF_ERROR_INIT;
        t0             = clock();
        if (d == NULL || !cf_doctor_diff_report(d, ra, la, "a", rb, lb, "b", &err)) {
            fprintf(stderr, "bench: diff failed: %s\n", err.message);
        } else {
            row("doctor diff", n, seconds_since(t0), 1);
        }
        cf_doctor_free(d);
    }
    printf("  (checksum %ld, report %lu bytes)\n", (long)(sum % 1000),
           (unsigned long)la);
    free(ra);
    free(rb);
    cf_config_free(a);
    cf_config_free(b);
    free(pa);
    free(pb);
    free(sa);
    free(sb);
    return 0;
}

int main(int argc, char** argv) {
    static const size_t dflt[] = {100, 1000, 10000, 20000};
    int                 i;

    printf("%-14s %7s  %13s  %15s  %14s\n", "operation", "keys", "time", "per op",
           "per key");
    if (argc < 2) {
        for (i = 0; i < (int)(sizeof dflt / sizeof dflt[0]); ++i) {
            if (run(dflt[i]) != 0) {
                return 1;
            }
        }
        return 0;
    }
    for (i = 1; i < argc; ++i) {
        if (run((size_t)strtoul(argv[i], NULL, 10)) != 0) {
            return 1;
        }
    }
    return 0;
}
