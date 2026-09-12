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

#define CF_KEY_MAX 256
#define CF_SOURCE_MAX 64
#define CF_MESSAGE_MAX 256

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
   smaller struct and writes only what fits. */
#define CF_ERROR_INIT {sizeof(cf_error), 0, {0}, {0}, {0}}

void cf_error_clear(cf_error* e);

/* Set code, key, source and a formatted message. Safe with a NULL
   error, so every call site can report unconditionally instead of
   guarding. Returns CF_FALSE so a caller can write
   `return cf_error_set(err, ...);` */
cf_bool cf_error_set(cf_error* e, cf_code code, const char* key, const char* source,
                     const char* fmt, ...);

const char* cf_code_name(cf_code c);
const char* cf_code_description(cf_code c);

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_CF_ERROR_H */
