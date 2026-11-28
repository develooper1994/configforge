/*
 * cf_emit.h - writing a configuration out.
 *
 * Restricted, on purpose, to a file ConfigForge owns and rewrites in
 * full. Editing a hand-written file in place means preserving its
 * comments, its ordering and its whitespace: the parser would have to
 * keep a concrete syntax tree that every codec then maintains, and
 * getting it slightly wrong silently destroys something a person wrote.
 *
 * What "save settings" actually needs is an overrides file, which is
 * what git config, systemd drop-ins and sudoers.d all are.
 */

#ifndef CONFIGFORGE_CF_EMIT_H
#define CONFIGFORGE_CF_EMIT_H

#include "configforge/cf_file.h"
#include "configforge/cf_flatten.h"
#include "configforge/cf_serialize.h"

#ifdef __cplusplus
extern "C" {
#endif

struct cf_config;

typedef enum cf_emit_what {
    /* Every resolved key, defaults included. A complete snapshot. */
    CF_EMIT_ALL = 0,

    /* Only what a source configured, which is what an overrides file
       should contain: small, readable, and it lets a later release move
       a default the user never touched. */
    CF_EMIT_CONFIGURED,

    /* Only what one rank supplied. */
    CF_EMIT_FROM_RANK
} cf_emit_what;

/* Measure-or-write, like everything else that renders: with a NULL
   buffer nothing is stored and the length still accumulates.
   `rank` is read only for CF_EMIT_FROM_RANK.
   Refuses rather than truncates: a format with no array syntax meeting
   an array it would have to write is an error before anything is
   written, not a surprise halfway through a file. An array that `what`
   leaves out is not one it would have to write.
   A configuration nested deeper than CF_MAX_DEPTH (cf_build_options.h)
   is refused as CF_E_KEY in every format: it is as deep as a reader
   takes, and a file past that could not be read back.
   A selected key that is both a value and a container is also CF_E_KEY.
   Literal ${ in a text value is written as $${ so construction from
   the emitted file preserves it, rather than expanding it again.
   The lower-level cf_doc_emit does not interpret or escape references.
   Whatever the reason, a refusal returns 0 with `err` set, and a buffer
   that was given holds an empty string - not the front of a document
   that the walk had written before it came to the reason.
   Guarantee: strong. The configuration is untouched; a refusal returns 0 with
   `err` set and an empty `buf`, and the size needed is returned otherwise. */
CF_API size_t cf_emit(const struct cf_config* c, const char* format, cf_emit_what what,
                      int rank, char* buf, size_t cap, cf_error* err);

/* cf_emit, then cf_file_replace: `path` holds the whole document or keeps its
   old bytes. `format` NULL: cf_format_of_path(path), and a name it cannot tell
   is CF_E_CAPABILITY. `o` NULL: the defaults of cf_file_replace (CF_SYNC_COMMIT,
   one sync a save). A refusal of cf_emit keeps its own message and writes
   nothing; a failed write is cf_file_replace's `cannot write '%s': <reason>`.
   Returns CF_TRUE when the file holds the document.
   Guarantee: strong. A failure leaves `path` as it was and `c` untouched; it
   returns CF_FALSE with `err` set. */
CF_NODISCARD CF_API cf_bool cf_emit_file(const struct cf_config* c, const char* path,
                                         const char* format, cf_emit_what what,
                                         int rank, const cf_file_options* o,
                                         cf_error* err);

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_CF_EMIT_H */
