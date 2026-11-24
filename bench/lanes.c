/*
 * lanes.c - `cf_bench lanes`: every span operation of the word-at-a-time
 * backend against the scalar loop, on spans of 8 to 4096 bytes (P-2).
 *
 *   cf_bench lanes            the table, in MB/s and as a ratio
 *   cf_bench lanes <op>...    only these operations (find_byte, find_any,
 *                             find_lt, ascii_run, count_byte, utf8_valid)
 *
 * The rule that decides what the word backend keeps is P-2's: an operation
 * keeps its word body when it is at least 5% faster than the loop on the
 * spans of 64 bytes and longer, and loses by no more than 5% on 16 bytes
 * (lib/lanes/src/swar.c, LANES_SWAR_KEEP_*). Each figure is the best of
 * fifteen runs of about a hundredth of a second, from clock() as bench.c's are.
 *
 * lanes is compiled into this program unprefixed (bench/CMakeLists.txt), so
 * what is measured is the library as a program uses it, with this build's
 * compiler and flags.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lanes/lanes.h"

/* Called by bench.c's main. */
int run_lanes(int argc, char** argv);

#define SPANS 5
static const size_t g_spans[SPANS] = {8, 16, 64, 256, 4096};

/* The sink keeps a compiler from dropping the calls. */
static volatile size_t g_sink;

typedef struct kase {
    const char*    op;
    const char*    what;
    unsigned char* data; /* g_spans[SPANS - 1] bytes at least */
    int            kind;
    unsigned char  c;
    lanes_set      set;
} kase;

enum { K_FIND_BYTE, K_FIND_ANY, K_FIND_LT, K_ASCII_RUN, K_COUNT_BYTE, K_UTF8_VALID };

static size_t call(const lanes_ops* ops, const kase* k, size_t n) {
    switch (k->kind) {
    case K_FIND_BYTE:
        return ops->find_byte(k->data, n, k->c);
    case K_FIND_ANY:
        return ops->find_any(k->data, n, &k->set);
    case K_FIND_LT:
        return ops->find_lt(k->data, n, k->c);
    case K_ASCII_RUN:
        return ops->ascii_run(k->data, n);
    case K_COUNT_BYTE:
        return ops->count_byte(k->data, n, k->c);
    default:
        return ops->utf8_valid(k->data, n);
    }
}

/* Bytes a second, best of fifteen short runs (the best, because the machine
   is shared and a slow run says what else ran), for one backend, one case, one span. */
static double rate(const lanes_ops* ops, const kase* k, size_t n) {
    size_t iters = (size_t)(16u * 1024u * 1024u) / n;
    double best  = 0.0;
    int    rep;

    if (iters < 1) {
        iters = 1;
    }
    for (rep = 0; rep < 15; ++rep) {
        clock_t t0 = clock();
        size_t  i, acc = 0;
        double  sec, r;

        for (i = 0; i < iters; ++i) {
            acc += call(ops, k, n);
        }
        sec    = (double)(clock() - t0) / (double)CLOCKS_PER_SEC;
        g_sink = acc;
        if (sec <= 0.0) {
            sec = 1e-9;
        }
        r = (double)n * (double)iters / sec;
        if (r > best) {
            best = r;
        }
    }
    return best;
}

/* Does the case answer the same on both backends, on every span? A bench
   that measured two different answers would measure nothing. */
static int agree(const lanes_ops* a, const lanes_ops* b, const kase* k) {
    size_t s;

    for (s = 0; s < SPANS; ++s) {
        if (call(a, k, g_spans[s]) != call(b, k, g_spans[s])) {
            return 0;
        }
    }
    return 1;
}

static void report(const lanes_ops* ref, const lanes_ops* swar, const kase* k) {
    size_t s;

    if (!agree(ref, swar, k)) {
        printf("%-11s %-26s  DIFFER\n", k->op, k->what);
        return;
    }
    printf("%-11s %-26s", k->op, k->what);
    for (s = 0; s < SPANS; ++s) {
        double a = rate(ref, k, g_spans[s]);
        double b = rate(swar, k, g_spans[s]);

        printf(" %5.2f", b / a);
    }
    printf("   (scalar %.0f MB/s at %lu)\n", rate(ref, k, g_spans[SPANS - 1]) / 1e6,
           (unsigned long)g_spans[SPANS - 1]);
}

/* Text that a JSON string or a blank scan runs through: letters and
   digits, no control byte, no quote or backslash, no blank. */
static void fill_text(unsigned char* d, size_t n) {
    size_t i;

    for (i = 0; i < n; ++i) {
        d[i] = (unsigned char)("abcdefghijklmnopqrstuvwxyz0123456789"[i % 36]);
    }
}

/* Valid UTF-8: every `period`-th sequence is a 2-byte one (period 0: none;
   1: all 3-byte, a dense non-ASCII text). */
static void fill_utf8(unsigned char* d, size_t n, size_t period) {
    size_t i = 0, k = 0;

    while (i < n) {
        if (period == 1 && i + 3 <= n) {
            d[i++] = 0xE4;
            d[i++] = 0xB8;
            d[i++] = 0xAD;
        } else if (period > 1 && k % period == 0 && i + 2 <= n) {
            d[i++] = 0xC3;
            d[i++] = 0xA9;
        } else {
            d[i] = (unsigned char)('a' + (i % 26));
            ++i;
        }
        ++k;
    }
}

int run_lanes(int argc, char** argv) {
    const lanes_ops* ref  = lanes_scalar();
    const lanes_ops* swar = lanes_swar();
    size_t           big  = g_spans[SPANS - 1];
    unsigned char*   text = (unsigned char*)malloc(big + 64);
    unsigned char*   cnt  = (unsigned char*)malloc(big + 64);
    unsigned char*   ctl  = (unsigned char*)malloc(big + 64);
    unsigned char*   u0   = (unsigned char*)malloc(big + 64);
    unsigned char*   u1   = (unsigned char*)malloc(big + 64);
    unsigned char*   u2   = (unsigned char*)malloc(big + 64);
    unsigned char    json[3], blanks[4];
    kase             cases[16];
    size_t           nc = 0, i, s;
    int              j, any = 0;

    if (text == NULL || cnt == NULL || ctl == NULL || u0 == NULL || u1 == NULL ||
        u2 == NULL) {
        printf("cf_bench lanes: out of memory\n");
        return 1;
    }
    if (swar == NULL) {
        printf("cf_bench lanes: this build has no word backend (CF_LANES_BACKENDS)\n");
        free(text);
        free(cnt);
        free(ctl);
        free(u0);
        free(u1);
        free(u2);
        return 0;
    }
    fill_text(text, big);
    fill_text(cnt, big);
    for (i = 0; i < big; i += 8) {
        cnt[i] = ';';
    }
    fill_text(ctl, big);
    fill_utf8(u0, big, 0);
    fill_utf8(u1, big, 12);
    fill_utf8(u2, big, 1);
    json[0] = '"';
    json[1] = '\\';
    json[2] = 0x1F; /* the low run is made below, from 0 to 0x1F */
    memset(blanks, 0, sizeof blanks);
    blanks[0] = ' ';
    blanks[1] = '\t';
    blanks[2] = '\n';
    blanks[3] = '\r';

    memset(cases, 0, sizeof cases);
    cases[nc].op   = "find_byte";
    cases[nc].what = "none (the whole span)";
    cases[nc].data = text;
    cases[nc].kind = K_FIND_BYTE;
    cases[nc].c    = ';';
    ++nc;
    cases[nc].op   = "find_any";
    cases[nc].what = "blanks (4), none";
    cases[nc].data = text;
    cases[nc].kind = K_FIND_ANY;
    lanes_set_init(&cases[nc].set, blanks, 4);
    ++nc;
    {
        unsigned char stops[34];
        size_t        m = 0;

        for (i = 0; i < 0x20; ++i) {
            stops[m++] = (unsigned char)i;
        }
        stops[m++]     = json[0];
        stops[m++]     = json[1];
        cases[nc].op   = "find_any";
        cases[nc].what = "JSON stops (controls \" \\), none";
        cases[nc].data = text;
        cases[nc].kind = K_FIND_ANY;
        lanes_set_init(&cases[nc].set, stops, m);
        ++nc;
    }
    cases[nc].op   = "find_lt";
    cases[nc].what = "below 0x20, none";
    cases[nc].data = text;
    cases[nc].kind = K_FIND_LT;
    cases[nc].c    = 0x20;
    ++nc;
    cases[nc].op   = "ascii_run";
    cases[nc].what = "all ASCII";
    cases[nc].data = ctl;
    cases[nc].kind = K_ASCII_RUN;
    ++nc;
    cases[nc].op   = "count_byte";
    cases[nc].what = "one byte in eight";
    cases[nc].data = cnt;
    cases[nc].kind = K_COUNT_BYTE;
    cases[nc].c    = ';';
    ++nc;
    cases[nc].op   = "utf8_valid";
    cases[nc].what = "ASCII";
    cases[nc].data = u0;
    cases[nc].kind = K_UTF8_VALID;
    ++nc;
    cases[nc].op   = "utf8_valid";
    cases[nc].what = "a 2-byte letter in 12";
    cases[nc].data = u1;
    cases[nc].kind = K_UTF8_VALID;
    ++nc;
    cases[nc].op   = "utf8_valid";
    cases[nc].what = "dense 3-byte (CJK)";
    cases[nc].data = u2;
    cases[nc].kind = K_UTF8_VALID;
    ++nc;

    printf("cf_bench lanes: %s against scalar; ratio (>1 is faster) on spans",
           swar->name);
    for (s = 0; s < SPANS; ++s) {
        printf(" %lu", (unsigned long)g_spans[s]);
    }
    printf("\n");
    for (i = 0; i < nc; ++i) {
        int want = argc <= 2;

        for (j = 2; j < argc; ++j) {
            if (strcmp(argv[j], cases[i].op) == 0) {
                want = 1;
            }
        }
        if (want) {
            report(ref, swar, &cases[i]);
            any = 1;
        }
    }
    if (!any) {
        printf("cf_bench lanes: no such operation\n");
    }
    free(text);
    free(cnt);
    free(ctl);
    free(u0);
    free(u1);
    free(u2);
    return any ? 0 : 1;
}
