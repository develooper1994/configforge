/*
 * cf_live.h - a configuration that reloads, under a mode the program
 * chooses.
 *
 * cf_changed and cf_watcher say a file moved on; what to do then is a
 * rebuild, a check and a swap that readers on other threads must not
 * see half done. cf_live is that, once, so every program does not write
 * it again:
 *
 *     static cf_builder* build(void* user) {
 *         cf_builder* b = cf_app_builder("myapp", 0, NULL);
 *         cf_builder_int(b, "server.port", 8080, 1, 65535, CF_RESTART);
 *         cf_builder_text(b, "log.level", "info", 0);
 *         return b;
 *     }
 *
 *     cf_live* live = cf_live_open(CF_LIVE_AUTO, build, NULL, NULL, &err);
 *
 *     on any thread, for each request:
 *         cf_config* cfg = cf_live_get(live);
 *         ... read cfg ...
 *         cf_config_free(cfg);
 *
 *     on one thread, in the event loop or a timer:
 *         cf_live_poll(live, &err);
 *
 * Three modes:
 *
 *   CF_LIVE_MANUAL   nothing reloads until cf_live_reload is called -
 *                    an admin command, a SIGHUP.
 *   CF_LIVE_APPROVE  a change is noticed and built, then waits:
 *                    cf_live_pending shows it, cf_live_approve swaps it
 *                    in, cf_live_reject drops it. An operator, or a
 *                    Doctor preflight in `check`, decides.
 *   CF_LIVE_AUTO     a change is noticed, built, checked and swapped.
 *
 * Still no thread and no callback from a thread of the library's: the
 * caller's loop calls cf_live_poll, or cf_live_wait where it has none.
 * A build or a check that fails leaves the running snapshot in place,
 * and the error says why - a reload never makes a program worse.
 *
 * A key defined CF_RESTART cannot change while the program runs: the
 * port it listens on, the size of a pool it allocated. Its new value
 * does not reach readers; the running one is kept as the top source,
 * named "running", so explain() shows both - the file's new value
 * shadowed, the running one selected - and cf_live_restarts lists the
 * keys waiting for a restart.
 *
 * cf_live_get may be called from any thread, at any time. The others
 * belong to one thread at a time, the one that drives the reloads.
 */

#ifndef CONFIGFORGE_CF_LIVE_H
#define CONFIGFORGE_CF_LIVE_H

#include "configforge/cf_port.h"
#include "configforge/cf_error.h"
#include "configforge/cf_alloc.h"

#ifdef __cplusplus
extern "C" {
#endif

struct cf_config;
struct cf_builder;

typedef struct cf_live cf_live;

typedef enum cf_live_mode {
    CF_LIVE_MANUAL  = 0,
    CF_LIVE_APPROVE = 1,
    CF_LIVE_AUTO    = 2
} cf_live_mode;

/* What a poll, a reload or an approval did. */
typedef enum cf_live_result {
    CF_LIVE_FAILED    = -1, /* refused or failed; the running one stays */
    CF_LIVE_UNCHANGED = 0,  /* nothing to do                           */
    CF_LIVE_SWAPPED   = 1,  /* readers now get the new snapshot        */
    CF_LIVE_PENDING   = 2   /* built, and waiting for cf_live_approve  */
} cf_live_result;

/* A builder for the next snapshot: the same sources and contract each
   call. It is consumed whether or not it creates. */
typedef struct cf_builder* (*cf_live_build_fn)(void* user);

/* Asked about each snapshot before readers can get it, the first one
   included; CF_FALSE, with `err` saying why, refuses it. `running` is
   NULL for the first. A preflight (cf_doctor_check), a policy of the
   program's, a log line. */
typedef cf_bool (*cf_live_check_fn)(void* user, const struct cf_config* running,
                                    const struct cf_config* next, cf_error* err);

/* Builds and checks the first snapshot, and fails as that does. `check`
   may be NULL; `a` NULL is the default allocator. */
CF_API cf_live* cf_live_open(cf_live_mode mode, cf_live_build_fn build,
                             cf_live_check_fn check, void* user, const cf_allocator* a,
                             cf_error* err);
CF_API void     cf_live_close(cf_live* l);

/* The current snapshot, with a reference the caller releases with
   cf_config_free. Any thread. A reader keeps the snapshot it got for as
   long as it holds it, so one request sees one configuration. */
CF_API struct cf_config* cf_live_get(cf_live* l);

/* Under APPROVE and AUTO: if a file of the running snapshot (or of the
   one pending) has changed, rebuild. MANUAL: CF_LIVE_UNCHANGED. */
CF_API cf_live_result cf_live_poll(cf_live* l, cf_error* err);

/* Rebuild now, in any mode, as if a file had changed: SWAPPED, or
   PENDING under APPROVE. */
CF_API cf_live_result cf_live_reload(cf_live* l, cf_error* err);

/* cf_live_poll after waiting up to `timeout_ms` for a file to change,
   on a watcher cf_live keeps open on the running snapshot's files. For
   a program with no loop of its own, on a thread it gives to this.
   Where the platform has no watcher, or the snapshot read no file, it
   polls at once: C89 has no portable way to sleep. */
CF_API cf_live_result cf_live_wait(cf_live* l, long timeout_ms, cf_error* err);

/* The snapshot waiting for approval, or NULL; it stays the cf_live's. */
CF_API const struct cf_config* cf_live_pending(const cf_live* l);
CF_API cf_live_result          cf_live_approve(cf_live* l, cf_error* err);
CF_API void                    cf_live_reject(cf_live* l);

/* How many snapshots readers have been given: 1 after open, one more a
   swap. A reader can tell that what it holds is no longer current. */
CF_API cf_u64 cf_live_generation(cf_live* l);

/* The CF_RESTART keys whose new value waits for a restart, one a line,
   in `buf` as far as `cap` allows; the length it needs, 0 for none. */
CF_API size_t cf_live_restarts(const cf_live* l, char* buf, size_t cap);

/* What a health page says about the reloads: how many were tried and
   failed, what the last one did, whether a change waits for approval,
   and why the last failure failed - "running generation 7; the last
   edit was refused: CFD007 ..." - without the program keeping a copy of
   its own. Any thread. */
typedef struct cf_live_status {
    size_t         size;       /* sizeof(cf_live_status) at the call site */
    cf_u64         generation; /* as cf_live_generation                  */
    cf_u64         reloads;    /* rebuilds tried, from poll, reload, wait */
    cf_u64         failures;   /* of those, refused or failed            */
    cf_live_result last;       /* what the last rebuild or approval did  */
    cf_bool        pending;    /* a change waits for cf_live_approve     */
    cf_error       error;      /* the last failure's; code CF_OK if none */
} cf_live_status;

/* Copies the status into `out`, whose `size` the caller sets; false for
   a NULL or a size this library does not know. */
CF_API cf_bool cf_live_status_get(cf_live* l, cf_live_status* out);

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_CF_LIVE_H */
