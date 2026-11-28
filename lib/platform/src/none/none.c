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

cf_bool cf_platform_has_fd_io(void) {
    return CF_FALSE;
}

cf_bool cf_platform_fd_is_socket(int fd) {
    (void)fd;
    return CF_FALSE;
}

int cf_platform_fd_nonblock(int fd, int on) {
    (void)fd;
    (void)on;
    return -1;
}

long cf_platform_fd_read(int fd, void* buf, size_t cap, long timeout_ms) {
    (void)fd;
    (void)buf;
    (void)cap;
    (void)timeout_ms;
    return -1;
}

cf_bool cf_platform_fd_write(int fd, const void* buf, size_t n, long timeout_ms) {
    (void)fd;
    (void)buf;
    (void)n;
    (void)timeout_ms;
    return CF_FALSE;
}

char* const* cf_platform_args(int argc, char* const* argv, cf_arena* arena,
                              int* out_argc) {
    (void)arena;
    *out_argc = argc;
    return argv;
}

int cf_platform_lock_take(cf_lock* held, const char* path, unsigned long wait_ms,
                          cf_error* err) {
    (void)wait_ms;
    if (held != NULL) {
        held->handle = -1;
    }
    (void)cf_error_set(err, CF_E_CAPABILITY, NULL, path,
                       "this build has no files to lock");
    return 0;
}

void cf_platform_lock_give(cf_lock* held) {
    if (held != NULL) {
        held->handle = -1;
    }
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

/* No operating system, so nobody to ask who may write (D-83). */
cf_bool cf_platform_has_trusted_dir(void) {
    return CF_FALSE;
}

int cf_platform_dir_list_trusted(const char* path, char (*names)[CF_PLATFORM_NAME_MAX],
                                 int max, cf_error* err) {
    return cf_platform_dir_list(path, names, max, err);
}

int cf_platform_world_readable(const char* path) {
    (void)path;
    return -1;
}

cf_bool cf_platform_has_private_create(void) {
    return CF_FALSE;
}

int cf_platform_create_private(const char* path, cf_error* err) {
    cf_error_set(err, CF_E_CAPABILITY, NULL, path,
                 "this build cannot create a file only its owner can read");
    return -1;
}

/* There is no registry here, and a source that read none would look
   like an empty one. */
cf_bool cf_platform_has_registry(void) {
    return CF_FALSE;
}

int cf_platform_registry_walk(const char* root, cf_arena* arena, cf_platform_reg_fn fn,
                              void* ctx, cf_error* err) {
    (void)arena;
    (void)fn;
    (void)ctx;
    cf_error_set(err, CF_E_CAPABILITY, NULL, root,
                 "'%s' is in the Windows Registry, which exists only on Windows",
                 (root != NULL) ? root : "");
    return -1;
}

/* No user defaults are read here, and a source that read none would
   look like an empty domain. */
cf_bool cf_platform_has_preferences(void) {
    return CF_FALSE;
}

int cf_platform_preferences_walk(const char* domain, cf_arena* arena,
                                 cf_platform_pref_fn fn, void* ctx, cf_error* err) {
#if CF_WITH_PREFERENCES
    static const char says[] =
        "'%s' is in macOS's user defaults, which exist only on macOS";
#else
    static const char says[] = "'%s' is in macOS's user defaults, and this build was "
                               "compiled without CF_WITH_PREFERENCES";
#endif
    (void)arena;
    (void)fn;
    (void)ctx;
    cf_error_set(err, CF_E_CAPABILITY, NULL, domain, says,
                 (domain != NULL) ? domain : "");
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
    cf_error_set(err, CF_E_CAPABILITY, NULL, NULL,
                 "this build has no change handle; poll cf_changed instead");
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
