/*
 * cf_source.h - sources.
 *
 * A source produces candidates. It does not resolve anything and it
 * does not know what other sources exist; precedence is a rank it is
 * given, and the key space does the rest. That is what lets a source
 * be tested in isolation, and it is why the resolver has no per-source
 * special cases in it.
 */

#ifndef CONFIGFORGE_CF_SOURCE_H
#define CONFIGFORGE_CF_SOURCE_H

#include "configforge/cf_keyspace.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Conventional ranks. Lower wins. Callers may use any integers; these
   are the ones cf_app installs. */
enum {
    /* Above the command line, for a value set at run time: the user
       just did it, deliberately, now. */
    CF_RANK_OVERRIDE  = -10,
    CF_RANK_CLI       = 0,
    CF_RANK_ENV       = 10,
    CF_RANK_USER_FILE = 20,
    CF_RANK_SYS_FILE  = 30,
    CF_RANK_DEFAULT   = 40
};

typedef struct cf_source cf_source;

typedef struct cf_source_vtable {
    const char* kind;
    cf_bool (*load)(const cf_source* s, cf_keyspace* ks);
    void (*destroy)(cf_source* s);

    /* The file this source reads, and NULL for a source that reads
       none - written out at every construction site rather than left to
       C89's implicit zero, because -Wmissing-field-initializers is on
       and that is the point of having it: a new member is a decision at
       each site, not a default that nobody looked at. */
    const char* (*path)(const cf_source* s);
} cf_source_vtable;

typedef struct cf_pair {
    const char* key;
    const char* value;
} cf_pair;

/* State for the simple sources lives inside the source object. The
   alternative, a static pool inside each source file, would be global
   mutable state: not reentrant, not thread-safe, and a direct breach
   of the architectural principle that forbids it. Sources that need
   heap state later use the `ext` pointer instead. */
struct cf_source {
    const cf_source_vtable* vt;
    const char*             name; /* instance name, shown in explain() */
    int                     rank;
    void*                   ext; /* owned mutable state, or NULL    */
    const void*             ref; /* borrowed read-only reference    */
    union {
        struct {
            const cf_pair* pairs;
            size_t         count;
        } mem;
        struct {
            const char*  prefix;
            char* const* env;
        } env;
        struct {
            int          argc;
            char* const* argv;
        } cli;
    } u;
};

CF_API cf_bool cf_source_load(const cf_source* s, cf_keyspace* ks);
CF_API void    cf_source_destroy(cf_source* s);

/* ------------------------------------------------------------------ *
 * A view over the sources
 *
 * Defined here rather than with the diagnostics that consume it: it is
 * a view over sources, and having it in cf_explain.h meant the core
 * could not name a source without including the top layer. The apply
 * pass needs exactly that to resolve a per-parameter policy, which is
 * what made the misplacement visible.
 * ------------------------------------------------------------------ */

typedef struct cf_source_view {
    const cf_source* const* items;
    size_t                  count;
} cf_source_view;

/* ------------------------------------------------------------------ *
 * Memory source
 *
 * Explicit pairs. Also serves as the defaults source: a default is a
 * value from the lowest-precedence source, not a separate mechanism.
 * ------------------------------------------------------------------ */

CF_API cf_bool cf_source_memory(cf_source* s, const char* name, int rank,
                                const cf_pair* pairs, size_t count);

/* ------------------------------------------------------------------ *
 * Environment source
 *
 * The environment is passed in rather than taken from a global,
 * because C89 has no portable environ and because a source that can be
 * handed a fake environment can be tested properly.
 *
 * Key derivation, with no definitions present:
 *
 *     MYAPP_SERVER__PORT   ->  server.port
 *     MYAPP_TLS_MIN_VERSION ->  tls_min_version
 *
 * A double underscore is the separator; a single underscore is a
 * literal underscore in the key. The obvious alternative, mapping a
 * single underscore to a dot, cannot be reversed: MYAPP_A_B_C could be
 * a.b.c, a.b_c, a_b.c or a_b_c, and picking one silently is exactly
 * the kind of guess this library exists to avoid. .NET reached the
 * same conclusion and uses the same escape.
 *
 * Variables that do not carry the prefix, or that derive an invalid
 * key, are skipped rather than guessed at.
 * ------------------------------------------------------------------ */

CF_API cf_bool cf_source_env(cf_source* s, const char* name, int rank,
                             const char* prefix, char* const* environment);

/* The platform environment, or NULL where none is reachable. */
CF_API char* const* cf_environ(void); /* cf_platform_environ, kept for callers */

/* Exposed for testing: derive a canonical key from a variable name.
   Returns the length written, or 0 when the name does not belong to
   this prefix or does not derive a valid key. */
CF_API size_t cf_env_key(const char* prefix, const char* var, size_t varlen, char* out,
                         size_t cap);

/* ------------------------------------------------------------------ *
 * Command line source
 *
 * Accepted forms:
 *
 *     --server.port=8080       value
 *     --tls.enabled            bool true
 *     --                       stop parsing
 *
 * The separate form "--key value" is deliberately absent at Tier 0: it
 * cannot be parsed correctly without knowing which options take
 * values, and a parser that guesses eats positional arguments. It
 * becomes available at Tier 1, where definitions say.
 *
 * Keys are written with dots, not dashes. A command line can carry a
 * dot, so there is no reason to introduce an ambiguous escape the way
 * the environment needs one.
 *
 * Arguments that are not options are ignored: they are the program's
 * positional arguments, not configuration.
 * ------------------------------------------------------------------ */

CF_API cf_bool cf_source_cli(cf_source* s, const char* name, int rank, int argc,
                             char* const* argv);

/* ------------------------------------------------------------------ *
 * File source
 *
 * Reads bytes, picks a codec by extension unless a format is named,
 * and hands the events to the flattener. It parses nothing itself.
 *
 * A format that is not compiled in fails loudly and names the option
 * to turn on. Silently skipping the file would be the one thing a
 * library about provenance must never do.
 * ------------------------------------------------------------------ */

enum {
    /* A missing file is not an error. It is also not silence: it shows
       up in explain() as a source that supplied nothing. */
    CF_FILE_OPTIONAL = 1u << 0
};

CF_API size_t cf_source_file_state_size(void);

CF_API cf_bool cf_source_file(cf_source* s, const char* name, int rank,
                              const char* path, const char* format, unsigned flags,
                              void* state, size_t state_size);

/* The reason a file source failed, or NULL when it did not. Sources
   return a plain boolean because most cannot fail in interesting ways;
   a file can fail in a dozen, and the caller needs the detail. */
CF_API const cf_error* cf_source_file_error(const cf_source* s);

/* A .env file: the same key mapping as the environment, because a .env
   file is an environment written down. Unlike the environment it
   belongs to this program, so a name that derives no key is an error
   rather than another program's variable. */
CF_API size_t cf_source_dotenv_state_size(void);

CF_API cf_bool cf_source_dotenv(cf_source* s, const char* name, int rank,
                                const char* path, const char* prefix, unsigned flags,
                                void* state, size_t state_size);

CF_API const cf_error* cf_source_dotenv_error(const cf_source* s);

/* ------------------------------------------------------------------ *
 * A drop-in directory
 *
 * conf.d is how systemd, sudo, apt and a dozen others let a package add
 * configuration without editing a file it does not own. Files are read
 * in byte order, and every one shares the source's rank, so 10-base
 * losing to 90-site is precedence rather than a merge rule and
 * explain() names the file that won.
 *
 * Dotfiles are skipped: an editor's .swp and a package manager's
 * .dpkg-new both land in such a directory and neither is
 * configuration.
 * ------------------------------------------------------------------ */

/* An existing key space, replayed. Every candidate at the rank it
   originally had, not only the winners: a library whose premise is
   explaining itself must not lose provenance at the moment a value
   changes, which is exactly when somebody asks. */
CF_API cf_bool cf_source_snapshot(cf_source* s, const char* name,
                                  const cf_keyspace* ks);

/* The file a source reads, or NULL. */
CF_API const char* cf_source_path(const cf_source* s);

CF_API size_t cf_source_dir_state_size(void);

CF_API cf_bool cf_source_dir(cf_source* s, const char* name, int rank, const char* path,
                             const char* format, unsigned flags, void* state,
                             size_t state_size);

CF_API const cf_error* cf_source_dir_error(const cf_source* s);

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_CF_SOURCE_H */
