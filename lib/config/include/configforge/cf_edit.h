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

#include "configforge/cf_port.h"
#include "configforge/cf_error.h"

#ifdef __cplusplus
extern "C" {
#endif

struct cf_config;
typedef struct cf_edit cf_edit;

/* A change to `target`, a source of `c` by name. The edit keeps its own
   reference to `c`. NULL, with `err` saying why, when `c` or `target`
   is NULL or there is no memory - an edit allocates from the default
   allocator, also for a configuration built in an arena. Whether the
   source exists, can be written and may be, is asked at commit with the
   rest, so every refusal comes from one place. */
CF_NODISCARD CF_API cf_edit* cf_edit_begin(struct cf_config* c, const char* target,
                                           cf_error* err);

/* Set a key: as text, which the contract types as it types a file's, or
   already typed. A key set twice keeps the second. False for no memory
   or a key that is not one, which commit then reports. */
CF_API cf_bool cf_edit_set_text(cf_edit* e, const char* key, const char* value);
CF_API cf_bool cf_edit_set_int(cf_edit* e, const char* key, cf_i64 value);
CF_API cf_bool cf_edit_set_bool(cf_edit* e, const char* key, cf_bool value);
#if CF_WITH_REAL
CF_API cf_bool cf_edit_set_real(cf_edit* e, const char* key, double value);
#endif

/* Delete a key from the target, so that the next source down - a file,
   the defaults - supplies it again. */
CF_API cf_bool cf_edit_unset(cf_edit* e, const char* key);

/* How many keys the edit changes. */
CF_API size_t cf_edit_count(const cf_edit* e);

/* Check and write the whole change, `note` (who, when, why) beside each
   value. Consumes the edit either way. */
CF_NODISCARD CF_API cf_bool cf_edit_commit(cf_edit* e, const char* note, cf_error* err);

/* Drop the edit; nothing is written. */
CF_API void cf_edit_abort(cf_edit* e);

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_CF_EDIT_H */
