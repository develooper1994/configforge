/*
 * posix.c - the POSIX platform. C89 plus the three POSIX calls that
 * C89 has no answer for.
 *
 * The whole body is guarded once at the top. That guard is not a
 * scattered #ifdef: the build normally compiles only this file, and the
 * guard exists so the amalgamation - one translation unit, no build to
 * choose for it - can carry every platform and let the preprocessor
 * select.
 */

#include "configforge/cf_platform.h"

#if CF_PLATFORM_POSIX

#include <dirent.h>
#include <sys/stat.h>
#include <string.h>

/* Not C89, which is the whole reason this file exists. Declared here
   rather than pulled in from unistd.h so the translation unit stays
   readable about exactly what it is reaching for. */
extern char** environ;

const char* cf_platform_name(void) {
    return "posix";
}

cf_bool cf_platform_has_environ(void) {
    return CF_TRUE;
}

cf_bool cf_platform_has_dir_list(void) {
    return CF_TRUE;
}

cf_bool cf_platform_has_wide_args(void) {
    /* Arguments are already UTF-8 here, so there is nothing to widen
       and nothing that could have been lost. */
    return CF_FALSE;
}

char* const* cf_platform_environ(void) {
    return (char* const*)environ;
}

char* const* cf_platform_args(int argc, char* const* argv, cf_arena* arena,
                              int* out_argc) {
    (void)arena;
    *out_argc = argc;
    return argv;
}

cf_bool cf_platform_stamp(const char* path, cf_stamp* out) {
    struct stat st;

    if (out == NULL) {
        return CF_FALSE;
    }
    out->mtime   = 0;
    out->size    = 0;
    out->present = CF_FALSE;
    if (path == NULL || stat(path, &st) != 0) {
        return CF_FALSE;
    }
    out->mtime   = (cf_i64)st.st_mtime;
    out->size    = (cf_i64)st.st_size;
    out->present = CF_TRUE;
    return CF_TRUE;
}

int cf_platform_dir_list(const char* path, char (*names)[CF_PLATFORM_NAME_MAX], int max,
                         cf_error* err) {
    DIR*           d;
    struct dirent* e;
    int            n = 0;

    if (path == NULL || names == NULL) {
        cf_error_set(err, CF_E_SOURCE, NULL, path, "no directory given");
        return -1;
    }
    d = opendir(path);
    if (d == NULL) {
        cf_error_set(err, CF_E_SOURCE, NULL, path, "cannot read '%s'", path);
        return -1;
    }
    while ((e = readdir(d)) != NULL) {
        size_t len;
        if (e->d_name[0] == '.') {
            /* Skips . and .. and, deliberately, dotfiles: an editor's
               .swp and a package manager's .dpkg-new both land in a
               drop-in directory and neither is configuration. */
            continue;
        }
        if (n >= max) {
            break;
        }
        len = strlen(e->d_name);
        if (len + 1 > CF_PLATFORM_NAME_MAX) {
            continue;
        }
        memcpy(names[n], e->d_name, len + 1);
        ++n;
    }
    closedir(d);
    return n;
}

#endif /* CF_PLATFORM_POSIX */
