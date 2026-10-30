/*
 * cf_numeric.h - locale-independent numeric conversion.
 *
 * INTERNAL (D-51): exported for ConfigForge Doctor, the bindings and the
 * tests, and outside the 1.x promise - a minor release may change it.
 * A program uses the configforge.h functions instead.
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

/* Worst case including sign and terminator. */
#define CF_I64_DIGITS 21

/* Write the decimal form of v. Returns the length written, or 0 when
   cap is too small. Always terminates when cap > 0. */
CF_API size_t cf_i64_to_text(cf_i64 v, char* buf, size_t cap);
CF_API size_t cf_u64_to_text(cf_u64 v, char* buf, size_t cap);

/* Parse an optionally signed decimal integer. Returns CF_TRUE on
   success. Rejects overflow, empty input, and surrounding space;
   callers trim deliberately. When end is NULL the whole input must be
   consumed; otherwise *end receives the number of bytes consumed. */
CF_API cf_bool cf_text_to_i64(const char* s, size_t len, cf_i64* out, size_t* end);
CF_API cf_bool cf_text_to_u64(const char* s, size_t len, cf_u64* out, size_t* end);

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
CF_API size_t cf_f64_to_text(double v, char* buf, size_t cap);

/* Parse a C-locale floating point number. The decimal separator is
   always '.', never the locale's. Accepts the forms produced by
   cf_f64_to_text and the usual hand-written ones. */
CF_API cf_bool cf_text_to_f64(const char* s, size_t len, double* out, size_t* end);

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

CF_API cf_scalar_kind cf_infer_scalar(const char* s, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_CF_NUMERIC_H */
