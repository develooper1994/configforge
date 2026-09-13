/*
 * cf_entry.h - flat entry producers, and how a source spells a key.
 *
 * Sources divide by what they have to do with the bytes.
 *
 *   flat        env, command line, .env files, a registry
 *               already key-and-value; the work is lexing and naming
 *   hierarchical  JSON, TOML, XML, INI
 *               a document; the work is parsing, and the flattener
 *               turns the events into keys
 *   direct      memory pairs, defaults, a replayed report
 *               no bytes to read at all
 *
 * The middle group already had a seam: a parser is an event producer
 * and never touches a key space. The first group did not, and each of
 * its sources reached into the key space itself. That made every lexer
 * need an arena and a key space to be tested at all, and it put the
 * decision "what does a candidate look like" in three files.
 *
 * A lexer now produces entries into a sink. Recording them into a key
 * space is one implementation of that sink; a test's implementation is
 * an array.
 */

#ifndef CONFIGFORGE_CF_ENTRY_H
#define CONFIGFORGE_CF_ENTRY_H

#include "configforge/cf_keyspace.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ *
 * Key mapping
 *
 * Each flat source spells a key its own way, and the spelling is a
 * separate concern from the lexing. Keeping them apart is what makes
 * the direction that matters possible: given a definition, a source
 * can be asked what *it* would call that key, which is how the
 * environment gains its natural single-underscore spelling once a
 * contract exists.
 * ------------------------------------------------------------------ */

typedef enum cf_keymap_kind {
    /* The source already writes canonical keys. A command line can
       carry a dot, so it has no escape to invent. */
    CF_KEYMAP_DIRECT = 0,
    /* PREFIX_A__B  ->  a.b, and a single underscore stays literal.
       Mapping a single underscore to a dot cannot be reversed:
       MYAPP_A_B_C could be a.b.c, a.b_c, a_b.c or a_b_c. */
    CF_KEYMAP_ENV
} cf_keymap_kind;

typedef struct cf_keymap {
    cf_keymap_kind kind;
    const char*    prefix; /* "MYAPP_", or NULL */
} cf_keymap;

void cf_keymap_init(cf_keymap* m, cf_keymap_kind kind, const char* prefix);

/* Source spelling to canonical. Returns the length written, or 0 when
   the name does not belong to this source or does not derive a valid
   key. Skipped rather than repaired: PATH and HOME land here. */
size_t cf_keymap_to_key(const cf_keymap* m, const char* raw, size_t n, char* out,
                        size_t cap);

/* Canonical to source spelling. `escaped` selects between the form the
   mapping can read back unaided and the natural one a person writes:

     server.tls_min_version
       escaped -> MYAPP_SERVER__TLS_MIN_VERSION   enumerable
       natural -> MYAPP_SERVER_TLS_MIN_VERSION    ambiguous alone,
                                                  unambiguous with a
                                                  definition to match
   Both are accepted when a contract exists, which is the whole gain:
   the escape is what enumeration needs, and nobody has to type it. */
size_t cf_keymap_from_key(const cf_keymap* m, const char* key, size_t n,
                          cf_bool escaped, char* out, size_t cap);

/* ------------------------------------------------------------------ *
 * Entry sink
 * ------------------------------------------------------------------ */

typedef struct cf_entry_sink {
    /* As written. The type is decided later, by a definition if one
       exists and by Tier 0 inference otherwise. */
    cf_bool (*text)(void* ctx, const char* key, size_t klen, const char* value,
                    size_t vlen, const char* at);
    /* Already typed, from a source that carries types. */
    cf_bool (*typed)(void* ctx, const char* key, size_t klen, const cf_value* v,
                     const char* at);
    void* ctx;
} cf_entry_sink;

/* The sink that records into a key space at a rank. */
typedef struct cf_keysink {
    cf_keyspace*  ks;
    int           rank;
    size_t        count;
    cf_bool       failed;
    cf_entry_sink sink;
} cf_keysink;

cf_bool              cf_keysink_init(cf_keysink* k, cf_keyspace* ks, int rank);
const cf_entry_sink* cf_keysink_of(cf_keysink* k);

/* ------------------------------------------------------------------ *
 * Lexers
 *
 * No arena, no key space, no source object. Everything these need is
 * an argument, which is what makes them testable against an array.
 * ------------------------------------------------------------------ */

/* Entries that do not belong to this prefix, or do not derive a valid
   key, are skipped. An environment is shared with every other program
   on the machine, so refusing it would be refusing the machine. */
cf_bool cf_lex_env(char* const* environment, const cf_keymap* map,
                   const cf_entry_sink* out, cf_error* err);

/* ------------------------------------------------------------------ *
 * Command line
 *
 *   --key=value        everywhere
 *   --key              true, when the key is a flag or undeclared
 *   --key value        when a declaration says the key takes one
 *   --no-key           false, when the key is a declared flag
 *   -k value           when a declaration names the alias
 *   --                 stop
 *
 * The forms below the first two need to know whether an option takes a
 * value, which is the one thing a command line cannot be parsed
 * correctly without. A parser that guesses eats positional arguments,
 * and getopt takes a spec for exactly this reason.
 *
 * So the lexer asks. Tier 0 passes no spec and gets the two forms that
 * need no knowledge; Tier 1 passes one built from the definitions and
 * gets the rest. Same shape as the environment's two spellings: a
 * contract buys capability, and its absence buys predictability rather
 * than guesswork.
 * ------------------------------------------------------------------ */

typedef enum cf_cli_kind {
    /* No declaration. Only the = form carries a value. */
    CF_CLI_UNKNOWN = 0,
    /* A boolean: the bare form is true and --no- makes it false. */
    CF_CLI_FLAG,
    /* Takes a value, so the separate form is unambiguous. */
    CF_CLI_VALUE
} cf_cli_kind;

/* The context is const: a spec answers questions and changes nothing.
   The entry sink's context is not, because recording is what it does. */
typedef struct cf_cli_spec {
    cf_cli_kind (*classify)(const void* ctx, const char* key, size_t klen);
    /* Canonical key for an alias such as "-p" or "--port", or 0 when
       the token is not an alias. */
    size_t (*alias)(const void* ctx, const char* raw, size_t rawlen, char* out,
                    size_t cap);
    const void* ctx;
} cf_cli_spec;

/* `spec` may be NULL, which is Tier 0.
 *
 * A malformed option is rejected rather than skipped. A command line
 * belongs to this program alone, so an unparseable option is the user's
 * typo, and ignoring it is how a misspelled flag becomes an hour of
 * confusion. */
cf_bool cf_lex_cli(int argc, char* const* argv, const cf_keymap* map,
                   const cf_cli_spec* spec, const cf_entry_sink* out, cf_error* err);

/* ------------------------------------------------------------------ *
 * Response files
 *
 *   myapp @args.txt --port=8080
 *
 * One argument per line, `#` comments, blank lines skipped. Accepted
 * because the Windows command-line length limit is real and this is the
 * universal way around it.
 *
 * Expansion is a separate pass rather than something the lexer does, so
 * the lexer keeps needing nothing but its arguments. Nesting is
 * refused: a response file that includes another is a command line
 * nobody can follow from the one they typed, and one that includes
 * itself is a loop.
 * ------------------------------------------------------------------ */

cf_bool cf_cli_expand(int argc, char* const* argv, cf_arena* arena, int* out_argc,
                      char*** out_argv, cf_error* err);

/* ------------------------------------------------------------------ *
 * The command line in UTF-8
 *
 * The real portability problem with a command line is not syntax. On
 * Windows `main`'s argv arrives in the active ANSI code page, so a
 * value containing any non-ASCII character is wrong before any parser
 * sees it - and what the code page dropped cannot be converted back,
 * which is why the Windows path reads GetCommandLineW rather than
 * converting argv.
 *
 * Identity everywhere else, so a caller writes the same two lines on
 * every platform and only one of them needed them.
 * ------------------------------------------------------------------ */

char* const* cf_cli_args(int argc, char* const* argv, cf_arena* arena, int* out_argc);

/* ------------------------------------------------------------------ *
 * A .env file
 *
 *   # comment
 *   KEY=value
 *   export KEY=value        the shell convention, accepted and ignored
 *   KEY="quoted value"
 *
 * The same shape as the environment, which is the point: a .env file is
 * an environment written down. It takes the same key mapping, so the
 * escape and the natural spelling behave identically in both.
 * ------------------------------------------------------------------ */

cf_bool cf_lex_dotenv(const char* text, size_t n, const char* origin,
                      const cf_keymap* map, const cf_entry_sink* out, cf_error* err);

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_CF_ENTRY_H */
