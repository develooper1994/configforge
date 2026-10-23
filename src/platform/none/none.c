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

cf_bool cf_platform_has_wake(void) {
    return CF_FALSE;
}

cf_platform_wake* cf_platform_wake_open(const char* const* paths, size_t n,
                                        const cf_allocator* a, cf_error* err) {
    (void)paths;
    (void)n;
    (void)a;
    cf_error_set(err, CF_E_CAPABILITY, NULL, NULL, "this build has no change handle");
    return NULL;
}

int cf_platform_wake_fd(const cf_platform_wake* w) {
    (void)w;
    return -1;
}

void* cf_platform_wake_handle(const cf_platform_wake* w) {
    (void)w;
    return NULL;
}

cf_bool cf_platform_wake_drain(cf_platform_wake* w) {
    (void)w;
    return CF_FALSE;
}

int cf_platform_wake_wait(cf_platform_wake* w, long timeout_ms) {
    (void)w;
    (void)timeout_ms;
    return -1;
}

void cf_platform_wake_close(cf_platform_wake* w) {
    (void)w;
}

#endif /* CF_PLATFORM_NONE */
