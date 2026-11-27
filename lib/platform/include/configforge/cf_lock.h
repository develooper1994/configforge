/*
 * cf_lock.h - a lock that plugs in (D-227, D-220).
 *
 * A write may exclude another writer, and what does it is the program's
 * choice: `cf_lock_none()` takes nothing (the default), `cf_lock_sidecar()`
 * takes a kernel lock on a file kept beside the one written, and a program
 * may fill a `cf_locker` of its own (a service it asks, a database lock, an
 * embedded mutex), as it fills a `cf_packer`. The locker knows nothing of
 * configuration; it is given a path and a wait.
 */
#ifndef CONFIGFORGE_CF_LOCK_H
#define CONFIGFORGE_CF_LOCK_H

#include "configforge/cf_error.h"
#include "configforge/cf_port.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A held lock. `handle` is -1 when nothing is held; the locker's `take`
   fills it and its `give` clears it. `data` is the locker's own (the
   sidecar does not use it). */
typedef struct cf_lock {
    long  handle;
    void* data;
} cf_lock;

typedef struct cf_locker {
    const char*   name;              /* "none", "sidecar", or the program's */
    unsigned long suggested_wait_ms; /* the wait this locker asks for when neither the
                                        source nor the configuration gives one (as git's
                                        locks do); 0: none */
    /* 1 with the lock held on `path`, `held->handle` set; 0 with `err` set:
       CF_E_BUSY after `wait_ms` (place: `path`, reason: who holds it, D-223),
       another code for any other failure. */
    cf_bool (*take)(const struct cf_locker* self, cf_lock* held, const char* path,
                    unsigned long wait_ms, cf_error* err);
    /* Releases; does nothing for a lock not held. */
    void (*give)(const struct cf_locker* self, cf_lock* held);
    void* user; /* the program's own; given back through `self` */
} cf_locker;

/* The built-in lockers, as functions (no data is exported from the
   library): the default, which takes nothing and always succeeds, and the
   kernel lock on the file `<dir>/.<base>.cf-lock` beside `path`, which is
   kept and never removed. A `cf_lock_sidecar()` on the `none` platform
   returns a locker whose `take` fails CF_E_CAPABILITY, `this build has no
   files to lock`. */
CF_API const cf_locker* cf_lock_none(void);
CF_API const cf_locker* cf_lock_sidecar(void);

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_CF_LOCK_H */
