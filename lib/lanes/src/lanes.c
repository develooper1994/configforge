/*
 * lanes.c - which backends are built, which this processor runs, and the
 * choice between them.
 *
 * This is lanes' own portability layer: the compiler's and the platform's
 * macros are read here and nowhere else in it (gate.detection exempts
 * lib/lanes/, as it does lib/platform/), because what a processor can run
 * is only known to the instructions that ask it.
 *
 * Detection:
 *   x86      cpuid for what the processor has, and xgetbv for what the
 *            operating system saves: AVX2 and AVX-512 are counted only
 *            when it saves their registers, since a processor that has
 *            them under an operating system that does not would fault.
 *   AArch64  NEON always; SVE from the auxiliary vector on Linux.
 *   Arm      NEON from the auxiliary vector on Linux.
 *   all      the scalar and word-at-a-time bits, which need nothing.
 * A compiler this cannot be written for reports those last bits only.
 */

#include "lanes/lanes.h"

#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) ||                     \
    defined(_M_AMD64) || defined(_M_IX86)
#define LANES_X86 1
#else
#define LANES_X86 0
#endif

#define LANES_X86_MSVC 0
#define LANES_X86_GNU 0
#if LANES_X86 && defined(_MSC_VER)
#undef LANES_X86_MSVC
#define LANES_X86_MSVC 1
#include <intrin.h>
#include <immintrin.h>
#elif LANES_X86 && (defined(__GNUC__) || defined(__TINYC__) || defined(__PCC__))
#undef LANES_X86_GNU
#define LANES_X86_GNU 1
#endif

#if (defined(__aarch64__) || defined(__arm__)) && defined(__linux__)
#define LANES_ARM_AUXV 1
#include <sys/auxv.h>
#else
#define LANES_ARM_AUXV 0
#endif

#if LANES_X86_MSVC || LANES_X86_GNU

/* One cpuid: leaf and subleaf in, eax ebx ecx edx out. */
static void lanes_cpuid(unsigned leaf, unsigned sub, unsigned r[4]) {
#if LANES_X86_MSVC
    int v[4];

    __cpuidex(v, (int)leaf, (int)sub);
    r[0] = (unsigned)v[0];
    r[1] = (unsigned)v[1];
    r[2] = (unsigned)v[2];
    r[3] = (unsigned)v[3];
#else
    unsigned a, b, c, d;

#if defined(__i386__)
    /* ebx holds the GOT pointer in position-independent code: keep it. */
    __asm__ __volatile__("xchgl %%ebx, %1\n\t"
                         "cpuid\n\t"
                         "xchgl %%ebx, %1"
                         : "=a"(a), "=r"(b), "=c"(c), "=d"(d)
                         : "0"(leaf), "2"(sub));
#else
    __asm__ __volatile__("cpuid"
                         : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
                         : "0"(leaf), "2"(sub));
#endif
    r[0] = a;
    r[1] = b;
    r[2] = c;
    r[3] = d;
#endif
}

/* The low word of XCR0: which register states the operating system saves. */
static unsigned lanes_xcr0(void) {
#if LANES_X86_MSVC
    return (unsigned)(_xgetbv(0) & 0xFFFFFFFFu);
#else
    unsigned lo, hi;

    /* xgetbv, as bytes: an assembler too old to know it still builds. */
    __asm__ __volatile__(".byte 0x0f, 0x01, 0xd0" : "=a"(lo), "=d"(hi) : "c"(0));
    (void)hi;
    return lo;
#endif
}

static unsigned lanes_cpu_x86(void) {
    unsigned r[4];
    unsigned bits = 0u;
    unsigned top, ecx, edx, xcr0;

    lanes_cpuid(0u, 0u, r);
    top = r[0];
    if (top < 1u) {
        return bits;
    }
    lanes_cpuid(1u, 0u, r);
    ecx = r[2];
    edx = r[3];
    if (edx & (1u << 26)) {
        bits |= LANES_ISA_SSE2;
    }
    if ((bits & LANES_ISA_SSE2) && (ecx & (1u << 20))) {
        bits |= LANES_ISA_SSE42;
    }
    /* AVX needs the OSXSAVE bit, the AVX bit, and the system saving the
       SSE and AVX state (XCR0 bits 1 and 2). */
    if ((bits & LANES_ISA_SSE42) && (ecx & (1u << 27)) && (ecx & (1u << 28)) &&
        top >= 7u) {
        xcr0 = lanes_xcr0();
        if ((xcr0 & 0x6u) == 0x6u) {
            lanes_cpuid(7u, 0u, r);
            if (r[1] & (1u << 5)) {
                bits |= LANES_ISA_AVX2;
                /* AVX-512 for bytes is F and BW, and the system saving
                   the opmask and the upper halves (XCR0 bits 5 to 7). */
                if ((r[1] & (1u << 16)) && (r[1] & (1u << 30)) &&
                    (xcr0 & 0xE0u) == 0xE0u) {
                    bits |= LANES_ISA_AVX512;
                }
            }
        }
    }
    return bits;
}

#endif /* x86 and a compiler this can be written for */

unsigned lanes_cpu(void) {
    unsigned bits = LANES_ISA_SCALAR | LANES_ISA_SWAR32 | LANES_ISA_SWAR64;

#if LANES_X86_MSVC || LANES_X86_GNU
    bits |= lanes_cpu_x86();
#endif
#if defined(__aarch64__) || defined(_M_ARM64)
    bits |= LANES_ISA_NEON;
#endif
#if LANES_ARM_AUXV
#if defined(__aarch64__)
    /* HWCAP_SVE */
    if (getauxval(AT_HWCAP) & (1UL << 22)) {
        bits |= LANES_ISA_SVE;
    }
#else
    /* HWCAP_NEON, the 32-bit Arm bit */
    if (getauxval(AT_HWCAP) & (1UL << 12)) {
        bits |= LANES_ISA_NEON;
    }
#endif
#endif
    return bits;
}

unsigned lanes_built(void) {
    return LANES_ISA_SCALAR;
}

const lanes_ops* lanes_by_isa(unsigned isa) {
    if (isa == LANES_ISA_SCALAR) {
        return lanes_scalar();
    }
    return NULL;
}

const lanes_ops* lanes_best(void) {
    return lanes_scalar();
}
