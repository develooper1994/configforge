/*
 * cf_platform_file.h - which platform this is, and opening a file by a
 * UTF-8 path: the one thing of the platform layer that the serialization
 * library uses.
 *
 * INTERNAL (D-51): exported for ConfigForge Doctor and the tests, and
 * outside the 1.x promise. Its function is declared only under
 * CF_INTERNAL_API (D-51, D-88). cf_platform.h includes this header, so
 * everything that saw the declaration there still does.
 *
 * It is a header of its own so that the serialization library can open a
 * file without carrying the environment, the directory listing, the
 * Registry or the change handle behind cf_platform.h. For the same
 * reason it holds the choice of platform: the three files that
 * implement the opener guard themselves by it (src/platform/<name>/file.c),
 * and may include nothing of the platform layer but this.
 */

#ifndef CONFIGFORGE_CF_PLATFORM_FILE_H
#define CONFIGFORGE_CF_PLATFORM_FILE_H

#include "configforge/cf_error.h"
#include "configforge/cf_file.h"
#include "configforge/cf_port.h"

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

#if CF_INTERNAL_API
CF_API void* cf_platform_fopen(const char* path, const char* mode);

/* Whether this build has files to open: CF_TRUE on posix and win32, and on
 * none unless CF_NO_STDIO is defined. A reader asks before it opens, so that
 * a build with no files answers CF_E_CAPABILITY and not a plain "cannot
 * open" (K-11b, D-220). */
CF_NODISCARD CF_API cf_bool cf_platform_has_files(void);

/* ------------------------------------------------------------------ *
 * The core of cf_file_replace (cf_file.h; D-224, D-127)
 *
 * 1 or 0, failing at step `fail_at` (1 to cf_platform_write_steps()) as if
 * that system call had failed (0: no failure), and calling `on_retry(user)`
 * before each sleep of the Windows rename retry when not NULL (for the
 * test, which closes the holder there). No global state (D-101).
 * cf_file_replace is this with fail_at 0 and no hook.
 * ------------------------------------------------------------------ */

CF_API int cf_platform_write_file_with(const char* path, const void* data, size_t size,
                                       const cf_file_options* o, unsigned  fail_at,
                                       void (*on_retry)(void* user), void* user,
                                       cf_error* err);

/* ------------------------------------------------------------------ *
 * The replace refused when the file changed (D-186)
 * ------------------------------------------------------------------ */

/* 1 when `path` holds exactly `size` bytes of `data`. -1 when the file no
   longer holds `expect` (`expect_size` bytes; NULL and 0: the file must not
   exist), with `err` set (CF_E_SOURCE, "'%s' changed while it was written")
   and the file as the other writer left it. 0 with `err` set for any other
   failure, as cf_file_replace. */
CF_API int cf_platform_write_file_if(const char* path, const void* data, size_t size,
                                     const void* expect, size_t expect_size,
                                     const cf_file_options* o, cf_error* err);

/* The same, calling `between(user)` once, after the temporary is complete
   and before the comparison: for the test, which changes the file there. No
   global state (D-101). NULL: no call. */
CF_API int cf_platform_write_file_if_hook(const char* path, const void* data,
                                          size_t size, const void* expect,
                                          size_t expect_size, const cf_file_options* o,
                                          void (*between)(void* user), void* user,
                                          cf_error* err);

/* The core of both, failing at step `fail_at` as cf_platform_write_file_with
   does (the test's loop over the steps). */
CF_API int cf_platform_write_file_if_with(const char* path, const void* data,
                                          size_t size, const void* expect,
                                          size_t expect_size, const cf_file_options* o,
                                          unsigned fail_at, void (*between)(void* user),
                                          void* user, cf_error* err);

/* How many steps can fail: POSIX 6, Windows 5, none 0. */
CF_API unsigned cf_platform_write_steps(void);

/* A monotonic millisecond counter from an arbitrary start, for
   CF_SYNC_EVERY's sync_ms: clock_gettime(CLOCK_MONOTONIC) where that is
   defined, GetTickCount64 on Windows; (unsigned long)-1 where there is no
   clock. */
CF_API unsigned long cf_platform_now_ms(void);

/* For the test: how many syncs this writer has issued, and the close that
   says so (cf_file_writer_close, which also reports it in *syncs when that
   is not NULL). A count of the writer's own, never a global. */
CF_API unsigned long        cf_platform_writer_syncs(const cf_file_writer* w);
CF_NODISCARD CF_API cf_bool cf_platform_writer_close_counted(cf_file_writer* w,
                                                             unsigned long*  syncs,
                                                             cf_error*       err);
#endif /* CF_INTERNAL_API */

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_CF_PLATFORM_FILE_H */
