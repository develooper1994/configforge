/*
 * swar.c - the word-at-a-time backend: SIMD within a register, in C89.
 *
 * A word of unsigned long is read from any address with memcpy (no pointer
 * is cast, so strict aliasing holds and an unaligned address is the
 * compiler's business), and tested for a byte of interest a word, not a
 * byte, at a time. The width is that of unsigned long: 4 bytes where it is
 * 32 bits (isa SWAR32), 8 where it is 64 (SWAR64); C89 has no wider type.
 *
 * The tests only say "there is a match in this word". The index is then
 * found by the byte loop, from the word's start, at most one word of
 * bytes, so nothing here depends on the byte order, and the carry that
 * makes the tests exact only for the first match (the bytes above it may
 * be flagged wrongly) never reaches an answer.
 *
 * What the table holds, per operation, is chosen by LANES_SWAR_KEEP_<OP>
 * below: 1 where cf_bench lanes showed the word body winning (P-2's rule,
 * docs/plan/P-word-at-a-time.md), 0 where the entry is the scalar loop.
 * The word body of a loser stays in the file, compiled out by the #if,
 * to be measured again with the next compiler.
 *
 * CF_LANES_BACKENDS=scalar defines LANES_SCALAR_ONLY: this file is then
 * empty but for a lanes_swar() that returns NULL, so no word is read
 * through memcpy (MISRA C Rule 21.15) in that build.
 */

#include <limits.h>
#include <string.h>

#include "lanes/lanes.h"

#if defined(LANES_SCALAR_ONLY)
#define LANES_SWAR_BUILT 0
#elif defined(LANES_SWAR_WORD32) && UINT_MAX == 0xFFFFFFFFUL
/* Only for the tests: the 32-bit form on a machine whose long is wider, so
   that the one CI runs on Windows is also run where a build is 64-bit. */
#define LANES_SWAR_BUILT 1
#define LANES_SWAR_ISA LANES_ISA_SWAR32
#define LANES_SWAR_NAME "swar32"
typedef unsigned int swar_word;
#define SWAR_MAX UINT_MAX
#elif ULONG_MAX == 0xFFFFFFFFUL
#define LANES_SWAR_BUILT 1
#define LANES_SWAR_ISA LANES_ISA_SWAR32
#define LANES_SWAR_NAME "swar32"
typedef unsigned long swar_word;
#define SWAR_MAX ULONG_MAX
#elif ((ULONG_MAX >> 31) >> 1) == 0xFFFFFFFFUL
#define LANES_SWAR_BUILT 1
#define LANES_SWAR_ISA LANES_ISA_SWAR64
#define LANES_SWAR_NAME "swar64"
typedef unsigned long swar_word;
#define SWAR_MAX ULONG_MAX
#else
#define LANES_SWAR_BUILT 0
#endif

#if !LANES_SWAR_BUILT

const lanes_ops* lanes_swar(void) {
    return NULL;
}

#else

/* Which operations keep their word body (P-2's rule: the benchmark's). */
#define LANES_SWAR_KEEP_FIND_BYTE 1
#define LANES_SWAR_KEEP_FIND_ANY 0
#define LANES_SWAR_KEEP_FIND_LT 1
#define LANES_SWAR_KEEP_ASCII_RUN 1
#define LANES_SWAR_KEEP_UTF8_VALID 0
#define LANES_SWAR_KEEP_COUNT_BYTE 1

/* The tests build every word body into the table (LANES_SWAR_KEEP_ALL), so
   that a body the benchmark reverted is still tested. */
#ifdef LANES_SWAR_KEEP_ALL
#undef LANES_SWAR_KEEP_FIND_BYTE
#undef LANES_SWAR_KEEP_FIND_ANY
#undef LANES_SWAR_KEEP_FIND_LT
#undef LANES_SWAR_KEEP_ASCII_RUN
#undef LANES_SWAR_KEEP_UTF8_VALID
#undef LANES_SWAR_KEEP_COUNT_BYTE
#define LANES_SWAR_KEEP_FIND_BYTE 1
#define LANES_SWAR_KEEP_FIND_ANY 1
#define LANES_SWAR_KEEP_FIND_LT 1
#define LANES_SWAR_KEEP_ASCII_RUN 1
#define LANES_SWAR_KEEP_UTF8_VALID 1
#define LANES_SWAR_KEEP_COUNT_BYTE 1
#endif

/* A set is taken apart (one pass over its 32 bytes) only for a span at
   least this long; shorter, the loop over the bitmap is as fast. */
#define LANES_SWAR_ANY_MIN 128

/* How many bytes the scalar table is asked to validate at a byte of 0x80 or
   more, whole sequences only. */
#define LANES_SWAR_UTF8_WINDOW 512

/* At most this many tests of a word for find_any; more is the scalar. */
#define LANES_SWAR_ANY_TESTS 4

#define SWAR_W (sizeof(swar_word))
#define SWAR_ONES ((swar_word)(SWAR_MAX / 255UL))
#define SWAR_HIGHS ((swar_word)(SWAR_ONES * 128UL))
#define SWAR_LOW7 (~SWAR_HIGHS)

/* Non-zero when some byte of x is zero (exact for "is there one"; the
   bytes above the first zero may be flagged wrongly). */
#define SWAR_HAS_ZERO(x) ((((x) - SWAR_ONES) & ~(x)) & SWAR_HIGHS)
/* Non-zero when some byte of x is below k, for k <= 128 (the same). */
#define SWAR_HAS_LESS(x, k) ((((x) - SWAR_ONES * (swar_word)(k)) & ~(x)) & SWAR_HIGHS)

static swar_word swar_load(const unsigned char* p) {
    swar_word w;

    memcpy(&w, p, sizeof w);
    return w;
}

/* ---------------------------------------------------------------- *
 * The scalar table, for what the word does not do better
 * ---------------------------------------------------------------- */

#if !LANES_SWAR_KEEP_FIND_BYTE
static size_t swar_fallback_find_byte(const unsigned char* p, size_t n,
                                      unsigned char c) {
    return lanes_scalar()->find_byte(p, n, c);
}
#endif

#if !LANES_SWAR_KEEP_FIND_ANY
static size_t swar_fallback_find_any(const unsigned char* p, size_t n,
                                     const lanes_set* s) {
    return lanes_scalar()->find_any(p, n, s);
}
#endif

#if !LANES_SWAR_KEEP_FIND_LT
static size_t swar_fallback_find_lt(const unsigned char* p, size_t n, unsigned char c) {
    return lanes_scalar()->find_lt(p, n, c);
}
#endif

#if !LANES_SWAR_KEEP_ASCII_RUN
static size_t swar_fallback_ascii_run(const unsigned char* p, size_t n) {
    return lanes_scalar()->ascii_run(p, n);
}
#endif

#if !LANES_SWAR_KEEP_UTF8_VALID
static size_t swar_fallback_utf8_valid(const unsigned char* p, size_t n) {
    return lanes_scalar()->utf8_valid(p, n);
}
#endif

#if !LANES_SWAR_KEEP_COUNT_BYTE
static size_t swar_fallback_count_byte(const unsigned char* p, size_t n,
                                       unsigned char c) {
    return lanes_scalar()->count_byte(p, n, c);
}
#endif

static int swar_fallback_digits8(const unsigned char* p, unsigned long* value) {
    return lanes_scalar()->digits8(p, value);
}

/* ---------------------------------------------------------------- *
 * The word bodies
 * ---------------------------------------------------------------- */

static size_t swar_find_byte(const unsigned char* p, size_t n, unsigned char c) {
    size_t    i   = 0;
    swar_word rep = SWAR_ONES * (swar_word)c;

    while (n - i >= SWAR_W) {
        swar_word x = swar_load(p + i) ^ rep;

        if (SWAR_HAS_ZERO(x)) {
            break; /* the byte loop below finds it, within this swar_word */
        }
        i += SWAR_W;
    }
    for (; i < n; ++i) {
        if (p[i] == c) {
            return i;
        }
    }
    return n;
}

static size_t swar_find_lt(const unsigned char* p, size_t n, unsigned char c) {
    size_t i = 0;

    if (c == 0) {
        return n; /* nothing is below 0 */
    }
    /* The test is exact only up to 128; above, the byte loop alone. */
    while (c <= 128 && n - i >= SWAR_W) {
        swar_word x = swar_load(p + i);

        if (SWAR_HAS_LESS(x, c)) {
            break;
        }
        i += SWAR_W;
    }
    for (; i < n; ++i) {
        if (p[i] < c) {
            return i;
        }
    }
    return n;
}

static size_t swar_ascii_run(const unsigned char* p, size_t n) {
    size_t i = 0;

    while (n - i >= SWAR_W) {
        if (swar_load(p + i) & SWAR_HIGHS) {
            break;
        }
        i += SWAR_W;
    }
    while (i < n && p[i] < 0x80) {
        ++i;
    }
    return i;
}

static size_t swar_count_byte(const unsigned char* p, size_t n, unsigned char c) {
    size_t    i     = 0;
    size_t    count = 0;
    swar_word rep   = SWAR_ONES * (swar_word)c;

    while (n - i >= SWAR_W) {
        swar_word x = swar_load(p + i) ^ rep;
        /* The exact flag of each byte of x that is zero: its low seven
           bits plus 0x7F carry into bit 7 only when non-zero, and bit 7
           of x is or-ed in, so no byte reaches the next. */
        swar_word t = ~((((x & SWAR_LOW7) + SWAR_LOW7) | x)) & SWAR_HIGHS;

        /* One flag a byte, summed into the top byte by the multiplication. */
        count += (size_t)(((t >> 7) * SWAR_ONES) >> ((SWAR_W - 1) * 8));
        i += SWAR_W;
    }
    for (; i < n; ++i) {
        if (p[i] == c) {
            ++count;
        }
    }
    return count;
}

/* The tests of find_any: a low run [0, lt) as one find_lt test, and up to
   LANES_SWAR_ANY_TESTS - 1 or - 0 members as find_byte tests. */
typedef struct swar_any_shape {
    unsigned      lt;   /* the run [0, lt), lt <= 128; 0 for none */
    size_t        nmem; /* members past the run */
    unsigned char mem[LANES_SWAR_ANY_TESTS];
} swar_any_shape;

/* Take a set apart; 0 when it needs more tests than a word scan makes. */
static int swar_any_shape_of(const lanes_set* s, swar_any_shape* sh) {
    unsigned b, bit, v;
    unsigned run = 0;

    while (run < 128 && LANES_SET_HAS(s, run)) {
        ++run;
    }
    sh->lt   = run;
    sh->nmem = 0;
    for (b = 0; b < 32; ++b) {
        unsigned bits = s->bits[b];

        if (bits == 0) {
            continue;
        }
        for (bit = 0; bit < 8; ++bit) {
            v = b * 8u + bit;
            if (((bits >> bit) & 1u) && v >= run) {
                if (sh->nmem + (run != 0 ? 1u : 0u) >= LANES_SWAR_ANY_TESTS) {
                    return 0;
                }
                sh->mem[sh->nmem++] = (unsigned char)v;
            }
        }
    }
    return 1;
}

/* The word phase of find_any: how many leading bytes are known to hold no
   member (a multiple of the word, or n for the empty set), 0 when the set
   is not one a word scan can test. A function of its own so that the
   short spans, which never get here, run without its stack. */
static size_t swar_any_words(const unsigned char* p, size_t n, const lanes_set* s) {
    size_t         i = 0, j;
    swar_any_shape sh;
    swar_word      rep[LANES_SWAR_ANY_TESTS];

    if (!swar_any_shape_of(s, &sh)) {
        return 0;
    }
    if (sh.lt == 0 && sh.nmem == 0) {
        return n; /* the empty set finds nothing */
    }
    for (j = 0; j < sh.nmem; ++j) {
        rep[j] = SWAR_ONES * (swar_word)sh.mem[j];
    }
    while (n - i >= SWAR_W) {
        swar_word x   = swar_load(p + i);
        swar_word hit = sh.lt != 0 ? SWAR_HAS_LESS(x, sh.lt) : 0;

        for (j = 0; j < sh.nmem; ++j) {
            swar_word y = x ^ rep[j];

            hit |= SWAR_HAS_ZERO(y);
        }
        if (hit) {
            break;
        }
        i += SWAR_W;
    }
    return i;
}

static size_t swar_find_any(const unsigned char* p, size_t n, const lanes_set* s) {
    size_t i = 0;

    /* A span shorter than the minimum, or a set that needs more tests than
       a word scan makes, is the byte loop below: the scalar one. */
    if (n >= LANES_SWAR_ANY_MIN) {
        i = swar_any_words(p, n, s);
    }
    for (; i < n; ++i) {
        if (LANES_SET_HAS(s, p[i])) {
            return i;
        }
    }
    return n;
}

static size_t swar_utf8_valid(const unsigned char* p, size_t n) {
    size_t i = 0;

    if (n == 0) {
        return 0; /* no pointer arithmetic on a NULL */
    }
    for (;;) {
        size_t rest, take, got;

        i += swar_ascii_run(p + i, n - i);
        if (i == n) {
            return n;
        }
        /* A byte of 0x80 or more: the sequence's meaning is the scalar
           table's, asked of the next few bytes. It accepts whole
           sequences only, so what it takes ends on a boundary. */
        rest = n - i;
        take = rest < LANES_SWAR_UTF8_WINDOW ? rest : LANES_SWAR_UTF8_WINDOW;
        got  = lanes_scalar()->utf8_valid(p + i, take);
        if (take == rest || got == 0) {
            return i + got; /* the window reached n, or a bad byte: the end */
        }
        i += got;
    }
}

/* ---------------------------------------------------------------- *
 * The table
 * ---------------------------------------------------------------- */

#if LANES_SWAR_KEEP_FIND_BYTE
#define SWAR_ENTRY_FIND_BYTE swar_find_byte
#else
#define SWAR_ENTRY_FIND_BYTE swar_fallback_find_byte
#endif
#if LANES_SWAR_KEEP_FIND_ANY
#define SWAR_ENTRY_FIND_ANY swar_find_any
#else
#define SWAR_ENTRY_FIND_ANY swar_fallback_find_any
#endif
#if LANES_SWAR_KEEP_FIND_LT
#define SWAR_ENTRY_FIND_LT swar_find_lt
#else
#define SWAR_ENTRY_FIND_LT swar_fallback_find_lt
#endif
#if LANES_SWAR_KEEP_ASCII_RUN
#define SWAR_ENTRY_ASCII_RUN swar_ascii_run
#else
#define SWAR_ENTRY_ASCII_RUN swar_fallback_ascii_run
#endif
#if LANES_SWAR_KEEP_UTF8_VALID
#define SWAR_ENTRY_UTF8_VALID swar_utf8_valid
#else
#define SWAR_ENTRY_UTF8_VALID swar_fallback_utf8_valid
#endif
#if LANES_SWAR_KEEP_COUNT_BYTE
#define SWAR_ENTRY_COUNT_BYTE swar_count_byte
#else
#define SWAR_ENTRY_COUNT_BYTE swar_fallback_count_byte
#endif

/* A reverted body is compiled but not in the table; naming it here is what
   keeps a compiler from calling it unused. */
static void swar_touch(void) {
#if !LANES_SWAR_KEEP_FIND_BYTE
    (void)swar_find_byte;
#endif
#if !LANES_SWAR_KEEP_FIND_ANY
    (void)swar_find_any;
#endif
#if !LANES_SWAR_KEEP_FIND_LT
    (void)swar_find_lt;
#endif
#if !LANES_SWAR_KEEP_ASCII_RUN
    (void)swar_ascii_run;
#endif
#if !LANES_SWAR_KEEP_UTF8_VALID
    (void)swar_utf8_valid;
#endif
#if !LANES_SWAR_KEEP_COUNT_BYTE
    (void)swar_count_byte;
#endif
}

static const lanes_ops swar_ops = {sizeof(lanes_ops),     LANES_SWAR_NAME,
                                   LANES_SWAR_ISA,        SWAR_ENTRY_FIND_BYTE,
                                   SWAR_ENTRY_FIND_ANY,   SWAR_ENTRY_FIND_LT,
                                   SWAR_ENTRY_ASCII_RUN,  SWAR_ENTRY_UTF8_VALID,
                                   SWAR_ENTRY_COUNT_BYTE, swar_fallback_digits8};

const lanes_ops* lanes_swar(void) {
    if (swar_ops.size == 0) {
        swar_touch(); /* never: the table says its size */
    }
    return &swar_ops;
}

#endif /* LANES_SWAR_BUILT */
