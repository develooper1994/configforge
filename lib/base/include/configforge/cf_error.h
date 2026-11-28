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

/* A cf_error owns its text (D-176). key, source and message are
   `const char*`, "" when unset and never NULL after cf_error_clear or any
   cf_error_set; a struct filled with zeros before its first use has NULL
   pointers, so a caller reads them only after a failure. A text that fits
   the old arrays (CF_KEY_MAX, CF_SOURCE_MAX, CF_MESSAGE_MAX) lives in the
   error's inline store, which takes the place the arrays had, and costs no
   allocation; a longer one goes to a block from cf_default_allocator()
   that the error owns and cf_error_clear frees. The pointers may point
   into the struct itself, so a cf_error is never copied by value (a plain
   assignment leaves the texts pointing into the source): use
   cf_error_copy. A build with CF_FIXED_LIMITS never allocates and cuts a
   text at the old array's size.

   The inline store and the pointers are 1.0's region of the struct, always
   written. The blocks (cfi_heap) are the one member added behind `offset`:
   an error whose `size` does not reach them keeps its texts inline, cut,
   and never owns a block. A struct is zero or CF_ERROR_INIT before its first
   use: the blocks are read as it was. An error that holds a long text must
   be cleared (or set again) before it goes out of scope, as any owner must.
   The members whose names start with cfi_ are the store: not for callers. */
typedef struct cf_error {
    size_t      size; /* sizeof(cf_error) at the call site */
    int         code;
    const char* key;
    const char* source;
    const char* message;
    char        cfi_key[CF_KEY_MAX];
    char        cfi_source[CF_SOURCE_MAX];
    char        cfi_message[CF_MESSAGE_MAX];
    cf_loc      where;  /* (1.8) line and column, 0 when unknown; file is always NULL */
    int         reason; /* (1.8) CF_REASON_*, CF_REASON_NONE when unset */
    size_t      offset; /* (2.0) byte offset of the cause, (size_t)-1 if
                           unknown */
    char* cfi_heap[3];  /* (2.0) the blocks of key, source, message past the store */
} cf_error;

/* The size field is what allows fields to be added after 1.0 without
   breaking the interface (D-51). The fields through the inline store are
   1.0's and are always written, whatever `size` says, so a cf_error left
   without CF_ERROR_INIT still works; a field added later is written only
   where `size` says the caller's struct has it. The library never copies a
   whole cf_error into a caller's: cf_error_copy does it field by field.

   Fenced from the formatter because clang-format 18 and 22 disagree
   about a brace initialiser inside a macro - one keeps it on a line,
   the other spreads it over six - and the gate cannot pass under both
   unless neither touches it. */
/* clang-format off */
#define CF_ERROR_INIT {sizeof(cf_error), 0, "", "", "", {0}, {0}, {0}, {NULL, 0, 0}, 0, (size_t)-1, {NULL, NULL, NULL}}
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

/* Guarantee: strong. A pure check; CF_FALSE says the key is not canonical.
   Thread: safe.
   Signal: safe.
   Alloc: none. */
CF_NODISCARD CF_API cf_bool cf_key_valid(const char* key, size_t len);

/* The same rule with the longest key a caller allows: the key space and
   the loader pass the sizing mode's limit (D-176); cf_key_valid is this
   with CF_KEY_MAX - 1. Internal. */
cf_bool cfi_key_valid_n(const char* key, size_t len, size_t limit);

/* Empties `e`: code CF_OK, key, source, message, place and reason empty, and
   frees a long text it owned. `size` is kept, so a cleared error can be
   reused. NULL is ignored.
   Guarantee: nothrow. */
CF_API void cf_error_clear(cf_error* e);

/* What `src` says, copied into `dst`: 1.0's fields always, a later field
   where dst->size covers it. The texts are copied into `dst`'s own store
   (a long one into a block `dst` owns), never pointed at in `src`. If
   src->size does not cover a later field, that field is cleared. dst->size
   is kept. Safe with NULL on either side.
   Guarantee: nothrow. A text that needs a block and gets none is cut to the
   old array's size; a NULL side is ignored. */
CF_API void cf_error_copy(cf_error* dst, const cf_error* src);

/* Set code, key, source and a formatted message, clearing place and reason.
   Safe with a NULL
   error, so every call site can report unconditionally instead of
   guarding. Returns CF_FALSE so a caller can write
   `return cf_error_set(err, ...);`
   The key is cut at CF_KEY_MAX - 1 bytes, the source at CF_SOURCE_MAX - 1
   and the message at CF_MESSAGE_MAX - 1, whatever the sizing mode: what
   cf_error_set writes is what it always wrote. cf_error_set_long is the
   call that keeps more.
   Guarantee: nothrow. The texts are cut to fit; a NULL `e` is ignored. */
CF_API cf_bool cf_error_set(cf_error* e, cf_code code, const char* key,
                            const char* source, const char* fmt, ...);

/* As cf_error_set, with each of the three texts allowed `cap` bytes; 0 means
   no cap (the sizing mode's cap, from cf_config_sizing, when the caller knows
   one). A text past the old array's size goes to a block the error owns. A
   build with CF_FIXED_LIMITS never allocates, and when an allocation fails the
   text is cut, as cf_error_set cuts it. Returns CF_FALSE.
   Guarantee: nothrow. The texts are cut to `cap` and to what can be
   allocated; a NULL `e` is ignored. */
CF_API cf_bool cf_error_set_long(cf_error* e, cf_code code, const char* key,
                                 const char* source, size_t cap, const char* fmt, ...);

/* Guarantee: nothrow. "?" for an unknown code. */
CF_API const char* cf_code_name(cf_code c);
/* Guarantee: nothrow. "unknown" for an unknown code. */
CF_API const char* cf_code_description(cf_code c);

/* The name ("number-range") and description of a reason. Returns "?" and
   "unknown", respectively, for an unknown value.
   Guarantee: nothrow. */
CF_API const char* cf_reason_name(int reason);
/* Guarantee: nothrow. */
CF_API const char* cf_reason_description(int reason);
/* Whether a refusal is "error" (the input is malformed by its format's own
   rules), "warning" (well formed, refused for a limit or a policy) or "none";
   and the CWE it defends, such as "CWE-674", or "" for none. "?" and "" for
   an unknown value. Promoted in 2.0.
   Guarantee: nothrow. */
CF_API const char* cf_reason_severity(int reason);
/* Guarantee: nothrow. */
CF_API const char* cf_reason_weakness(int reason);

/* Sets where (file NULL, line, col) and reason on an error whose message
   is already set, where e->size reaches them. NULL is ignored.
   Guarantee: nothrow. */
CF_API void cf_error_place(cf_error* e, int reason, unsigned int line,
                           unsigned int col);

/* As cf_error_place, and the byte offset of the cause in the input (the
   index of the byte, from 0) where e->size reaches it; (size_t)-1 says the
   reader has none. cf_error_place leaves it (size_t)-1. Promised since 2.1
   (experimental in 2.0, D-89).
   Guarantee: nothrow. */
CF_API void cf_error_place_at(cf_error* e, int reason, unsigned int line,
                              unsigned int col, size_t offset);

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_CF_ERROR_H */
