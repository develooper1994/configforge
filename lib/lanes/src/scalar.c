/*
 * scalar.c - the reference: every span operation as a plain loop, written
 * for clarity and not for speed. Every other backend is compared with it.
 */

#include <string.h>

#include "lanes/lanes.h"

void lanes_set_init(lanes_set* s, const unsigned char* bytes, size_t n) {
    size_t i;

    memset(s->bits, 0, sizeof s->bits);
    for (i = 0; i < n; ++i) {
        s->bits[bytes[i] >> 3] =
            (unsigned char)(s->bits[bytes[i] >> 3] | (1u << (bytes[i] & 7)));
    }
}

static size_t scalar_find_byte(const unsigned char* p, size_t n, unsigned char c) {
    size_t i;

    for (i = 0; i < n; ++i) {
        if (p[i] == c) {
            return i;
        }
    }
    return n;
}

static size_t scalar_find_any(const unsigned char* p, size_t n, const lanes_set* s) {
    size_t i;

    for (i = 0; i < n; ++i) {
        if (LANES_SET_HAS(s, p[i])) {
            return i;
        }
    }
    return n;
}

static size_t scalar_find_lt(const unsigned char* p, size_t n, unsigned char c) {
    size_t i;

    for (i = 0; i < n; ++i) {
        if (p[i] < c) {
            return i;
        }
    }
    return n;
}

static size_t scalar_ascii_run(const unsigned char* p, size_t n) {
    size_t i = 0;

    while (i < n && p[i] < 0x80) {
        ++i;
    }
    return i;
}

/* Is the byte within [lo, hi]? */
static int in_range(unsigned char b, unsigned char lo, unsigned char hi) {
    return b >= lo && b <= hi;
}

/* The length of the sequence at p[0..n), or 0 when it is not whole and
   valid (Unicode's Table 3-7: the second byte's range depends on the
   lead, which is what excludes overlong forms, surrogates and what is
   above U+10FFFF). */
static size_t sequence_length(const unsigned char* p, size_t n) {
    unsigned char lead = p[0];
    unsigned char lo = 0x80, hi = 0xBF;
    size_t        len, k;

    if (lead < 0x80) {
        return 1;
    }
    if (lead >= 0xC2 && lead <= 0xDF) {
        len = 2;
    } else if (lead >= 0xE0 && lead <= 0xEF) {
        len = 3;
        if (lead == 0xE0) {
            lo = 0xA0;
        } else if (lead == 0xED) {
            hi = 0x9F;
        }
    } else if (lead >= 0xF0 && lead <= 0xF4) {
        len = 4;
        if (lead == 0xF0) {
            lo = 0x90;
        } else if (lead == 0xF4) {
            hi = 0x8F;
        }
    } else {
        return 0; /* a continuation byte, C0, C1, or F5 and above */
    }
    if (n < len) {
        return 0; /* cut off by n */
    }
    if (!in_range(p[1], lo, hi)) {
        return 0;
    }
    for (k = 2; k < len; ++k) {
        if (!in_range(p[k], 0x80, 0xBF)) {
            return 0;
        }
    }
    return len;
}

static size_t scalar_utf8_valid(const unsigned char* p, size_t n) {
    size_t i = 0;

    while (i < n) {
        size_t len = sequence_length(p + i, n - i);

        if (len == 0) {
            break;
        }
        i += len;
    }
    return i;
}

static size_t scalar_count_byte(const unsigned char* p, size_t n, unsigned char c) {
    size_t i, count = 0;

    for (i = 0; i < n; ++i) {
        if (p[i] == c) {
            ++count;
        }
    }
    return count;
}

static int scalar_digits8(const unsigned char* p, unsigned long* value) {
    unsigned long v = 0;
    int           i;

    for (i = 0; i < 8; ++i) {
        if (p[i] < '0' || p[i] > '9') {
            return 0;
        }
        v = v * 10UL + (unsigned long)(p[i] - '0');
    }
    *value = v;
    return 1;
}

static const lanes_ops scalar_ops = {
    sizeof(lanes_ops), "scalar",       LANES_ISA_SCALAR, scalar_find_byte,
    scalar_find_any,   scalar_find_lt, scalar_ascii_run, scalar_utf8_valid,
    scalar_count_byte, scalar_digits8};

const lanes_ops* lanes_scalar(void) {
    return &scalar_ops;
}
