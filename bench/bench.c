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
 *     ./cf_bench allocation     builder loads of mixed and many-tiny documents
 *     ./cf_bench --emit-docs <dir> [--size 1|16|256] [--only <format>]
 *                               the documents as files, for bench/compare/
 *                               (P-1b), and a line each: name, bytes, FNV-1a
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
static size_t calls;

static void* counted_alloc(void* ctx, size_t n) {
    (void)ctx;
    asked += n;
    ++calls;
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

/* ------------------------------------------------------------------ *
 * The documents as files (P-1b): `cf_bench --emit-docs <dir>`.
 *
 * The comparison (bench/compare/) reads these files and generates
 * nothing itself, so every library, and every compiler, is given the same
 * bytes. A document is one logical content written by a small writer in
 * each format's syntax: the records are chosen to hurt a reader (long
 * strings, every byte an escape, 31 levels of nesting, 100,000 unique
 * keys, a late error), the same for every format, from one seed.
 * ------------------------------------------------------------------ */

/* 64 bits in two 32-bit halves held in unsigned long: C89 has no
   64-bit type, and the seed must give the same bytes on every compiler. */
typedef struct u64 {
    unsigned long hi, lo;
} u64;

#define M32 0xFFFFFFFFul

static u64 u64_of(unsigned long hi, unsigned long lo) {
    u64 r;
    r.hi = hi & M32;
    r.lo = lo & M32;
    return r;
}

static u64 u64_xor(u64 a, u64 b) {
    return u64_of(a.hi ^ b.hi, a.lo ^ b.lo);
}

static u64 u64_shr(u64 a, int n) {
    return u64_of(a.hi >> n, (a.lo >> n) | (a.hi << (32 - n)));
}

static u64 u64_shl(u64 a, int n) {
    return u64_of((a.hi << n) | (a.lo >> (32 - n)), a.lo << n);
}

/* The 64-bit product of two 32-bit values, as hi and lo. */
static void mul32(unsigned long a, unsigned long b, unsigned long* hi,
                  unsigned long* lo) {
    unsigned long a0 = a & 0xFFFFul, a1 = (a >> 16) & 0xFFFFul;
    unsigned long b0 = b & 0xFFFFul, b1 = (b >> 16) & 0xFFFFul;
    unsigned long p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
    unsigned long mid = (p00 >> 16) + (p01 & 0xFFFFul) + (p10 & 0xFFFFul);

    *lo = (((mid & 0xFFFFul) << 16) | (p00 & 0xFFFFul)) & M32;
    *hi = (p11 + (p01 >> 16) + (p10 >> 16) + (mid >> 16)) & M32;
}

/* The low 64 bits of a * b. */
static u64 u64_mul(u64 a, u64 b) {
    unsigned long hi, lo;

    mul32(a.lo, b.lo, &hi, &lo);
    hi = (hi + ((a.hi * b.lo) & M32) + ((a.lo * b.hi) & M32)) & M32;
    return u64_of(hi, lo);
}

/* xorshift64*: the next value of the state, and its high 32 bits. */
static unsigned long xs_next(u64* s) {
    u64 x = *s;
    u64 r;

    x  = u64_xor(x, u64_shr(x, 12));
    x  = u64_xor(x, u64_shl(x, 25));
    x  = u64_xor(x, u64_shr(x, 27));
    *s = x;
    r  = u64_mul(x, u64_of(0x2545F491ul, 0x4F6CDD1Dul));
    return r.hi;
}

/* One constant for the seed, run through xorshift64* and mixed with the
   document's number, so that a document does not change when another is
   added. */
#define SEED_HI 0x9E3779B9ul
#define SEED_LO 0x7F4A7C15ul

static u64 doc_seed(int which) {
    u64 s = u64_of(SEED_HI, SEED_LO);

    s = u64_xor(s, u64_mul(u64_of(0, (unsigned long)which + 1ul),
                           u64_of(0xD6E8FEB8ul, 0x6659FD93ul)));
    (void)xs_next(&s);
    (void)xs_next(&s);
    return s;
}

enum { F_JSON, F_YAML, F_TOML, F_CBOR, F_INI, F_XML, F_COUNT };

static const char* const fname[F_COUNT]       = {"json", "yaml", "toml",
                                                 "cbor", "ini",  "xml"};
static const char* const fext[F_COUNT]        = {".json", ".yaml", ".toml",
                                                 ".cbor", ".ini",  ".xml"};
static const char        fletter[F_COUNT + 1] = "JYTCIX";

enum {
    D_MIXED,
    D_STR_PLAIN,
    D_STR_ESC,
    D_STR_UTF8,
    D_NUM_INT,
    D_NUM_EDGE,
    D_NEST_DEEP,
    D_ARRAY_HUGE,
    D_TABLE_WIDE,
    D_KEYS_PREFIX,
    D_WHITESPACE,
    D_MANY_TINY,
    D_INVALID_LATE,
    D_RECORDS,
    D_ALIASES,
    D_COUNT
};

static const char* const dname[D_COUNT] = {
    "mixed",        "strings-plain", "strings-escape", "strings-utf8", "numbers-int",
    "numbers-edge", "nest-deep",     "array-huge",     "table-wide",   "keys-prefix",
    "whitespace",   "many-tiny",     "invalid-late",   "records",      "yaml-aliases"};

/* The formats a document exists in: the letters of the table in
   docs/plan/P-word-at-a-time.md. */
static const char* const dhas[D_COUNT] = {
    "JYTCIX", "JYTCIX", "JYTX",  "JYTCIX", "JYTC",   "JYTC",  "JYTCX", "JYTCIX",
    "JYTCIX", "JYTCI",  "JYTIX", "JYTCIX", "JYTCIX", "JYTCI", "Y"};

static int doc_exists(int format, int docn) {
    return strchr(dhas[docn], fletter[format]) != NULL;
}

#define WR_MAXD 40

typedef struct wlvl {
    int           arr;
    unsigned long n;
    size_t        at;   /* CBOR: where the head is */
    size_t        plen; /* the dotted path's length outside this level */
    char          key[24];
} wlvl;

typedef struct wr {
    doc           d;
    int           fmt;
    int           depth; /* 0 is the root table */
    wlvl          st[WR_MAXD];
    char          path[512];
    size_t        plen;
    int           inarr;   /* TOML: inside an inline array */
    int           nl_each; /* a line to an element of an array */
    int           yplain;  /* YAML: simple strings plain */
    u64           rng;
    unsigned char bad; /* a writer misuse: the document is not made */
} wr;

static unsigned long rn(wr* w) {
    return xs_next(&w->rng);
}

static void wr_puts(wr* w, const char* s) {
    puts_(&w->d, s);
}

static void wr_ch(wr* w, char c) {
    put(&w->d, &c, 1);
}

static void wr_ws(wr* w, int n) {
    while (n-- > 0) {
        wr_ch(w, ' ');
    }
}

/* A CBOR head with a four-byte argument, whatever the number: the count
   of a container is patched in when it closes. */
static void wr_head4(wr* w, int major, unsigned long n) {
    unsigned char h[5];

    h[0] = (unsigned char)((major << 5) | 26);
    h[1] = (unsigned char)((n >> 24) & 0xFFu);
    h[2] = (unsigned char)((n >> 16) & 0xFFu);
    h[3] = (unsigned char)((n >> 8) & 0xFFu);
    h[4] = (unsigned char)(n & 0xFFu);
    put(&w->d, (const char*)h, 5);
}

/* A CBOR head at the shortest length. */
static void wr_head(wr* w, int major, unsigned long n) {
    cbor_head(&w->d, major, n);
}

static void wr_ctext(wr* w, const char* s, size_t n) {
    wr_head(w, 3, (unsigned long)n);
    put(&w->d, s, n);
}

static void wr_begin(wr* w, int fmt, size_t cap, int docn) {
    memset(w, 0, sizeof *w);
    w->d.p   = (char*)malloc(cap);
    w->d.cap = cap;
    w->fmt   = fmt;
    w->rng   = doc_seed(docn);
    if (w->d.p == NULL) {
        w->bad = 1;
        return;
    }
    switch (fmt) {
    case F_JSON:
        wr_ch(w, '{');
        break;
    case F_CBOR:
        w->st[0].at = w->d.n;
        wr_head4(w, 5, 0);
        break;
    case F_XML:
        wr_puts(w, "<r>");
        break;
    default:
        break;
    }
}

static void wr_close(wr* w);

static void wr_end(wr* w) {
    while (w->depth > 0) {
        wr_close(w);
    }
    switch (w->fmt) {
    case F_JSON:
        wr_puts(w, "\n}\n");
        break;
    case F_CBOR:
        if (!w->d.over) {
            size_t        at = w->st[0].at;
            unsigned long n  = w->st[0].n;
            w->d.p[at + 1]   = (char)((n >> 24) & 0xFFu);
            w->d.p[at + 2]   = (char)((n >> 16) & 0xFFu);
            w->d.p[at + 3]   = (char)((n >> 8) & 0xFFu);
            w->d.p[at + 4]   = (char)(n & 0xFFu);
        }
        break;
    case F_XML:
        wr_puts(w, "</r>\n");
        break;
    default:
        break;
    }
}

/* What comes before an item of the open container, `key` its name (NULL
   in an array): a comma, an indent, a dash, the key. */
static void wr_pre(wr* w, const char* key) {
    wlvl* l = &w->st[w->depth];

    switch (w->fmt) {
    case F_JSON:
        if (l->n > 0) {
            wr_ch(w, ',');
        }
        if (w->depth == 0 ||
            (l->arr && (w->nl_each || (l->n > 0 && (l->n & 63u) == 0)))) {
            wr_ch(w, '\n');
        }
        if (!l->arr) {
            wr_ch(w, '"');
            wr_puts(w, key);
            wr_puts(w, "\":");
        }
        break;
    case F_YAML:
        wr_ws(w, 2 * w->depth);
        if (l->arr) {
            wr_puts(w, "- ");
        } else {
            wr_puts(w, key);
            wr_puts(w, ": ");
        }
        break;
    case F_TOML:
        if (w->inarr > 0) {
            if (l->n > 0) {
                wr_puts(w, ((l->n & 63u) == 0 || w->nl_each) ? ",\n" : ", ");
            }
        } else {
            wr_puts(w, key);
            wr_puts(w, " = ");
        }
        break;
    case F_INI:
        wr_puts(w, l->arr ? l->key : key);
        wr_puts(w, l->arr ? "[] = " : " = ");
        break;
    case F_XML:
        wr_ch(w, '<');
        wr_puts(w, l->arr ? l->key : key);
        wr_ch(w, '>');
        break;
    case F_CBOR:
        if (!l->arr) {
            wr_ctext(w, key, strlen(key));
        }
        break;
    default:
        break;
    }
    ++l->n;
}

static void wr_post(wr* w, const char* key) {
    wlvl* l = &w->st[w->depth];

    switch (w->fmt) {
    case F_YAML:
    case F_INI:
        wr_ch(w, '\n');
        break;
    case F_TOML:
        if (w->inarr == 0) {
            wr_ch(w, '\n');
        }
        break;
    case F_XML:
        wr_puts(w, "</");
        wr_puts(w, l->arr ? l->key : key);
        wr_puts(w, ">\n");
        break;
    default:
        break;
    }
}

/* A number is told from the text it is written as. */
static int is_real_token(const char* s) {
    return strchr(s, '.') != NULL || strchr(s, 'e') != NULL || strchr(s, 'E') != NULL;
}

/* An integer token's magnitude into two halves, and its sign. */
static void parse_int(const char* s, int* neg, u64* mag) {
    u64 ten = u64_of(0, 10);
    u64 v   = u64_of(0, 0);

    *neg = (*s == '-');
    if (*s == '-') {
        ++s;
    }
    for (; *s >= '0' && *s <= '9'; ++s) {
        u64           m  = u64_mul(v, ten);
        unsigned long lo = (m.lo + (unsigned long)(*s - '0')) & M32;

        v.hi = (m.hi + ((lo < m.lo) ? 1ul : 0ul)) & M32;
        v.lo = lo;
    }
    *mag = v;
}

/* A 64-bit CBOR head: major, then the magnitude. */
static void wr_head64(wr* w, int major, u64 v) {
    unsigned char h[9];

    if (v.hi == 0) {
        wr_head(w, major, v.lo);
        return;
    }
    h[0] = (unsigned char)((major << 5) | 27);
    h[1] = (unsigned char)((v.hi >> 24) & 0xFFu);
    h[2] = (unsigned char)((v.hi >> 16) & 0xFFu);
    h[3] = (unsigned char)((v.hi >> 8) & 0xFFu);
    h[4] = (unsigned char)(v.hi & 0xFFu);
    h[5] = (unsigned char)((v.lo >> 24) & 0xFFu);
    h[6] = (unsigned char)((v.lo >> 16) & 0xFFu);
    h[7] = (unsigned char)((v.lo >> 8) & 0xFFu);
    h[8] = (unsigned char)(v.lo & 0xFFu);
    put(&w->d, (const char*)h, 9);
}

/* A string, its bytes as they are to be written in the format's own
   quoting (escapes already made), or raw. */
static int simple_word(const char* s, size_t n) {
    size_t i;
    if (n == 0 || !((s[0] >= 'a' && s[0] <= 'z') || (s[0] >= 'A' && s[0] <= 'Z'))) {
        return 0;
    }
    for (i = 0; i < n; ++i) {
        char c = s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9'))) {
            return 0;
        }
    }
    return 1;
}

static void wr_str(wr* w, const char* key, const char* s, size_t n) {
    wr_pre(w, key);
    switch (w->fmt) {
    case F_CBOR:
        wr_ctext(w, s, n);
        break;
    case F_INI:
    case F_XML:
        put(&w->d, s, n);
        break;
    case F_YAML:
        if (w->yplain && simple_word(s, n)) {
            put(&w->d, s, n);
            break;
        }
        wr_ch(w, '"');
        put(&w->d, s, n);
        wr_ch(w, '"');
        break;
    default:
        wr_ch(w, '"');
        put(&w->d, s, n);
        wr_ch(w, '"');
        break;
    }
    wr_post(w, key);
}

/* A number or a boolean, as the text of its token. */
static void wr_tok(wr* w, const char* key, const char* tok, int isbool) {
    wr_pre(w, key);
    if (w->fmt == F_CBOR) {
        if (isbool) {
            wr_ch(w, (char)((tok[0] == 't') ? 0xF5 : 0xF4));
        } else if (is_real_token(tok)) {
            double        v = strtod(tok, NULL);
            unsigned char b[9];
            unsigned char t[sizeof(double)];
            unsigned long one    = 1;
            int           little = (*(unsigned char*)&one == 1);
            size_t        i;
            memcpy(t, &v, sizeof t);
            b[0] = 0xFB;
            for (i = 0; i < 8; ++i) {
                b[1 + i] = little ? t[7 - i] : t[i];
            }
            put(&w->d, (const char*)b, 9);
        } else {
            int neg;
            u64 mag;
            parse_int(tok, &neg, &mag);
            if (neg) {
                /* -1 - n: the argument is magnitude - 1 */
                if (mag.lo == 0) {
                    mag.hi = (mag.hi - 1ul) & M32;
                    mag.lo = M32;
                } else {
                    mag.lo = (mag.lo - 1ul) & M32;
                }
                wr_head64(w, 1, mag);
            } else {
                wr_head64(w, 0, mag);
            }
        }
    } else {
        wr_puts(w, tok);
    }
    wr_post(w, key);
}

static void wr_int(wr* w, const char* key, long v) {
    char t[32];
    sprintf(t, "%ld", v);
    wr_tok(w, key, t, 0);
}

static void wr_bool(wr* w, const char* key, int v) {
    wr_tok(w, key, v ? "true" : "false", 1);
}

/* A table (arr 0) or an array (arr 1) opens; wr_close ends the innermost. */
static void wr_open(wr* w, const char* key, int arr) {
    wlvl*  l = &w->st[w->depth];
    wlvl*  c;
    size_t kn  = key != NULL ? strlen(key) : 0;
    size_t old = w->plen;

    if (w->depth + 1 >= WR_MAXD) {
        w->bad = 1;
        return;
    }
    switch (w->fmt) {
    case F_JSON:
        wr_pre(w, key);
        wr_ch(w, arr ? '[' : '{');
        break;
    case F_YAML:
        wr_ws(w, 2 * w->depth);
        if (l->arr) {
            wr_puts(w, "-\n");
        } else {
            wr_puts(w, key);
            wr_puts(w, ":\n");
        }
        ++l->n;
        break;
    case F_TOML:
        if (w->inarr > 0 || arr) {
            /* an array, inline: `key = [` or `[` inside an array */
            wr_pre(w, key);
            wr_ch(w, '[');
            ++w->inarr;
        } else {
            size_t more = (w->plen > 0 ? 1 : 0) + kn;
            if (w->plen + more + 1 >= sizeof w->path) {
                w->bad = 1;
                return;
            }
            if (w->plen > 0) {
                w->path[w->plen++] = '.';
            }
            memcpy(w->path + w->plen, key, kn);
            w->plen += kn;
            w->path[w->plen] = '\0';
            wr_puts(w, "\n[");
            wr_puts(w, w->path);
            wr_puts(w, "]\n");
            ++l->n;
        }
        break;
    case F_INI:
        if (!arr) {
            size_t more = (w->plen > 0 ? 1 : 0) + kn;
            if (w->plen + more + 1 >= sizeof w->path) {
                w->bad = 1;
                return;
            }
            if (w->plen > 0) {
                w->path[w->plen++] = '.';
            }
            memcpy(w->path + w->plen, key, kn);
            w->plen += kn;
            w->path[w->plen] = '\0';
            wr_puts(w, "[");
            wr_puts(w, w->path);
            wr_puts(w, "]\n");
        }
        ++l->n;
        break;
    case F_XML:
        if (!arr) {
            wr_ch(w, '<');
            wr_puts(w, l->arr ? l->key : key);
            wr_puts(w, ">\n");
        }
        ++l->n;
        break;
    case F_CBOR:
        if (!l->arr) {
            wr_ctext(w, key, kn);
        }
        ++l->n;
        break;
    default:
        break;
    }
    ++w->depth;
    c = &w->st[w->depth];
    memset(c, 0, sizeof *c);
    c->arr  = arr;
    c->plen = old;
    if (l->arr && w->fmt == F_XML) {
        strncpy(c->key, l->key, sizeof c->key - 1);
    } else if (key != NULL) {
        strncpy(c->key, key, sizeof c->key - 1);
    }
    if (w->fmt == F_CBOR) {
        c->at = w->d.n;
        wr_head4(w, arr ? 4 : 5, 0);
    }
}

static void wr_close(wr* w) {
    wlvl* c = &w->st[w->depth];

    if (w->depth == 0) {
        return;
    }
    switch (w->fmt) {
    case F_JSON:
        wr_ch(w, c->arr ? ']' : '}');
        break;
    case F_TOML:
        if (w->inarr > 0) {
            --w->inarr;
            wr_ch(w, ']');
            if (w->inarr == 0) {
                wr_ch(w, '\n');
            }
        } else {
            w->plen          = c->plen;
            w->path[w->plen] = '\0';
        }
        break;
    case F_INI:
        w->plen          = c->plen;
        w->path[w->plen] = '\0';
        break;
    case F_XML:
        if (!c->arr) {
            wr_puts(w, "</");
            wr_puts(w, c->key);
            wr_puts(w, ">\n");
        }
        break;
    case F_CBOR:
        if (!w->d.over) {
            w->d.p[c->at + 1] = (char)((c->n >> 24) & 0xFFu);
            w->d.p[c->at + 2] = (char)((c->n >> 16) & 0xFFu);
            w->d.p[c->at + 3] = (char)((c->n >> 8) & 0xFFu);
            w->d.p[c->at + 4] = (char)(c->n & 0xFFu);
        }
        break;
    default:
        break;
    }
    --w->depth;
}

/* ---- the strings the documents are made of ---- */

static const char letters[] = "abcdefghijklmnopqrstuvwxyz";

/* ASCII: letters, digits and single spaces, a letter first and last. */
static size_t fill_ascii(wr* w, char* b, size_t n) {
    size_t i;

    for (i = 0; i < n; ++i) {
        unsigned long r = rn(w);
        if (i == 0 || i + 1 == n) {
            b[i] = letters[r % 26];
        } else if (r % 7 == 0 && b[i - 1] != ' ') {
            b[i] = ' ';
        } else if (r % 7 == 1) {
            b[i] = (char)('0' + (r >> 8) % 10);
        } else {
            b[i] = letters[(r >> 4) % 26];
        }
    }
    b[n] = '\0';
    return n;
}

/* A word of 3 to 8 letters; with a w first, so that no word is a
   YAML or TOML keyword. */
static void fill_word(wr* w, char* b) {
    size_t n = 3 + rn(w) % 6;
    size_t i;

    b[0] = 'w';
    for (i = 1; i < n; ++i) {
        b[i] = letters[rn(w) % 26];
    }
    b[n] = '\0';
}

static const char* const mb2[] = {"\xc3\xa9", "\xc3\xb6", "\xc3\xbc", "\xce\xb1",
                                  "\xd0\xb6"};
static const char* const mb3[] = {"\xe6\x97\xa5", "\xe6\x9c\xac", "\xe2\x9c\x93",
                                  "\xe4\xb8\xad", "\xe2\x82\xac"};
static const char* const mb4[] = {"\xf0\x9f\x98\x80", "\xf0\x9f\x8e\x89",
                                  "\xf0\x9d\x84\x9e", "\xf0\x9f\x92\xa1"};

/* About half the bytes non-ASCII: units of four ASCII letters, and of a
   two, three or four-byte sequence, a letter at each end. */
static size_t fill_utf8(wr* w, char* b, size_t want) {
    size_t n = 0;
    int    k;

    b[n++] = 'a';
    while (n + 6 < want) {
        unsigned long r = rn(w);
        if (r % 2 == 0) {
            for (k = 0; k < 4; ++k) {
                b[n++] = letters[rn(w) % 26];
            }
        } else {
            const char* s;
            switch ((r >> 3) % 3) {
            case 0:
                s = mb2[(r >> 6) % 5];
                break;
            case 1:
                s = mb3[(r >> 6) % 5];
                break;
            default:
                s = mb4[(r >> 6) % 4];
                break;
            }
            memcpy(b + n, s, strlen(s));
            n += strlen(s);
        }
    }
    b[n++] = 'z';
    b[n]   = '\0';
    return n;
}

/* Strings in which every byte is an escape: a line feed, a backslash, a
   quote, an e-acute, a character outside the plane (a surrogate pair in
   JSON). The ends are never a line feed, which XML would trim. */
static size_t fill_escapes(wr* w, char* b, size_t units) {
    static const char* const json[] = {"\\n", "\\\\", "\\\"", "\\u00e9",
                                       "\\ud83d\\ude00"};
    static const char* const yaml[] = {"\\n", "\\\\", "\\\"", "\\u00e9", "\\U0001F600"};
    static const char* const xml[]  = {"&#10;", "&amp;", "&lt;", "&#233;", "&#x1F600;"};
    const char* const* t = (w->fmt == F_XML) ? xml : (w->fmt == F_JSON) ? json : yaml;
    size_t             i, n = 0;

    for (i = 0; i < units; ++i) {
        unsigned long u = rn(w) % 5;
        const char*   s;
        if ((i == 0 || i + 1 == units) && u == 0) {
            u = 1;
        }
        s = t[u];
        memcpy(b + n, s, strlen(s));
        n += strlen(s);
    }
    b[n] = '\0';
    return n;
}

static const char* const edge_tok[] = {"9223372036854775807",
                                       "9223372036854775806",
                                       "-9223372036854775808",
                                       "-9223372036854775807",
                                       "0",
                                       "-1",
                                       "1.2345678901234567",
                                       "3.1415926535897931",
                                       "2.7182818284590451",
                                       "1.7976931348623157e308",
                                       "-1.7976931348623157e308",
                                       "2.2250738585072014e-308",
                                       "4.9406564584124654e-324",
                                       "1e-320",
                                       "5e-324",
                                       "1e22",
                                       "1.0e-5",
                                       "0.1"};

#define EDGE_N (sizeof edge_tok / sizeof edge_tok[0])

static char* tmpbuf(void) {
    static char* b;
    if (b == NULL) {
        b = (char*)malloc(300000);
    }
    return b;
}

/* ---- one record of each document ---- */

static void rec_mixed(wr* w, unsigned long i) {
    char                     key[32], b[64], r[32];
    unsigned long            whole, frac;
    static const char* const modes[] = {"fast", "safe", "debug", "release"};
    int                      k;

    sprintf(key, "s%lu", i);
    wr_open(w, key, 0);
    fill_word(w, b);
    wr_str(w, "name", b, strlen(b));
    wr_int(w, "port", 1024 + (long)(rn(w) % 60000));
    whole = rn(w) % 1000;
    frac  = rn(w) % 100;
    sprintf(r, "%lu.%02lu", whole, frac);
    wr_tok(w, "ratio", r, 0);
    wr_bool(w, "enabled", (int)(rn(w) & 1u));
    k = (int)(rn(w) % 4);
    wr_str(w, "mode", modes[k], strlen(modes[k]));
    wr_open(w, "tags", 1);
    for (k = 0; k < 3; ++k) {
        fill_word(w, b);
        wr_str(w, NULL, b, strlen(b));
    }
    wr_close(w);
    wr_open(w, "limits", 1);
    for (k = 0; k < 4; ++k) {
        wr_int(w, NULL, (long)(rn(w) % 100000));
    }
    wr_close(w);
    wr_close(w);
}

static void rec_str(wr* w, unsigned long i, int docn) {
    char   key[32];
    char*  b = tmpbuf();
    size_t n;

    sprintf(key, "s%lu", i);
    if (docn == D_STR_PLAIN) {
        n = fill_ascii(w, b, 64 + rn(w) % 4033);
    } else if (docn == D_STR_UTF8) {
        n = fill_utf8(w, b, 64 + rn(w) % 1985);
    } else {
        n = fill_escapes(w, b, 20 + rn(w) % 181);
    }
    wr_str(w, key, b, n);
}

/* Deep: member r<i>, then 30 more levels below it, 31 inside the root.
   XML's root element is a level of its own, so one fewer there. */
static void rec_deep(wr* w, unsigned long i) {
    char key[32];
    int  k, lv = (w->fmt == F_XML) ? 29 : 30;
    int  arr = (int)(i & 1u) && w->fmt != F_XML && w->fmt != F_INI;

    sprintf(key, "r%lu", i);
    if (arr) {
        for (k = 0; k < 31; ++k) {
            wr_open(w, k == 0 ? key : NULL, 1);
        }
        wr_int(w, NULL, 1);
        for (k = 0; k < 31; ++k) {
            wr_close(w);
        }
    } else {
        wr_open(w, key, 0);
        for (k = 0; k < lv; ++k) {
            wr_open(w, "d", 0);
        }
        wr_int(w, "v", 1);
        for (k = 0; k < lv + 1; ++k) {
            wr_close(w);
        }
    }
}

/* Whitespace, comments and indentation, a value now and then. */
static void ws_run(wr* w, size_t n) {
    size_t i;
    for (i = 0; i < n; ++i) {
        unsigned long r = rn(w) % 16;
        if (r < 11) {
            wr_ch(w, ' ');
        } else if (r < 15) {
            wr_ch(w, '\n');
        } else {
            wr_ch(w, (w->fmt == F_JSON || w->fmt == F_XML || w->fmt == F_TOML) ? '\t'
                                                                               : ' ');
        }
    }
}

static void comment_line(wr* w, const char* lead, size_t n, const char* tail) {
    size_t i;
    wr_ws(w, (int)(rn(w) % 6));
    wr_puts(w, lead);
    for (i = 0; i < n; ++i) {
        wr_ch(w, (rn(w) % 6 == 0) ? ' ' : letters[rn(w) % 26]);
    }
    wr_puts(w, tail);
    wr_ch(w, '\n');
}

static void wr_rawint(wr* w, long v) {
    char t[32];
    sprintf(t, "%ld", v);
    wr_puts(w, t);
}

static void rec_white(wr* w, unsigned long i) {
    char key[32];
    int  k;

    sprintf(key, "k%lu", i);
    switch (w->fmt) {
    case F_JSON:
        ws_run(w, 300 + rn(w) % 400);
        wr_ch(w, '"');
        wr_puts(w, key);
        wr_ch(w, '"');
        ws_run(w, 20);
        wr_ch(w, ':');
        ws_run(w, 300 + rn(w) % 400);
        wr_rawint(w, (long)i);
        ws_run(w, 20);
        wr_ch(w, ',');
        break;
    case F_XML:
        ws_run(w, 200 + rn(w) % 300);
        wr_puts(w, "<!--");
        for (k = 0; k < 60; ++k) {
            wr_ch(w, letters[rn(w) % 26]);
        }
        wr_puts(w, "-->");
        ws_run(w, 200 + rn(w) % 300);
        wr_ch(w, '<');
        wr_puts(w, key);
        wr_ch(w, '>');
        wr_rawint(w, (long)i);
        wr_puts(w, "</");
        wr_puts(w, key);
        wr_ch(w, '>');
        break;
    default:
        for (k = 0; k < 8; ++k) {
            comment_line(w, w->fmt == F_INI ? "; " : "# ", 40 + rn(w) % 60, "");
            ws_run(w, 20 + rn(w) % 60);
            wr_ch(w, '\n');
        }
        wr_puts(w, key);
        wr_puts(w, w->fmt == F_YAML ? ": " : " = ");
        wr_rawint(w, (long)i);
        wr_ch(w, '\n');
        break;
    }
}

/* ---- a document of `want` bytes in `fmt` ---- */

static unsigned char* g_idx;
static size_t         g_idx_len;

static void put_u32(char* d, unsigned long v) {
    d[0] = (char)(v & 0xFFu);
    d[1] = (char)((v >> 8) & 0xFFu);
    d[2] = (char)((v >> 16) & 0xFFu);
    d[3] = (char)((v >> 24) & 0xFFu);
}

/* Documents of 20 to 64 bytes, one after another; their lengths, as
   four bytes each, in g_idx. */
static unsigned char* tiny_docs(int fmt, size_t want, size_t* len) {
    size_t idx_cap = (want / 16 + 16) * 4;
    u64    rs      = doc_seed(D_MANY_TINY);
    doc    out;

    out.p     = (char*)malloc(want + 4096);
    out.cap   = want + 4096;
    out.n     = 0;
    out.over  = 0;
    g_idx     = (unsigned char*)malloc(idx_cap);
    g_idx_len = 0;
    if (out.p == NULL || g_idx == NULL) {
        free(out.p);
        free(g_idx);
        g_idx = NULL;
        return NULL;
    }
    while (out.n < want) {
        static wr wstore;
        wr*       wp = &wstore;
        char      b[64];
        size_t    n;

        wr_begin(wp, fmt, 1024, D_MANY_TINY);
        wp->rng    = rs;
        wp->yplain = 1;
        wr_int(wp, "a", (long)(rn(wp) % 100000));
        n = fill_ascii(wp, b, 10 + rn(wp) % 33);
        wr_str(wp, "b", b, n);
        wr_end(wp);
        rs = wp->rng;
        if (wp->bad || wp->d.over || g_idx_len + 4 > idx_cap) {
            free(wp->d.p);
            free(out.p);
            free(g_idx);
            g_idx = NULL;
            return NULL;
        }
        put(&out, wp->d.p, wp->d.n);
        put_u32((char*)g_idx + g_idx_len, (unsigned long)wp->d.n);
        g_idx_len += 4;
        free(wp->d.p);
    }
    *len = out.n;
    return (unsigned char*)out.p;
}

/* The one error of `invalid-late`, in the last KiB. */
static void late_error(wr* w) {
    switch (w->fmt) {
    case F_JSON:
        wr_puts(w, ",\n\"late\":1]");
        break;
    case F_YAML:
        wr_puts(w, "late:\n\tx: 1\n");
        break;
    case F_TOML:
        wr_puts(w, "\n[late]\na = 1\na = 2\n");
        break;
    case F_INI:
        wr_puts(w, "[late]\nnoequals\n");
        break;
    case F_XML:
        wr_puts(w, "<late><a>1</b></late>\n");
        break;
    case F_CBOR:
        /* the pair is counted; its text value is cut short */
        wr_ctext(w, "late", 4);
        wr_ch(w, (char)0x6A);
        wr_puts(w, "abc");
        ++w->st[0].n;
        break;
    default:
        break;
    }
}

static unsigned char* yaml_aliases(size_t want, size_t* len) {
    static wr wstore;
    wr*       wp = &wstore;
    int       lv, k;
    char      b[64];

    wr_begin(wp, F_YAML, want + (4u << 20), D_ALIASES);
    wp->yplain = 1;
    /* padding, so that the document is the size asked: plain entries */
    {
        unsigned long i = 0;
        while (wp->d.n + 1200 < want) {
            char key[32];
            sprintf(key, "p%lu", i++);
            fill_word(wp, b);
            wr_str(wp, key, b, strlen(b));
        }
    }
    wr_puts(wp, "a0: &a0 [\"lol\", \"lol\", \"lol\", \"lol\", \"lol\", \"lol\", "
                "\"lol\", \"lol\", \"lol\"]\n");
    for (lv = 1; lv <= 9; ++lv) {
        sprintf(b, "a%d: &a%d [", lv, lv);
        wr_puts(wp, b);
        for (k = 0; k < 9; ++k) {
            sprintf(b, "%s*a%d", k == 0 ? "" : ", ", lv - 1);
            wr_puts(wp, b);
        }
        wr_puts(wp, "]\n");
    }
    if (wp->bad || wp->d.over) {
        free(wp->d.p);
        return NULL;
    }
    *len = wp->d.n;
    return (unsigned char*)wp->d.p;
}

/* The bytes of document `docn` in format `format`, `size` MiB of them
   (within a record); NULL when it does not exist or memory runs out.
   P-1's generators (make_doc) are wrapped; the rest are written here. */
static unsigned char* bench_doc(int format, int docn, size_t size, size_t* len) {
    size_t        want = size << 20;
    static wr     wstore;
    wr*           wp = &wstore;
    unsigned long i  = 0;

    *len = 0;
    if (!doc_exists(format, docn)) {
        return NULL;
    }
    if (docn == D_RECORDS) {
        size_t start = 0;
        char*  p     = make_doc(fname[format], want, &start, len);
        if (p != NULL && start > 0) {
            memmove(p, p + start, *len);
        }
        return (unsigned char*)p;
    }
    if (docn == D_MANY_TINY) {
        return tiny_docs(format, want, len);
    }
    if (docn == D_ALIASES) {
        return yaml_aliases(size << 20, len);
    }
    wr_begin(wp, format, want + (8u << 20), docn);
    if (wp->bad) {
        return NULL;
    }
    wp->yplain = (docn == D_MIXED);
    if (docn == D_ARRAY_HUGE || docn == D_NUM_INT) {
        wp->nl_each = (docn == D_NUM_INT);
        wr_open(wp, "a", 1);
        while (wp->d.n < want && !wp->d.over) {
            if (docn == D_ARRAY_HUGE) {
                wr_int(wp, NULL, (long)(rn(wp) % 10));
            } else {
                long v = (long)(rn(wp) % 1000);
                wr_int(wp, NULL, (rn(wp) % 5 == 0) ? -v : v);
            }
        }
    } else {
        int tail = (docn == D_INVALID_LATE);
        while (!wp->d.over && wp->d.n + (tail ? 1500u : 0u) < want) {
            static char key[300];
            switch (docn) {
            case D_MIXED:
            case D_INVALID_LATE:
                rec_mixed(wp, i);
                break;
            case D_STR_PLAIN:
            case D_STR_ESC:
            case D_STR_UTF8:
                rec_str(wp, i, docn);
                break;
            case D_NUM_EDGE:
                if (i == 0) {
                    wr_open(wp, "n", 1);
                    wp->nl_each = 1;
                }
                wr_tok(wp, NULL, edge_tok[i % EDGE_N], 0);
                break;
            case D_NEST_DEEP:
                rec_deep(wp, i);
                break;
            case D_TABLE_WIDE:
                sprintf(key, "k%07lx", i);
                wr_int(wp, key, (long)(rn(wp) % 1000));
                break;
            case D_KEYS_PREFIX:
                memset(key, 'p', 200);
                sprintf(key + 200, "%07lu", i);
                wr_int(wp, key, (long)(rn(wp) % 1000));
                break;
            case D_WHITESPACE:
                rec_white(wp, i);
                break;
            default:
                break;
            }
            ++i;
        }
        if (docn == D_WHITESPACE && format == F_JSON) {
            wr_puts(wp, "\"end\":0");
        }
        if (docn == D_INVALID_LATE) {
            late_error(wp);
        }
    }
    wr_end(wp);
    if (wp->bad || wp->d.over) {
        free(wp->d.p);
        return NULL;
    }
    *len = wp->d.n;
    return (unsigned char*)wp->d.p;
}

/* FNV-1a, 64 bits, in 16 hex digits. */
static void fnv1a64(const unsigned char* p, size_t n, char* out) {
    u64    h     = u64_of(0xCBF29CE4ul, 0x84222325ul);
    u64    prime = u64_of(0x00000100ul, 0x000001B3ul);
    size_t i;

    for (i = 0; i < n; ++i) {
        h.lo ^= p[i];
        h = u64_mul(h, prime);
    }
    sprintf(out, "%08lx%08lx", h.hi, h.lo);
}

static int emit_file(const char* dir, const char* name, const unsigned char* p,
                     size_t n) {
    char  path[1024];
    char  hex[20];
    FILE* f;

    sprintf(path, "%s/%s", dir, name);
    f = fopen(path, "wb");
    if (f == NULL || fwrite(p, 1, n, f) != n || fclose(f) != 0) {
        fprintf(stderr, "bench: cannot write %s\n", path);
        return 1;
    }
    fnv1a64(p, n, hex);
    printf("%s %lu %s\n", name, (unsigned long)n, hex);
    return 0;
}

/* `--emit-docs <dir> [--size 1|16|256] [--only <format>]`: every document
   that exists, as `<format>-<doc>-<size>.<ext>`, and a line each. */
static int run_emit_docs(int argc, char** argv) {
    const char* dir  = NULL;
    const char* only = NULL;
    size_t      sizes[3];
    size_t      nsize = 0, s;
    int         j, f, d;

    for (j = 2; j < argc; ++j) {
        if (strcmp(argv[j], "--size") == 0 && j + 1 < argc) {
            if (nsize < 3) {
                sizes[nsize++] = (size_t)strtoul(argv[++j], NULL, 10);
            }
        } else if (strcmp(argv[j], "--only") == 0 && j + 1 < argc) {
            only = argv[++j];
        } else if (dir == NULL) {
            dir = argv[j];
        }
    }
    if (dir == NULL) {
        fprintf(
            stderr,
            "usage: cf_bench --emit-docs <dir> [--size 1|16|256] [--only <format>]\n");
        return 2;
    }
    if (nsize == 0) {
        sizes[nsize++] = 1;
        sizes[nsize++] = 16;
        sizes[nsize++] = 256;
    }
    for (s = 0; s < nsize; ++s) {
        for (f = 0; f < F_COUNT; ++f) {
            if (only != NULL && strcmp(only, fname[f]) != 0) {
                continue;
            }
            for (d = 0; d < D_COUNT; ++d) {
                unsigned char* p;
                size_t         n = 0;
                char           name[128];

                if (!doc_exists(f, d)) {
                    continue;
                }
                if (d == D_ALIASES && sizes[s] != 1) {
                    continue;
                }
                p = bench_doc(f, d, sizes[s], &n);
                if (p == NULL) {
                    fprintf(stderr, "bench: cannot make %s %s %lu MiB\n", fname[f],
                            dname[d], (unsigned long)sizes[s]);
                    return 1;
                }
                if (d == D_MANY_TINY) {
                    int rc;
                    sprintf(name, "%s-%s-%lu.bin", fname[f], dname[d],
                            (unsigned long)sizes[s]);
                    rc = emit_file(dir, name, p, n);
                    sprintf(name, "%s-%s-%lu.idx", fname[f], dname[d],
                            (unsigned long)sizes[s]);
                    rc |= emit_file(dir, name, g_idx, g_idx_len);
                    free(g_idx);
                    g_idx = NULL;
                    free(p);
                    if (rc != 0) {
                        return 1;
                    }
                    continue;
                }
                if (strncmp(dname[d], fname[f], strlen(fname[f])) == 0) {
                    sprintf(name, "%s-%lu%s", dname[d], (unsigned long)sizes[s],
                            fext[f]);
                } else {
                    sprintf(name, "%s-%s-%lu%s", fname[f], dname[d],
                            (unsigned long)sizes[s], fext[f]);
                }
                if (emit_file(dir, name, p, n) != 0) {
                    free(p);
                    return 1;
                }
                free(p);
            }
        }
    }
    return 0;
}

/* Count before cf_build, sample the arena after create, and include the wipe
   and release in the time. Tiny documents have separate configuration lifetimes. */
static int allocation_case(int f, int d, size_t mib) {
    size_t         bytes, docs, i;
    unsigned char* data = bench_doc(f, d, mib, &bytes);
    double         best = 0, total_calls = 0, total_asked = 0, total_slack = 0;
    int            r, rc = 1;

    if (data == NULL) {
        return 1;
    }
    docs = d == D_MANY_TINY ? g_idx_len / 4 : 1;
    for (r = 0; r < 3; ++r) {
        size_t  offset = 0;
        clock_t begin  = clock();
        double  elapsed;
        total_calls = total_asked = total_slack = 0;
        for (i = 0; i < docs; ++i) {
            size_t          len = bytes;
            cf_error        err = CF_ERROR_INIT;
            cf_builder*     b;
            cf_config*      c;
            const cf_arena* arena;
            if (d == D_MANY_TINY) {
                const unsigned char* p = g_idx + i * 4;
                len = p[0] | ((size_t)p[1] << 8) | ((size_t)p[2] << 16) |
                      ((size_t)p[3] << 24);
            }
            calls = asked = 0;
            b             = cf_build(&counting);
            if (b == NULL ||
                !cf_builder_document(b, "doc", CF_RANK_USER_FILE,
                                     (const char*)data + offset, len, fname[f], 0)) {
                cf_builder_abort(b);
                goto done;
            }
            c = cf_builder_create(b, &err);
            if (c == NULL || cf_count(c) == 0) {
                fprintf(stderr, "bench: %s %s: %s\n", fname[f], dname[d], err.message);
                cf_config_free(c);
                goto done;
            }
            arena = cf_ks_arena(cf_keys(c));
            total_slack += (double)(cf_arena_capacity(arena) - cf_arena_used(arena));
            cf_config_free(c);
            total_calls += (double)calls;
            total_asked += (double)asked;
            offset += len;
        }
        elapsed = seconds_since(begin);
        if (r == 0 || elapsed < best) {
            best = elapsed;
        }
    }
    printf("%s %s %lu %lu %.0f %.0f %.0f %.3f\n", fname[f], dname[d],
           (unsigned long)mib, (unsigned long)docs, total_calls, total_asked,
           total_slack, best);
    rc = 0;
done:
    free(data);
    free(g_idx);
    g_idx = NULL;
    return rc;
}

static int run_allocation(void) {
    int f;
    printf(
        "format case MiB docs calls bytes_asked capacity_minus_used seconds_best3\n");
    for (f = 0; f < F_COUNT; ++f) {
        size_t          mib;
        const cf_codec* codec = cf_codec_by_name(fname[f]);
        if (codec == NULL || !cf_codec_can_read(codec)) {
            continue;
        }
        for (mib = 1; mib <= 16; mib *= 16) {
            if (allocation_case(f, D_MIXED, mib) ||
                allocation_case(f, D_MANY_TINY, mib)) {
                return 1;
            }
        }
    }
    return 0;
}

int main(int argc, char** argv) {
    static const size_t dflt[] = {100, 1000, 10000, 20000};
    int                 i;

    if (argc == 2 && strcmp(argv[1], "allocation") == 0) {
        return run_allocation();
    }
    if (argc >= 2 && strcmp(argv[1], "docs") == 0) {
        return run_docs(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "--emit-docs") == 0) {
        return run_emit_docs(argc, argv);
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
