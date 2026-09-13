/*
 * cf_platform.h - what the platform provides, behind one door.
 *
 * Three things a configuration library needs that C89 has no answer
 * for: the process environment, the command line as UTF-8, and a
 * directory listing. Each has a POSIX shape, a Windows shape, and a
 * target with neither.
 *
 * The rule is the same one the portability layer follows for compilers:
 * **the build picks the file, the call site never asks**. A platform
 * implementation lives in src/platform/<name>/ and is the only place
 * allowed to name a platform macro, which gate.detection enforces.
 *
 * Each implementation guards its whole body with its own
 * CF_PLATFORM_* macro. That guard is not a scattered #ifdef: it exists
 * so the amalgamation, which is one translation unit and cannot have
 * the build choose for it, can carry every platform and let the
 * preprocessor select.
 */

#ifndef CONFIGFORGE_CF_PLATFORM_H
#define CONFIGFORGE_CF_PLATFORM_H

#include "configforge/cf_alloc.h"
#include "configforge/cf_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ *
 * Which platform
 *
 * Decided here and nowhere else. CF_PLATFORM_NONE can be set by hand
 * for a target that has none of this, and then every capability below
 * reports that it is absent rather than pretending.
 * ------------------------------------------------------------------ */

#if defined(CF_PLATFORM_NONE)
/* chosen by the caller */
#elif defined(_WIN32) || defined(_WIN64)
#define CF_PLATFORM_WIN32 1
#else
#define CF_PLATFORM_POSIX 1
#endif

#ifndef CF_PLATFORM_WIN32
#define CF_PLATFORM_WIN32 0
#endif
#ifndef CF_PLATFORM_POSIX
#define CF_PLATFORM_POSIX 0
#endif
#ifndef CF_PLATFORM_NONE
#define CF_PLATFORM_NONE 0
#endif

/* The name this build was compiled for, for explain() and the report. */
const char* cf_platform_name(void);

/* ------------------------------------------------------------------ *
 * Capabilities
 *
 * Queried rather than assumed, so a caller can say "this build cannot
 * list a directory" instead of handing back an empty one - which looks
 * exactly like an empty configuration and is the more dangerous of the
 * two answers.
 * ------------------------------------------------------------------ */

cf_bool cf_platform_has_environ(void);
cf_bool cf_platform_has_dir_list(void);
cf_bool cf_platform_has_wide_args(void);

/* ------------------------------------------------------------------ *
 * The environment
 * ------------------------------------------------------------------ */

/* NULL where none is reachable, which is not an error: a program with
   no environment is a normal thing on a small target. */
char* const* cf_platform_environ(void);

/* ------------------------------------------------------------------ *
 * The command line in UTF-8
 *
 * On Windows `main`'s argv arrives in the active ANSI code page, so a
 * value containing any non-ASCII character is wrong before any parser
 * sees it - and what the code page dropped cannot be converted back,
 * which is why the Windows implementation reads the wide command line
 * rather than converting argv. Identity everywhere else, so a caller
 * writes the same two lines on every platform.
 * ------------------------------------------------------------------ */

char* const* cf_platform_args(int argc, char* const* argv, cf_arena* arena,
                              int* out_argc);

/* ------------------------------------------------------------------ *
 * Directory listing
 * ------------------------------------------------------------------ */

#define CF_PLATFORM_NAME_MAX 512

/* Names only, no path, no order, dotfiles already skipped. Returns the
   count, or -1 with `err` set. The caller sorts, because the order
   decides which file wins and that is the caller's rule, not the
   platform's. */
/* ------------------------------------------------------------------ *
 * File stamps
 *
 * Modification time and size, which is what a change check compares.
 * Cheap, portable and pollable - and it misses a file rewritten inside
 * one clock tick with the same length, which is said out loud here
 * rather than discovered later. The exact answer is to rebuild and
 * compare digests.
 * ------------------------------------------------------------------ */

typedef struct cf_stamp {
    cf_i64  mtime;
    cf_i64  size;
    cf_bool present;
} cf_stamp;

/* False when the path cannot be stamped, which is not an error: a file
   that is not there yet has a stamp saying so, and its arrival is a
   change like any other. */
cf_bool cf_platform_stamp(const char* path, cf_stamp* out);

int cf_platform_dir_list(const char* path, char (*names)[CF_PLATFORM_NAME_MAX], int max,
                         cf_error* err);

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_CF_PLATFORM_H */
