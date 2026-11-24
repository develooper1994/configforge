/*
 * lanes.h - span operations over bytes, with a scalar reference and the
 * processor's detection. C89, and not ConfigForge's: it includes nothing
 * of it (D-101).
 *
 *     const lanes_ops* ops = lanes_best();
 *     size_t at = ops->find_byte(p, n, ',');
 *
 * A backend is a constant table of function pointers: nothing is detected
 * or chosen behind the caller's back, and no state is shared, so two
 * threads can never race over a choice. lanes_scalar() is the reference
 * every other backend is compared with, and always built.
 *
 * Meanings, which the scalar code and the tests hold to:
 *
 *   find_byte   the index of the first byte equal to c, or n
 *   find_any    the index of the first byte in the set, or n
 *   find_lt     the index of the first byte below c, or n
 *   ascii_run   the number of leading bytes below 0x80
 *   utf8_valid  the length of the longest prefix that is whole, valid
 *               UTF-8: no overlong form, no surrogate U+D800-U+DFFF,
 *               nothing above U+10FFFF; a sequence cut off by n is not
 *               in the prefix
 *   count_byte  the number of bytes equal to c
 *   digits8     reads exactly eight bytes; returns 1 and stores their
 *               value when all are '0'-'9', else returns 0 and leaves
 *               *value alone
 *
 * A pointer may be NULL when n is 0. No function reads at or past p + n
 * (digits8: past p + 8).
 */

#ifndef LANES_LANES_H
#define LANES_LANES_H

#include <stddef.h>

#define LANES_VERSION_MAJOR 0
#define LANES_VERSION_MINOR 1
#define LANES_VERSION_PATCH 0

/* ------------------------------------------------------------------ *
 * Export control. Nothing in a static build; in a shared one, what makes
 * a Windows DLL export anything, and on ELF and Mach-O what pairs with
 * -fvisibility=hidden. LANES_SHARED says the library is shared;
 * LANES_BUILDING_LIBRARY is defined only while its own sources compile.
 * ------------------------------------------------------------------ */

#ifndef LANES_API
#if defined(LANES_SHARED)
#if defined(_WIN32) || defined(__CYGWIN__)
#ifdef LANES_BUILDING_LIBRARY
#define LANES_API __declspec(dllexport)
#else
#define LANES_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) && __GNUC__ >= 4
#define LANES_API __attribute__((visibility("default")))
#else
#define LANES_API
#endif
#else
#define LANES_API
#endif
#endif

/* ------------------------------------------------------------------ *
 * Embedded under another library's prefix, as zlib's Z_PREFIX does:
 * built with LANES_PREFIX_CF, every public name is cf_lanes_<name>, so
 * it cannot clash with a lanes the program links itself, and the callers
 * write lanes_<name> either way. The block is before the declarations,
 * so that the definitions are renamed with them.
 * ------------------------------------------------------------------ */

#ifdef LANES_PREFIX_CF
#define lanes_set_init cf_lanes_set_init
#define lanes_scalar cf_lanes_scalar
#define lanes_swar cf_lanes_swar
#define lanes_by_isa cf_lanes_by_isa
#define lanes_best cf_lanes_best
#define lanes_cpu cf_lanes_cpu
#define lanes_built cf_lanes_built
#endif

/* A set of up to 256 bytes, made once. */
typedef struct lanes_set {
    unsigned char bits[32];
} lanes_set;
LANES_API void lanes_set_init(lanes_set* s, const unsigned char* bytes, size_t n);
#define LANES_SET_HAS(s, b) (((s)->bits[(unsigned char)(b) >> 3] >> ((b) & 7)) & 1)

/* One bit a backend: what a processor runs, and what is compiled in. */
enum {
    LANES_ISA_SCALAR = 1u << 0,
    LANES_ISA_SWAR32 = 1u << 1,
    LANES_ISA_SWAR64 = 1u << 2,
    LANES_ISA_SSE2   = 1u << 3,
    LANES_ISA_SSE42  = 1u << 4,
    LANES_ISA_AVX2   = 1u << 5,
    LANES_ISA_AVX512 = 1u << 6,
    LANES_ISA_NEON   = 1u << 7,
    LANES_ISA_SVE    = 1u << 8
};

typedef struct lanes_ops {
    size_t      size; /* sizeof(lanes_ops) where it was compiled */
    const char* name; /* "scalar", "swar64", "avx2", ... */
    unsigned    isa;  /* one LANES_ISA_* bit */
    size_t (*find_byte)(const unsigned char* p, size_t n, unsigned char c);
    size_t (*find_any)(const unsigned char* p, size_t n, const lanes_set* s);
    size_t (*find_lt)(const unsigned char* p, size_t n, unsigned char c);
    size_t (*ascii_run)(const unsigned char* p, size_t n);
    size_t (*utf8_valid)(const unsigned char* p, size_t n);
    size_t (*count_byte)(const unsigned char* p, size_t n, unsigned char c);
    int (*digits8)(const unsigned char* p, unsigned long* value);
} lanes_ops;

/* The reference: plain loops, always built. */
LANES_API const lanes_ops* lanes_scalar(void);
/* The word-at-a-time backend, at the width of `unsigned long` (LANES_ISA_SWAR32
   where it is 32 bits, LANES_ISA_SWAR64 where it is 64): C89, words read with
   memcpy. NULL when it is not built (CF_LANES_BACKENDS=scalar). Where an
   operation showed no gain over its loop, its entry is the scalar one. */
LANES_API const lanes_ops* lanes_swar(void);
/* The backend of one LANES_ISA_* bit; NULL when it was not built or this
   processor lacks it (manual choice). */
LANES_API const lanes_ops* lanes_by_isa(unsigned isa);
/* The best built backend this processor runs (automatic choice). */
LANES_API const lanes_ops* lanes_best(void);
/* LANES_ISA_* bits this processor runs. */
LANES_API unsigned lanes_cpu(void);
/* LANES_ISA_* bits compiled in. */
LANES_API unsigned lanes_built(void);

#endif /* LANES_LANES_H */
