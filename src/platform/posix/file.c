/*
 * file.c (posix) - opening a file by a UTF-8 path, on POSIX. C89.
 *
 * Apart from posix.c so that the serialization library, which opens
 * files and calls nothing else of the platform, carries this file
 * without it (docs/Architecture.md, "Layers").
 *
 * Guarded once at the top, for the reason given in posix.c.
 */

#include "configforge/cf_platform_file.h"

#if CF_PLATFORM_POSIX

#include <stdio.h>

void* cf_platform_fopen(const char* path, const char* mode) {
    /* Paths are bytes here, and UTF-8 bytes are a path like any other. */
    return (path != NULL && mode != NULL) ? fopen(path, mode) : NULL;
}

#endif /* CF_PLATFORM_POSIX */
