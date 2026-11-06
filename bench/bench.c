/*
 * bench.c - what the library's common work costs, as a function of how
 * much configuration there is. Not a test and not a gate: numbers to
 * start an optimisation from, so that one starts from a measurement.
 *
 *     ./cf_bench [N ...]        default: 100 1000 10000 20000
 *     ./cf_bench files|reload|define|emit [N ...]
 *                               the sizing cases of D-76 (below), the
 *                               median of nine runs, default 10 to 100000
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

/* ------------------------------------------------------------------ *
 * Sizes and work predicted (D-76): each case is timed REPS times, the
 * median taken, and an operation too short to time alone is repeated
 * until the run is long enough to read.
 * ------------------------------------------------------------------ */

#define REPS 9

static int cmp_double(const void* a, const void* b) {
    double x = *(const double*)a, y = *(const double*)b;
    return (x < y) ? -1 : (x > y) ? 1 : 0;
}

/* The fastest run of the last median() call: on a shared machine the
   median wanders by tens of percent where the minimum holds. */
static double fastest;

static double median(double* v, int n) {
    qsort(v, (size_t)n, sizeof *v, cmp_double);
    fastest = v[0];
    return v[n / 2];
}

/* A file of `n` keys in sections of 64, each value `width` bytes or more. */
static int write_file(const char* path, size_t n, size_t width) {
    FILE*  f = fopen(path, "wb");
    size_t i;

    if (f == NULL) {
        return 0;
    }
    if (strstr(path, ".json") != NULL) {
        /* minified: all on one line, so no line to count */
        fputs("{\"s\":{", f);
        for (i = 0; i < n; ++i) {
            fprintf(f, "%s\"key%lu\":\"value%0*lu\"", i == 0 ? "" : ",",
                    (unsigned long)i, (int)width, (unsigned long)i);
        }
        fputs("}}", f);
        return fclose(f) == 0;
    }
    for (i = 0; i < n; ++i) {
        if (i % 64 == 0) {
            fprintf(f, "[section%lu]\n", (unsigned long)(i / 64));
        }
        fprintf(f, "key%lu = value%0*lu\n", (unsigned long)i, (int)width,
                (unsigned long)i);
    }
    return fclose(f) == 0;
}

/* malloc, counting what it was asked for: the memory a load takes from
   the system, abandoned blocks and all (not the arena's own accounting). */
static size_t asked;

static void* counted_alloc(void* ctx, size_t n) {
    (void)ctx;
    asked += n;
    return malloc(n);
}

static void counted_release(void* ctx, void* p) {
    (void)ctx;
    free(p);
}

static const cf_allocator counting = {counted_alloc, counted_release, NULL};

static cf_config* from_file(const char* path, cf_error* err) {
    cf_builder* b = cf_build(&counting);

    asked = 0;
    cf_builder_file(b, "f", CF_RANK_USER_FILE, path, "ini", 0);
    return cf_builder_create(b, err);
}

static cf_builder* live_builder(void* path) {
    cf_builder* b = cf_build(NULL);
    cf_builder_file(b, "f", CF_RANK_USER_FILE, (const char*)path, NULL, 0);
    return b;
}

/* ms of one operation: the median over REPS runs of `inner` repeats. */
static double time_file(const char* path, int inner, size_t* keys) {
    double v[REPS];
    int    r, k;

    for (r = 0; r < REPS; ++r) {
        clock_t t0 = clock();
        for (k = 0; k < inner; ++k) {
            cf_error   err = CF_ERROR_INIT;
            cf_config* c   = from_file(path, &err);
            if (c == NULL) {
                fprintf(stderr, "bench: %s\n", err.message);
                return -1.0;
            }
            *keys = cf_count(c);
            cf_config_free(c);
        }
        v[r] = seconds_since(t0) * 1e3 / (double)inner;
    }
    return median(v, REPS);
}

static int run_files(size_t n, size_t width) {
    const char* path = "cf_bench_file.ini";
    long        bytes;
    size_t      keys = 0;
    double      ms;
    FILE*       f;

    if (!write_file(path, n, width)) {
        return 1;
    }
    f = fopen(path, "rb");
    if (f == NULL) {
        return 1;
    }
    fseek(f, 0, SEEK_END);
    bytes = ftell(f);
    fclose(f);
    ms = time_file(path, (n < 1000) ? 200 : (n < 20000) ? 10 : 2, &keys);
    remove(path);
    if (ms < 0.0) {
        return 1;
    }
    printf(
        "%-14s %7lu  %10.3f ms (min %9.3f)  %9lu bytes  %7lu keys read  %9lu asked\n",
        "file load", (unsigned long)n, ms, fastest, (unsigned long)bytes,
        (unsigned long)keys, (unsigned long)asked);
    return 0;
}

static int run_reload(size_t n, char* path, const char* label) {
    cf_error err = CF_ERROR_INIT;
    cf_live* l;
    double   v[REPS];
    int      r, k, inner = (n < 1000) ? 100 : (n < 20000) ? 10 : 2;

    if (!write_file(path, n, 0)) {
        return 1;
    }
    l = cf_live_open(CF_LIVE_MANUAL, live_builder, NULL, path, NULL, &err);
    if (l == NULL) {
        fprintf(stderr, "bench: %s\n", err.message);
        remove(path);
        return 1;
    }
    for (r = 0; r < REPS; ++r) {
        clock_t t0 = clock();
        for (k = 0; k < inner; ++k) {
            if (cf_live_reload(l, &err) == CF_LIVE_FAILED) {
                fprintf(stderr, "bench: reload: %s\n", err.message);
                cf_live_close(l);
                remove(path);
                return 1;
            }
        }
        v[r] = seconds_since(t0) * 1e3 / (double)inner;
    }
    {
        double m = median(v, REPS);
        printf("%-14s %7lu  %10.3f ms (min %9.3f)\n", label, (unsigned long)n, m,
               fastest);
    }
    cf_live_close(l);
    remove(path);
    return 0;
}

/* N definitions and nothing else: what the definitions' set costs. */
static int run_defs(size_t n) {
    char*  names = (char*)malloc(n * 24);
    double v[REPS];
    int    r, inner = (n < 1000) ? 50 : (n < 20000) ? 5 : 1;
    size_t i;

    if (names == NULL) {
        return 1;
    }
    for (i = 0; i < n; ++i) {
        sprintf(names + i * 24, "section%lu.key%lu", (unsigned long)(i / 64),
                (unsigned long)i);
    }
    for (r = 0; r < REPS; ++r) {
        clock_t t0 = clock();
        int     k;
        for (k = 0; k < inner; ++k) {
            cf_builder* b   = cf_build(NULL);
            cf_error    err = CF_ERROR_INIT;
            cf_config*  c;
            for (i = 0; i < n; ++i) {
                cf_builder_int(b, names + i * 24, 0, 0, 0, 0);
            }
            c = cf_builder_create(b, &err);
            if (c == NULL) {
                fprintf(stderr, "bench: defs: %s\n", err.message);
                free(names);
                return 1;
            }
            cf_config_free(c);
        }
        v[r] = seconds_since(t0) * 1e3 / (double)inner;
    }
    {
        double m = median(v, REPS);
        printf("%-14s %7lu  %10.3f ms (min %9.3f)\n", "define", (unsigned long)n, m,
               fastest);
    }
    free(names);
    return 0;
}

/* A configuration of N keys written out as INI: the measuring call and
   the writing call apart, then the two together (how a caller sizes a
   buffer today). */
static int run_emit(size_t n) {
    char*      sa = NULL;
    cf_pair*   pa = make_pairs(n, 0, &sa);
    cf_config* c;
    char*      buf;
    size_t     need;
    double     vm[REPS], vw[REPS];
    int        r, k, inner = (n < 1000) ? 100 : (n < 20000) ? 10 : 2;

    if (pa == NULL) {
        return 1;
    }
    c = build(pa, n);
    if (c == NULL) {
        return 1;
    }
    need = cf_emit(c, "ini", CF_EMIT_ALL, 0, NULL, 0, NULL);
    buf  = (char*)malloc(need + 1);
    if (buf == NULL || need == 0) {
        return 1;
    }
    for (r = 0; r < REPS; ++r) {
        clock_t t0 = clock();
        for (k = 0; k < inner; ++k) {
            need = cf_emit(c, "ini", CF_EMIT_ALL, 0, NULL, 0, NULL);
        }
        vm[r] = seconds_since(t0) * 1e3 / (double)inner;
        t0    = clock();
        for (k = 0; k < inner; ++k) {
            need = cf_emit(c, "ini", CF_EMIT_ALL, 0, buf, need + 1, NULL);
        }
        vw[r] = seconds_since(t0) * 1e3 / (double)inner;
    }
    printf("%-14s %7lu  measure %9.3f ms  write %9.3f ms  %lu bytes\n", "emit ini",
           (unsigned long)n, median(vm, REPS), median(vw, REPS), (unsigned long)need);
    free(buf);
    cf_config_free(c);
    free(pa);
    free(sa);
    return 0;
}

static int run_one(const char* what, size_t n) {
    int bad;

    if (strcmp(what, "files") == 0) {
        bad = run_files(n, 0);
        /* long values: about 10 MB at 100,000 keys */
        if (!bad && n == 100000) {
            bad = run_files(n, 90);
        }
        return bad;
    }
    if (strcmp(what, "reload") == 0) {
        static char ini[]  = "cf_bench_reload.ini";
        static char json[] = "cf_bench_reload.json";
        return run_reload(n, ini, "reload ini") || run_reload(n, json, "reload 1-line");
    }
    if (strcmp(what, "define") == 0) {
        return run_defs(n);
    }
    return run_emit(n);
}

static int run_new(const char* what, int argc, char** argv) {
    static const size_t keys[] = {10, 100, 1000, 10000, 100000};
    size_t              i;
    int                 j;

    if (argc > 2) {
        for (j = 2; j < argc; ++j) {
            if (run_one(what, (size_t)strtoul(argv[j], NULL, 10)) != 0) {
                return 1;
            }
        }
        return 0;
    }
    for (i = 0; i < sizeof keys / sizeof keys[0]; ++i) {
        if (run_one(what, keys[i]) != 0) {
            return 1;
        }
    }
    return 0;
}

int main(int argc, char** argv) {
    static const size_t dflt[] = {100, 1000, 10000, 20000};
    int                 i;

    if (argc >= 2 && (strcmp(argv[1], "files") == 0 || strcmp(argv[1], "reload") == 0 ||
                      strcmp(argv[1], "define") == 0 || strcmp(argv[1], "emit") == 0)) {
        return run_new(argv[1], argc, argv);
    }
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
