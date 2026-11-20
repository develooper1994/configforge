/*
 * test_lanes.c - every span operation of lanes_scalar() against an oracle
 * written here, and the processor's detection.
 *
 * Each operation runs on lengths 0 to 70 at offsets 0 to 15, in a buffer
 * whose bytes after the span are poison (0xA5, which is a UTF-8
 * continuation byte and a byte no data below uses): a function that reads
 * past the span gets a wrong answer. The same span is also run in a heap
 * block of exactly its length, so that a sanitizer sees the overread
 * where a poisoned buffer only changes the result.
 *
 * Self-contained on purpose: lanes uses nothing of ConfigForge's, tests
 * included.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lanes/lanes.h"

#define MAXN 70
#define MAXOFF 15
#define POISON 0xA5

static int g_fail = 0, g_run = 0;

static void check(int c, const char* what) {
    ++g_run;
    if (!c) {
        ++g_fail;
        printf("FAIL: %s\n", what);
    }
}

/* A check that names the case it failed in. */
static void check_at(int c, const char* what, size_t n, size_t off, size_t extra) {
    ++g_run;
    if (!c) {
        ++g_fail;
        if (g_fail < 40) {
            printf("FAIL: %s (n=%lu off=%lu extra=%lu)\n", what, (unsigned long)n,
                   (unsigned long)off, (unsigned long)extra);
        }
    }
}

/* A buffer: bytes 0..off-1 poison, the span at off, poison after. */
static unsigned char g_buf[MAXOFF + MAXN + 64];

static const unsigned char* place(const unsigned char* data, size_t n, size_t off) {
    memset(g_buf, POISON, sizeof g_buf);
    if (n > 0) {
        memcpy(g_buf + off, data, n);
    }
    return g_buf + off;
}

/* The same span in a block of exactly n bytes, for a sanitizer. NULL when
   n is 0, which the operations accept. */
static unsigned char* exact(const unsigned char* data, size_t n) {
    unsigned char* p;

    if (n == 0) {
        return NULL;
    }
    p = (unsigned char*)malloc(n);
    if (p == NULL) {
        printf("FAIL: out of memory\n");
        exit(2);
    }
    memcpy(p, data, n);
    return p;
}

/* A repeatable stream of bytes (xorshift on 32 bits kept in an unsigned long). */
static unsigned long g_rng = 2463534242UL;

static unsigned next_byte(void) {
    g_rng ^= (g_rng << 13) & 0xFFFFFFFFUL;
    g_rng ^= g_rng >> 17;
    g_rng ^= (g_rng << 5) & 0xFFFFFFFFUL;
    return (unsigned)((g_rng >> 8) & 0xFF);
}

/* ---------------------------------------------------------------- *
 * The oracles: the simplest thing that is plainly the meaning.
 * ---------------------------------------------------------------- */

static size_t want_find_byte(const unsigned char* d, size_t n, unsigned c) {
    size_t i;

    for (i = 0; i < n; ++i) {
        if (d[i] == c) {
            return i;
        }
    }
    return n;
}

static size_t want_find_lt(const unsigned char* d, size_t n, unsigned c) {
    size_t i;

    for (i = 0; i < n; ++i) {
        if (d[i] < c) {
            return i;
        }
    }
    return n;
}

static size_t want_find_any(const unsigned char* d, size_t n, const int* member) {
    size_t i;

    for (i = 0; i < n; ++i) {
        if (member[d[i]]) {
            return i;
        }
    }
    return n;
}

static size_t want_count(const unsigned char* d, size_t n, unsigned c) {
    size_t i, k = 0;

    for (i = 0; i < n; ++i) {
        if (d[i] == c) {
            ++k;
        }
    }
    return k;
}

static size_t want_ascii(const unsigned char* d, size_t n) {
    size_t i;

    for (i = 0; i < n && d[i] < 0x80; ++i) {
    }
    return i;
}

/* ---------------------------------------------------------------- */

static void test_set(void) {
    lanes_set     s;
    unsigned char all[256];
    unsigned char some[3];
    unsigned      i;
    int           ok = 1;

    memset(s.bits, 0xFF, sizeof s.bits);
    lanes_set_init(&s, NULL, 0);
    for (i = 0; i < 256; ++i) {
        if (LANES_SET_HAS(&s, i)) {
            ok = 0;
        }
    }
    check(ok, "an empty set has no byte, and init clears what was there");

    for (i = 0; i < 256; ++i) {
        all[i] = (unsigned char)i;
    }
    lanes_set_init(&s, all, 256);
    ok = 1;
    for (i = 0; i < 256; ++i) {
        if (!LANES_SET_HAS(&s, i)) {
            ok = 0;
        }
    }
    check(ok, "a full set has every byte");

    some[0] = 0;
    some[1] = ',';
    some[2] = 0xFF;
    lanes_set_init(&s, some, 3);
    ok = 1;
    for (i = 0; i < 256; ++i) {
        int want = (i == 0 || i == ',' || i == 0xFF);
        if ((LANES_SET_HAS(&s, i) != 0) != want) {
            ok = 0;
        }
    }
    check(ok, "a set has the bytes it was made of and no others");
    {
        /* A char holding 0xFF, whatever char's sign is, without a constant
           cast that truncates. */
        unsigned char ub = 0xFF;
        char          cb;

        memcpy(&cb, &ub, 1);
        check(LANES_SET_HAS(&s, cb) != 0, "the macro takes a char");
    }
}

static void run_find_byte(const lanes_ops* ops, const unsigned char* d, size_t n,
                          size_t off, unsigned c) {
    const unsigned char* p    = place(d, n, off);
    unsigned char*       e    = exact(d, n);
    size_t               want = want_find_byte(d, n, c);

    check_at(ops->find_byte(p, n, (unsigned char)c) == want, "find_byte", n, off, c);
    check_at(ops->find_byte(e, n, (unsigned char)c) == want, "find_byte, exact block",
             n, off, c);
    free(e);
}

static void test_find_byte(const lanes_ops* ops) {
    static unsigned char d[MAXN];
    size_t               n, off, k, i;
    unsigned             c;

    /* A match at every place, and none; the target is a byte the data
       does not otherwise hold, and the poison is never it. */
    for (n = 0; n <= MAXN; ++n) {
        for (off = 0; off <= MAXOFF; ++off) {
            for (k = 0; k <= n; ++k) {
                for (i = 0; i < n; ++i) {
                    d[i] = 'a';
                }
                if (k < n) {
                    d[k] = 'B';
                }
                run_find_byte(ops, d, n, off, 'B');
                run_find_byte(ops, d, n, off, POISON);
            }
        }
    }
    /* Every byte value as the target, on random data. */
    for (c = 0; c < 256; ++c) {
        for (n = 0; n <= MAXN; n += 7) {
            for (i = 0; i < n; ++i) {
                d[i] = (unsigned char)next_byte();
            }
            run_find_byte(ops, d, n, c & MAXOFF, c);
        }
    }
    /* The first of two matches. */
    memset(d, 'x', sizeof d);
    d[20] = 'y';
    d[40] = 'y';
    check(ops->find_byte(d, MAXN, 'y') == 20, "find_byte finds the first of two");
}

static void test_find_any(const lanes_ops* ops) {
    static unsigned char d[MAXN];
    static unsigned char members[8];
    int                  member[256];
    lanes_set            s;
    size_t               n, off, i, m, round;

    for (round = 0; round < 40; ++round) {
        m = round % 9;
        memset(member, 0, sizeof member);
        for (i = 0; i < m; ++i) {
            members[i]         = (unsigned char)next_byte();
            member[members[i]] = 1;
        }
        lanes_set_init(&s, members, m);
        for (n = 0; n <= MAXN; ++n) {
            for (off = 0; off <= MAXOFF; off += (round & 1) ? 1 : 5) {
                const unsigned char* p;
                unsigned char*       e;
                size_t               want;

                for (i = 0; i < n; ++i) {
                    d[i] = (unsigned char)next_byte();
                    /* Mostly bytes outside the set, so that a match is
                       somewhere and not always first. */
                    if (member[d[i]] && (next_byte() & 3) != 0) {
                        d[i] = (unsigned char)(d[i] ^ 0x55);
                        if (member[d[i]]) {
                            d[i] = (unsigned char)(d[i] ^ 0xAA);
                        }
                    }
                }
                want = want_find_any(d, n, member);
                p    = place(d, n, off);
                e    = exact(d, n);
                check_at(ops->find_any(p, n, &s) == want, "find_any", n, off, m);
                check_at(ops->find_any(e, n, &s) == want, "find_any, exact block", n,
                         off, m);
                free(e);
            }
        }
    }
    /* A set holding the poison byte and a span with none of it. */
    members[0] = POISON;
    lanes_set_init(&s, members, 1);
    memset(d, 'a', sizeof d);
    for (n = 0; n <= MAXN; ++n) {
        check_at(ops->find_any(place(d, n, 3), n, &s) == n, "find_any is stopped by n",
                 n, 3, 0);
    }
}

static void test_find_lt(const lanes_ops* ops) {
    static unsigned char d[MAXN];
    size_t               n, off, k, i;
    unsigned             c;

    for (c = 0; c < 256; c += 5) {
        for (n = 0; n <= MAXN; n += 3) {
            for (i = 0; i < n; ++i) {
                d[i] = (unsigned char)next_byte();
            }
            for (off = 0; off <= MAXOFF; off += 4) {
                const unsigned char* p    = place(d, n, off);
                unsigned char*       e    = exact(d, n);
                size_t               want = want_find_lt(d, n, c);

                check_at(ops->find_lt(p, n, (unsigned char)c) == want, "find_lt", n,
                         off, c);
                check_at(ops->find_lt(e, n, (unsigned char)c) == want,
                         "find_lt, exact block", n, off, c);
                free(e);
            }
        }
    }
    /* A byte below at every place, none below, and c = 0 (nothing is). */
    for (n = 0; n <= MAXN; ++n) {
        for (k = 0; k <= n; ++k) {
            for (i = 0; i < n; ++i) {
                d[i] = 0x80;
            }
            if (k < n) {
                d[k] = 0x1F;
            }
            check_at(ops->find_lt(place(d, n, 1), n, 0x20) == k, "find_lt at a place",
                     n, 1, k);
            check_at(ops->find_lt(place(d, n, 1), n, 0) == n, "find_lt below zero", n,
                     1, k);
        }
    }
    check(ops->find_lt(NULL, 0, 0xFF) == 0, "find_lt of nothing is 0");
}

static void test_ascii_run(const lanes_ops* ops) {
    static unsigned char d[MAXN];
    size_t               n, off, k, i;

    for (n = 0; n <= MAXN; ++n) {
        for (off = 0; off <= MAXOFF; ++off) {
            for (k = 0; k <= n; ++k) {
                const unsigned char* p;
                unsigned char*       e;

                for (i = 0; i < n; ++i) {
                    d[i] = (unsigned char)(next_byte() & 0x7F);
                }
                if (k < n) {
                    d[k] = (unsigned char)(0x80 | (next_byte() & 0x7F));
                }
                p = place(d, n, off);
                e = exact(d, n);
                check_at(ops->ascii_run(p, n) == want_ascii(d, n), "ascii_run", n, off,
                         k);
                check_at(ops->ascii_run(e, n) == k, "ascii_run, exact block", n, off,
                         k);
                free(e);
            }
        }
    }
    d[0] = 0x7F;
    d[1] = 0x80;
    check(ops->ascii_run(d, 2) == 1, "0x7F is ASCII and 0x80 is not");
    check(ops->ascii_run(NULL, 0) == 0, "ascii_run of nothing is 0");
}

static void test_count_byte(const lanes_ops* ops) {
    static unsigned char d[MAXN];
    size_t               n, off, i;
    unsigned             c;

    for (c = 0; c < 256; c += 3) {
        for (n = 0; n <= MAXN; ++n) {
            for (i = 0; i < n; ++i) {
                /* Few distinct values, so that matches are many. */
                d[i] = (unsigned char)(next_byte() & 7);
                if ((next_byte() & 3) == 0) {
                    d[i] = (unsigned char)c;
                }
            }
            off = (n + c) & MAXOFF;
            {
                const unsigned char* p    = place(d, n, off);
                unsigned char*       e    = exact(d, n);
                size_t               want = want_count(d, n, c);

                check_at(ops->count_byte(p, n, (unsigned char)c) == want, "count_byte",
                         n, off, c);
                check_at(ops->count_byte(e, n, (unsigned char)c) == want,
                         "count_byte, exact block", n, off, c);
                free(e);
            }
        }
    }
    /* The poison byte is counted only inside the span. */
    memset(d, 'a', sizeof d);
    for (n = 0; n <= MAXN; ++n) {
        check_at(ops->count_byte(place(d, n, 2), n, POISON) == 0,
                 "count_byte stops at n", n, 2, 0);
    }
}

static void test_digits8(const lanes_ops* ops) {
    static const unsigned char bad[] = {'/', ':', 0x00, 0x80, 0xB0, 'a', ' ', '-'};
    unsigned char              d[8];
    unsigned long              v;
    size_t                     place_at, b, off;

    v = 777;
    memcpy(d, "12345678", 8);
    check(ops->digits8(d, &v) == 1 && v == 12345678UL, "digits8 reads 12345678");
    memcpy(d, "00000000", 8);
    check(ops->digits8(d, &v) == 1 && v == 0UL, "digits8 reads 00000000");
    memcpy(d, "99999999", 8);
    check(ops->digits8(d, &v) == 1 && v == 99999999UL, "digits8 reads 99999999");
    memcpy(d, "00000001", 8);
    check(ops->digits8(d, &v) == 1 && v == 1UL, "digits8 reads 00000001");
    memcpy(d, "10000000", 8);
    check(ops->digits8(d, &v) == 1 && v == 10000000UL, "digits8 reads 10000000");

    for (place_at = 0; place_at < 8; ++place_at) {
        for (b = 0; b < sizeof bad; ++b) {
            v = 12345;
            memcpy(d, "31415926", 8);
            d[place_at] = bad[b];
            check_at(ops->digits8(d, &v) == 0, "digits8 refuses a non-digit", place_at,
                     b, 0);
            check_at(v == 12345UL, "digits8 leaves the value alone", place_at, b, 0);
        }
    }
    /* The eight bytes at every offset, with poison after them. */
    for (off = 0; off <= MAXOFF; ++off) {
        const unsigned char* p;

        memcpy(d, "27182818", 8);
        p = place(d, 8, off);
        v = 0;
        check_at(ops->digits8(p, &v) == 1 && v == 27182818UL, "digits8 at an offset", 8,
                 off, 0);
    }
}

/* ---------------------------------------------------------------- *
 * UTF-8
 * ---------------------------------------------------------------- */

typedef struct u8case {
    const char*   what;
    unsigned char bytes[8];
    size_t        n;
    size_t        want; /* the longest whole, valid prefix */
} u8case;

/* clang-format off */
static const u8case u8cases[] = {
    {"empty", {0}, 0, 0},
    {"ascii", {'a', 'b', 'c'}, 3, 3},
    {"nul is ascii", {0, 'a'}, 2, 2},
    {"two bytes", {0xC3, 0xA9}, 2, 2},
    {"lowest two bytes U+0080", {0xC2, 0x80}, 2, 2},
    {"highest two bytes U+07FF", {0xDF, 0xBF}, 2, 2},
    {"three bytes", {0xE2, 0x82, 0xAC}, 3, 3},
    {"lowest three bytes U+0800", {0xE0, 0xA0, 0x80}, 3, 3},
    {"before the surrogates U+D7FF", {0xED, 0x9F, 0xBF}, 3, 3},
    {"after the surrogates U+E000", {0xEE, 0x80, 0x80}, 3, 3},
    {"highest three bytes U+FFFF", {0xEF, 0xBF, 0xBF}, 3, 3},
    {"four bytes", {0xF0, 0x9F, 0x98, 0x80}, 4, 4},
    {"lowest four bytes U+10000", {0xF0, 0x90, 0x80, 0x80}, 4, 4},
    {"highest, U+10FFFF", {0xF4, 0x8F, 0xBF, 0xBF}, 4, 4},
    {"valid then ascii", {0xC3, 0xA9, 'z'}, 3, 3},

    {"overlong two bytes C0 80", {0xC0, 0x80}, 2, 0},
    {"overlong two bytes C1 BF", {0xC1, 0xBF}, 2, 0},
    {"overlong three bytes E0 80 80", {0xE0, 0x80, 0x80}, 3, 0},
    {"overlong three bytes E0 9F BF", {0xE0, 0x9F, 0xBF}, 3, 0},
    {"overlong four bytes F0 80 80 80", {0xF0, 0x80, 0x80, 0x80}, 4, 0},
    {"overlong four bytes F0 8F BF BF", {0xF0, 0x8F, 0xBF, 0xBF}, 4, 0},
    {"surrogate D800", {0xED, 0xA0, 0x80}, 3, 0},
    {"surrogate DFFF", {0xED, 0xBF, 0xBF}, 3, 0},
    {"above U+10FFFF, F4 90", {0xF4, 0x90, 0x80, 0x80}, 4, 0},
    {"above U+10FFFF, F5", {0xF5, 0x80, 0x80, 0x80}, 4, 0},
    {"above U+10FFFF, F8", {0xF8, 0x88, 0x80, 0x80, 0x80}, 5, 0},
    {"FE", {0xFE}, 1, 0},
    {"FF", {0xFF}, 1, 0},
    {"stray continuation 80", {0x80}, 1, 0},
    {"stray continuation BF", {0xBF}, 1, 0},
    {"stray continuation after ascii", {'a', 0x80}, 2, 1},
    {"truncated two bytes", {0xC3}, 1, 0},
    {"truncated three bytes, one", {0xE2}, 1, 0},
    {"truncated three bytes, two", {0xE2, 0x82}, 2, 0},
    {"truncated four bytes, one", {0xF0}, 1, 0},
    {"truncated four bytes, two", {0xF0, 0x9F}, 2, 0},
    {"truncated four bytes, three", {0xF0, 0x9F, 0x98}, 3, 0},
    {"truncated after a valid prefix", {'a', 'b', 0xE2, 0x82}, 4, 2},
    {"truncated after a valid sequence", {0xC3, 0xA9, 0xF0, 0x9F, 0x98}, 5, 2},
    {"continuation is ascii", {0xC3, 'A'}, 2, 0},
    {"third byte is ascii", {0xE2, 0x82, 'A'}, 3, 0},
    {"fourth byte is a lead", {0xF0, 0x9F, 0x98, 0xC3}, 4, 0},
    {"valid, then invalid", {'a', 0xC3, 0xA9, 0xFF}, 4, 3},
    {"valid, then overlong", {0xE2, 0x82, 0xAC, 0xC0, 0x80}, 5, 3},
    {"valid, then a surrogate", {'a', 0xED, 0xA0, 0x80}, 4, 1}
};
/* clang-format on */

/* "a", U+00E9, U+20AC, U+1F600: one, two, three and four bytes. */
static const unsigned char u8mix[] = {'a',  0xC3, 0xA9, 0xE2, 0x82,
                                      0xAC, 0xF0, 0x9F, 0x98, 0x80};

static void test_utf8_valid(const lanes_ops* ops) {
    size_t               i, n, off, k, take, bound;
    static unsigned char d[MAXN];

    for (i = 0; i < sizeof u8cases / sizeof u8cases[0]; ++i) {
        const u8case*        c = &u8cases[i];
        const unsigned char* p = place(c->bytes, c->n, 0);
        unsigned char*       e = exact(c->bytes, c->n);

        check_at(ops->utf8_valid(p, c->n) == c->want, c->what, c->n, 0, i);
        check_at(ops->utf8_valid(e, c->n) == c->want, c->what, c->n, 1, i);
        free(e);
        for (off = 1; off <= MAXOFF; off += 7) {
            p = place(c->bytes, c->n, off);
            check_at(ops->utf8_valid(p, c->n) == c->want, c->what, c->n, off, i);
        }
    }
    check(ops->utf8_valid(NULL, 0) == 0, "utf8_valid of nothing is 0");

    /* The mixed text repeated and cut at every length: the answer is the
       last place a sequence ended at or before the cut. The poison, a
       continuation byte, would complete a cut sequence if it were read. */
    for (n = 0; n <= MAXN; ++n) {
        take = 0;
        for (k = 0; k < n; ++k) {
            d[k] = u8mix[k % sizeof u8mix];
        }
        bound = 0;
        for (k = 0; k <= n; ++k) {
            size_t at = k % sizeof u8mix;
            /* k is a boundary when it is the start of the pattern or of
               one of its characters. */
            if (at == 0 || at == 1 || at == 3 || at == 6) {
                bound = k;
            }
        }
        /* A cut at k = n is whole only if n is itself a boundary. */
        take = bound;
        for (off = 0; off <= MAXOFF; ++off) {
            const unsigned char* p = place(d, n, off);
            unsigned char*       e = exact(d, n);

            check_at(ops->utf8_valid(p, n) == take, "utf8_valid on the cut text", n,
                     off, take);
            check_at(ops->utf8_valid(e, n) == take, "utf8_valid on the cut text, exact",
                     n, off, take);
            free(e);
        }
    }
    /* A bad byte at every place among valid text: the prefix ends there. */
    for (n = 1; n <= 40; ++n) {
        for (k = 0; k < n; ++k) {
            size_t j;

            for (j = 0; j < n; ++j) {
                d[j] = 'a';
            }
            d[k] = 0xFF;
            check_at(ops->utf8_valid(place(d, n, 5), n) == k,
                     "utf8_valid stops at a bad byte", n, 5, k);
        }
    }
}

/* ---------------------------------------------------------------- *
 * Detection
 * ---------------------------------------------------------------- */

static void test_selection(void) {
    const lanes_ops* s     = lanes_scalar();
    unsigned         cpu   = lanes_cpu();
    unsigned         built = lanes_built();
    unsigned         bit;

    /* What CI's logs show of each runner. */
    printf("lanes %d.%d.%d: cpu 0x%03x, built 0x%03x, best %s\n", LANES_VERSION_MAJOR,
           LANES_VERSION_MINOR, LANES_VERSION_PATCH, cpu, built, lanes_best()->name);
    printf("  runs:");
    printf("%s", (cpu & LANES_ISA_SSE2) ? " sse2" : "");
    printf("%s", (cpu & LANES_ISA_SSE42) ? " sse42" : "");
    printf("%s", (cpu & LANES_ISA_AVX2) ? " avx2" : "");
    printf("%s", (cpu & LANES_ISA_AVX512) ? " avx512" : "");
    printf("%s", (cpu & LANES_ISA_NEON) ? " neon" : "");
    printf("%s", (cpu & LANES_ISA_SVE) ? " sve" : "");
    printf(" (and scalar, swar32, swar64)\n");

    check(s != NULL, "lanes_scalar is never NULL");
    if (s == NULL) {
        return; /* check() goes on; the rest would read through NULL */
    }
    check(s->size == sizeof(lanes_ops), "the table says its own size");
    check(strcmp(s->name, "scalar") == 0, "the scalar table is named scalar");
    check(s->isa == LANES_ISA_SCALAR, "the scalar table is the scalar bit");
    check(s->find_byte != NULL && s->find_any != NULL && s->find_lt != NULL &&
              s->ascii_run != NULL && s->utf8_valid != NULL && s->count_byte != NULL &&
              s->digits8 != NULL,
          "the scalar table has every operation");
    check(lanes_scalar() == s, "lanes_scalar returns one table");

    check((cpu & LANES_ISA_SCALAR) != 0, "a processor runs scalar");
    check((cpu & LANES_ISA_SWAR32) != 0 && (cpu & LANES_ISA_SWAR64) != 0,
          "a processor runs the word backends");
    check((built & LANES_ISA_SCALAR) != 0, "scalar is built");
    check(built == LANES_ISA_SCALAR, "only scalar is built so far");

    check(lanes_by_isa(LANES_ISA_SCALAR) == s,
          "lanes_by_isa(scalar) is the scalar table");
    for (bit = LANES_ISA_SWAR32; bit <= LANES_ISA_SVE; bit <<= 1) {
        check(lanes_by_isa(bit) == NULL, "a backend not built is NULL");
    }
    check(lanes_by_isa(0) == NULL, "no bit is no backend");
    check(lanes_by_isa(LANES_ISA_SCALAR | LANES_ISA_SWAR32) == NULL,
          "two bits are no backend");
    check(lanes_best() == s, "the best of what is built is scalar");

#if defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64)
    check((cpu & LANES_ISA_SSE2) != 0, "x86-64 has SSE2");
#endif
#if defined(__aarch64__) || defined(_M_ARM64)
    check((cpu & LANES_ISA_NEON) != 0, "AArch64 has NEON");
#endif
    /* Wider sets imply the narrower ones, on x86. */
    if (cpu & LANES_ISA_AVX512) {
        check((cpu & LANES_ISA_AVX2) != 0, "AVX-512 implies AVX2");
    }
    if (cpu & LANES_ISA_AVX2) {
        check((cpu & LANES_ISA_SSE42) != 0, "AVX2 implies SSE4.2");
    }
    if (cpu & LANES_ISA_SSE42) {
        check((cpu & LANES_ISA_SSE2) != 0, "SSE4.2 implies SSE2");
    }
    check(lanes_cpu() == cpu, "detection answers the same twice");
}

int main(void) {
    const lanes_ops* ops;

    test_selection();
    test_set();
    ops = lanes_scalar();
    test_find_byte(ops);
    test_find_any(ops);
    test_find_lt(ops);
    test_ascii_run(ops);
    test_count_byte(ops);
    test_digits8(ops);
    test_utf8_valid(ops);

    printf("test_lanes: %d checks, %d failed\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
