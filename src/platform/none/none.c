/*
 * none.c - a target with none of this.
 *
 * Every capability reports that it is absent and every call fails with
 * a message that says which one. That is the whole point: an empty
 * directory listing looks exactly like an empty configuration, and of
 * the two answers the silent one is the dangerous one.
 */

#include "configforge/cf_platform.h"

#if CF_PLATFORM_NONE

const char* cf_platform_name(void) {
    return "none";
}

cf_bool cf_platform_has_environ(void) {
    return CF_FALSE;
}

cf_bool cf_platform_has_dir_list(void) {
    return CF_FALSE;
}

cf_bool cf_platform_has_wide_args(void) {
    return CF_FALSE;
}

char* const* cf_platform_environ(void) {
    return NULL;
}

char* const* cf_platform_args(int argc, char* const* argv, cf_arena* arena,
                              int* out_argc) {
    (void)arena;
    *out_argc = argc;
    return argv;
}

cf_bool cf_platform_stamp(const char* path, cf_stamp* out) {
    (void)path;
    if (out != NULL) {
        out->mtime   = 0;
        out->size    = 0;
        out->present = CF_FALSE;
    }
    return CF_FALSE;
}

int cf_platform_dir_list(const char* path, char (*names)[CF_PLATFORM_NAME_MAX], int max,
                         cf_error* err) {
    (void)names;
    (void)max;
    cf_error_set(err, CF_E_CAPABILITY, NULL, path,
                 "this build has no directory listing");
    return -1;
}

#endif /* CF_PLATFORM_NONE */
