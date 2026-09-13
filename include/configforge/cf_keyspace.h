/*
 * cf_keyspace.h - the flat key space and the provenance chain.
 *
 * This is the core data structure, and the ordering of the design
 * depends on it: the chain is not a diagnostic that was added later,
 * it is what get() reads. A resolver that selected a winner and threw
 * the rest away could not explain itself afterwards.
 */

#ifndef CONFIGFORGE_CF_KEYSPACE_H
#define CONFIGFORGE_CF_KEYSPACE_H

#include "configforge/cf_value.h"
#include "configforge/cf_alloc.h"
#include "configforge/cf_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ *
 * Keys
 *
 * Canonical form is dotted, lowercase ASCII. Comparison is byte-wise
 * and case-sensitive: case-insensitive keys are Viper's most-reported
 * defect, because they silently collapse two distinct parameters.
 * Derivation to MYAPP_SERVER_PORT and --server-port belongs to the
 * source that needs it, not here.
 * ------------------------------------------------------------------ */

cf_bool cf_key_valid(const char* key, size_t len);

/* Number of dot-separated segments, 0 when the key is invalid. */
size_t cf_key_segments(const char* key, size_t len);

/* Byte-wise, never strcoll: a reproducible snapshot hash depends on a
   reproducible order. */
int cf_key_cmp(const char* a, size_t alen, const char* b, size_t blen);

/* ------------------------------------------------------------------ *
 * Provenance
 * ------------------------------------------------------------------ */

typedef enum cf_outcome {
#define CF_OUTCOME_X(sym, name, wins) sym,
#define CF_OUTCOME_LAST_X(sym, name, wins) sym
#include "configforge/cf_outcome.def"
#undef CF_OUTCOME_X
#undef CF_OUTCOME_LAST_X
} cf_outcome;

const char* cf_outcome_name(cf_outcome o);

/* Whether a candidate with this outcome may still be selected. The
   resolver asks this instead of listing the losing outcomes, so a new
   one cannot be added and forgotten in the one place it matters. */
cf_bool cf_outcome_can_win(cf_outcome o);

/* A candidate from a textual source carries the bytes as written; its
   type is decided later, by a definition if one exists and by Tier 0
   inference otherwise. A candidate from a typed source already has a
   type, and a definition that disagrees with it is a mismatch rather
   than something to coerce. Keeping the two apart is what lets JSON
   reject a string where an integer was declared while the environment,
   which has only strings, does not have to. */
#define CF_CAND_TEXTUAL 1u

typedef struct cf_candidate {
    int                  source; /* precedence rank; lower wins      */
    unsigned             flags;
    cf_value             value;
    const char*          at; /* location, arena-owned, may be NULL */
    cf_outcome           outcome;
    const char*          reason; /* set when rejected                */
    struct cf_candidate* next;   /* next lower precedence            */
} cf_candidate;

/* Candidates are ordered by precedence rank, and within one rank the
   later occurrence comes first: a source that can repeat a key means
   the last assignment to win. */
typedef struct cf_entry {
    const char*   key;
    size_t        keylen;
    cf_candidate* chain;    /* highest precedence first         */
    cf_candidate* selected; /* NULL until resolved              */
} cf_entry;

/* ------------------------------------------------------------------ *
 * Key space
 * ------------------------------------------------------------------ */

/* Construction is two-phase, and the phases use different structures
   because they have different access patterns.

   Loading appends candidates and never searches, so appending is O(1).
   Resolving sorts once and groups, which is O(m log m) in the number
   of candidates. The obvious alternative, keeping the entries sorted
   on every insert, costs a memmove per key and is quadratic overall:
   at fifty thousand keys that is tens of gigabytes of copying, which
   is the difference between a pause and a hang. */
typedef struct cf_cand_rec cf_cand_rec;

typedef struct cf_keyspace {
    cf_arena* arena;

    cf_cand_rec* cands; /* build phase: flat, append-only     */
    size_t       ncand;
    size_t       ccand;

    cf_entry* entries; /* resolved phase: sorted, exact size */
    size_t    count;

    cf_bool resolved;
} cf_keyspace;

cf_bool cf_ks_init(cf_keyspace* ks, cf_arena* arena, size_t hint);

/* The arena everything in this space lives in. Sources that have to
   read a document into memory borrow it, so a configuration remains
   one arena and one release. */
cf_arena* cf_ks_arena(const cf_keyspace* ks);

/* Record a candidate. Keys and text are copied into the arena, so the
   caller's buffers need not outlive the call. Candidates may arrive in
   any order; the chain is kept ordered by precedence. */
cf_bool cf_ks_add(cf_keyspace* ks, const char* key, size_t keylen, int source,
                  const cf_value* value, const char* at);

/* Record a candidate that failed conversion or validation. It never
   wins, and it is never erased: explain() has to be able to say that
   the environment did supply a value and why it was refused. */
/* Record a candidate as written, leaving its type to be decided. */
cf_bool cf_ks_add_text(cf_keyspace* ks, const char* key, size_t keylen, int source,
                       const char* text, size_t textlen, const char* at);

cf_bool cf_ks_reject(cf_keyspace* ks, const char* key, size_t keylen, int source,
                     const cf_value* value, const char* at, const char* reason);

/* Sort the candidates, group them into entries, and mark the
   highest-precedence non-rejected candidate of each as selected.
   Lookup is available afterwards, not before. */
void cf_ks_resolve(cf_keyspace* ks);

/* Tier 0 typing. Every textual candidate that no definition claimed
   gets a type by inference, which is deliberately timid: see D-24.

   This is a separate pass rather than something the sources do,
   because a source cannot know whether a definition exists for the key
   it just read. Running it before the definitions were applied would
   mean inferring first and overriding afterwards, which is how a
   declared type ends up losing to a guess. */
void cf_ks_infer_untyped(cf_keyspace* ks);

/* Mutable access, for the passes that legitimately change outcomes:
   applying definitions marks candidates rejected. Casting away const
   at the call site would work and would also hide which passes write,
   so the write path is named instead. */
cf_entry* cf_ks_at_mut(cf_keyspace* ks, size_t i);

/* Recompute the selected candidate of every entry. Needed after
   anything that changes an outcome, which in practice means after the
   definitions have been applied and some winners have been refused. */
void cf_ks_reselect(cf_keyspace* ks);

/* Binary search over the resolved entries. NULL when absent, and NULL
   before cf_ks_resolve: a half-built space has no answer to give, and
   returning a provisional one would be worse than returning none. */
const cf_entry* cf_ks_find(const cf_keyspace* ks, const char* key, size_t keylen);

/* The selected value, or NULL when the key is absent or every
   candidate was rejected. */
const cf_value* cf_ks_get(const cf_keyspace* ks, const char* key, size_t keylen);

size_t          cf_ks_count(const cf_keyspace* ks);
const cf_entry* cf_ks_at(const cf_keyspace* ks, size_t i);

/* Verifies the ordering invariant the design depends on. */
cf_bool cf_ks_is_sorted(const cf_keyspace* ks);

/* Order-independent digest over the resolved space. Two runs that
   produce the same configuration produce the same digest, on every
   platform and in both portability modes. This is the determinism
   requirement made checkable. */
cf_u64 cf_ks_digest(const cf_keyspace* ks);

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_CF_KEYSPACE_H */
