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
#endif /* CF_INTERNAL_API */

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_CF_PLATFORM_FILE_H */
