/*
 * numeric.c - locale-independent numeric conversion.
 *
 * C89. No <ctype.h>, no <locale.h>, no strtod, no printf.
 *
 * Floating point formatting is exact integer digit generation, the
 * Steele & White algorithm in the Burger & Dybvig formulation. It
 * emits the shortest decimal string that reads back as the same
 * double, and it produces identical bytes on every platform because
 * every step is integer arithmetic.
 */

#include "configforge/cf_numeric.h"
#include "configforge/cf_build_options.h"
#include "configforge/cf_port.h"

#include <string.h>

#if CF_WITH_REAL
#include <math.h> /* frexp, ldexp, fabs - none of them locale-sensitive */
#include <float.h>
#endif

/* ================================================================== *
 * Integers
 * ================================================================== */

cf_bool cf_text_to_u64(const char* s, size_t len, cf_u64* out, size_t* end) {
    cf_u64 acc = 0;
    size_t i   = 0;

    if (s == NULL || out == NULL || len == 0) {
        return CF_FALSE;
    }
    if (!cf_is_digit((unsigned char)s[0])) {
        return CF_FALSE;
    }
    while (i < len && cf_is_digit((unsigned char)s[i])) {
        cf_u64 d = (cf_u64)(s[i] - '0');
        if (acc > (CF_U64_MAX - d) / (cf_u64)10) {
            return CF_FALSE;
        }
        acc = acc * (cf_u64)10 + d;
        ++i;
    }
    if (end != NULL) {
        *end = i;
    } else if (i != len) {
        return CF_FALSE;
    }
    *out = acc;
    return CF_TRUE;
}

cf_bool cf_text_to_i64(const char* s, size_t len, cf_i64* out, size_t* end) {
    cf_u64 mag      = 0;
    size_t i        = 0;
    size_t consumed = 0;
    int    neg      = 0;

    if (s == NULL || out == NULL || len == 0) {
        return CF_FALSE;
    }
    if (s[0] == '+' || s[0] == '-') {
        neg = (s[0] == '-');
        i   = 1;
        if (i >= len) {
            return CF_FALSE;
        }
    }
    if (!cf_text_to_u64(s + i, len - i, &mag, &consumed)) {
        return CF_FALSE;
    }
    if (end != NULL) {
        *end = i + consumed;
    } else if (i + consumed != len) {
        return CF_FALSE;
    }

    if (neg) {
        if (mag > (cf_u64)CF_I64_MAX + (cf_u64)1) {
            return CF_FALSE;
        }
        if (mag == (cf_u64)CF_I64_MAX + (cf_u64)1) {
            *out = CF_I64_MIN;
        } else {
            *out = -(cf_i64)mag;
        }
    } else {
        if (mag > (cf_u64)CF_I64_MAX) {
            return CF_FALSE;
        }
        *out = (cf_i64)mag;
    }
    return CF_TRUE;
}

/* ================================================================== *
 * Floating point
 * ================================================================== */

#if CF_WITH_REAL

/* ------------------------------------------------------------------ *
 * Fixed-capacity big integer.
 *
 * The digit generator reaches about 2^1075 for subnormal inputs, which
 * is 34 limbs. The parser reaches further, to a 10^613 denominator of
 * 2037 bits, and CF_PARSE_DIGITS is chosen so that it stops there; 64
 * limbs keeps each value at 260 bytes.
 * ------------------------------------------------------------------ */

#define CF_BIG_LIMBS 64

typedef struct cf_big {
    int    n;               /* limbs in use; 0 means zero      */
    cf_u32 d[CF_BIG_LIMBS]; /* little-endian, base 2^32        */
} cf_big;

static void big_zero(cf_big* b) {
    b->n = 0;
}

/* The limb count is bounded by the format, not by luck: 2^1075 is the
   widest quantity the digit generator reaches, and the parser's bound is
   CF_PARSE_DIGITS, both inside the 64 limbs allocated. Enforcing it anyway costs one
   comparison per growth and turns "cannot happen" into "does not happen" - the
   difference the static analyser was pointing at when it could not prove the bound
   through big_add and big_shl. */
static void big_clamp(cf_big* b) {
    if (b->n > CF_BIG_LIMBS) {
        b->n = CF_BIG_LIMBS;
    } else if (b->n < 0) {
        b->n = 0;
    }
}

/* The count of one that is only read, held to the limbs the same way. */
static int big_count(const cf_big* b) {
    if (b->n > CF_BIG_LIMBS) {
        return CF_BIG_LIMBS;
    }
    return (b->n < 0) ? 0 : b->n;
}

static void big_set_u64(cf_big* b, cf_u64 v) {
    big_zero(b);
    if (v == 0) {
        return;
    }
    b->d[0] = (cf_u32)(v & (cf_u64)0xFFFFFFFF);
    b->d[1] = (cf_u32)(v >> 32);
    b->n    = (b->d[1] != 0) ? 2 : 1;
}

static void big_copy(cf_big* dst, const cf_big* src) {
    int n = big_count(src);
    int i;

    dst->n = n;
    for (i = 0; i < n; ++i) {
        dst->d[i] = src->d[i];
    }
}

static int big_is_zero(const cf_big* b) {
    return b->n == 0;
}

static void big_trim(cf_big* b) {
    big_clamp(b);
    while (b->n > 0 && b->d[b->n - 1] == 0) {
        b->n--;
    }
}

static int big_bitlen(const cf_big* b) {
    cf_u32 top;
    int    bits;
    int    n = b->n;

    /* Every writer keeps n within the limbs; holding it there here too
       is what lets a reader of this function alone see the index fits. */
    if (n <= 0) {
        return 0;
    }
    if (n > CF_BIG_LIMBS) {
        n = CF_BIG_LIMBS;
    }
    top  = b->d[n - 1];
    bits = 0;
    while (top != 0) {
        top >>= 1;
        ++bits;
    }
    return (n - 1) * 32 + bits;
}

static int big_cmp(const cf_big* a, const cf_big* b) {
    int i;
    if (a->n != b->n) {
        return (a->n < b->n) ? -1 : 1;
    }
    /* The limbs read are bounded by the array at both ends, as big_clamp
       bounds the ones written: "cannot exceed it" made "does not" - a
       negative count would make n - 1 overflow to the far end. */
    i = (a->n <= 0) ? 0 : (a->n < CF_BIG_LIMBS) ? a->n : CF_BIG_LIMBS;
    for (--i; i >= 0; --i) {
        if (a->d[i] != b->d[i]) {
            return (a->d[i] < b->d[i]) ? -1 : 1;
        }
    }
    return 0;
}

/* a += b */
static void big_add(cf_big* a, const cf_big* b) {
    cf_u64 carry = 0;
    int    bn    = big_count(b);
    int    n;
    int    i;

    big_clamp(a);
    n = (a->n > bn) ? a->n : bn;
    for (i = 0; i < n || carry != 0; ++i) {
        cf_u64 sum = carry;
        if (i >= CF_BIG_LIMBS) {
            break;
        }
        if (i < a->n) {
            sum += (cf_u64)a->d[i];
        }
        if (i < bn) {
            sum += (cf_u64)b->d[i];
        }
        a->d[i] = (cf_u32)(sum & (cf_u64)0xFFFFFFFF);
        carry   = sum >> 32;
        if (i >= a->n) {
            a->n = i + 1;
        }
    }
    if (i > a->n) {
        a->n = i;
    }
    big_clamp(a);
    big_trim(a);
}

/* a -= b, requires a >= b */
static void big_sub(cf_big* a, const cf_big* b) {
    cf_i64 borrow = 0;
    int    bn     = big_count(b);
    int    i;

    big_clamp(a);
    for (i = 0; i < a->n; ++i) {
        cf_i64 diff = (cf_i64)a->d[i] - borrow;
        if (i < bn) {
            diff -= (cf_i64)b->d[i];
        }
        if (diff < 0) {
            diff += ((cf_i64)1 << 32);
            borrow = 1;
        } else {
            borrow = 0;
        }
        a->d[i] = (cf_u32)diff;
    }
    big_trim(a);
}

/* a *= m */
static void big_mul_u32(cf_big* a, cf_u32 m) {
    cf_u64 carry = 0;
    int    i;

    big_clamp(a);
    if (m == 1 || a->n == 0) {
        if (m == 0) {
            big_zero(a);
        }
        return;
    }
    for (i = 0; i < a->n; ++i) {
        cf_u64 prod = (cf_u64)a->d[i] * (cf_u64)m + carry;
        a->d[i]     = (cf_u32)(prod & (cf_u64)0xFFFFFFFF);
        carry       = prod >> 32;
    }
    while (carry != 0 && a->n < CF_BIG_LIMBS) {
        a->d[a->n++] = (cf_u32)(carry & (cf_u64)0xFFFFFFFF);
        carry >>= 32;
    }
    big_clamp(a);
    big_trim(a);
}

/* a <<= bits */
static void big_shl(cf_big* a, int bits) {
    int limbs = bits / 32;
    int rem   = bits % 32;
    int i;

    big_clamp(a);
    if (a->n == 0 || bits == 0) {
        return;
    }
    if (rem != 0) {
        cf_u32 carry = 0;
        for (i = 0; i < a->n; ++i) {
            cf_u32 v = a->d[i];
            a->d[i]  = (cf_u32)((v << rem) | carry);
            carry    = (cf_u32)(v >> (32 - rem));
        }
        if (carry != 0 && a->n < CF_BIG_LIMBS) {
            a->d[a->n++] = carry;
        }
    }
    if (limbs > 0) {
        if (limbs >= CF_BIG_LIMBS) {
            /* Everything shifted past the end. Zero is the only answer
               the format can represent, and returning a partly-shifted
               value would be worse than losing it visibly. */
            big_zero(a);
            return;
        }
        if (a->n + limbs > CF_BIG_LIMBS) {
            a->n = CF_BIG_LIMBS - limbs;
        }
        for (i = a->n - 1; i >= 0; --i) {
            a->d[i + limbs] = a->d[i];
        }
        for (i = 0; i < limbs; ++i) {
            a->d[i] = 0;
        }
        a->n += limbs;
        big_clamp(a);
    }
    big_trim(a);
}

/* a *= 10 */
static void big_mul10(cf_big* a) {
    big_mul_u32(a, 10);
}

/* a *= 2^k */
static void big_mul_pow2(cf_big* a, int k) {
    big_shl(a, k);
}

/* q = floor(a / b), a = a mod b, with 0 <= q <= 9. Requires a < 10*b. */
static int big_div_digit(cf_big* a, const cf_big* b) {
    int q = 0;
    while (big_cmp(a, b) >= 0) {
        big_sub(a, b);
        ++q;
        if (q > 10) {
            break; /* precondition violated; cannot happen */
        }
    }
    return q;
}

/* ------------------------------------------------------------------ *
 * Shortest round-trip digit generation.
 *
 * Produces digits d1..dn and an exponent k such that
 *     v = 0.d1d2...dn x 10^k
 * and no shorter string reads back as the same double.
 * ------------------------------------------------------------------ */

typedef struct cf_digits {
    char d[24];
    int  n;
    int  k;
} cf_digits;

static void cf_gen_digits(double v, cf_digits* out) {
    cf_big R, S, mp, mm, t;
    cf_u64 f;
    int    e, is_boundary, k, n, low, high, d;
    int    e2;

    out->n = 0;
    out->k = 0;

    if (v < DBL_MIN) {
        /* Subnormal: v = f * 2^-1074 exactly, and neighbours are
           evenly spaced, so the gap is symmetric. */
        double scaled = ldexp(v, 1074);
        f             = (cf_u64)scaled;
        e             = -1074;
        is_boundary   = 0;
    } else {
        double scaled;
        double m;
        m      = frexp(v, &e2); /* v = m * 2^e2, 0.5 <= m < 1 */
        scaled = ldexp(m, 53);  /* 2^52 <= scaled < 2^53      */
        f      = (cf_u64)scaled;
        e      = e2 - 53;
        /* The gap below is half the gap above only at a power of two,
           and not at the smallest normal. */
        is_boundary = (f == ((cf_u64)1 << 52)) && (e2 > -1021);
    }

    if (e >= 0) {
        big_set_u64(&R, f);
        big_mul_pow2(&R, e + (is_boundary ? 2 : 1));
        big_set_u64(&S, is_boundary ? (cf_u64)4 : (cf_u64)2);
        big_set_u64(&mp, (cf_u64)1);
        big_mul_pow2(&mp, e + (is_boundary ? 1 : 0));
        big_set_u64(&mm, (cf_u64)1);
        big_mul_pow2(&mm, e);
    } else {
        big_set_u64(&R, f);
        big_mul_pow2(&R, is_boundary ? 2 : 1);
        big_set_u64(&S, (cf_u64)1);
        big_mul_pow2(&S, (-e) + (is_boundary ? 2 : 1));
        big_set_u64(&mp, is_boundary ? (cf_u64)2 : (cf_u64)1);
        big_set_u64(&mm, (cf_u64)1);
    }

    /* Scale so that 1/10 < (R + mp) / S <= 1. */
    k = 0;
    for (;;) {
        big_copy(&t, &R);
        big_add(&t, &mp);
        if (big_cmp(&t, &S) > 0) {
            big_mul10(&S);
            ++k;
            continue;
        }
        big_copy(&t, &R);
        big_add(&t, &mp);
        big_mul10(&t);
        if (big_cmp(&t, &S) <= 0) {
            big_mul10(&R);
            big_mul10(&mp);
            big_mul10(&mm);
            --k;
            continue;
        }
        break;
    }

    n = 0;
    for (;;) {
        big_mul10(&R);
        big_mul10(&mp);
        big_mul10(&mm);

        d = big_div_digit(&R, &S);

        low = (big_cmp(&R, &mm) < 0);
        big_copy(&t, &R);
        big_add(&t, &mp);
        high = (big_cmp(&t, &S) > 0);

        if (low || high) {
            break;
        }
        if (n < (int)sizeof out->d - 1) {
            out->d[n++] = (char)('0' + d);
        }
    }

    if (low && !high) {
        /* keep d */
    } else if (high && !low) {
        ++d;
    } else {
        cf_big r2;
        big_copy(&r2, &R);
        big_mul_u32(&r2, 2);
        if (big_cmp(&r2, &S) >= 0) {
            ++d;
        }
    }
    if (n < (int)sizeof out->d - 1) {
        out->d[n++] = (char)('0' + d);
    }

    /* The final digit may have been rounded past nine, and the carry
       has to travel. The first version only looked at the leading
       digit, which is correct for a one-digit result and silently
       wrong for "1:" - clang-tidy noticed the dead store that was the
       symptom of it. */
    {
        int j;
        for (j = n - 1; j > 0; --j) {
            if (out->d[j] != ':') {
                break;
            }
            out->d[j] = '0';
            ++out->d[j - 1];
        }
        if (out->d[0] == ':') {
            /* 0.99..9 rounded up: one digit, one larger exponent. */
            out->d[0] = '1';
            n         = 1;
            ++k;
        }
        /* A carry can leave a trailing zero behind, and the algorithm
           promises the shortest string that round-trips. */
        while (n > 1 && out->d[n - 1] == '0') {
            --n;
        }
    }

    out->n = n;
    out->k = k;
}

/* ------------------------------------------------------------------ *
 * Formatting
 *
 * Fixed notation when the decimal exponent is in [-4, 16), scientific
 * otherwise. The boundary is a readability choice, not a numeric one,
 * and it is fixed here so that output is identical everywhere. A decimal point or an
 * exponent is always present so that the text is recognisably CF_REAL rather than
 * CF_INT.
 * ------------------------------------------------------------------ */

static size_t put(char* buf, size_t cap, size_t at, char c) {
    if (at + 1 < cap) {
        buf[at] = c;
    }
    return at + 1;
}

size_t cf_f64_to_text(double v, char* buf, size_t cap) {
    cf_digits g;
    size_t    at = 0;
    int       exp10, i;

    if (buf == NULL || cap == 0) {
        return 0;
    }
    /* Reject NaN and infinity: a configuration value cannot be either,
       and no data format this library writes has a spelling for them. */
    if (!(v == v) || v > DBL_MAX || v < -DBL_MAX) {
        buf[0] = '\0';
        return 0;
    }
    if (v == 0.0) {
        if (cap < 4) {
            buf[0] = '\0';
            return 0;
        }
        memcpy(buf, "0.0", 4);
        return 3;
    }
    if (v < 0.0) {
        at = put(buf, cap, at, '-');
        v  = -v;
    }

    cf_gen_digits(v, &g);
    exp10 = g.k - 1;

    if (exp10 >= -4 && exp10 < 16) {
        int z;
        if (g.k <= 0) {
            at = put(buf, cap, at, '0');
            at = put(buf, cap, at, '.');
            for (z = 0; z < -g.k; ++z) {
                at = put(buf, cap, at, '0');
            }
            for (i = 0; i < g.n; ++i) {
                at = put(buf, cap, at, g.d[i]);
            }
        } else if (g.k >= g.n) {
            for (i = 0; i < g.n; ++i) {
                at = put(buf, cap, at, g.d[i]);
            }
            for (z = 0; z < g.k - g.n; ++z) {
                at = put(buf, cap, at, '0');
            }
            at = put(buf, cap, at, '.');
            at = put(buf, cap, at, '0');
        } else {
            for (i = 0; i < g.k; ++i) {
                at = put(buf, cap, at, g.d[i]);
            }
            at = put(buf, cap, at, '.');
            for (; i < g.n; ++i) {
                at = put(buf, cap, at, g.d[i]);
            }
        }
    } else {
        char   eb[8];
        size_t en, ei;

        at = put(buf, cap, at, g.d[0]);
        at = put(buf, cap, at, '.');
        if (g.n == 1) {
            at = put(buf, cap, at, '0');
        } else {
            for (i = 1; i < g.n; ++i) {
                at = put(buf, cap, at, g.d[i]);
            }
        }
        at = put(buf, cap, at, 'e');
        at = put(buf, cap, at, (exp10 < 0) ? '-' : '+');
        en = cf_u64_to_text((cf_u64)((exp10 < 0) ? -exp10 : exp10), eb, sizeof eb);
        if (en < 2) {
            at = put(buf, cap, at, '0');
        }
        for (ei = 0; ei < en; ++ei) {
            at = put(buf, cap, at, eb[ei]);
        }
    }

    if (at + 1 > cap) {
        buf[0] = '\0';
        return 0;
    }
    buf[at] = '\0';
    return at;
}

/* ------------------------------------------------------------------ *
 * Parsing
 *
 * Fast path: when the significand fits in 53 bits and the power of ten
 * is exactly representable, one multiply or divide is correctly
 * rounded (Clinger). Slow path: exact big-integer long division.
 * ------------------------------------------------------------------ */

static const double cf_pow10_exact[23] = {
    1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,  1e8,  1e9,  1e10, 1e11,
    1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22};

/* At most this many significant digits go into the big integer; the rest
   only move the exponent and, when any is not zero, set a sticky flag.
   The bound is the big integer's width, not the 767 digits a double's
   halfway points can need: a value cf_text_to_f64 lets through is at
   least 10^-324, so its denominator is at most 10^(digits + 323), and
   that has to fit in 2048 bits with one to spare for the long
   division's shift: 10^613 is 2037 bits. Truncation with the sticky
   flag rounds every input of up to 290 digits correctly, and a longer
   one correctly unless its first 290 digits are those of a halfway
   point between two doubles and the rest are not - an input built for
   the purpose, and still within one unit in the last place. */
#define CF_PARSE_DIGITS 290

static double cf_slow_path(const cf_big* digits, int exp10, int neg, int lost) {
    cf_big N, D;
    cf_u64 q = 0;
    int    i, bn, bd, shiftN, shiftD, exponent, bits, guard;
    double r;

    big_copy(&N, digits);
    big_set_u64(&D, (cf_u64)1);

    if (exp10 > 0) {
        for (i = 0; i < exp10; ++i) {
            big_mul10(&N);
        }
    } else if (exp10 < 0) {
        for (i = 0; i < -exp10; ++i) {
            big_mul10(&D);
        }
    }

    if (big_is_zero(&N)) {
        return neg ? -0.0 : 0.0;
    }

    /* Align so that 1 <= N/D < 2. */
    bn     = big_bitlen(&N);
    bd     = big_bitlen(&D);
    shiftN = 0;
    shiftD = 0;
    if (bn > bd) {
        shiftD = bn - bd;
    } else {
        shiftN = bd - bn;
    }
    big_shl(&N, shiftN);
    big_shl(&D, shiftD);
    if (big_cmp(&N, &D) < 0) {
        big_shl(&N, 1);
        shiftN += 1;
    }

    /* Round once at the destination precision. Subnormals have a fixed
       unit of 2^-1074; rounding to 53 bits first can manufacture a tie. */
    exponent = shiftD - shiftN;
    if (exponent < -1075) {
        return neg ? -0.0 : 0.0;
    }
    bits = exponent < -1022 ? exponent + 1075 : 53;
    for (i = 0; i < bits; ++i) {
        q <<= 1;
        if (big_cmp(&N, &D) >= 0) {
            big_sub(&N, &D);
            q |= (cf_u64)1;
        }
        big_shl(&N, 1);
    }
    /* N/D is twice the remaining fraction. At exactly one, decimal
       digits omitted from N break the tie; otherwise choose even. */
    guard = big_cmp(&N, &D);
    if (guard > 0 || (guard == 0 && (lost || (q & (cf_u64)1) != 0))) {
        ++q;
    }

    r = ldexp((double)q, exponent - bits + 1);
    return neg ? -r : r;
}

/* Where the digit count and the power of ten stop counting. Each moves
   by one a digit, and text of more than 2^31 digits would overflow an
   int. Past these the answer is already decided: a digit count beyond
   CF_PARSE_DIGITS and 19 is read only as "more than both", and a power
   of ten beyond ten million stays beyond the 309 and -400 the range
   checks test even after the exponent part, which is read to no more
   than 999999, moves it, so it is refused as it was. A count that
   stops changes no comparison made of it, and every result is as
   before. */
#define NUMERIC_NDIG_CLAMP (CF_PARSE_DIGITS + 20)
#define NUMERIC_EXP10_CLAMP 10000000

cf_bool cf_text_to_f64(const char* s, size_t len, double* out, size_t* end) {
    cf_big digits;
    cf_u64 fast = 0;
    size_t i    = 0;
    int    neg = 0, any = 0, fast_ok = 1, ndig = 0, dropped = 0;
    int    exp10   = 0;
    int    mag     = 0;
    long   exppart = 0;

    if (s == NULL || out == NULL || len == 0) {
        return CF_FALSE;
    }
    big_set_u64(&digits, (cf_u64)0);

    if (s[0] == '+' || s[0] == '-') {
        neg = (s[0] == '-');
        i   = 1;
    }

    while (i < len && cf_is_digit((unsigned char)s[i])) {
        cf_u32 dv = (cf_u32)(s[i] - '0');
        any       = 1;
        if (ndig >= CF_PARSE_DIGITS) {
            /* Past the digits kept: it still counts ten times over. */
            if (exp10 < NUMERIC_EXP10_CLAMP) {
                ++exp10;
            }
            dropped |= (dv != 0);
        } else {
            big_mul_u32(&digits, 10);
            if (dv != 0) {
                cf_big dd;
                big_set_u64(&dd, (cf_u64)dv);
                big_add(&digits, &dd);
            }
        }
        if (ndig < 19) {
            fast = fast * (cf_u64)10 + (cf_u64)dv;
        } else {
            fast_ok = 0;
        }
        if ((ndig > 0 || dv != 0) && ndig < NUMERIC_NDIG_CLAMP) {
            ++ndig;
        }
        ++i;
    }

    if (i < len && s[i] == '.') {
        ++i;
        while (i < len && cf_is_digit((unsigned char)s[i])) {
            cf_u32 dv = (cf_u32)(s[i] - '0');
            cf_big dd;
            any = 1;
            if (ndig >= CF_PARSE_DIGITS) {
                /* Past the digits kept: below them, so only sticky. */
                dropped |= (dv != 0);
                ++i;
                continue;
            }
            big_mul_u32(&digits, 10);
            if (dv != 0) {
                big_set_u64(&dd, (cf_u64)dv);
                big_add(&digits, &dd);
            }
            if (ndig < 19) {
                fast = fast * (cf_u64)10 + (cf_u64)dv;
            } else {
                fast_ok = 0;
            }
            if (ndig > 0 || dv != 0) {
                ++ndig;
            }
            if (exp10 > -NUMERIC_EXP10_CLAMP) {
                --exp10;
            }
            ++i;
        }
    }
    if (!any) {
        return CF_FALSE;
    }

    if (i < len && (s[i] == 'e' || s[i] == 'E')) {
        int    esign  = 0;
        size_t before = i;
        int    edig   = 0;
        ++i;
        if (i < len && (s[i] == '+' || s[i] == '-')) {
            esign = (s[i] == '-');
            ++i;
        }
        while (i < len && cf_is_digit((unsigned char)s[i])) {
            if (exppart < 100000L) {
                exppart = exppart * 10 + (long)(s[i] - '0');
            }
            ++edig;
            ++i;
        }
        if (edig == 0) {
            i = before; /* not an exponent after all */
        } else {
            exp10 += (int)(esign ? -exppart : exppart);
        }
    }

    if (end != NULL) {
        *end = i;
    } else if (i != len) {
        return CF_FALSE;
    }

    if (fast_ok && ndig <= 19 && fast <= ((cf_u64)1 << 53)) {
        if (exp10 == 0) {
            *out = neg ? -(double)fast : (double)fast;
            return CF_TRUE;
        }
        if (exp10 > 0 && exp10 <= 22) {
            double r = (double)fast * cf_pow10_exact[exp10];
            *out     = neg ? -r : r;
            return CF_TRUE;
        }
        if (exp10 < 0 && exp10 >= -22) {
            double r = (double)fast / cf_pow10_exact[-exp10];
            *out     = neg ? -r : r;
            return CF_TRUE;
        }
    }

    if (big_is_zero(&digits)) {
        /* Zero is zero at any scale, and 10^exp10 need not be built. */
        *out = neg ? -0.0 : 0.0;
        return CF_TRUE;
    }

    /* The value is in [10^(mag-1), 10^mag). From 10^309 up it is past
       the largest double. Below 10^-324 it is under half the smallest
       subnormal and rounds to zero, which is the answer down to 10^-400
       (the bound this function has always had); further down the text
       is refused as no number anyone meant. */
    mag = exp10 + ((ndig < CF_PARSE_DIGITS) ? ndig : CF_PARSE_DIGITS);
    if (mag > 309 || mag < -400) {
        return CF_FALSE;
    }
    if (mag < -323) {
        *out = neg ? -0.0 : 0.0;
        return CF_TRUE;
    }

    /* Between DBL_MAX and 10^309 the rounding can still reach 2^1024,
       which is infinity: refuse it rather than put a non-finite real
       into the key space, where nothing could write it back. */
    {
        double r = cf_slow_path(&digits, exp10, neg, dropped);
        if (!(r <= DBL_MAX && r >= -DBL_MAX)) {
            return CF_FALSE;
        }
        *out = r;
    }
    return CF_TRUE;
}

/* A double travels as its bit pattern. memcpy rather than a union or a
   cast: it is the one spelling that is defined on every compiler and
   optimises to the same instruction on all of them. */
cf_u64 cf_f64_bits(double d) {
    cf_u64 v = 0;
    memcpy(&v, &d, (sizeof d < sizeof v) ? sizeof d : sizeof v);
    return v;
}

double cf_f64_from_bits(cf_u64 bits) {
    double d = 0.0;
    memcpy(&d, &bits, (sizeof d < sizeof bits) ? sizeof d : sizeof bits);
    return d;
}

/* Half and single precision as RFC 8949 Appendix D reads them: the
   exponent 0 is subnormal, the all-ones one infinity or NaN. The sign
   is applied last, so -0 and -infinity come out as themselves. */
double cf_f64_from_f16_bits(cf_u32 bits) {
    cf_u32 exp  = (bits >> 10) & 0x1Fu;
    cf_u32 mant = bits & 0x3FFu;
    double val;

    if (exp == 0) {
        val = ldexp((double)mant, -24);
    } else if (exp != 31) {
        val = ldexp((double)(mant + 1024u), (int)exp - 25);
    } else if (mant == 0) {
        val = HUGE_VAL;
    } else {
        val = cf_f64_from_bits((cf_u64)0x7FF80000u << 32);
    }
    return ((bits & 0x8000u) != 0) ? -val : val;
}

double cf_f64_from_f32_bits(cf_u32 bits) {
    cf_u32 exp  = (bits >> 23) & 0xFFu;
    cf_u32 mant = bits & 0x7FFFFFu;
    double val;

    if (exp == 0) {
        val = ldexp((double)mant, -149);
    } else if (exp != 255) {
        val = ldexp((double)(mant | 0x800000u), (int)exp - 150);
    } else if (mant == 0) {
        val = HUGE_VAL;
    } else {
        val = cf_f64_from_bits((cf_u64)0x7FF80000u << 32);
    }
    return ((bits & 0x80000000u) != 0) ? -val : val;
}

#endif /* CF_WITH_REAL */

/* ================================================================== *
 * Tier 0 type inference
 *
 * Deliberately timid. "yes", "no", "on", "off" stay text: YAML 1.1
 * reads NO as boolean false, which turns Norway's country code into
 * false. Leading zeros stay text, so 08 is not an octal accident and
 * the version string 1.10 does not become the number 1.1.
 * ================================================================== */

static cf_bool cf_looks_real(const char* s, size_t len) {
    size_t i             = 0;
    int    digits_before = 0;
    int    digits_after  = 0;
    int    seen_dot      = 0;
    int    seen_exp      = 0;

    if (len == 0) {
        return CF_FALSE;
    }
    if (s[0] == '+' || s[0] == '-') {
        i = 1;
    }
    while (i < len && cf_is_digit((unsigned char)s[i])) {
        ++i;
        ++digits_before;
    }

    if (i < len && s[i] == '.') {
        seen_dot = 1;
        ++i;
        while (i < len && cf_is_digit((unsigned char)s[i])) {
            ++i;
            ++digits_after;
        }
    }
    if (digits_before == 0 && digits_after == 0) {
        return CF_FALSE;
    }
    if (i < len && (s[i] == 'e' || s[i] == 'E')) {
        int expd = 0;
        seen_exp = 1;
        ++i;
        if (i < len && (s[i] == '+' || s[i] == '-')) {
            ++i;
        }
        while (i < len && cf_is_digit((unsigned char)s[i])) {
            ++i;
            ++expd;
        }
        if (expd == 0) {
            return CF_FALSE;
        }
    }
    if (i != len) {
        return CF_FALSE;
    }
    return (seen_dot || seen_exp) ? CF_TRUE : CF_FALSE;
}

static cf_bool cf_has_leading_zero(const char* s, size_t len) {
    size_t i = 0;

    if (len == 0) {
        return CF_FALSE;
    }
    if (s[0] == '+' || s[0] == '-') {
        i = 1;
    }
    if (len - i < 2) {
        return CF_FALSE;
    }
    return (s[i] == '0') ? CF_TRUE : CF_FALSE;
}

cf_scalar_kind cf_infer_scalar(const char* s, size_t len) {
    cf_i64 iv;

    if (s == NULL || len == 0) {
        return CF_SCALAR_TEXT;
    }
    if (len == 4 && memcmp(s, "true", 4) == 0) {
        return CF_SCALAR_BOOL;
    }
    if (len == 5 && memcmp(s, "false", 5) == 0) {
        return CF_SCALAR_BOOL;
    }
    if (cf_has_leading_zero(s, len)) {
        return CF_SCALAR_TEXT;
    }
    if (cf_text_to_i64(s, len, &iv, NULL)) {
        return CF_SCALAR_INT;
    }
    if (cf_looks_real(s, len)) {
        return CF_SCALAR_REAL;
    }
    return CF_SCALAR_TEXT;
}

/* ================================================================== *
 * Units: durations and sizes (D-113, D-222)
 *
 * Everything below is integer arithmetic, so a fraction ("1.5h") is read
 * exactly, with CF_WITH_REAL off as with it on, and the same on every
 * platform. A term is a decimal number, kept as its digits and the count
 * of those after the point, times its factor in the table's finest unit,
 * in a number of 192 bits: 38 digits and a factor of 2^50 are below that,
 * so no step of the sum overflows unnoticed. What does not fit a cf_i64
 * at the end is refused.
 * ================================================================== */

#define UBIG_LIMBS 6
#define UNIT_DIGITS 38

typedef struct ubig {
    cf_u32 w[UBIG_LIMBS]; /* least significant limb first */
} ubig;

static void ubig_zero(ubig* a) {
    int i;

    for (i = 0; i < UBIG_LIMBS; ++i) {
        a->w[i] = 0;
    }
}

/* a = a * m + add, CF_FALSE when the result does not fit. */
static cf_bool ubig_muladd(ubig* a, cf_u32 m, cf_u32 add) {
    cf_u64 carry = add;
    int    i;

    for (i = 0; i < UBIG_LIMBS; ++i) {
        cf_u64 t = (cf_u64)a->w[i] * m + carry;

        a->w[i] = (cf_u32)(t & 0xFFFFFFFFu);
        carry   = t >> 32;
    }
    return (carry == 0) ? CF_TRUE : CF_FALSE;
}

static cf_bool ubig_add(ubig* a, const ubig* b) {
    cf_u64 carry = 0;
    int    i;

    for (i = 0; i < UBIG_LIMBS; ++i) {
        cf_u64 t = (cf_u64)a->w[i] + b->w[i] + carry;

        a->w[i] = (cf_u32)(t & 0xFFFFFFFFu);
        carry   = t >> 32;
    }
    return (carry == 0) ? CF_TRUE : CF_FALSE;
}

/* a = a * m. */
static cf_bool ubig_mul64(ubig* a, cf_u64 m) {
    ubig high = *a;
    int  i;

    if (!ubig_muladd(&high, (cf_u32)(m >> 32), 0) || high.w[UBIG_LIMBS - 1] != 0) {
        return CF_FALSE;
    }
    for (i = UBIG_LIMBS - 1; i > 0; --i) {
        high.w[i] = high.w[i - 1];
    }
    high.w[0] = 0;
    return (ubig_muladd(a, (cf_u32)(m & 0xFFFFFFFFu), 0) && ubig_add(a, &high))
               ? CF_TRUE
               : CF_FALSE;
}

/* a = a / d, and the remainder. */
static cf_u32 ubig_divmod(ubig* a, cf_u32 d) {
    cf_u64 rem = 0;
    int    i;

    for (i = UBIG_LIMBS - 1; i >= 0; --i) {
        cf_u64 t = (rem << 32) | a->w[i];

        a->w[i] = (cf_u32)(t / d);
        rem     = t % d;
    }
    return (cf_u32)rem;
}

static cf_bool ubig_to_i64(const ubig* a, cf_i64* out) {
    cf_u64 v;
    int    i;

    for (i = 2; i < UBIG_LIMBS; ++i) {
        if (a->w[i] != 0) {
            return CF_FALSE;
        }
    }
    v = ((cf_u64)a->w[1] << 32) | a->w[0];
    if (v > (cf_u64)CF_I64_MAX) {
        return CF_FALSE;
    }
    *out = (cf_i64)v;
    return CF_TRUE;
}

/* The reasons a unit gives: static, so a caller may keep the pointer. */
static const char unit_why_overflow[]    = "overflows";
static const char unit_why_needs_unit[]  = "needs a unit";
static const char unit_why_ambiguous[]   = "is ambiguous: use days";
static const char unit_why_whole_ms[]    = "is not a whole number of ms";
static const char unit_why_whole_s[]     = "is not a whole number of s";
static const char unit_why_whole_us[]    = "is not a whole number of us";
static const char unit_why_whole_b[]     = "is not a whole number of B";
static const char unit_why_whole_plain[] = "is not a whole number";

#define UNIT_OK 0
#define UNIT_NOT_WHOLE 1
#define UNIT_OVERFLOW 2

typedef struct unit_num {
    ubig    n;     /* the digits, as an integer, leading zeros dropped */
    int     sig;   /* how many of them */
    int     scale; /* those after the point, trailing zeros dropped */
    cf_bool wide;  /* more digits than a ubig is asked to hold */
} unit_num;

static cf_bool unit_digit(const char* s, size_t n, size_t i) {
    return (i < n && cf_is_digit((unsigned char)s[i])) ? CF_TRUE : CF_FALSE;
}

static void unit_push(unit_num* x, char c) {
    cf_u32 digit = (cf_u32)(c - '0');

    if (x->wide || (x->sig == 0 && digit == 0)) {
        return;
    }
    if (x->sig >= UNIT_DIGITS) {
        x->wide = CF_TRUE;
        return;
    }
    (void)ubig_muladd(&x->n, 10, digit);
    ++x->sig;
}

/* digits [ "." digits ], at *i; CF_FALSE when there is not one. */
static cf_bool unit_number(const char* s, size_t n, size_t* i, unit_num* x) {
    size_t a = *i;
    size_t b = a;
    size_t f0;
    size_t f1;
    size_t k;

    while (unit_digit(s, n, b)) {
        ++b;
    }
    if (b == a) {
        return CF_FALSE;
    }
    f0 = b;
    f1 = b;
    if (b < n && s[b] == '.') {
        f0 = b + 1;
        f1 = f0;
        while (unit_digit(s, n, f1)) {
            ++f1;
        }
        if (f1 == f0) {
            return CF_FALSE;
        }
        *i = f1;
        /* The zeros that end a fraction carry nothing. */
        while (f1 > f0 && s[f1 - 1] == '0') {
            --f1;
        }
    } else {
        *i = b;
    }
    ubig_zero(&x->n);
    x->sig   = 0;
    x->wide  = CF_FALSE;
    x->scale = ((f1 - f0) > (size_t)UNIT_DIGITS) ? UNIT_DIGITS + 1 : (int)(f1 - f0);
    for (k = a; k < b; ++k) {
        unit_push(x, s[k]);
    }
    for (k = f0; k < f1; ++k) {
        unit_push(x, s[k]);
    }
    return CF_TRUE;
}

/* acc += x * factor, where `factor` is the number's unit in the table's
   finest unit; what the product makes of the fraction must be whole. */
static int unit_add(ubig* acc, const unit_num* x, cf_u64 factor) {
    ubig t = x->n;
    int  k;

    if (x->wide) {
        return (x->scale == 0) ? UNIT_OVERFLOW : UNIT_NOT_WHOLE;
    }
    if (x->scale > UNIT_DIGITS) {
        return UNIT_NOT_WHOLE;
    }
    if (!ubig_mul64(&t, factor)) {
        return UNIT_OVERFLOW;
    }
    for (k = 0; k < x->scale; ++k) {
        if (ubig_divmod(&t, 10) != 0) {
            return UNIT_NOT_WHOLE;
        }
    }
    return ubig_add(acc, &t) ? UNIT_OK : UNIT_OVERFLOW;
}

/* The sum, in base units. */
static int unit_finish(ubig* acc, cf_u64 per_base, cf_i64* out) {
    if (ubig_divmod(acc, (cf_u32)per_base) != 0) {
        return UNIT_NOT_WHOLE;
    }
    return ubig_to_i64(acc, out) ? UNIT_OK : UNIT_OVERFLOW;
}

static cf_bool unit_refuse(const char** reason, const char* why) {
    if (reason != NULL) {
        *reason = why;
    }
    return CF_FALSE;
}

static cf_bool unit_failed(int st, const char* not_whole, const char** reason) {
    if (st == UNIT_OVERFLOW) {
        return unit_refuse(reason, unit_why_overflow);
    }
    return unit_refuse(reason, (not_whole != NULL) ? not_whole : unit_why_whole_plain);
}

/* [+-] digits, and nothing else. */
static cf_bool unit_is_bare(const char* s, size_t n) {
    size_t i = 0;

    if (n > 0 && (s[0] == '-' || s[0] == '+')) {
        i = 1;
    }
    if (i == n) {
        return CF_FALSE;
    }
    while (i < n && cf_is_digit((unsigned char)s[i])) {
        ++i;
    }
    return (i == n) ? CF_TRUE : CF_FALSE;
}

static const cf_unit_term* unit_find(const cf_unit_table* t, const char* name,
                                     size_t len) {
    size_t i;

    for (i = 0; i < t->count; ++i) {
        if (t->terms[i].name != NULL && strlen(t->terms[i].name) == len &&
            memcmp(t->terms[i].name, name, len) == 0) {
            return &t->terms[i];
        }
    }
    return NULL;
}

cf_bool cf_unit_table_parse(const cf_unit_table* t, const char* s, size_t n,
                            cf_i64* out, const char** reason) {
    ubig   acc;
    size_t i     = 0;
    cf_u64 prev  = 0;
    int    terms = 0;
    int    st;

    if (reason != NULL) {
        *reason = NULL;
    }
    if (t == NULL || t->terms == NULL || s == NULL || out == NULL || n == 0 ||
        t->per_base < 1 || (cf_u64)t->per_base > (cf_u64)0xFFFFFFFFu) {
        return CF_FALSE;
    }
    if (unit_is_bare(s, n)) {
        if (!t->allow_bare) {
            return unit_refuse(reason, unit_why_needs_unit);
        }
        return cf_text_to_i64(s, n, out, NULL) ? CF_TRUE
                                               : unit_refuse(reason, unit_why_overflow);
    }
    ubig_zero(&acc);
    while (i < n) {
        unit_num            x;
        const cf_unit_term* term;
        size_t              from;
        cf_u64              f;

        if (!unit_number(s, n, &i, &x)) {
            return CF_FALSE;
        }
        from = i;
        while (i < n && !cf_is_digit((unsigned char)s[i]) && s[i] != '.') {
            ++i;
        }
        term = (i > from) ? unit_find(t, s + from, i - from) : NULL;
        if (term == NULL || term->factor < 0) {
            return CF_FALSE;
        }
        if (term->factor == 0) {
            return unit_refuse(reason, t->refused);
        }
        f = (cf_u64)term->factor;
        if (terms > 0 && (!t->sum || f >= prev)) {
            return CF_FALSE;
        }
        st = unit_add(&acc, &x, f);
        if (st != UNIT_OK) {
            return unit_failed(st, t->not_whole, reason);
        }
        prev = f;
        ++terms;
    }
    st = unit_finish(&acc, (cf_u64)t->per_base, out);
    return (st == UNIT_OK) ? CF_TRUE : unit_failed(st, t->not_whole, reason);
}

/* ------------------------------------------------------------------ *
 * The options, and the front door
 * ------------------------------------------------------------------ */

void cf_unit_options_init(cf_unit_options* o) {
    if (o == NULL) {
        return;
    }
    o->size       = sizeof *o;
    o->base       = CF_UNIT_BASE_DEFAULT;
    o->allow_bare = CF_TRUE;
    o->calendar   = CF_FALSE;
    o->bare_si    = CF_FALSE;
}

cf_bool cf_unit_parse(const cf_unit* u, const char* s, size_t n, cf_i64* out,
                      const char** reason) {
    if (reason != NULL) {
        *reason = NULL;
    }
    if (u == NULL || u->parse == NULL || s == NULL || out == NULL) {
        return CF_FALSE;
    }
    return u->parse(u, s, n, out, reason);
}

size_t cf_unit_format(const cf_unit* u, cf_i64 v, char* buf, size_t cap) {
    if (buf != NULL && cap > 0) {
        buf[0] = '\0';
    }
    if (u == NULL || u->format == NULL) {
        return 0;
    }
    return u->format(u, v, buf, cap);
}

/* A formatter writes into a local buffer and hands it over whole, so that a
   buffer too small gets nothing rather than the front of a number. */
#define UNIT_TEXT 64

static size_t unit_put_num(char* tmp, size_t at, cf_u64 v) {
    char   digits[24];
    size_t k = 0;

    if (v == 0) {
        digits[k++] = '0';
    }
    while (v != 0) {
        digits[k++] = (char)('0' + (int)(v % 10));
        v /= 10;
    }
    while (k > 0) {
        tmp[at++] = digits[--k];
    }
    return at;
}

static size_t unit_put_str(char* tmp, size_t at, const char* s) {
    while (*s != '\0') {
        tmp[at++] = *s++;
    }
    return at;
}

static size_t unit_hand_over(const char* tmp, size_t len, char* buf, size_t cap) {
    if (buf == NULL || cap == 0 || len + 1 > cap) {
        return 0;
    }
    memcpy(buf, tmp, len);
    buf[len] = '\0';
    return len;
}

/* A negative number is the bare number: a suffixed form has no sign. */
static size_t unit_put_negative(char* tmp, cf_i64 v) {
    tmp[0] = '-';
    /* -(v + 1) is representable for every negative v; the magnitude is one more. */
    return unit_put_num(tmp, 1, (cf_u64)(-(v + 1)) + 1);
}

/* ------------------------------------------------------------------ *
 * Durations
 * ------------------------------------------------------------------ */

#define NS_S ((cf_u64)1000000000)
#define NS_M ((cf_u64)60 * NS_S)
#define NS_H ((cf_u64)3600 * NS_S)
#define NS_D ((cf_u64)86400 * NS_S)
#define NS_W ((cf_u64)7 * NS_D)
#define NS_MONTH ((cf_u64)30 * NS_D)
#define NS_YEAR ((cf_u64)365 * NS_D)

/* Go's time.ParseDuration spellings, C++ chrono's names, and d and w as
   Rust's humantime has them; both mu signs (U+00B5 and U+03BC), in UTF-8. */
#define DURATION_ROWS                                                                  \
    {"ns", 1}, {"us", 1000}, {"\xC2\xB5s", 1000}, {"\xCE\xBCs", 1000},                 \
        {"ms", 1000000}, {"s", (cf_i64)NS_S}, {"m", (cf_i64)NS_M},                     \
        {"min", (cf_i64)NS_M}, {"h", (cf_i64)NS_H}, {"d", (cf_i64)NS_D}, {             \
        "w", (cf_i64)NS_W                                                              \
    }

static const cf_unit_term duration_terms[] = {
    DURATION_ROWS, {"mo", (cf_i64)NS_MONTH}, {"y", (cf_i64)NS_YEAR}};
static const cf_unit_term duration_ambiguous[] = {DURATION_ROWS, {"mo", 0}, {"y", 0}};

/* The finest unit of a duration is the nanosecond. */
static cf_bool duration_base(int base, cf_i64* per_base, const char** whole) {
    switch (base) {
    case CF_UNIT_BASE_DEFAULT:
        *per_base = 1000000;
        *whole    = unit_why_whole_ms;
        return CF_TRUE;
    case CF_UNIT_BASE_S:
        *per_base = 1000000000;
        *whole    = unit_why_whole_s;
        return CF_TRUE;
    case CF_UNIT_BASE_US:
        *per_base = 1000;
        *whole    = unit_why_whole_us;
        return CF_TRUE;
    default:
        return CF_FALSE;
    }
}

/* ISO 8601: the designator after a number, in the date part (Y, M, W, D) or
   the time part (H, M, S). `idx` is its place in the order they come in. */
static cf_bool iso_designator(cf_bool in_time, char c, int* idx, cf_u64* factor,
                              cf_bool* calendar) {
    *calendar = CF_FALSE;
    if (!in_time) {
        switch (c) {
        case 'Y':
            *idx      = 0;
            *factor   = NS_YEAR;
            *calendar = CF_TRUE;
            return CF_TRUE;
        case 'M':
            *idx      = 1;
            *factor   = NS_MONTH;
            *calendar = CF_TRUE;
            return CF_TRUE;
        case 'W':
            *idx    = 2;
            *factor = NS_W;
            return CF_TRUE;
        case 'D':
            *idx    = 3;
            *factor = NS_D;
            return CF_TRUE;
        default:
            return CF_FALSE;
        }
    }
    switch (c) {
    case 'H':
        *idx    = 4;
        *factor = NS_H;
        return CF_TRUE;
    case 'M':
        *idx    = 5;
        *factor = NS_M;
        return CF_TRUE;
    case 'S':
        *idx    = 6;
        *factor = NS_S;
        return CF_TRUE;
    default:
        return CF_FALSE;
    }
}

/* P, then years, months, weeks and days in that order, then T and hours,
   minutes and seconds in that order, each at most once; a number may have
   a fraction. */
static cf_bool duration_iso(const cf_unit_options* o, cf_i64 per_base,
                            const char* whole, const char* s, size_t n, cf_i64* out,
                            const char** reason) {
    ubig    acc;
    size_t  i       = 1;
    int     last    = -1;
    int     parts   = 0;
    cf_bool in_time = CF_FALSE;
    int     st;

    if (n < 2) {
        return CF_FALSE;
    }
    ubig_zero(&acc);
    while (i < n) {
        unit_num x;
        int      idx = 0;
        cf_u64   f   = 0;
        cf_bool  calendar;

        if (s[i] == 'T') {
            if (in_time || i + 1 >= n) {
                return CF_FALSE;
            }
            in_time = CF_TRUE;
            ++i;
            if (last < 3) {
                last = 3;
            }
            continue;
        }
        if (!unit_number(s, n, &i, &x) || i >= n ||
            !iso_designator(in_time, s[i], &idx, &f, &calendar) || idx <= last) {
            return CF_FALSE;
        }
        ++i;
        if (calendar && !o->calendar) {
            return unit_refuse(reason, unit_why_ambiguous);
        }
        st = unit_add(&acc, &x, f);
        if (st != UNIT_OK) {
            return unit_failed(st, whole, reason);
        }
        last = idx;
        ++parts;
    }
    if (parts == 0) {
        return CF_FALSE;
    }
    st = unit_finish(&acc, (cf_u64)per_base, out);
    return (st == UNIT_OK) ? CF_TRUE : unit_failed(st, whole, reason);
}

static cf_bool duration_parse(const cf_unit* u, const char* s, size_t n, cf_i64* out,
                              const char** reason) {
    cf_unit_table t;
    const char*   whole = unit_why_whole_ms;
    cf_i64        per_base;

    if (reason != NULL) {
        *reason = NULL;
    }
    if (u == NULL || s == NULL || out == NULL || n == 0 ||
        !duration_base(u->options.base, &per_base, &whole)) {
        return CF_FALSE;
    }
    if (s[0] == 'P') {
        return duration_iso(&u->options, per_base, whole, s, n, out, reason);
    }
    t.terms      = u->options.calendar ? duration_terms : duration_ambiguous;
    t.count      = sizeof duration_terms / sizeof duration_terms[0];
    t.per_base   = per_base;
    t.sum        = CF_TRUE;
    t.allow_bare = u->options.allow_bare;
    t.not_whole  = whole;
    t.refused    = unit_why_ambiguous;
    return cf_unit_table_parse(&t, s, n, out, reason);
}

/* The parts of a duration, largest first: how many base units make one, in
   each base, and 0 for a part finer than the base. */
static const struct {
    const char* name;
    cf_i64      per[3]; /* CF_UNIT_BASE_DEFAULT (ms), CF_UNIT_BASE_S, CF_UNIT_BASE_US */
} duration_parts[] = {{"d", {86400000, 86400, (cf_i64)86400 * 1000000}},
                      {"h", {3600000, 3600, (cf_i64)3600 * 1000000}},
                      {"m", {60000, 60, (cf_i64)60 * 1000000}},
                      {"s", {1000, 1, 1000000}},
                      {"ms", {1, 0, 1000}},
                      {"us", {0, 0, 1}}};

static const char* const duration_base_names[3] = {"ms", "s", "us"};

static size_t duration_format(const cf_unit* u, cf_i64 v, char* buf, size_t cap) {
    char   tmp[UNIT_TEXT];
    size_t at = 0;
    size_t k;
    cf_u64 left;
    int    base;

    if (u == NULL || u->options.base < CF_UNIT_BASE_DEFAULT ||
        u->options.base > CF_UNIT_BASE_US) {
        return 0;
    }
    base = u->options.base;
    if (v < 0) {
        return unit_hand_over(tmp, unit_put_negative(tmp, v), buf, cap);
    }
    if (v == 0) {
        at = unit_put_num(tmp, 0, 0);
        at = unit_put_str(tmp, at, duration_base_names[base]);
        return unit_hand_over(tmp, at, buf, cap);
    }
    left = (cf_u64)v;
    for (k = 0; k < sizeof duration_parts / sizeof duration_parts[0]; ++k) {
        cf_u64 per = (cf_u64)duration_parts[k].per[base];

        if (per == 0 || left < per) {
            continue;
        }
        at = unit_put_num(tmp, at, left / per);
        at = unit_put_str(tmp, at, duration_parts[k].name);
        left %= per;
    }
    return unit_hand_over(tmp, at, buf, cap);
}

/* ------------------------------------------------------------------ *
 * Sizes
 * ------------------------------------------------------------------ */

#define SZ_KB ((cf_i64)1000)
#define SZ_MB ((cf_i64)1000000)
#define SZ_GB ((cf_i64)1000000000)
#define SZ_TB (SZ_MB * SZ_MB)
#define SZ_PB (SZ_TB * SZ_KB)
#define SZ_KIB ((cf_i64)1 << 10)
#define SZ_MIB ((cf_i64)1 << 20)
#define SZ_GIB ((cf_i64)1 << 30)
#define SZ_TIB ((cf_i64)1 << 40)
#define SZ_PIB ((cf_i64)1 << 50)

/* IEC 80000-13 (1024^n) and SI (1000^n); kB is the SI spelling and KB is
   read as it too. */
#define SIZE_ROWS                                                                      \
    {"B", 1}, {"kB", SZ_KB}, {"KB", SZ_KB}, {"MB", SZ_MB}, {"GB", SZ_GB},              \
        {"TB", SZ_TB}, {"PB", SZ_PB}, {"KiB", SZ_KIB}, {"MiB", SZ_MIB},                \
        {"GiB", SZ_GIB}, {"TiB", SZ_TIB}, {                                            \
        "PiB", SZ_PIB                                                                  \
    }

/* The bare K, M, G and T of Kubernetes and the JVM: IEC unless told. */
static const cf_unit_term size_terms_iec[] = {
    SIZE_ROWS, {"K", SZ_KIB}, {"M", SZ_MIB}, {"G", SZ_GIB}, {"T", SZ_TIB}};
static const cf_unit_term size_terms_si[] = {
    SIZE_ROWS, {"K", SZ_KB}, {"M", SZ_MB}, {"G", SZ_GB}, {"T", SZ_TB}};

static cf_bool size_parse(const cf_unit* u, const char* s, size_t n, cf_i64* out,
                          const char** reason) {
    cf_unit_table t;

    if (reason != NULL) {
        *reason = NULL;
    }
    if (u == NULL || s == NULL || out == NULL || n == 0) {
        return CF_FALSE;
    }
    t.terms      = u->options.bare_si ? size_terms_si : size_terms_iec;
    t.count      = sizeof size_terms_iec / sizeof size_terms_iec[0];
    t.per_base   = 1;
    t.sum        = CF_FALSE;
    t.allow_bare = u->options.allow_bare;
    t.not_whole  = unit_why_whole_b;
    t.refused    = NULL;
    return cf_unit_table_parse(&t, s, n, out, reason);
}

/* The largest suffix that divides the size exactly, IEC or SI. */
static const struct {
    const char* name;
    cf_i64      factor;
} size_parts[] = {{"PiB", SZ_PIB}, {"PB", SZ_PB}, {"TiB", SZ_TIB}, {"TB", SZ_TB},
                  {"GiB", SZ_GIB}, {"GB", SZ_GB}, {"MiB", SZ_MIB}, {"MB", SZ_MB},
                  {"KiB", SZ_KIB}, {"kB", SZ_KB}};

static size_t size_format(const cf_unit* u, cf_i64 v, char* buf, size_t cap) {
    char   tmp[UNIT_TEXT];
    size_t at;
    size_t k;

    (void)u;
    if (v < 0) {
        return unit_hand_over(tmp, unit_put_negative(tmp, v), buf, cap);
    }
    for (k = 0; v > 0 && k < sizeof size_parts / sizeof size_parts[0]; ++k) {
        if (v % size_parts[k].factor == 0) {
            at = unit_put_num(tmp, 0, (cf_u64)(v / size_parts[k].factor));
            at = unit_put_str(tmp, at, size_parts[k].name);
            return unit_hand_over(tmp, at, buf, cap);
        }
    }
    at = unit_put_num(tmp, 0, (cf_u64)v);
    at = unit_put_str(tmp, at, "B");
    return unit_hand_over(tmp, at, buf, cap);
}

/* ------------------------------------------------------------------ *
 * The built-in units
 * ------------------------------------------------------------------ */

static const cf_unit unit_duration = {
    sizeof(cf_unit),
    "duration",
    duration_parse,
    duration_format,
    {sizeof(cf_unit_options), CF_UNIT_BASE_DEFAULT, CF_TRUE, CF_FALSE, CF_FALSE}};

static const cf_unit unit_size = {
    sizeof(cf_unit),
    "size",
    size_parse,
    size_format,
    {sizeof(cf_unit_options), CF_UNIT_BASE_DEFAULT, CF_TRUE, CF_FALSE, CF_FALSE}};

const cf_unit* cf_unit_duration_get(void) {
    return &unit_duration;
}

const cf_unit* cf_unit_size_get(void) {
    return &unit_size;
}
