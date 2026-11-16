/*
 * file.c (win32) - opening a file by a UTF-8 path, on Windows.
 *
 * Apart from win32.c so that the serialization library, which opens
 * files and calls nothing else of the platform, carries this file
 * without it (docs/Architecture.md, "Layers").
 *
 * Guarded once at the top, for the reason given in posix.c.
 */

#include "configforge/cf_platform_file.h"

#if CF_PLATFORM_WIN32

#include <stdio.h>
#include <wchar.h>
#include <windows.h>

/* win32.c's win32_wide, copied: UTF-8 in, UTF-16 out, refusing a
   malformed sequence rather than substituting for it - a path with a
   replacement character in it names a different file. False when it
   does not convert or does not fit. It is a copy because the
   serialization library carries this file without win32.c, and sharing
   the helper would make it an exported name of win32.c's. */
static cf_bool win32f_wide(const char* utf8, wchar_t* out, int cap) {
    if (utf8 == NULL || out == NULL) {
        return CF_FALSE;
    }
    return (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, out, cap) > 0)
               ? CF_TRUE
               : CF_FALSE;
}

void* cf_platform_fopen(const char* path, const char* mode) {
    wchar_t wpath[CF_PATH_MAX];
    wchar_t wmode[8];

    if (!win32f_wide(path, wpath, CF_PATH_MAX) || !win32f_wide(mode, wmode, 8)) {
        return NULL;
    }
    return _wfopen(wpath, wmode);
}

#endif /* CF_PLATFORM_WIN32 */
