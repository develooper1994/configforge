/*
 * cf_keyspace.h - the flat key space and the provenance chain.
 *
 * The 1.x promise (D-51) covers what a source written by a program uses:
 * cf_key_valid, cf_ks_arena and the cf_ks_add* functions, called from a
 * source's load. The cf_keyspace struct's members, and every other
 * cf_ks_ function, are INTERNAL - exported for ConfigForge Doctor and the
 * tests, and free to change in a minor release.
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

CF_API cf_bool cf_key_valid(const char* key, size_t len);

/* Number of dot-separated segments, 0 when the key is invalid. */
CF_API size_t cf_key_segments(const char* key, size_t len);

/* Byte-wise, never strcoll: a reproducible snapshot hash depends on a
   reproducible order. */
CF_API int cf_key_cmp(const char* a, size_t alen, const char* b, size_t blen);

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

CF_API const char* cf_outcome_name(cf_outcome o);

/* Whether a candidate with this outcome may still be selected. The
   resolver asks this instead of listing the losing outcomes, so a new
   one cannot be added and forgotten in the one place it matters. */
CF_API cf_bool cf_outcome_can_win(cf_outcome o);

/* A candidate from a textual source carries the bytes as written; its
   type is decided later, by a definition if one exists and by Tier 0
   inference otherwise. A candidate from a typed source already has a
   type, and a definition that disagrees with it is a mismatch rather
   than something to coerce. Keeping the two apart is what lets JSON
   reject a string where an integer was declared while the environment,
   which has only strings, does not have to. */
#define CF_CAND_TEXTUAL 1u

/* A candidate somebody wrote: text from a file, the environment, the
   command line. References, ${key} (D-10), are read in these and only
   these. Not in a default, which is the program's own code and changes
   with it; and not in a value replayed from a report or loaded from an
   image, which was resolved once already - an escaped $${ that became
   ${ there would otherwise be read the second time round. */
#define CF_CAND_AUTHORED 2u

/* A value with references in it, waiting for them: `written` holds the
   text as written, and the value becomes it with each ${key} replaced
   once precedence has chosen the keys it names. */
#define CF_CAND_TEMPLATE 4u

/* Internal to that pass: its references are being resolved, so meeting
   it again is a cycle. */
#define CF_CAND_EXPANDING 8u

/* The program's own default, from its definition. A per-key policy
   (D-33) says which sources may set a key, and nobody sets a default:
   it is what the key is when no permitted source says otherwise, so a
   policy passes over it rather than refusing it. */
#define CF_CAND_DEFAULT 16u

/* A copy of a value written under a key's old name (CF_DEPRECATED with
   replaced_by), supplying the new key: at the same rank, and below
   anything written under the new name in the same source, because the
   new spelling is the deliberate one. Its place says "(as <old>)". */
#define CF_CAND_RENAMED 32u

typedef struct cf_candidate {
    int                  source; /* precedence rank; lower wins      */
    unsigned             flags;
    cf_value             value;
    const char*          at; /* location, arena-owned, may be NULL */
    cf_outcome           outcome;
    const char*          reason;  /* set when rejected                */
    const char*          written; /* with references: as written      */
    struct cf_candidate* next;    /* next lower precedence            */
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

CF_API cf_bool cf_ks_init(cf_keyspace* ks, cf_arena* arena, size_t hint);

/* The arena everything in this space lives in. Sources that have to
   read a document into memory borrow it, so a configuration remains
   one arena and one release. */
CF_API cf_arena* cf_ks_arena(const cf_keyspace* ks);

/* Record a candidate. Keys and text are copied into the arena, so the
   caller's buffers need not outlive the call. Candidates may arrive in
   any order; the chain is kept ordered by precedence. */
CF_API cf_bool cf_ks_add(cf_keyspace* ks, const char* key, size_t keylen, int source,
                         const cf_value* value, const char* at);

/* Record a candidate as written, leaving its type to be decided. */
CF_API cf_bool cf_ks_add_text(cf_keyspace* ks, const char* key, size_t keylen,
                              int source, const char* text, size_t textlen,
                              const char* at);

/* Record a typed candidate that somebody wrote - a JSON string, say -
   so the reserved syntax applies to it as it does to raw text. */
CF_API cf_bool cf_ks_add_authored(cf_keyspace* ks, const char* key, size_t keylen,
                                  int source, const cf_value* value, const char* at);

/* Record a definition's default (CF_CAND_DEFAULT). */
CF_API cf_bool cf_ks_add_default(cf_keyspace* ks, const char* key, size_t keylen,
                                 int source, const cf_value* value);

/* Before resolution: every candidate of `from` offered to `to` as well,
   CF_CAND_RENAMED. The old key keeps its own, for code that still reads
   it. False when there is no room. */
CF_API cf_bool cf_ks_rename(cf_keyspace* ks, const char* from, size_t fromlen,
                            const char* to, size_t tolen);

/* Record a copy of another key space's candidate, at its rank, with what
   it was - textual, a default, the template it was made from - and not
   what it was waiting for: it is not read for references again. */
CF_API cf_bool cf_ks_add_candidate(cf_keyspace* ks, const char* key, size_t keylen,
                                   const cf_candidate* c);

/* Record a candidate that failed conversion or validation. It never
   wins, and it is never erased: explain() has to be able to say that
   the environment did supply a value and why it was refused. */
CF_API cf_bool cf_ks_reject(cf_keyspace* ks, const char* key, size_t keylen, int source,
                            const cf_value* value, const char* at, const char* reason);

/* Sort the candidates, group them into entries, and mark the
   highest-precedence non-rejected candidate of each as selected.
   Lookup is available afterwards, not before. */
/* Sorts and groups the candidates. Returns false when there was no room
   to do it, which is not the same as an empty configuration: the caller
   must distinguish, or a target out of memory gets a snapshot with no
   keys and nothing to say why. */
CF_API cf_bool cf_ks_resolve(cf_keyspace* ks);

/* Candidates appended so far, which is what an out-of-room message
   should name: the caller sized a buffer, not a key count. */
CF_API size_t cf_ks_candidate_count(const cf_keyspace* ks);

/* Tier 0 typing. Every textual candidate that no definition claimed
   gets a type by inference, which is deliberately timid: see D-24.

   This is a separate pass rather than something the sources do,
   because a source cannot know whether a definition exists for the key
   it just read. Running it before the definitions were applied would
   mean inferring first and overriding afterwards, which is how a
   declared type ends up losing to a guess. */
CF_API void cf_ks_infer_untyped(cf_keyspace* ks);

/* Mutable access, for the passes that legitimately change outcomes:
   applying definitions marks candidates rejected. Casting away const
   at the call site would work and would also hide which passes write,
   so the write path is named instead. */
CF_API cf_entry* cf_ks_at_mut(cf_keyspace* ks, size_t i);

/* Recompute the selected candidate of every entry. Needed after
   anything that changes an outcome, which in practice means after the
   definitions have been applied and some winners have been refused. */
CF_API void cf_ks_reselect(cf_keyspace* ks);

/* Binary search over the resolved entries. NULL when absent, and NULL
   before cf_ks_resolve: a half-built space has no answer to give, and
   returning a provisional one would be worse than returning none. */
CF_API const cf_entry* cf_ks_find(const cf_keyspace* ks, const char* key,
                                  size_t keylen);

/* Index of the first key that is not less than `key`, which is where a
   prefix scan starts. Returns cf_ks_count when every key sorts before
   it. The space is sorted, so this is a binary search and a prefix scan
   costs O(log n) plus the matches rather than a walk. */
CF_API size_t cf_ks_lower_bound(const cf_keyspace* ks, const char* key, size_t keylen);

/* The selected value, or NULL when the key is absent or every
   candidate was rejected. */
CF_API const cf_value* cf_ks_get(const cf_keyspace* ks, const char* key, size_t keylen);

CF_API size_t          cf_ks_count(const cf_keyspace* ks);
CF_API const cf_entry* cf_ks_at(const cf_keyspace* ks, size_t i);

/* Verifies the ordering invariant the design depends on. */
CF_API cf_bool cf_ks_is_sorted(const cf_keyspace* ks);

/* Order-independent digest over the resolved space. Two runs that
   produce the same configuration produce the same digest, on every
   platform and in both portability modes. This is the determinism
   requirement made checkable. */
CF_API cf_u64 cf_ks_digest(const cf_keyspace* ks);

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_CF_KEYSPACE_H */
