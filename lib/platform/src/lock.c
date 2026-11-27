/*
 * lock.c - the built-in lockers (D-227): `cf_lock_none`, which takes
 * nothing, and `cf_lock_sidecar`, the kernel lock on a kept file that the
 * platform files implement (cf_platform_lock_take, cf_platform_lock_give).
 *
 * C89. A program's own locker is a `cf_locker` it fills; nothing here
 * registers it.
 */

#include "configforge/cf_error.h"
#include "configforge/cf_lock.h"
#include "configforge/cf_platform.h"
#include "configforge/cf_port.h"

#include <stddef.h>

static cf_bool lock_none_take(const cf_locker* self, cf_lock* held, const char* path,
                              unsigned long wait_ms, cf_error* err) {
    (void)self;
    (void)path;
    (void)wait_ms;
    (void)err;
    if (held != NULL) {
        held->handle = -1;
    }
    return CF_TRUE;
}

static void lock_none_give(const cf_locker* self, cf_lock* held) {
    (void)self;
    if (held != NULL) {
        held->handle = -1;
    }
}

static cf_bool lock_sidecar_take(const cf_locker* self, cf_lock* held, const char* path,
                                 unsigned long wait_ms, cf_error* err) {
    (void)self;
    return cf_platform_lock_take(held, path, wait_ms, err) != 0 ? CF_TRUE : CF_FALSE;
}

static void lock_sidecar_give(const cf_locker* self, cf_lock* held) {
    (void)self;
    cf_platform_lock_give(held);
}

const cf_locker* cf_lock_none(void) {
    static const cf_locker none = {"none", 0ul, lock_none_take, lock_none_give, NULL};

    return &none;
}

const cf_locker* cf_lock_sidecar(void) {
    static const cf_locker sidecar = {"sidecar", 0ul, lock_sidecar_take,
                                      lock_sidecar_give, NULL};

    return &sidecar;
}
