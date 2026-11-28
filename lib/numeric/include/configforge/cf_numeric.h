/*
 * cf_numeric.h - locale-independent numeric conversion, and units.
 *
 * INTERNAL (D-51), but for the units at the end: the conversions are
 * exported for ConfigForge Doctor and the tests, and outside the 1.x
 * promise - a minor release may change them. Their functions are declared
 * only under CF_INTERNAL_API (D-51, D-88). A program uses the
 * configforge.h functions instead. The units ("30s", "512MiB"; D-113,
 * D-222) are public, in their first minor release (D-89).
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
CF_NODISCARD CF_API cf_bool cf_text_to_i64(const char* s, size_t len, cf_i64* out,
                                           size_t* end);
CF_NODISCARD CF_API cf_bool cf_text_to_u64(const char* s, size_t len, cf_u64* out,
                                           size_t* end);
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
CF_NODISCARD CF_API cf_bool cf_text_to_f64(const char* s, size_t len, double* out,
                                           size_t* end);
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

/* ------------------------------------------------------------------ *
 * Units: durations and sizes (D-113, D-222)
 *
 * `"1h30m"` and `"512MiB"` read as the whole number of the unit's base
 * (5400000 ms, 536870912 B). A unit is an open value: the two built-in
 * ones are reached through cf_unit_duration_get and cf_unit_size_get
 * (functions, not data, so that a shared library on every platform
 * exports them), and a program writes a cf_unit of its own with its own
 * parser. The parsers know no configuration: cf_def.unit (cf_define.h)
 * is what binds a key to one. Every abbreviation the two read is a row
 * of the table in docs/Guide.md, "Durations and sizes".
 *
 * Notation: case-sensitive, no whitespace, no locale. A fraction ("1.5h")
 * is read when the result is a whole number of base units, and refused
 * otherwise, never rounded. A bare number is the base unit. A suffixed
 * form has no sign. Every product and sum is checked against the
 * largest cf_i64 before it is made; an overflow is a refusal.
 * ------------------------------------------------------------------ */

/* The base of a duration: CF_UNIT_BASE_DEFAULT is the unit's own (ms for a
   duration, B for a size); a duration also takes seconds or microseconds. */
#define CF_UNIT_BASE_DEFAULT 0
#define CF_UNIT_BASE_S 1
#define CF_UNIT_BASE_US 2

typedef struct cf_unit_options {
    size_t  size;       /* sizeof(cf_unit_options): set by cf_unit_options_init */
    int     base;       /* CF_UNIT_BASE_DEFAULT, or a duration's CF_UNIT_BASE_S/_US */
    cf_bool allow_bare; /* CF_TRUE (default): "30" is 30 base units; else refused */
    cf_bool calendar;   /* CF_FALSE (default): a month or a year is refused as
                           "is ambiguous: use days"; CF_TRUE: month 30 d, year 365 d */
    cf_bool bare_si;    /* CF_FALSE (default): a size's bare K, M, G, T are IEC
                           (1024^n), as Kubernetes and the JVM read them; CF_TRUE: SI */
} cf_unit_options;

/* The defaults above. A NULL pointer is accepted and left alone.
   Guarantee: nothrow. */
CF_EXPERIMENTAL(2, 1) void cf_unit_options_init(cf_unit_options* o);

typedef struct cf_unit {
    size_t      size; /* sizeof(cf_unit) */
    const char* name; /* "duration", "size": what a refusal and the schema call it */
    /* Text to number. CF_TRUE with *out, or CF_FALSE with *reason a static
       sentence ("is ambiguous: use days", "is not a whole number of ms",
       "overflows") or NULL. */
    cf_bool (*parse)(const struct cf_unit* u, const char* s, size_t n, cf_i64* out,
                     const char** reason);
    /* Number to text, the text `explain` shows: the length written, 0 and an
       empty string when `cap` is too small. */
    size_t (*format)(const struct cf_unit* u, cf_i64 v, char* buf, size_t cap);
    cf_unit_options options; /* the unit's own defaults; a program copies the unit and
                                changes them */
} cf_unit;

/* The two built-in units, valid for the life of the program.
   Guarantee: nothrow. */
CF_EXPERIMENTAL(2, 1) const cf_unit* cf_unit_duration_get(void);
CF_EXPERIMENTAL(2, 1) const cf_unit* cf_unit_size_get(void);

/* `u->parse`, usable alone: NULL arguments (no unit, no text, no place for the
   number) are refused. `reason` may be NULL, and is NULL on a refusal that has
   no sentence of its own (the text is not in the unit's notation).
   Guarantee: strong. */
CF_NODISCARD CF_EXPERIMENTAL(2, 1) cf_bool cf_unit_parse(const cf_unit* u,
                                                         const char* s, size_t n,
                                                         cf_i64*      out,
                                                         const char** reason);

/* `u->format`: a duration as "1h30m", a size as "512MiB" (the largest exact
   suffix), a negative number as the bare number. Returns the length written
   (the text is terminated), or 0 with an empty string when `cap` is too small
   or the unit has no formatter.
   Guarantee: nothrow. */
CF_EXPERIMENTAL(2, 1) size_t cf_unit_format(const cf_unit* u, cf_i64 v, char* buf,
                                            size_t cap);

/* The helper for a unit of a program's own: a table of abbreviations, so that the
   program writes only the table. A term is an abbreviation and its factor in
   the table's finest unit; a factor of 0 names a spelling that is recognised and
   refused with the table's `refused` sentence (a month, say). */
typedef struct cf_unit_term {
    const char* name;
    cf_i64      factor;
} cf_unit_term;

typedef struct cf_unit_table {
    const cf_unit_term* terms;
    size_t              count;
    cf_i64              per_base; /* finest units in one base unit: 1..4294967295 */
    cf_bool             sum;      /* CF_TRUE: "1h30m", terms strictly decreasing;
                                     CF_FALSE: one term */
    cf_bool     allow_bare;       /* a bare number is the base unit */
    const char* not_whole;        /* the reason for a result that is no whole number of
                                     base units (NULL: "is not a whole number") */
    const char* refused;          /* the reason for a term whose factor is 0 */
} cf_unit_table;

/* One term, or a sum of terms, `number` and an abbreviation, read exactly (no
   floating point): the notation above, over this table. A bare number
   (optionally signed) is the base unit when `allow_bare`.
   Guarantee: strong. */
CF_NODISCARD CF_EXPERIMENTAL(2, 1) cf_bool cf_unit_table_parse(const cf_unit_table* t,
                                                               const char* s, size_t n,
                                                               cf_i64*      out,
                                                               const char** reason);

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_CF_NUMERIC_H */
