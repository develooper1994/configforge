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
CF_API const char* cf_platform_name(void);

/* ------------------------------------------------------------------ *
 * Capabilities
 *
 * Queried rather than assumed, so a caller can say "this build cannot
 * list a directory" instead of handing back an empty one - which looks
 * exactly like an empty configuration and is the more dangerous of the
 * two answers.
 * ------------------------------------------------------------------ */

CF_API cf_bool cf_platform_has_environ(void);
CF_API cf_bool cf_platform_has_dir_list(void);
CF_API cf_bool cf_platform_has_wide_args(void);

/* ------------------------------------------------------------------ *
 * The environment
 * ------------------------------------------------------------------ */

/* NULL where none is reachable, which is not an error: a program with
   no environment is a normal thing on a small target. */
CF_API char* const* cf_platform_environ(void);

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

CF_API char* const* cf_platform_args(int argc, char* const* argv, cf_arena* arena,
                                     int* out_argc);

/* ------------------------------------------------------------------ *
 * Directory listing
 * ------------------------------------------------------------------ */

#define CF_PLATFORM_NAME_MAX 512

/* Names only, no path, no order, dotfiles already skipped. Returns the
   count, or -1 with `err` set. The caller sorts, because the order
   decides which file wins and that is the caller's rule, not the
   platform's. */
CF_API int cf_platform_dir_list(const char* path, char (*names)[CF_PLATFORM_NAME_MAX],
                                int max, cf_error* err);

/* ------------------------------------------------------------------ *
 * File stamps
 *
 * Modification time and size, which is what a change check compares.
 * Cheap, portable and pollable - and it misses a file rewritten inside
 * one clock tick with the same length, which is said out loud here
 * rather than discovered later. The exact answer is to rebuild and
 * compare digests.
 *
 * A directory is stamped by what it holds: the latest modification time
 * among it and its entries, and their total size plus their count, with
 * dotfiles skipped as the listing skips them. A directory's own time
 * moves when an entry is added or removed and not when one is edited in
 * place, so stamping only the directory missed the commonest change to
 * a drop-in directory there is.
 * ------------------------------------------------------------------ */

typedef struct cf_stamp {
    cf_i64  mtime;
    cf_i64  size;
    cf_bool present;
} cf_stamp;

/* False when the path cannot be stamped, which is not an error: a file
   that is not there yet has a stamp saying so, and its arrival is a
   change like any other. */
CF_API cf_bool cf_platform_stamp(const char* path, cf_stamp* out);

/* ------------------------------------------------------------------ *
 * Opening a file by a UTF-8 path
 *
 * fopen, with the path in UTF-8 everywhere (D-15). On Windows fopen
 * takes the active code page, which cannot spell most of the world's
 * names - a home directory under a Turkish or Greek user name cannot
 * be opened through it on a Western code page - so the Windows
 * implementation converts to UTF-16 and calls _wfopen. Identity
 * elsewhere.
 *
 * Returns a FILE* as void*, so that this header does not include
 * <stdio.h> for everyone who includes the library: gate.interface
 * holds the public headers to the system headers they need. NULL when
 * the file cannot be opened, or on a build with no stdio.
 * ------------------------------------------------------------------ */

CF_API void* cf_platform_fopen(const char* path, const char* mode);

/* ------------------------------------------------------------------ *
 * Who can read a file
 *
 * 1 when every user of the machine can read it, 0 when not, -1 when
 * this platform cannot say. Windows says -1: whether "everyone" may read
 * a file is a walk over an access control list with inheritance and
 * group membership in it, and an answer that guessed would be wrong in
 * both directions - which, for the one question this exists to answer,
 * "is this secret exposed", is worse than no answer.
 * ------------------------------------------------------------------ */

CF_API int cf_platform_world_readable(const char* path);

/* ------------------------------------------------------------------ *
 * A handle to wait on (D-35, option 3)
 *
 * Something an event loop can wait on beside its own descriptors: an
 * inotify descriptor on Linux, a kqueue descriptor on macOS and the
 * BSDs, an event HANDLE on Windows. It is not a callback and it owns no
 * thread - the caller waits, on a thread of its choosing, when it
 * chooses.
 *
 * What is watched is the directory each path lives in, or the path
 * itself when that is a directory. Editors save by writing a new file
 * and renaming it over the old one, and a watch on the old file follows
 * it into the bin. So a wake-up means "ask now", never "it changed": a
 * neighbouring file wakes it too, and cf_changed stays the authority.
 *
 * A directory that does not exist yet is watched through its nearest
 * ancestor that does, and drain() moves the watch down as the missing
 * levels appear - so an optional file in a directory nobody has created
 * still wakes the caller when it arrives.
 * ------------------------------------------------------------------ */

typedef struct cf_platform_wake cf_platform_wake;

CF_API cf_bool cf_platform_has_wake(void);

/* NULL with `err` set, naming the path and the reason. The paths are
   copied; nothing passed here need outlive the call. */
CF_API cf_platform_wake* cf_platform_wake_open(const char* const* paths, size_t n,
                                               const cf_allocator* a, cf_error* err);

/* A descriptor that becomes readable, or -1 where the handle is not a
   descriptor. */
CF_API int cf_platform_wake_fd(const cf_platform_wake* w);

/* A HANDLE that becomes signalled, or NULL where the handle is not one. */
CF_API void* cf_platform_wake_handle(const cf_platform_wake* w);

/* Consume what woke the handle and re-arm anything that needs it. False
   when some path can no longer wake it; polling cf_changed still works. */
CF_API cf_bool cf_platform_wake_drain(cf_platform_wake* w);

/* Wait up to `timeout_ms` (negative: without limit). 1 when woken, and
   then already drained; 0 on timeout; -1 when waiting failed. */
CF_API int cf_platform_wake_wait(cf_platform_wake* w, long timeout_ms);

CF_API void cf_platform_wake_close(cf_platform_wake* w);

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_CF_PLATFORM_H */
