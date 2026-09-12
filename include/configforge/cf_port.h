/*
 * cf_port.h - ConfigForge portability layer.
 *
 * This is the ONLY header in the project permitted to test __STDC_VERSION__,
 * _MSC_VER, __GNUC__ or any comparable macro. Enforced by CI.
 *
 * Baseline is C89. Where the toolchain offers more, it is used - but only for
 * capability and performance, never for anything whose output is part of a
 * contract. See the Portability Layer section of the specification.
 *
 * Define CF_FORCE_PORTABLE to disable every opportunistic upgrade. CI builds
 * the whole suite both ways and compares the output byte for byte.
 */

#ifndef CONFIGFORGE_CF_PORT_H
#define CONFIGFORGE_CF_PORT_H

#include <stddef.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ *
 * Standard level detection
 * ------------------------------------------------------------------ */

#if defined(CF_FORCE_PORTABLE)
#define CF_HAVE_C99 0
#define CF_HAVE_C11 0
#else
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define CF_HAVE_C99 1
#else
#define CF_HAVE_C99 0
#endif
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define CF_HAVE_C11 1
#else
#define CF_HAVE_C11 0
#endif
#endif

/* ------------------------------------------------------------------ *
 * Build features
 *
 * Defaults live here rather than in the header that uses each one, so
 * that every translation unit agrees on them whether or not it
 * includes that header. A feature macro visible in one file and absent
 * in another is how two objects end up with different struct layouts.
 * ------------------------------------------------------------------ */

#ifndef CF_WITH_REAL
#define CF_WITH_REAL 1
#endif
#ifndef CF_WITH_INI
#define CF_WITH_INI 1
#endif
#ifndef CF_WITH_JSON
#define CF_WITH_JSON 1
#endif
#ifndef CF_WITH_TOML
#define CF_WITH_TOML 0
#endif
#ifndef CF_WITH_XML
#define CF_WITH_XML 0
#endif
#ifndef CF_WITH_SQLITE
#define CF_WITH_SQLITE 0
#endif
#ifndef CF_WITH_WATCH
#define CF_WITH_WATCH 0
#endif

/* ------------------------------------------------------------------ *
 * 64-bit integer
 *
 * Required. A build without one fails here rather than silently
 * narrowing CF_INT, because a value model that varies per platform
 * makes cross-platform determinism impossible.
 * ------------------------------------------------------------------ */

/* C++11 guarantees <stdint.h> even though it defines no
   __STDC_VERSION__, so it takes the same branch. */
#if CF_HAVE_C99 || (defined(__cplusplus) && __cplusplus >= 201103L)
#include <stdint.h>
typedef int64_t       cf_i64;
typedef uint64_t      cf_u64;
typedef int32_t       cf_i32;
typedef uint32_t      cf_u32;
typedef uint16_t      cf_u16;
typedef unsigned char cf_u8;
#elif defined(_MSC_VER)
typedef __int64          cf_i64;
typedef unsigned __int64 cf_u64;
typedef int              cf_i32;
typedef unsigned int     cf_u32;
typedef unsigned short   cf_u16;
typedef unsigned char    cf_u8;
#elif defined(__GNUC__) || defined(__clang__) || defined(__TINYC__)
/* long long is a compiler extension here, and the library knows it.
   __extension__ marks the declaration but does not suppress
   -Wlong-long in C++98, so the diagnostic is turned off for exactly
   these two lines rather than pushed onto every consumer as a
   -Wno-long-long they would have to discover. */
#if defined(__cplusplus) && __cplusplus < 201103L && defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wlong-long"
#endif
__extension__ typedef long long          cf_i64;
__extension__ typedef unsigned long long cf_u64;
#if defined(__cplusplus) && __cplusplus < 201103L && defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
typedef int            cf_i32;
typedef unsigned int   cf_u32;
typedef unsigned short cf_u16;
typedef unsigned char  cf_u8;
#else
#error "ConfigForge requires a 64-bit integer type. See the support matrix."
#endif

/* Computed rather than written as literals: a 64-bit literal needs the
   LL suffix, which is C99 and is rejected by -std=c89 -pedantic. This
   form is exact and needs no suffix on any toolchain. */
#define CF_U64_MAX ((cf_u64) ~(cf_u64)0)
#define CF_I64_MAX ((cf_i64)(CF_U64_MAX >> 1))
#define CF_I64_MIN (-CF_I64_MAX - 1)

/* ------------------------------------------------------------------ *
 * Boolean
 *
 * Deliberately not _Bool even where available: an ABI type must not
 * change size with the consumer's language standard.
 * ------------------------------------------------------------------ */

typedef int cf_bool;
#define CF_TRUE 1
#define CF_FALSE 0

/* ------------------------------------------------------------------ *
 * inline
 * ------------------------------------------------------------------ */

#if CF_HAVE_C99
#define CF_INLINE static inline
#elif defined(_MSC_VER)
#define CF_INLINE static __inline
#elif defined(__GNUC__)
#define CF_INLINE static __inline__
#else
#define CF_INLINE static
#endif

/* ------------------------------------------------------------------ *
 * Compile-time assertion
 *
 * The C89 fallback is the negative-array-size idiom. The typedef name
 * carries the line number so two assertions can share a scope.
 * ------------------------------------------------------------------ */

#if CF_HAVE_C11
#define CF_STATIC_ASSERT(cond, tag) _Static_assert(cond, #tag)
#else
#define CF_SA_CAT2(a, b) a##b
#define CF_SA_CAT(a, b) CF_SA_CAT2(a, b)
#define CF_STATIC_ASSERT(cond, tag)                                                    \
    typedef char CF_SA_CAT(cf_static_assert_, __LINE__)[(cond) ? 1 : -1]
#endif

/* ------------------------------------------------------------------ *
 * Function attributes
 * ------------------------------------------------------------------ */

#if !defined(CF_FORCE_PORTABLE) && defined(__GNUC__)
#define CF_PRINTFLIKE(fmt, first) __attribute__((format(printf, fmt, first)))
#define CF_UNUSED_RESULT __attribute__((warn_unused_result))
#define CF_LIKELY(x) __builtin_expect(!!(x), 1)
#define CF_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define CF_PRINTFLIKE(fmt, first)
#define CF_UNUSED_RESULT
#define CF_LIKELY(x) (x)
#define CF_UNLIKELY(x) (x)
#endif

/* ------------------------------------------------------------------ *
 * Atomic reference counting
 *
 * Snapshot acquire/release needs one atomic increment. Toolchains with
 * no atomic support get a single-threaded build, refused at compile
 * time if threads were requested. Stated in the support matrix rather
 * than hidden.
 * ------------------------------------------------------------------ */

#if defined(CF_SINGLE_THREADED)
#define CF_ATOMIC_KIND "single-threaded"
#elif !defined(CF_FORCE_PORTABLE) && defined(__GNUC__) &&                              \
    (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7))
#define CF_ATOMIC_GCC_ATOMIC 1
#define CF_ATOMIC_KIND "gcc __atomic"
#elif !defined(CF_FORCE_PORTABLE) && defined(__GNUC__) &&                              \
    (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 1))
#define CF_ATOMIC_GCC_SYNC 1
#define CF_ATOMIC_KIND "gcc __sync"
#elif !defined(CF_FORCE_PORTABLE) && defined(_MSC_VER)
#define CF_ATOMIC_WIN32 1
#define CF_ATOMIC_KIND "Interlocked"
#else
#define CF_ATOMIC_NONE 1
#define CF_ATOMIC_KIND "none (single-threaded only)"
#endif

typedef struct cf_refcount {
    cf_i32 v;
} cf_refcount;

/* Returns the value after the operation. */
cf_i32 cf_ref_inc(cf_refcount* rc);
cf_i32 cf_ref_dec(cf_refcount* rc);
cf_i32 cf_ref_get(const cf_refcount* rc);

/* ------------------------------------------------------------------ *
 * ASCII-only character handling
 *
 * NEVER use <ctype.h>. tolower/toupper/isspace/isdigit are
 * locale-dependent: in a Turkish locale toupper('i') is 'I' with a dot
 * and tolower('I') loses it, so key normalisation through the C library
 * resolves different keys on a Turkish system than on an English one.
 * ------------------------------------------------------------------ */

CF_INLINE int cf_is_digit(int c) {
    return c >= '0' && c <= '9';
}
CF_INLINE int cf_is_upper(int c) {
    return c >= 'A' && c <= 'Z';
}
CF_INLINE int cf_is_lower(int c) {
    return c >= 'a' && c <= 'z';
}
CF_INLINE int cf_is_alpha(int c) {
    return cf_is_upper(c) || cf_is_lower(c);
}
CF_INLINE int cf_is_alnum(int c) {
    return cf_is_alpha(c) || cf_is_digit(c);
}
CF_INLINE int cf_is_space(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}
CF_INLINE int cf_to_lower(int c) {
    return cf_is_upper(c) ? c + ('a' - 'A') : c;
}
CF_INLINE int cf_to_upper(int c) {
    return cf_is_lower(c) ? c - ('a' - 'A') : c;
}

/* Byte-wise comparison. Never strcoll: ordering must be reproducible. */
int cf_cmp_bytes(const char* a, size_t alen, const char* b, size_t blen);
int cf_cmp_ascii_ci(const char* a, size_t alen, const char* b, size_t blen);

/* ------------------------------------------------------------------ *
 * Bounded formatting
 *
 * snprintf is C99. The C89 fallback is not "use sprintf" - an unbounded
 * formatter is a defect at any standard level - so a small bounded
 * formatter is implemented instead, and used unconditionally so that
 * output is identical in both portability modes.
 *
 * Supported conversions: %s %c %d %i %u %zu %x %% and the ConfigForge
 * extensions %I (cf_i64), %U (cf_u64) and %S (a const char* followed
 * by a size_t, for a slice that is not NUL-terminated). Width,
 * precision and flags are deliberately not supported; callers that
 * need alignment build it themselves.
 *
 * Returns the number of characters that would have been written,
 * excluding the terminator, as snprintf does. Always NUL-terminates
 * when cap > 0.
 * ------------------------------------------------------------------ */

size_t cf_fmt(char* buf, size_t cap, const char* fmt, ...);
size_t cf_fmt_append(char* buf, size_t cap, size_t at, const char* fmt, ...);
size_t cf_vfmt(char* buf, size_t cap, const char* fmt, va_list ap);

/* ------------------------------------------------------------------ *
 * Build description, reported in explain() and in the provenance
 * report so that a restricted build is distinguishable from a full one.
 * ------------------------------------------------------------------ */

const char* cf_port_description(void);

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_CF_PORT_H */
