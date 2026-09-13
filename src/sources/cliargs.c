/*
 * cliargs.c - the command line in UTF-8. C89 where it can be.
 *
 * The real portability problem with a command line is not syntax. On
 * Windows, `main`'s argv arrives in the active ANSI code page, so a
 * configuration value containing any non-ASCII character is already
 * wrong before any parser sees it. Every question about option spelling
 * is smaller than that one.
 *
 * Isolated here because it is the one place in the source layer that
 * has to know what a platform is.
 *
 * Honest limitation: the Windows path cannot be run in the environment
 * this was written in. The POSIX path is the tested one; the Windows
 * path is exercised by the CI matrix, and until a test asserts a
 * non-ASCII round trip there it is code, not a guarantee.
 */

#include "configforge/cf_entry.h"

#if defined(CF_NO_CLI_ARGS)

char* const* cf_cli_args(int argc, char* const* argv, cf_arena* arena, int* out_argc) {
    (void)arena;
    *out_argc = argc;
    return argv;
}

#elif defined(_WIN32)

#include <windows.h>

char* const* cf_cli_args(int argc, char* const* argv, cf_arena* arena, int* out_argc) {
    LPWSTR* wide;
    int     n = 0;
    char**  out;
    int     i;

    *out_argc = argc;
    if (arena == NULL) {
        return argv;
    }
    /* GetCommandLineW rather than a conversion of argv: argv has
       already lost anything the ANSI code page cannot represent, and
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

#else

char* const* cf_cli_args(int argc, char* const* argv, cf_arena* arena, int* out_argc) {
    /* Already UTF-8 everywhere else, so this is identity and costs
       nothing. A caller writes the same two lines on every platform and
       only one of them needed them. */
    (void)arena;
    *out_argc = argc;
    return argv;
}

#endif
