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

typedef struct cf_error {
    size_t size; /* sizeof(cf_error) at the call site */
    int    code;
    char   key[CF_KEY_MAX];
    char   source[CF_SOURCE_MAX];
    char   message[CF_MESSAGE_MAX];
} cf_error;

/* The size field is what allows fields to be added after 1.0 without
   breaking the interface: a newer library sees an older caller's
   smaller struct and writes only what fits.

   Fenced from the formatter because clang-format 18 and 22 disagree
   about a brace initialiser inside a macro - one keeps it on a line,
   the other spreads it over six - and the gate cannot pass under both
   unless neither touches it. */
/* clang-format off */
#define CF_ERROR_INIT {sizeof(cf_error), 0, {0}, {0}, {0}}
/* clang-format on */

CF_API void cf_error_clear(cf_error* e);

/* Set code, key, source and a formatted message. Safe with a NULL
   error, so every call site can report unconditionally instead of
   guarding. Returns CF_FALSE so a caller can write
   `return cf_error_set(err, ...);` */
CF_API cf_bool cf_error_set(cf_error* e, cf_code code, const char* key,
                            const char* source, const char* fmt, ...);

CF_API const char* cf_code_name(cf_code c);
CF_API const char* cf_code_description(cf_code c);

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_CF_ERROR_H */
