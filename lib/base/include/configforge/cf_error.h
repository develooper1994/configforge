/*
 * cf_error.h - the error model.
 *
 * Errors are concentrated at construction rather than spread across
 * every read. Once a configuration exists, reading is infallible for
 * any key with a default or a definition, so almost everything here is
 * a construction-time condition.
 *
 * The error is a caller-supplied struct. Not a thread-local
 * last-error, because global error state contradicts a stated
 * principle; not an allocation, because the failure path has to work
 * in the fixed-capacity profile, which is exactly where allocation
 * fails.
 */

#ifndef CONFIGFORGE_CF_ERROR_H
#define CONFIGFORGE_CF_ERROR_H

#include "configforge/cf_port.h"

#ifdef __cplusplus
extern "C" {
#endif

/* CF_KEY_MAX and CF_MESSAGE_MAX are limits the builder chooses; they
   live in cf_build_options.h with what each one costs. */
#define CF_SOURCE_MAX 64

typedef enum cf_code {
#define CF_CODE_X(sym, name, desc) sym,
#define CF_CODE_LAST_X(sym, name, desc) sym
#include "configforge/cf_code.def"
#undef CF_CODE_X
#undef CF_CODE_LAST_X
} cf_code;

typedef enum cf_reason {
#define CF_REASON_X(sym, name, desc) sym,
#define CF_REASON_LAST_X(sym, name, desc) sym
#include "configforge/cf_reason.def"
#undef CF_REASON_X
#undef CF_REASON_LAST_X
} cf_reason;

typedef struct cf_loc {
    const char*  file;
    unsigned int line;
    unsigned int col;
} cf_loc;

typedef struct cf_error {
    size_t size; /* sizeof(cf_error) at the call site */
    int    code;
    char   key[CF_KEY_MAX];
    char   source[CF_SOURCE_MAX];
    char   message[CF_MESSAGE_MAX];
    cf_loc where;  /* (1.8) line and column, 0 when unknown; file is always NULL */
    int    reason; /* (1.8) CF_REASON_*, CF_REASON_NONE when unset */
} cf_error;

/* The size field is what allows fields to be added after 1.0 without
   breaking the interface (D-51). The fields through message are 1.0's and are
   always written, whatever `size` says, so a cf_error left without
   CF_ERROR_INIT still works; a field added later is written only where
   `size` says the caller's struct has it. The library never copies a
   whole cf_error into a caller's: cf_error_copy does it field by field.

   Fenced from the formatter because clang-format 18 and 22 disagree
   about a brace initialiser inside a macro - one keeps it on a line,
   the other spreads it over six - and the gate cannot pass under both
   unless neither touches it. */
/* clang-format off */
#define CF_ERROR_INIT {sizeof(cf_error), 0, {0}, {0}, {0}, {NULL, 0, 0}, 0}
/* clang-format on */

/* ------------------------------------------------------------------ *
 * Keys
 *
 * Canonical form is dotted, lowercase ASCII, and at most CF_KEY_MAX - 1
 * bytes, so that a key and its terminator fit the buffers sized by the
 * limit. Comparison is byte-wise and case-sensitive: case-insensitive
 * keys are Viper's most-reported defect, because they silently collapse
 * two distinct parameters. Derivation to MYAPP_SERVER_PORT and
 * --server-port belongs to the source that needs it, not here.
 * ------------------------------------------------------------------ */

CF_API cf_bool cf_key_valid(const char* key, size_t len);

/* Empties `e`: code CF_OK, key, source, message, place and reason empty. `size` is
   kept, so a cleared error can be reused. NULL is ignored. */
CF_API void cf_error_clear(cf_error* e);

/* What `src` says, copied into `dst` as far as `dst` has room: 1.0's
   fields always, a later field where dst->size covers it. If src->size
   does not cover a later field, that field is cleared. dst->size is
   kept. Safe with NULL on either side. */
CF_API void cf_error_copy(cf_error* dst, const cf_error* src);

/* Set code, key, source and a formatted message, clearing place and reason.
   Safe with a NULL
   error, so every call site can report unconditionally instead of
   guarding. Returns CF_FALSE so a caller can write
   `return cf_error_set(err, ...);` */
CF_API cf_bool cf_error_set(cf_error* e, cf_code code, const char* key,
                            const char* source, const char* fmt, ...);

CF_API const char* cf_code_name(cf_code c);
CF_API const char* cf_code_description(cf_code c);

/* The name ("number-range") and description of a reason. Returns "?" and
   "unknown", respectively, for an unknown value. */
CF_EXPERIMENTAL(1, 8) const char* cf_reason_name(int reason);
CF_EXPERIMENTAL(1, 8) const char* cf_reason_description(int reason);

/* Sets where (file NULL, line, col) and reason on an error whose message
   is already set, where e->size reaches them. NULL is ignored. */
CF_EXPERIMENTAL(1, 8) void cf_error_place(cf_error* e, int reason, unsigned int line,
                                          unsigned int col);

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_CF_ERROR_H */
