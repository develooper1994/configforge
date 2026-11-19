/*
 * bench.c - what the library's common work costs, as a function of how
 * much configuration there is. Not a test and not a gate: numbers to
 * start an optimisation from, so that one starts from a measurement.
 *
 *     ./cf_bench [N ...]        default: 100 1000 10000 20000
 *     ./cf_bench files|reload|define|emit [N ...]
 *                               the sizing cases of D-76 (below), the
 *                               median of nine runs, default 10 to 100000
 *     ./cf_bench docs [--large] [format ...]
 *                               large documents (P-1, below): MiB/s per
 *                               format at 1 and 16 MiB, and 256 with --large
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

/* ------------------------------------------------------------------ *
 * Large documents (P-1): a format library is also handed megabytes.
 *
 * The documents are made here, the same ones every run (a fixed seed),
 * so none is committed: records of long strings, strings with escapes,
 * non-ASCII UTF-8, many small numbers and, in some, deep nesting, until
 * the document is the size asked. Each is read through the codec API,
 * the way a program would (cf_codec_decode), into a sink that only
 * counts; the median of the runs is MiB of document per second.
 * ------------------------------------------------------------------ */

typedef struct doc {
    char*  p;
    size_t n;
    size_t cap;
    int    over;
} doc;

static void put(doc* d, const char* s, size_t n) {
    if (d->n + n > d->cap) {
        d->over = 1;
        return;
    }
    memcpy(d->p + d->n, s, n);
    d->n += n;
}

static void puts_(doc* d, const char* s) {
    put(d, s, strlen(s));
}

static void putnum(doc* d, long v) {
    char t[32];
    sprintf(t, "%ld", v);
    puts_(d, t);
}

/* One record's shape, from its own seed: the same for every format. */
#define NNUM_MAX 32

typedef struct rec {
    unsigned long seed;
    size_t        text;   /* bytes of the plain string */
    int           nonasc; /* the "utf" string is non-ASCII */
    int           nnum;
    long          num[NNUM_MAX];
    int           deep; /* levels of nesting, 0 for none */
} rec;

static unsigned long next(unsigned long* s) {
    *s = (*s * 1103515245ul + 12345ul) & 0x7fffffffUL;
    return *s >> 8;
}

static void make_rec(rec* r, unsigned long i) {
    unsigned long s = i * 2654435761ul + 1ul;
    int           k;

    s &= 0x7fffffffUL;
    r->seed   = s;
    r->text   = (i % 64 == 63) ? 16384 + next(&s) % 49152 : 40 + next(&s) % 200;
    r->nonasc = (i % 4 == 0);
    r->nnum   = 8 + (int)(next(&s) % (NNUM_MAX - 8));
    for (k = 0; k < r->nnum; ++k) {
        long v    = (long)(next(&s) % 10000);
        r->num[k] = (next(&s) % 5 == 0) ? -v : v;
    }
    r->deep = (i % 16 == 15) ? 8 + (int)(next(&s) % 16) : 0;
}

static void plain(doc* d, size_t len, unsigned long seed) {
    static const char abc[] = "abcdefghijklmnopqrstuvwxyz      ";
    size_t            i;

    for (i = 0; i < len; ++i) {
        char c = abc[next(&seed) % (sizeof abc - 1)];
        put(d, &c, 1);
    }
}

/* "h" e-acute "llo w" o-umlaut "rld ", three Japanese characters, a check mark and
   an emoji: two, three and four-byte sequences. */
static const char phrase[] =
    "h\xc3\xa9llo w\xc3\xb6rld \xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e \xe2\x9c\x93 "
    "\xf0\x9f\x98\x80 ";

static void utf(doc* d, size_t len, int nonascii, unsigned long seed) {
    if (!nonascii) {
        plain(d, len, seed);
        return;
    }
    while (len > 0) {
        size_t k = sizeof phrase - 1;
        put(d, phrase, k);
        len = (len > k) ? len - k : 0;
    }
}

/* A string with escapes, the syntax of JSON, TOML and YAML's double
   quotes alike: a share of its bytes are escaped. */
static void escaped(doc* d, unsigned long seed, unsigned long kinds) {
    /* the last is JSON's and YAML's, and not TOML's: `kinds` leaves it out */
    static const char* const part[] = {"\\n", "\\t", "\\\"", "\\\\", "\\u00e9", "\\/"};
    int                      k;

    for (k = 0; k < 12; ++k) {
        size_t n = 6 + next(&seed) % 10;
        plain(d, n, seed + (unsigned long)k);
        puts_(d, part[next(&seed) % kinds]);
    }
}

/* ---- JSON: half the records minified, half indented. ---- */

static void json_rec(doc* d, unsigned long i, const rec* r) {
    int pretty = (int)(i & 1u);
    int k;

    puts_(d, (i == 0) ? "" : ",");
    puts_(d, pretty ? "\n  \"r" : "\"r");
    putnum(d, (long)i);
    puts_(d, pretty ? "\": {\n    \"text\": \"" : "\":{\"text\":\"");
    plain(d, r->text, r->seed);
    puts_(d, pretty ? "\",\n    \"esc\": \"" : "\",\"esc\":\"");
    escaped(d, r->seed, 6);
    puts_(d, pretty ? "\",\n    \"utf\": \"" : "\",\"utf\":\"");
    utf(d, 200, r->nonasc, r->seed);
    puts_(d, pretty ? "\",\n    \"nums\": [" : "\",\"nums\":[");
    for (k = 0; k < r->nnum; ++k) {
        puts_(d, (k == 0) ? "" : (pretty ? ", " : ","));
        putnum(d, r->num[k]);
    }
    puts_(d, "]");
    if (r->deep > 0) {
        puts_(d, pretty ? ",\n    \"deep\": " : ",\"deep\":");
        for (k = 0; k < r->deep; ++k) {
            puts_(d, "{\"d\":");
        }
        puts_(d, "1");
        for (k = 0; k < r->deep; ++k) {
            puts_(d, "}");
        }
    }
    puts_(d, pretty ? "\n  }" : "}");
}

/* ---- TOML: a table to a record, the deep ones by dotted headers. ---- */

static void toml_rec(doc* d, unsigned long i, const rec* r) {
    int k;

    puts_(d, "[r");
    putnum(d, (long)i);
    puts_(d, "]\ntext = \"");
    plain(d, r->text, r->seed);
    puts_(d, "\"\nesc = \"");
    escaped(d, r->seed, 5);
    puts_(d, "\"\nutf = \"");
    utf(d, 200, r->nonasc, r->seed);
    puts_(d, "\"\nnums = [");
    for (k = 0; k < r->nnum; ++k) {
        puts_(d, (k == 0) ? "" : ", ");
        putnum(d, r->num[k]);
    }
    puts_(d, "]\n");
    if (r->deep > 0) {
        puts_(d, "[r");
        putnum(d, (long)i);
        for (k = 0; k < r->deep; ++k) {
            puts_(d, ".d");
        }
        puts_(d, "]\nv = 1\n");
    }
}

/* ---- YAML: block style; the strings plain or double-quoted by turns. ---- */

static void indent(doc* d, int levels) {
    int k;
    for (k = 0; k < levels; ++k) {
        puts_(d, "  ");
    }
}

static void yaml_rec(doc* d, unsigned long i, const rec* r) {
    const char* q = (i & 1u) ? "\"" : "";
    int         k;

    puts_(d, "r");
    putnum(d, (long)i);
    puts_(d, ":\n  text: ");
    puts_(d, q);
    plain(d, r->text, r->seed);
    puts_(d, q);
    puts_(d, "\n  esc: \"");
    escaped(d, r->seed, 6);
    puts_(d, "\"\n  utf: ");
    puts_(d, q);
    utf(d, 200, r->nonasc, r->seed);
    puts_(d, q);
    puts_(d, (i & 2u) ? "\n  nums: [" : "\n  nums:\n");
    for (k = 0; k < r->nnum; ++k) {
        if (i & 2u) {
            puts_(d, (k == 0) ? "" : ", ");
            putnum(d, r->num[k]);
        } else {
            puts_(d, "    - ");
            putnum(d, r->num[k]);
            puts_(d, "\n");
        }
    }
    puts_(d, (i & 2u) ? "]\n" : "");
    if (r->deep > 0) {
        puts_(d, "  deep:\n");
        for (k = 0; k < r->deep; ++k) {
            indent(d, 2 + k);
            puts_(d, "d:\n");
        }
        indent(d, 2 + r->deep);
        puts_(d, "v: 1\n");
    }
}

/* ---- INI: a section to a record; a number to a line. ---- */

static void ini_rec(doc* d, unsigned long i, const rec* r) {
    int k;

    puts_(d, "[r");
    putnum(d, (long)i);
    puts_(d, "]\ntext = ");
    plain(d, r->text, r->seed);
    puts_(d, "\nesc = a=b, c:d (e) [f] 'g' - h/i ");
    plain(d, 150, r->seed);
    puts_(d, "\nutf = ");
    utf(d, 200, r->nonasc, r->seed);
    puts_(d, "\n");
    for (k = 0; k < r->nnum; ++k) {
        puts_(d, "n");
        putnum(d, (long)k);
        puts_(d, " = ");
        putnum(d, r->num[k]);
        puts_(d, "\n");
    }
    if (r->deep > 0) {
        puts_(d, "[r");
        putnum(d, (long)i);
        for (k = 0; k < r->deep; ++k) {
            puts_(d, ".d");
        }
        puts_(d, "]\nv = 1\n");
    }
}

/* ---- CBOR: a definite map of records; its head is written last, into
   the room left before the first byte. ---- */

#define CBOR_HEAD 9

static void cbor_head(doc* d, int major, unsigned long n) {
    unsigned char h[5];
    size_t        len;

    h[0] = (unsigned char)(major << 5);
    if (n < 24) {
        h[0] = (unsigned char)(h[0] | n);
        len  = 1;
    } else if (n < 256) {
        h[0] = (unsigned char)(h[0] | 24);
        h[1] = (unsigned char)n;
        len  = 2;
    } else if (n < 65536ul) {
        h[0] = (unsigned char)(h[0] | 25);
        h[1] = (unsigned char)(n >> 8);
        h[2] = (unsigned char)n;
        len  = 3;
    } else {
        h[0] = (unsigned char)(h[0] | 26);
        h[1] = (unsigned char)(n >> 24);
        h[2] = (unsigned char)(n >> 16);
        h[3] = (unsigned char)(n >> 8);
        h[4] = (unsigned char)n;
        len  = 5;
    }
    put(d, (const char*)h, len);
}

static void cbor_text(doc* d, const char* s) {
    cbor_head(d, 3, (unsigned long)strlen(s));
    puts_(d, s);
}

/* A string of known length from a generator: written, then its head put
   before it by moving it - strings are at most 64 KiB, so cheap. */
static void cbor_gen_text(doc* d, size_t len, int kind, int nonascii,
                          unsigned long seed) {
    size_t        at = d->n;
    unsigned char h[3];

    h[0] = 0x79; /* major 3, two length bytes */
    put(d, (const char*)h, 3);
    if (kind == 0) {
        plain(d, len, seed);
    } else if (kind == 1) {
        /* the bytes JSON would escape, raw */
        int k;
        for (k = 0; k < 12; ++k) {
            static const char* const part[] = {"\n", "\t", "\"", "\\", "\xc3\xa9", "/"};
            size_t                   n      = 6 + next(&seed) % 10;
            plain(d, n, seed + (unsigned long)k);
            puts_(d, part[next(&seed) % 6]);
        }
    } else {
        utf(d, len, nonascii, seed);
    }
    if (!d->over) {
        size_t n     = d->n - at - 3;
        d->p[at + 1] = (char)(n >> 8);
        d->p[at + 2] = (char)n;
    }
}

static void cbor_rec(doc* d, unsigned long i, const rec* r) {
    char key[16];
    int  k;

    sprintf(key, "r%lu", i);
    cbor_text(d, key);
    cbor_head(d, 5, (unsigned long)(r->deep > 0 ? 5 : 4));
    cbor_text(d, "text");
    cbor_gen_text(d, r->text, 0, 0, r->seed);
    cbor_text(d, "esc");
    cbor_gen_text(d, 0, 1, 0, r->seed);
    cbor_text(d, "utf");
    cbor_gen_text(d, 200, 2, r->nonasc, r->seed);
    cbor_text(d, "nums");
    cbor_head(d, 4, (unsigned long)r->nnum);
    for (k = 0; k < r->nnum; ++k) {
        if (r->num[k] >= 0) {
            cbor_head(d, 0, (unsigned long)r->num[k]);
        } else {
            cbor_head(d, 1, (unsigned long)(-1 - r->num[k]));
        }
    }
    if (r->deep > 0) {
        cbor_text(d, "deep");
        for (k = 0; k < r->deep; ++k) {
            cbor_head(d, 5, 1);
            cbor_text(d, "d");
        }
        cbor_head(d, 0, 1);
    }
}

typedef void (*rec_fn)(doc*, unsigned long, const rec*);

/* A document of at least `want` bytes in `name`'s syntax; `at` is where
   its first byte is (CBOR's head goes before it). NULL when out of
   memory. */
static char* make_doc(const char* name, size_t want, size_t* start, size_t* len) {
    static const struct {
        const char* name;
        rec_fn      fn;
    } fmt[]          = {{"json", json_rec},
                        {"toml", toml_rec},
                        {"yaml", yaml_rec},
                        {"ini", ini_rec},
                        {"cbor", cbor_rec}};
    rec_fn        fn = NULL;
    doc           d;
    unsigned long i = 0;
    size_t        k;

    for (k = 0; k < sizeof fmt / sizeof fmt[0]; ++k) {
        if (strcmp(fmt[k].name, name) == 0) {
            fn = fmt[k].fn;
        }
    }
    d.cap  = want + (1u << 20) + CBOR_HEAD;
    d.p    = (char*)malloc(d.cap);
    d.n    = 0;
    d.over = 0;
    if (d.p == NULL || fn == NULL) {
        free(d.p);
        return NULL;
    }
    if (strcmp(name, "cbor") == 0) {
        d.n = CBOR_HEAD;
    }
    if (strcmp(name, "json") == 0) {
        puts_(&d, "{");
    }
    while (d.n < want) {
        rec r;
        make_rec(&r, i);
        fn(&d, i, &r);
        ++i;
    }
    if (strcmp(name, "json") == 0) {
        puts_(&d, "\n}\n");
    }
    *start = 0;
    if (strcmp(name, "cbor") == 0) {
        /* the head of a map of i pairs, ending where the records begin */
        doc  h;
        char tmp[CBOR_HEAD];
        h.p    = tmp;
        h.n    = 0;
        h.cap  = sizeof tmp;
        h.over = 0;
        cbor_head(&h, 5, i);
        memcpy(d.p + CBOR_HEAD - h.n, tmp, h.n);
        *start = CBOR_HEAD - h.n;
    }
    if (d.over) {
        free(d.p);
        return NULL;
    }
    *len = d.n - *start;
    return d.p;
}

/* A sink that counts: the values, and the bytes of text. */
typedef struct tally {
    unsigned long values;
    unsigned long text;
    char*         room; /* one reader's text, reused */
    size_t        cap;
    void*         work; /* what a reader asked for, freed together */
} tally;

typedef union tally_align {
    double d;
    long   l;
    void*  p;
} tally_align;

static cf_bool t_open(void* c, const char* name, size_t n) {
    (void)c;
    (void)name;
    (void)n;
    return CF_TRUE;
}

static cf_bool t_scalar(void* c, const char* name, size_t n, const cf_value* v,
                        cf_bool textual, const cf_loc* at) {
    tally* t = (tally*)c;
    (void)name;
    (void)n;
    (void)at;
    (void)textual;
    ++t->values;
    if (v->type == CF_TYPE_TEXT) {
        t->text += (unsigned long)v->u.t.n;
    }
    return CF_TRUE;
}

static cf_bool t_end(void* c) {
    (void)c;
    return CF_TRUE;
}

static char* t_text(void* c, size_t n) {
    tally* t = (tally*)c;
    if (n > t->cap) {
        free(t->room);
        t->room = (char*)malloc(n);
        t->cap  = (t->room != NULL) ? n : 0;
    }
    return t->room;
}

static void* t_work(void* c, size_t n) {
    tally*       t = (tally*)c;
    tally_align* b = (tally_align*)malloc(sizeof(tally_align) * 2 + n);
    if (b == NULL) {
        return NULL;
    }
    ((void**)b)[0] = t->work;
    t->work        = b;
    return (char*)b + sizeof(tally_align) * 2;
}

static void t_done(tally* t) {
    while (t->work != NULL) {
        void* next_ = ((void**)t->work)[0];
        free(t->work);
        t->work = next_;
    }
}

static int run_doc(const char* name, size_t mib) {
    const cf_codec* c     = cf_codec_by_name(name);
    size_t          start = 0, len = 0;
    char*           bytes;
    double          v[REPS];
    int             r, k, inner = (mib < 16) ? 16 : 1;
    int             reps = (mib < 16) ? REPS : (mib < 256) ? 5 : 3;
    tally           t;

    if (c == NULL || !cf_codec_can_read(c)) {
        printf("%-5s %4lu MiB   (not built in)\n", name, (unsigned long)mib);
        return 0;
    }
    bytes = make_doc(name, mib << 20, &start, &len);
    if (bytes == NULL) {
        fprintf(stderr, "bench: cannot make a %lu MiB %s document\n",
                (unsigned long)mib, name);
        return 1;
    }
    memset(&t, 0, sizeof t);
    for (r = 0; r < reps; ++r) {
        cf_blob     blob;
        cf_doc_sink sink;
        cf_error    err = CF_ERROR_INIT;
        clock_t     t0;
        cf_bool     ok;

        cf_blob_mem(&blob, bytes + start, len, name);
        memset(&sink, 0, sizeof sink);
        sink.size         = sizeof sink;
        sink.begin_table  = t_open;
        sink.begin_array  = t_open;
        sink.scalar       = t_scalar;
        sink.end          = t_end;
        sink.ctx          = &t;
        sink.reserve_text = t_text;
        sink.reserve_work = t_work;
        t.values          = 0;
        t.text            = 0;
        ok                = CF_TRUE;
        t0                = clock();
        for (k = 0; k < inner && ok; ++k) {
            t.values = 0;
            t.text   = 0;
            ok       = cf_codec_decode(c, &blob, &sink, &err);
            t_done(&t);
        }
        v[r] = seconds_since(t0) / (double)inner;
        if (!ok) {
            fprintf(stderr, "bench: %s %lu MiB: %s\n", name, (unsigned long)mib,
                    err.message);
            printf("%-5s %4lu MiB   refused (%s)\n", name, (unsigned long)mib,
                   err.message);
            free(t.room);
            free(bytes);
            return 0;
        }
    }
    {
        double m  = median(v, reps);
        double mb = (double)len / 1048576.0;
        printf("%-5s %4lu MiB %9.1f MiB/s  (best %9.1f)  %lu values, %lu text bytes\n",
               name, (unsigned long)mib, mb / m, mb / fastest, t.values, t.text);
    }
    free(t.room);
    free(bytes);
    return 0;
}

/* `docs [--large] [format ...]`: every format at 1 and 16 MiB, and at
   256 MiB with --large (CI's run stays short without it). */
static int run_docs(int argc, char** argv) {
    static const char* const all[] = {"json", "yaml", "toml", "cbor", "ini"};
    const char*              want[8];
    size_t                   nwant   = 0, i, s;
    int                      large   = 0, j;
    static const size_t      sizes[] = {1, 16, 256};

    for (j = 2; j < argc; ++j) {
        if (strcmp(argv[j], "--large") == 0) {
            large = 1;
        } else if (nwant < 8) {
            want[nwant++] = argv[j];
        }
    }
    if (nwant == 0) {
        for (i = 0; i < sizeof all / sizeof all[0]; ++i) {
            want[nwant++] = all[i];
        }
    }
    printf("%-5s %8s %13s\n", "format", "size", "speed");
    for (s = 0; s < (large ? 3u : 2u); ++s) {
        for (i = 0; i < nwant; ++i) {
            if (run_doc(want[i], sizes[s]) != 0) {
                return 1;
            }
        }
    }
    return 0;
}

int main(int argc, char** argv) {
    static const size_t dflt[] = {100, 1000, 10000, 20000};
    int                 i;

    if (argc >= 2 && strcmp(argv[1], "docs") == 0) {
        return run_docs(argc, argv);
    }
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
