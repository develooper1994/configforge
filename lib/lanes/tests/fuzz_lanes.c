/*
 * fuzz_lanes.c - every backend against the scalar reference, on bytes
 * nobody chose. The harness a backend added later needs: it compares each
 * operation of lanes_best(), and of every backend lanes_by_isa() returns,
 * with lanes_scalar(). Today they are one table, so it holds by
 * construction; it is here so that a backend has something to fail.
 *
 * Two programs from one file. Built with LANES_LIBFUZZER (clang,
 * -fsanitize=fuzzer) it is a libFuzzer target. Without, it is a test: a
 * fixed number of inputs from a generator that gives the same ones every
 * run, so that it runs everywhere, in a ctest, in a moment.
 *
 * An input is c, the number of set bytes (0 to 7), the offset, and the
 * span, which starts at the offset into the input, so that the span is
 * unaligned and ends exactly where the input does: under a sanitizer, a
 * read past it is a report.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lanes/lanes.h"

static void differ(const char* backend, const char* op) {
    printf("FAIL: %s differs from scalar in %s\n", backend, op);
    abort();
}

/* One backend against the reference, on one input. */
static void compare(const lanes_ops* ops, const lanes_ops* ref, const unsigned char* p,
                    size_t n, unsigned char c, const lanes_set* s) {
    unsigned long va = 7, vb = 7;

    if (ops->find_byte(p, n, c) != ref->find_byte(p, n, c)) {
        differ(ops->name, "find_byte");
    }
    if (ops->find_any(p, n, s) != ref->find_any(p, n, s)) {
        differ(ops->name, "find_any");
    }
    if (ops->find_lt(p, n, c) != ref->find_lt(p, n, c)) {
        differ(ops->name, "find_lt");
    }
    if (ops->ascii_run(p, n) != ref->ascii_run(p, n)) {
        differ(ops->name, "ascii_run");
    }
    if (ops->utf8_valid(p, n) != ref->utf8_valid(p, n)) {
        differ(ops->name, "utf8_valid");
    }
    if (ops->count_byte(p, n, c) != ref->count_byte(p, n, c)) {
        differ(ops->name, "count_byte");
    }
    if (n >= 8) {
        if (ops->digits8(p, &va) != ref->digits8(p, &vb) || va != vb) {
            differ(ops->name, "digits8");
        }
    }
}

/* One input, against lanes_best() and every backend a program can ask for. */
static void one(const unsigned char* d, size_t size) {
    const lanes_ops* ref = lanes_scalar();
    lanes_set        s;
    unsigned char    c;
    size_t           nset, off, bit;

    if (size < 3) {
        return;
    }
    c    = d[0];
    nset = d[1] & 7u;
    off  = d[2] & 15u;
    d += 3;
    size -= 3;
    if (nset > size) {
        nset = size;
    }
    lanes_set_init(&s, d, nset);
    if (off > size) {
        off = size;
    }
    compare(lanes_best(), ref, d + off, size - off, c, &s);
    for (bit = 1; bit <= LANES_ISA_SVE; bit <<= 1) {
        const lanes_ops* ops = lanes_by_isa((unsigned)bit);

        if (ops != NULL) {
            compare(ops, ref, d + off, size - off, c, &s);
        }
    }
}

#ifdef LANES_LIBFUZZER

int LLVMFuzzerTestOneInput(const unsigned char* data, size_t size);

int LLVMFuzzerTestOneInput(const unsigned char* data, size_t size) {
    one(data, size);
    return 0;
}

#else

#define INPUTS 20000
#define MAXLEN 300

static unsigned long g_rng = 88172645UL;

static unsigned next_byte(void) {
    g_rng ^= (g_rng << 13) & 0xFFFFFFFFUL;
    g_rng ^= g_rng >> 17;
    g_rng ^= (g_rng << 5) & 0xFFFFFFFFUL;
    return (unsigned)((g_rng >> 8) & 0xFF);
}

int main(void) {
    unsigned char* buf;
    size_t         i, k, len;

    for (i = 0; i < INPUTS; ++i) {
        len = (size_t)(next_byte() + (next_byte() & 1u) * 45u) % (MAXLEN + 1);
        /* An exact block each time, so that a sanitizer sees an overread. */
        buf = (unsigned char*)malloc(len + 1);
        if (buf == NULL) {
            printf("FAIL: out of memory\n");
            return 2;
        }
        for (k = 0; k < len; ++k) {
            /* A mix of ASCII, UTF-8's lead and continuation bytes, and
               anything, so that every operation has something to stop at. */
            switch (next_byte() & 3u) {
            case 0:
                buf[k] = (unsigned char)(next_byte() & 0x7Fu);
                break;
            case 1:
                buf[k] = (unsigned char)(0x80u | (next_byte() & 0x3Fu));
                break;
            case 2:
                buf[k] = (unsigned char)(0xC0u | (next_byte() & 0x3Fu));
                break;
            default:
                buf[k] = (unsigned char)next_byte();
                break;
            }
        }
        one(buf, len);
        free(buf);
    }
    printf("fuzz_lanes: %d inputs, best is %s\n", INPUTS, lanes_best()->name);
    return 0;
}

#endif
