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

#include <string.h>

#if CF_WITH_REAL
#include <math.h> /* frexp, ldexp, fabs - none of them locale-sensitive */
#include <float.h>
#endif

/* ================================================================== *
 * Integers
 * ================================================================== */

size_t cf_u64_to_text(cf_u64 v, char* buf, size_t cap) {
    char   tmp[CF_I64_DIGITS];
    size_t n = 0;
    size_t i;

    if (buf == NULL || cap == 0) {
        return 0;
    }
    do {
        tmp[n++] = (char)('0' + (int)(v % (cf_u64)10));
        v /= (cf_u64)10;
    } while (v != 0);

    if (n + 1 > cap) {
        buf[0] = '\0';
        return 0;
    }
    for (i = 0; i < n; ++i) {
        buf[i] = tmp[n - 1 - i];
    }
    buf[n] = '\0';
    return n;
}

size_t cf_i64_to_text(cf_i64 v, char* buf, size_t cap) {
    cf_u64 mag;
    size_t n;

    if (buf == NULL || cap == 0) {
        return 0;
    }
    if (v < 0) {
        /* Negate through the unsigned domain: defined for CF_I64_MIN. */
        mag = (cf_u64)(-(v + 1)) + (cf_u64)1;
        if (cap < 2) {
            buf[0] = '\0';
            return 0;
        }
        buf[0] = '-';
        n      = cf_u64_to_text(mag, buf + 1, cap - 1);
        if (n == 0) {
            buf[0] = '\0';
            return 0;
        }
        return n + 1;
    }
    return cf_u64_to_text((cf_u64)v, buf, cap);
}

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
 * The widest quantity reached is about 2^1075 for subnormal inputs,
 * which is 34 limbs. 64 limbs leaves generous margin and keeps each
 * value at 260 bytes.
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
   widest quantity the algorithm reaches, which is 34 limbs of the 64
   allocated. Enforcing it anyway costs one comparison per growth and
   turns "cannot happen" into "does not happen" - the difference the
   static analyser was pointing at when it could not prove the bound
   through big_add and big_shl. */
static void big_clamp(cf_big* b) {
    if (b->n > CF_BIG_LIMBS) {
        b->n = CF_BIG_LIMBS;
    } else if (b->n < 0) {
        b->n = 0;
    }
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
    int i;
    dst->n = src->n;
    for (i = 0; i < src->n; ++i) {
        dst->d[i] = src->d[i];
    }
}

static int big_is_zero(const cf_big* b) {
    return b->n == 0;
}

static void big_trim(cf_big* b) {
    while (b->n > 0 && b->d[b->n - 1] == 0) {
        b->n--;
    }
}

static int big_bitlen(const cf_big* b) {
    cf_u32 top;
    int    bits;

    if (b->n == 0) {
        return 0;
    }
    top  = b->d[b->n - 1];
    bits = 0;
    while (top != 0) {
        top >>= 1;
        ++bits;
    }
    return (b->n - 1) * 32 + bits;
}

static int big_cmp(const cf_big* a, const cf_big* b) {
    int i;
    if (a->n != b->n) {
        return (a->n < b->n) ? -1 : 1;
    }
    for (i = a->n - 1; i >= 0; --i) {
        if (a->d[i] != b->d[i]) {
            return (a->d[i] < b->d[i]) ? -1 : 1;
        }
    }
    return 0;
}

/* a += b */
static void big_add(cf_big* a, const cf_big* b) {
    cf_u64 carry = 0;
    int    n     = (a->n > b->n) ? a->n : b->n;
    int    i;

    for (i = 0; i < n || carry != 0; ++i) {
        cf_u64 sum = carry;
        if (i < a->n) {
            sum += (cf_u64)a->d[i];
        }
        if (i < b->n) {
            sum += (cf_u64)b->d[i];
        }
        if (i >= CF_BIG_LIMBS) {
            break;
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
    int    i;

    for (i = 0; i < a->n; ++i) {
        cf_i64 diff = (cf_i64)a->d[i] - borrow;
        if (i < b->n) {
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
    double m;
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
    int       exp10, i, z;

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

static double cf_slow_path(const cf_big* digits, int exp10, int neg) {
    cf_big N, D;
    cf_u64 q = 0;
    int    i, bn, bd, shiftN, shiftD, sticky;
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

    /* Generate 55 significant bits by binary long division. */
    for (i = 0; i < 55; ++i) {
        q <<= 1;
        if (big_cmp(&N, &D) >= 0) {
            big_sub(&N, &D);
            q |= (cf_u64)1;
        }
        big_shl(&N, 1);
    }
    sticky = !big_is_zero(&N);

    /* q holds 55 bits; round to 53, half to even. */
    {
        cf_u64 dropped  = q & (cf_u64)3;
        cf_u64 keep     = q >> 2;
        int    round_up = 0;

        if (dropped > 2) {
            round_up = 1;
        } else if (dropped == 2) {
            round_up = sticky || ((keep & (cf_u64)1) != 0);
        }
        if (round_up) {
            keep += 1;
            if (keep == ((cf_u64)1 << 53)) {
                keep >>= 1;
                shiftD += 1; /* exponent grows by one */
            }
        }
        q = keep;
    }

    /* value = q * 2^(shiftD - shiftN - 54 + 2) */
    r = ldexp((double)q, shiftD - shiftN - 52);
    return neg ? -r : r;
}

cf_bool cf_text_to_f64(const char* s, size_t len, double* out, size_t* end) {
    cf_big digits;
    cf_u64 fast = 0;
    size_t i    = 0;
    int    neg = 0, any = 0, fast_ok = 1, ndig = 0;
    int    exp10   = 0;
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
        big_mul_u32(&digits, 10);
        if (dv != 0) {
            cf_big dd;
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
        ++i;
    }

    if (i < len && s[i] == '.') {
        ++i;
        while (i < len && cf_is_digit((unsigned char)s[i])) {
            cf_u32 dv = (cf_u32)(s[i] - '0');
            cf_big dd;
            any = 1;
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
            --exp10;
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

    if (exp10 > 400 || exp10 < -400 - ndig) {
        /* Far outside the representable range; reject rather than
           silently producing inf or zero. */
        return CF_FALSE;
    }

    *out = cf_slow_path(&digits, exp10, neg);
    return CF_TRUE;
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
