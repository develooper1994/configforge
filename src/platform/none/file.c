/*
 * file.c (none) - opening a file by a UTF-8 path, on a target with no
 * platform.
 *
 * Apart from none.c so that the serialization library, which opens
 * files and calls nothing else of the platform, carries this file
 * without it (docs/Architecture.md, "Layers").
 *
 * Guarded once at the top, for the reason given in posix.c.
 */

#include "configforge/cf_platform_file.h"

#if CF_PLATFORM_NONE

#ifndef CF_NO_STDIO
#include <stdio.h>
#endif

void* cf_platform_fopen(const char* path, const char* mode) {
#ifndef CF_NO_STDIO
    /* No operating system, but there may still be a C library with
       stdio; a target without one builds with CF_NO_STDIO. */
    return (path != NULL && mode != NULL) ? fopen(path, mode) : NULL;
#else
    (void)path;
    (void)mode;
    return NULL;
#endif
}

#endif /* CF_PLATFORM_NONE */
