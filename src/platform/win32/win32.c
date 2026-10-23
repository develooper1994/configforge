/*
 * win32.c - the Windows platform.
 *
 * Written without a Windows machine at first, and the first MSVC build
 * found the first bug: _environ declared by hand collided with the
 * CRT's own declaration. It is built and run under MSVC now, and
 * unit.test_lex.utf8 asserts the non-ASCII argument round trip that the
 * wide-argument path exists for.
 *
 * Guarded once at the top, for the reason given in posix.c.
 */

#include "configforge/cf_platform.h"

#if CF_PLATFORM_WIN32

#include <stdlib.h>
#include <string.h>
#include <windows.h>

/* _environ is MSVC's spelling, and it has to come from <stdlib.h>: the
   CRT defines it as a macro over __p__environ(), so declaring it by
   hand produces a second, conflicting declaration of that function. */

const char* cf_platform_name(void) {
    return "win32";
}

cf_bool cf_platform_has_environ(void) {
    return CF_TRUE;
}

cf_bool cf_platform_has_dir_list(void) {
    return CF_TRUE;
}

cf_bool cf_platform_has_wide_args(void) {
    return CF_TRUE;
}

char* const* cf_platform_environ(void) {
    return (char* const*)_environ;
}

char* const* cf_platform_args(int argc, char* const* argv, cf_arena* arena,
                              int* out_argc) {
    LPWSTR* wide;
    int     n = 0;
    char**  out;
    int     i;

    *out_argc = argc;
    if (arena == NULL) {
        return argv;
    }
    /* The wide command line rather than a conversion of argv: argv has
       already lost anything the active code page cannot represent, and
       what is lost cannot be converted back. */
    wide = CommandLineToArgvW(GetCommandLineW(), &n);
    if (wide == NULL || n <= 0) {
        return argv;
    }
    out = (char**)cf_arena_alloc(arena, (size_t)(n + 1) * sizeof(char*));
    if (out == NULL) {
        LocalFree(wide);
        return argv;
    }
    for (i = 0; i < n; ++i) {
        int   need = WideCharToMultiByte(CP_UTF8, 0, wide[i], -1, NULL, 0, NULL, NULL);
        char* buf;
        if (need <= 0) {
            LocalFree(wide);
            return argv;
        }
        buf = (char*)cf_arena_alloc(arena, (size_t)need);
        if (buf == NULL) {
            LocalFree(wide);
            return argv;
        }
        WideCharToMultiByte(CP_UTF8, 0, wide[i], -1, buf, need, NULL, NULL);
        out[i] = buf;
    }
    out[n] = NULL;
    LocalFree(wide);
    *out_argc = n;
    return (char* const*)out;
}

/* Two 32-bit halves, joined. A function rather than an expression at
   each site because one shift-and-or spread over two lines is laid out
   differently by every version of clang-format. */
static cf_i64 win32_join(DWORD hi, DWORD lo) {
    cf_i64 v = (cf_i64)hi;
    return (v << 32) | (cf_i64)lo;
}

cf_bool cf_platform_stamp(const char* path, cf_stamp* out) {
    WIN32_FILE_ATTRIBUTE_DATA fa;

    if (out == NULL) {
        return CF_FALSE;
    }
    out->mtime   = 0;
    out->size    = 0;
    out->present = CF_FALSE;
    if (path == NULL || !GetFileAttributesExA(path, GetFileExInfoStandard, &fa)) {
        return CF_FALSE;
    }
    out->mtime =
        win32_join(fa.ftLastWriteTime.dwHighDateTime, fa.ftLastWriteTime.dwLowDateTime);
    out->size    = win32_join(fa.nFileSizeHigh, fa.nFileSizeLow);
    out->present = CF_TRUE;
    return CF_TRUE;
}

int cf_platform_dir_list(const char* path, char (*names)[CF_PLATFORM_NAME_MAX], int max,
                         cf_error* err) {
    char             pattern[CF_PLATFORM_NAME_MAX];
    WIN32_FIND_DATAA fd;
    HANDLE           h;
    int              n = 0;

    if (path == NULL || names == NULL) {
        cf_error_set(err, CF_E_SOURCE, NULL, path, "no directory given");
        return -1;
    }
    if (cf_fmt(pattern, sizeof pattern, "%s\\*", path) >= sizeof pattern) {
        cf_error_set(err, CF_E_SOURCE, NULL, path, "path is too long");
        return -1;
    }
    h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        cf_error_set(err, CF_E_SOURCE, NULL, path, "cannot read '%s'", path);
        return -1;
    }
    do {
        size_t len;
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            continue;
        }
        if (fd.cFileName[0] == '.') {
            continue;
        }
        if (n >= max) {
            break;
        }
        len = strlen(fd.cFileName);
        if (len + 1 > CF_PLATFORM_NAME_MAX) {
            continue;
        }
        memcpy(names[n], fd.cFileName, len + 1);
        ++n;
    } while (FindNextFileA(h, &fd) != 0);
    FindClose(h);
    return n;
}

#endif /* CF_PLATFORM_WIN32 */
