/*
 * cf_define.h - definitions, the typed contract.
 *
 * A definition is what turns configuration from a bag of strings into
 * a contract. With one present, the definition decides the type and no
 * inference happens at all; without one, Tier 0 inference applies and
 * is deliberately timid.
 *
 * Definitions are also what make the useful diagnostics possible: typo
 * detection needs the set of legal keys, required-parameter checking
 * needs to know what is required, and redaction needs to know what is
 * sensitive. None of that can be recovered from the values alone.
 */

#ifndef CONFIGFORGE_CF_DEFINE_H
#define CONFIGFORGE_CF_DEFINE_H

#include "configforge/cf_keyspace.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    CF_REQUIRED   = 1u << 0, /* absence is an error                  */
    CF_SENSITIVE  = 1u << 1, /* redacted in diagnostics and reports  */
    CF_DEPRECATED = 1u << 2, /* accepted, reported                   */

    /* A command rather than a setting. --cf.explain asks a question; it
       configures nothing, so it must never be written into a file that
       is read back as configuration. */
    CF_TRANSIENT = 1u << 3,

    /* Cannot change while the program runs - a port it listens on, the
       size of a pool it allocated. cf_live keeps the running value and
       reports the new one as waiting for a restart (cf_live.h). */
    CF_RESTART = 1u << 4
};

typedef enum cf_on_invalid {
    /* Default. A malformed production value must not quietly degrade
       to a development default: that is precisely the implicit
       behaviour this library exists to prevent. */
    CF_INVALID_STRICT = 0,
    /* Opt-in. The value is refused and resolution continues down the
       chain; the refusal stays visible in explain(). */
    CF_INVALID_FALLBACK
} cf_on_invalid;

/* A descriptor rather than a chained builder. C89 has no designated
   initializers, so a positionally filled struct would be fragile and
   one filled only by assignment would be verbose; cf_def_init plus
   field assignment is safe and explicit, and the convenience
   functions below cover the ordinary cases.
   The size field is what lets fields be added later without changing
   a single signature. */
typedef struct cf_def {
    size_t        size;
    cf_type       type;
    const char*   key;
    unsigned      flags;
    cf_on_invalid on_invalid;
    const char*   description;
    const char*   replaced_by; /* when CF_DEPRECATED              */

    cf_bool  has_default;
    cf_value default_value;

    cf_bool has_range;
    cf_i64  imin, imax;
#if CF_WITH_REAL
    double rmin, rmax;
#endif
    size_t min_len, max_len; /* CF_TYPE_TEXT                    */

    /* NULL-terminated list of permitted spellings, for CF_TYPE_TEXT. */
    const char* const* allowed;

    /* NULL-terminated command-line aliases, written without the dashes:
       { "p", "port", NULL } accepts -p and --port. An alias is only
       meaningful with a declaration, which is what makes it
       unambiguous. */
    const char* const* aliases;

    /* Per-parameter policy: source names in decreasing precedence, and
       the only sources permitted to supply this key.

           { "env", "user-file", NULL }

       reads as the specification writes it - "database.password:
       Environment > Local" - and does both jobs at once. A source not
       named may not set the key; the named ones are tried in this order
       rather than the global one, which is how an operator-controlled
       setting survives a command line that tries to override it.

       NULL means the global precedence order, which is what almost
       every key wants. */
    const char* const* policy;
} cf_def;

CF_API void cf_def_init(cf_def* d, cf_type type, const char* key);

/* ------------------------------------------------------------------ *
 * Definition set
 * ------------------------------------------------------------------ */

/* The definition model knows that sources exist and nothing about what
   they are, which is the direction the layering requires. */
struct cf_source;

#define CF_MAX_OPEN_PREFIXES 8

typedef struct cf_defset {
    cf_arena* arena;
    cf_def*   defs;
    size_t    count;
    size_t    capacity;
    cf_bool   sealed;

    /* Prefixes exempt from unknown-key rejection.

       A plugin's settings cannot be in the program's contract, because
       the plugin is not in the program either. Without this, Tier 1 and
       a dynamic key space are mutually exclusive - which the first
       example that tried to iterate plugins discovered immediately. */
    const char* open_prefixes[CF_MAX_OPEN_PREFIXES];
    size_t      open_count;
} cf_defset;

CF_API cf_bool cf_defset_init(cf_defset* ds, cf_arena* arena, size_t hint);

/* Copies the descriptor. Rejects a definition that contradicts itself:
   a default outside its own range, an empty enum, a range on a type
   that has none. Those are programming errors and are worth refusing
   at the point they are written. */
CF_API cf_bool cf_define(cf_defset* ds, const cf_def* d, cf_error* err);

CF_API cf_bool cf_def_bool(cf_defset* ds, const char* key, cf_bool dflt, unsigned flags,
                           cf_error* err);
CF_API cf_bool cf_def_int(cf_defset* ds, const char* key, cf_i64 dflt, cf_i64 min,
                          cf_i64 max, unsigned flags, cf_error* err);
CF_API cf_bool cf_def_text(cf_defset* ds, const char* key, const char* dflt,
                           unsigned flags, cf_error* err);
#if CF_WITH_REAL
CF_API cf_bool cf_def_real(cf_defset* ds, const char* key, double dflt, double min,
                           double max, unsigned flags, cf_error* err);
#endif

/* Sorts. Required before lookup; calling twice is harmless. */
CF_API void cf_defset_seal(cf_defset* ds);

/* Exempt a prefix from unknown-key rejection. */
CF_API cf_bool cf_defset_open(cf_defset* ds, const char* prefix, cf_error* err);

/* Is this key under an exempt prefix? Matches at a segment boundary
   only, so opening "plugins" does not open "pluginsomething". */
CF_API cf_bool cf_defset_is_open(const cf_defset* ds, const char* key, size_t keylen);

CF_API const cf_def* cf_defset_find(const cf_defset* ds, const char* key,
                                    size_t keylen);
CF_API size_t        cf_defset_count(const cf_defset* ds);
CF_API const cf_def* cf_defset_at(const cf_defset* ds, size_t i);

/* Nearest defined key by edit distance, or NULL when nothing is close.
   This is what turns "unknown key server.pot" into "did you mean
   server.port", and it is available only because the definition set is
   a contract rather than documentation. */
CF_API const cf_def* cf_defset_nearest(const cf_defset* ds, const char* key,
                                       size_t keylen, size_t* distance);

/* ------------------------------------------------------------------ *
 * Applying the contract
 *
 * Run after cf_ks_resolve. Converts every textual candidate to the
 * type its definition names, validates it, and marks failures as
 * rejected with a reason rather than discarding them. Re-selects
 * afterwards, so a rejected winner hands over to the next candidate
 * down the chain only when that is what the definition asked for.
 * ------------------------------------------------------------------ */

typedef enum cf_unknown_policy {
    CF_UNKNOWN_IGNORE = 0, /* shared files legitimately carry others' keys */
    CF_UNKNOWN_WARN,
    CF_UNKNOWN_REJECT
} cf_unknown_policy;

typedef struct cf_apply_report {
    size_t converted;
    size_t rejected;
    size_t unknown;
    size_t deprecated;
    size_t defaults_used;
} cf_apply_report;

/* Turning a candidate's rank into the source name a policy names is the
   one thing the apply pass needs from outside the core. It asks rather
   than taking a cf_source_view, so the core keeps knowing that sources
   exist and nothing about what they are - the same shape as the command
   line's spec, and for the same reason: a test can answer the question
   with no sources existing.

   May be NULL when no key carries a policy. */
/* Renders a policy list as the specification writes it: "env > local".
   Shared because both the rejection message and the coherence check
   below want the same spelling, and two renderings of one thing drift. */
CF_API void cf_policy_text(const char* const* policy, char* buf, size_t cap);

typedef struct cf_rank_names {
    const char* (*name_of)(const void* ctx, int rank);
    const void* ctx;
} cf_rank_names;

CF_API cf_bool cf_defs_apply(cf_keyspace* ks, const cf_defset* ds,
                             const cf_rank_names* names, cf_unknown_policy unknown,
                             cf_apply_report* report, cf_error* err);

/* Definition defaults, as an ordinary source. Declared here because
   it is the one source that reads a definition set; it is implemented
   with the others.

   A default is a value from the lowest-precedence source rather than a
   separate mechanism, which is why explain() shows one losing to a
   file with no special case anywhere - and why it has a name to print
   rather than a question mark. */
struct cf_source;
CF_API cf_bool cf_source_defaults(struct cf_source* s, const char* name, int rank,
                                  const cf_defset* ds);

/* Give the environment source the key list, so it accepts the natural
   single-underscore spelling as well as the escaped one. Without a
   contract only the escaped spelling can be enumerated unambiguously;
   with one, both work and nobody has to type the escape. */
CF_API void cf_source_env_defs(struct cf_source* s, const cf_defset* ds);

/* Give the command-line source the key list, which is what unlocks
   --key value, --no-key and aliases. Without it only the = form carries
   a value, because nothing else can be parsed correctly. */
CF_API void cf_source_cli_defs(struct cf_source* s, const cf_defset* ds);

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_CF_DEFINE_H */
