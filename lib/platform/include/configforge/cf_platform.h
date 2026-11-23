/*
 * cf_platform.h - what the platform provides, behind one door.
 *
 * INTERNAL (D-51): exported for ConfigForge Doctor and the tests, and
 * outside the 1.x promise - a minor release may change it. Its functions
 * are declared only under CF_INTERNAL_API (D-51, D-88), but
 * cf_platform_name and the cf_platform_has_ queries, which are promised:
 * a program may ask what the build can do. A program uses the
 * configforge.h functions instead.
 *
 * Three things a configuration library needs that C89 has no answer
 * for: the process environment, the command line as UTF-8, and a
 * directory listing. Each has a POSIX shape, a Windows shape, and a
 * target with neither.
 *
 * The rule is the same one the portability layer follows for compilers:
 * **the build picks the file, the call site never asks**. A platform
 * implementation lives in src/platform/<name>/ and is the only place
 * allowed to name a platform macro, which gate.semgrep enforces.
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
#include "configforge/cf_platform_file.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ *
 * Which platform
 *
 * Decided in cf_platform_file.h, included above, and nowhere else:
 * CF_PLATFORM_WIN32, CF_PLATFORM_POSIX and CF_PLATFORM_NONE. NONE can be
 * set by hand for a target that has none of this, and then every
 * capability below reports that it is absent rather than pretending.
 * ------------------------------------------------------------------ */

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
#if CF_INTERNAL_API
CF_API char* const* cf_platform_environ(void);
#endif /* CF_INTERNAL_API */

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

#if CF_INTERNAL_API
CF_API char* const* cf_platform_args(int argc, char* const* argv, cf_arena* arena,
                                     int* out_argc);
#endif /* CF_INTERNAL_API */

/* ------------------------------------------------------------------ *
 * Directory listing
 * ------------------------------------------------------------------ */

/* A path: CF_PATH_MAX, a build limit (cf_build_options.h). */
#define CF_PLATFORM_NAME_MAX CF_PATH_MAX

/* Names only, no path, no order, dotfiles already skipped. Returns the
   count, or -1 with `err` set. The caller sorts, because the order
   decides which file wins and that is the caller's rule, not the
   platform's.

   Only files are listed, and links are followed to see what they
   name, because a Kubernetes mount is links to files. A subdirectory
   is skipped. Anything else - a FIFO, a socket, a device, a link to
   nothing - is -2 with `err` naming it: opened as a file, a FIFO waits
   for a writer for ever. -2 is also more than `max` files, or a name
   too long for a slot. -1 is a directory that cannot be read at all,
   which an optional source may accept; -2 is one that was read and
   cannot be handed over whole, which nothing should, because the file
   left out could be the one that decides a value. */
#if CF_INTERNAL_API
CF_API int cf_platform_dir_list(const char* path, char (*names)[CF_PLATFORM_NAME_MAX],
                                int max, cf_error* err);
#endif /* CF_INTERNAL_API */

/* Whether this platform can say who may write to a directory and its
   files (POSIX: yes; Windows and a build with no operating system:
   no - an access control list is not a mode, and an answer that
   guessed would be wrong both ways). */
CF_API cf_bool cf_platform_has_trusted_dir(void);

/* cf_platform_dir_list, and in the same pass the check of D-83: the
   directory, every entry and every file an entry names must be ones
   nobody but their owner and root can change, and a sticky
   directory's entries must belong to root, to the directory's owner
   or to the program's user. -2 with
   `err` naming the first that is not, as for any entry that cannot be
   handed over (CF_OPTIONAL does not excuse it). Where
   cf_platform_has_trusted_dir is false it is cf_platform_dir_list. */
#if CF_INTERNAL_API
CF_API int cf_platform_dir_list_trusted(const char* path,
                                        char (*names)[CF_PLATFORM_NAME_MAX], int max,
                                        cf_error* err);
#endif /* CF_INTERNAL_API */

/* ------------------------------------------------------------------ *
 * The Windows Registry
 *
 * Every value under one key, in UTF-8, each with the path of the
 * subkey holding it. Windows only; everywhere else the walk fails and
 * says so, because a registry source that read nothing on Linux would
 * look exactly like an empty one.
 * ------------------------------------------------------------------ */

enum {
    CF_REG_TEXT  = 1, /* REG_SZ, REG_EXPAND_SZ (not expanded)        */
    CF_REG_INT   = 2, /* REG_DWORD, REG_QWORD                        */
    CF_REG_LIST  = 3, /* REG_MULTI_SZ: `text` holds n NUL-ended items */
    CF_REG_OTHER = 4  /* anything else, REG_BINARY among them        */
};

typedef struct cf_platform_reg_value {
    const char* path; /* below the root, '\\'-separated; "" for it */
    const char* name; /* "" for the key's unnamed value            */
    int         kind;
    const char* text; /* CF_REG_TEXT, CF_REG_LIST                  */
    size_t      textlen;
    size_t      items; /* CF_REG_LIST                               */
    cf_i64      i;     /* CF_REG_INT                                */
} cf_platform_reg_value;

/* Return false to stop the walk; the walk then returns -1, and the
   callback is expected to have said why. */
typedef cf_bool (*cf_platform_reg_fn)(void* ctx, const cf_platform_reg_value* v);

CF_API cf_bool cf_platform_has_registry(void);

/* Every value under `root` - "HKCU\\Software\\MyApp", HKCU, HKLM, HKCR,
   HKU or HKCC first - and every subkey's, to CF_MAX_DEPTH. Buffers come
   from `arena`. 0 when walked, 1 when `root` does not exist, -1 with
   `err` set. */
#if CF_INTERNAL_API
CF_API int cf_platform_registry_walk(const char* root, cf_arena* arena,
                                     cf_platform_reg_fn fn, void* ctx, cf_error* err);
#endif /* CF_INTERNAL_API */

/* ------------------------------------------------------------------ *
 * macOS's user defaults
 *
 * One domain's values as a document - dictionaries, arrays and values
 * in order - with each value's path for explain(). macOS only, under
 * CF_WITH_PREFERENCES; everywhere else the walk fails and says so,
 * because a source that read nothing would look exactly like an empty
 * domain.
 * ------------------------------------------------------------------ */

enum {
    CF_PREF_BEGIN_DICT  = 1, /* the domain itself first, name NULL  */
    CF_PREF_BEGIN_ARRAY = 2,
    CF_PREF_END         = 3, /* closes the last dict or array begun */
    CF_PREF_TEXT        = 4, /* a CFString, UTF-8                   */
    CF_PREF_INT         = 5, /* a CFNumber that is not a float type */
    CF_PREF_REAL        = 6, /* a CFNumber of a float type          */
    CF_PREF_BOOL        = 7, /* a CFBoolean                         */
    CF_PREF_OTHER       = 8  /* a CFDate or CFData: `text` says which */
};

typedef struct cf_platform_pref_event {
    int         kind;
    const char* name; /* the member's name, its case kept; NULL in an
                         array and for the domain                  */
    size_t      namelen;
    const char* path; /* names from the domain down joined by '/', an
                         array's member by its index: "Server/Port",
                         "Hosts/1"; "" for the domain              */
    const char* text; /* CF_PREF_TEXT; CF_PREF_OTHER: "a date", "data" */
    size_t      textlen;
    cf_i64      i; /* CF_PREF_INT                               */
    double      r; /* CF_PREF_REAL                              */
    cf_bool     b; /* CF_PREF_BOOL                              */
} cf_platform_pref_event;

/* Return false to stop the walk; the walk then returns -1, and the
   callback is expected to have said why. */
typedef cf_bool (*cf_platform_pref_fn)(void* ctx, const cf_platform_pref_event* e);

CF_API cf_bool cf_platform_has_preferences(void);

/* Every value of `domain` ("com.example.app") in the scope W3-F2b's
   answer names. Buffers come from `arena`. 0 when walked, 1 when the
   domain has no values, -1 with `err` set. */
#if CF_INTERNAL_API
CF_API int cf_platform_preferences_walk(const char* domain, cf_arena* arena,
                                        cf_platform_pref_fn fn, void* ctx,
                                        cf_error* err);
#endif /* CF_INTERNAL_API */

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
#if CF_INTERNAL_API
CF_API cf_bool cf_platform_stamp(const char* path, cf_stamp* out);
#endif /* CF_INTERNAL_API */

/* cf_platform_fopen, opening a file by a UTF-8 path, is declared in
   cf_platform_file.h, which this header includes. */

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

#if CF_INTERNAL_API
CF_API int cf_platform_world_readable(const char* path);
#endif /* CF_INTERNAL_API */

/* ------------------------------------------------------------------ *
 * Creating a file only its owner can read
 *
 * For a file that may hold a secret and does not exist yet, such as a
 * new store. cf_platform_has_private_create says whether this platform
 * can do it; a caller that gets false creates the file its own way
 * rather than failing, because the default mode is what it had before.
 *
 * cf_platform_create_private returns 1 when it created an empty file
 * that only its owner can read and write, 0 when the path already
 * exists (it is left exactly as it was, mode and contents; "exists" is
 * an answer, not an error), and -1 with `err` set, naming the path and
 * the reason, when it cannot be created - a directory that is not
 * there, no permission. It never replaces or truncates anything.
 *
 * POSIX creates it with mode 0600 in the same call that creates it
 * (O_CREAT | O_EXCL), not by changing the mode afterwards, so there is
 * no moment at which another user could open it; the umask can only
 * remove bits from 0600, never add them. Windows creates it
 * (CREATE_NEW) with a protected access control list that grants full
 * access to the file's owner alone and inherits nothing from the
 * directory. What that does not do: an administrator can still take
 * ownership, and backup software still reads it. A build with no
 * operating system has no answer and reports it.
 * ------------------------------------------------------------------ */

CF_API cf_bool cf_platform_has_private_create(void);
#if CF_INTERNAL_API
CF_API int cf_platform_create_private(const char* path, cf_error* err);
#endif /* CF_INTERNAL_API */

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

#if CF_INTERNAL_API
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
#endif /* CF_INTERNAL_API */

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_CF_PLATFORM_H */
