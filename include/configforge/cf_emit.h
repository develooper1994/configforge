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
   an array is an error before anything is written, not a surprise
   halfway through a file. */
size_t cf_emit(const struct cf_config* c, const char* format, cf_emit_what what,
               int rank, char* buf, size_t cap, cf_error* err);

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_CF_EMIT_H */
