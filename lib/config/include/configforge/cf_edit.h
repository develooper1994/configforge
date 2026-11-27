/*
 * cf_edit.h - creating, changing and deleting settings, as one change.
 *
 * Reading is the snapshot's (cf_get_*, explain). Writing goes to a
 * source that can be written - a store (cf_builder_store) the program
 * granted with cf_builder_writable - and an edit gathers a change to it
 * before anything is written:
 *
 *     cf_edit* e = cf_edit_begin(cfg, "store", &err);
 *     cf_edit_set_int(e, "pool.size", 32);
 *     cf_edit_set_text(e, "log.level", "debug");
 *     cf_edit_unset(e, "feature.beta");            -- the next source down supplies it
 *     if (!cf_edit_commit(e, "admin, 2026-10-30: incident 42", &err)) { ... }
 *
 * Commit holds the whole change to the contract - the types, ranges and
 * spellings, and each key's policy (D-33) - and writes it in one
 * transaction, its note beside every value, or writes nothing and says
 * why. The snapshot does not change: rebuild, or under cf_live call
 * cf_live_commit, which commits and reloads in one step.
 *
 * An edit is used by one thread. Its strings are copied, so the caller's
 * may go as soon as each call returns.
 */

#ifndef CONFIGFORGE_CF_EDIT_H
#define CONFIGFORGE_CF_EDIT_H

#include "configforge/cf_alloc.h"
#include "configforge/cf_port.h"
#include "configforge/cf_error.h"

#ifdef __cplusplus
extern "C" {
#endif

struct cf_config;
typedef struct cf_edit cf_edit;

/* A change to `target`, a source of `c` by name. The edit keeps its own
   reference to `c`. NULL, with `err` saying why, when `c` or `target`
   is NULL or there is no memory - an edit allocates from the allocator the
   configuration was built with, and from the default one for a
   configuration built in a fixed buffer, which has none. Whether the
   source exists, can be written and may be, is asked at commit with the
   rest, so every refusal comes from one place.
   Guarantee: strong. The checks come before anything is made: on NULL, `c` is
   untouched and no edit exists; on success `c` gains one reference, which
   commit or abort gives back. */
CF_NODISCARD CF_API cf_edit* cf_edit_begin(struct cf_config* c, const char* target,
                                           cf_error* err);

/* Set a key: as text, which the contract types as it types a file's, or
   already typed. A key set twice keeps the second. False for no memory
   or a key that is not one, which commit then reports.
   Guarantee: basic. The edit stays valid: the first refusal is held for commit, the
   keys set before it stay set, and cf_edit_abort drops it. The target is not read
   or written until commit. */
CF_NODISCARD CF_API cf_bool cf_edit_set_text(cf_edit* e, const char* key,
                                             const char* value);
CF_NODISCARD CF_API cf_bool cf_edit_set_int(cf_edit* e, const char* key, cf_i64 value);
CF_NODISCARD CF_API cf_bool cf_edit_set_bool(cf_edit* e, const char* key,
                                             cf_bool value);
#if CF_WITH_REAL
CF_NODISCARD CF_API cf_bool cf_edit_set_real(cf_edit* e, const char* key, double value);
#endif

/* Delete a key from the target, so that the next source down - a file,
   the defaults - supplies it again.
   Guarantee: basic. As cf_edit_set_text. */
CF_NODISCARD CF_API cf_bool cf_edit_unset(cf_edit* e, const char* key);

/* How many keys the edit changes.
   Guarantee: nothrow. 0 for a NULL edit. */
CF_API size_t cf_edit_count(const cf_edit* e);

/* Check and write the whole change, `note` (who, when, why) beside each
   value. Consumes the edit either way.
   Guarantee: strong. Nothing is written until the whole document is made; a
   failure leaves the target as it was and the error is the first refusal, or
   the check's. A NULL edit fails with `err` set. */
CF_NODISCARD CF_API cf_bool cf_edit_commit(cf_edit* e, const char* note, cf_error* err);

/* Internal: the allocator `c` takes its memory from, NULL for a
   configuration built in a fixed buffer. The edit's own arena comes from
   it (K-9b). */
const cf_allocator* cfi_config_allocator(const struct cf_config* c);

/* Drop the edit; nothing is written.
   Guarantee: nothrow. A NULL edit is accepted; the edit is consumed. */
CF_API void cf_edit_abort(cf_edit* e);

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_CF_EDIT_H */
