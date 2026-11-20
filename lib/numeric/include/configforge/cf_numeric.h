/*
 * cf_numeric.h - locale-independent numeric conversion.
 *
 * INTERNAL (D-51): exported for ConfigForge Doctor and the tests, and
 * outside the 1.x promise - a minor release may change it. Its functions
 * are declared only under CF_INTERNAL_API (D-51, D-88). A program uses
 * the configforge.h functions instead.
 *
 * strtod, atoi and printf("%f") all follow the active locale's decimal
 * separator, so a configuration file written on one machine would not
 * parse identically on another. These conversions are therefore always
 * the library's own, at every standard level and on every toolchain.
 *
 * Floating point uses exact integer digit generation (Steele & White /
 * Burger & Dybvig), which produces the shortest decimal string that
 * round-trips, identically on every platform. Decision D-25.
 */

#ifndef CONFIGFORGE_CF_NUMERIC_H
#define CONFIGFORGE_CF_NUMERIC_H

#include "configforge/cf_port.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ *
 * Integers
 * ------------------------------------------------------------------ */

/* Parse an optionally signed decimal integer. Returns CF_TRUE on
   success. Rejects overflow, empty input, and surrounding space;
   callers trim deliberately. When end is NULL the whole input must be
   consumed; otherwise *end receives the number of bytes consumed. */
#if CF_INTERNAL_API
CF_API cf_bool cf_text_to_i64(const char* s, size_t len, cf_i64* out, size_t* end);
CF_API cf_bool cf_text_to_u64(const char* s, size_t len, cf_u64* out, size_t* end);
#endif /* CF_INTERNAL_API */

/* ------------------------------------------------------------------ *
 * Floating point
 *
 * Disable with CF_WITH_REAL=0 on targets where CF_REAL is not needed.
 * The exact digit generator uses about 1 KB of stack, which is the
 * only part of the library that a very small target might refuse.
 * ------------------------------------------------------------------ */

#if CF_WITH_REAL

/* Worst case: sign, 17 digits, point, 'e', exponent sign, 3 exponent
   digits, terminator. */
#define CF_F64_DIGITS 32

/* Write the shortest decimal string that reads back as exactly v.
   Output is identical on every platform. Returns the length written,
   or 0 when cap is too small or v is not finite.

   NaN and infinity are deliberately not representable: a configuration
   value cannot be either, and rejecting them here keeps every codec
   from having to invent a spelling. */
#if CF_INTERNAL_API
CF_API size_t cf_f64_to_text(double v, char* buf, size_t cap);
#endif /* CF_INTERNAL_API */

/* Parse a C-locale floating point number. The decimal separator is
   always '.', never the locale's. Accepts the forms produced by
   cf_f64_to_text and the usual hand-written ones.

   Range: a number from 10^309 up, or one that would round to infinity,
   is refused. Below the smallest subnormal the result is zero. At most
   290 significant digits decide the value; later digits move only the
   exponent and the rounding, so a literal of hundreds of digits is read
   as what it is, and one lying exactly on a halfway point may be one
   unit in the last place off. */
#if CF_INTERNAL_API
CF_API cf_bool cf_text_to_f64(const char* s, size_t len, double* out, size_t* end);
#endif /* CF_INTERNAL_API */

/* A double as its IEEE 754 bit pattern, and back, by memcpy. (1.5) */
#if CF_INTERNAL_API
CF_API cf_u64 cf_f64_bits(double d);
CF_API double cf_f64_from_bits(cf_u64 bits);
#endif /* CF_INTERNAL_API */

/* A half- and a single-precision pattern (in the low 16 and 32 bits) as
   a double, exactly, the non-finite ones included. (1.5) */
#if CF_INTERNAL_API
CF_API double cf_f64_from_f16_bits(cf_u32 bits);
CF_API double cf_f64_from_f32_bits(cf_u32 bits);
#endif /* CF_INTERNAL_API */

#endif /* CF_WITH_REAL */

/* ------------------------------------------------------------------ *
 * Tier 0 type inference
 *
 * Used only when no definition exists. Deliberately timid: see the
 * Type Inference section of the specification and decision D-24.
 * ------------------------------------------------------------------ */

typedef enum cf_scalar_kind {
    CF_SCALAR_TEXT = 0,
    CF_SCALAR_BOOL,
    CF_SCALAR_INT,
    CF_SCALAR_REAL
} cf_scalar_kind;

#if CF_INTERNAL_API
CF_API cf_scalar_kind cf_infer_scalar(const char* s, size_t len);
#endif /* CF_INTERNAL_API */

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_CF_NUMERIC_H */
