/*
 * cf_value.h - the value model.
 *
 * Five types. Structure is a naming convention on the key, never a
 * shape in the data: see the Value Model section of the design. That
 * decision is what keeps a C implementation reasonable, because there
 * is no recursive value and therefore no ownership graph.
 */

#ifndef CONFIGFORGE_CF_VALUE_H
#define CONFIGFORGE_CF_VALUE_H

#include "configforge/cf_numeric.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The list lives in cf_type.def and is expanded here and in the two
   name functions. An enum beside a hand-written switch drifts: adding a
   type and forgetting its name gives a "?" at runtime and no diagnostic
   at all. */
typedef enum cf_type {
#define CF_TYPE_X(sym, name, json) sym,
#define CF_TYPE_LAST_X(sym, name, json) sym
#include "configforge/cf_type.def"
#undef CF_TYPE_X
#undef CF_TYPE_LAST_X
} cf_type;

typedef struct cf_text {
    const char* p; /* NUL-terminated as well as length-carrying */
    size_t      n;
} cf_text;

typedef struct cf_value {
    cf_type type;
    union {
        cf_bool b;
        cf_i64  i;
#if CF_WITH_REAL
        double r;
#endif
        cf_text t;
    } u;
} cf_value;

const char* cf_type_name(cf_type t);

/* JSON Schema spells the same types differently; the report and the
   schema generator both need that spelling. Two names, one list. */
const char* cf_type_json_name(cf_type t);

void cf_value_null(cf_value* v);
void cf_value_bool(cf_value* v, cf_bool b);
void cf_value_int(cf_value* v, cf_i64 i);
#if CF_WITH_REAL
void cf_value_real(cf_value* v, double r);
#endif
void cf_value_text(cf_value* v, const char* p, size_t n);

/* Convert text to the requested type. Returns CF_FALSE when the text
   does not denote a value of that type; the caller then reports a
   conversion failure naming the parameter and the source, which is
   the only place conversion errors are allowed to appear. */
cf_bool cf_value_from_text(cf_type t, const char* s, size_t n, cf_value* out);

/* Infer a type from text. Used only where no definition exists, and
   deliberately timid: see decision D-24. */
cf_bool cf_value_infer(const char* s, size_t n, cf_value* out);

/* Canonical text form. Identical on every platform, which is what
   makes the determinism hash and the golden fixtures meaningful.
   Returns the length written, or 0 when cap is too small. */
size_t cf_value_to_text(const cf_value* v, char* buf, size_t cap);

/* Longest output cf_value_to_text can produce for a non-text value. */
#if CF_WITH_REAL
#define CF_VALUE_TEXT_MAX CF_F64_DIGITS
#else
#define CF_VALUE_TEXT_MAX CF_I64_DIGITS
#endif

int     cf_value_cmp(const cf_value* a, const cf_value* b);
cf_bool cf_value_equal(const cf_value* a, const cf_value* b);

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_CF_VALUE_H */
