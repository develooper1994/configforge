/*
 * live.c - a configuration that reloads: rebuild, check, keep what
 * cannot change live, and swap so that no reader sees it half done.
 * C89. cf_live.h has the contract.
 */

#include "configforge/configforge.h"
#include "configforge/cf_live.h"

#include <string.h>

struct cf_live {
    const cf_allocator* a;
    cf_live_mode        mode;
    cf_live_build_fn    build;
    cf_live_check_fn    check;
    void*               build_user;
    void*               check_user;
    cf_log_fn           log;
    void*               log_user;
    cf_log_level        log_min;

    /* Held only to read or replace `current` and the counters beside
       it; nothing that can fail or allocate happens under it. */
    cf_refcount ticket_next;
    cf_refcount ticket_serving;
    cf_config*  current;
    cf_u64      generation;
    char*       restarts; /* the CF_RESTART keys kept, a line each */
    size_t      restarts_len;

    /* For cf_live_status_get, under the lock; `scratch` is where the
       driving thread assembles a rebuild's error, off the stack. */
    cf_live_status status;
    cf_error       last_error;
    cf_error       scratch;

    /* The driving thread's alone. */
    cf_config*  pending;
    char*       pending_restarts;
    size_t      pending_restarts_len;
    cf_watcher* watcher;
};

/* A ticket lock, made of the atomics the port has: take the next
   ticket, wait until it is served, serve the next on the way out. A
   lock of one count - take it from 0 to 1, or give the step back and
   retry - livelocks: with five threads each can step up, see 2 or more,
   step back, and all of them do it again, forever (the tcc build ran
   for two hours that way). Tickets are served in order, so every waiter
   gets in. Equality survives the counters wrapping. The section it
   guards is a pointer copy and a reference count. */
static void live_lock(cf_live* l) {
    cf_i32 mine = cf_ref_inc(&l->ticket_next) - 1;
    while (cf_ref_get(&l->ticket_serving) != mine) {
    }
}

static void live_unlock(cf_live* l) {
    (void)cf_ref_inc(&l->ticket_serving);
}

static void live_release(cf_live* l, void* p) {
    if (p != NULL) {
        l->a->release(l->a->ctx, p);
    }
}

/* ------------------------------------------------------------------ *
 * Keeping what cannot change live
 * ------------------------------------------------------------------ */

/* Every CF_RESTART key whose value in `next` is not the one `running`
   has becomes a pair holding the running value, in `ar`. A key that had
   no value when it started has nothing to keep and takes the new one. */
static size_t live_pins(const cf_config* running, const cf_config* next, cf_arena* ar,
                        cf_pair* pairs, size_t cap) {
    const cf_defset* ds = cf_defs(next);
    size_t           i, n = 0;

    for (i = 0; i < ds->count; ++i) {
        const cf_def*   d = &ds->defs[i];
        size_t          klen;
        const cf_value* was;
        const cf_value* now;

        if ((d->flags & CF_RESTART) == 0) {
            continue;
        }
        klen = strlen(d->key);
        was  = cf_ks_get(cf_keys(running), d->key, klen);
        now  = cf_ks_get(cf_keys(next), d->key, klen);
        if (was == NULL || (now != NULL && cf_value_equal(was, now))) {
            continue;
        }
        if (pairs != NULL && n < cap) {
            pairs[n].key = d->key;
            if (was->type == CF_TYPE_TEXT) {
                /* Stored terminated, and `running` outlives the derive. */
                pairs[n].value = was->u.t.p;
            } else {
                char* t = (char*)cf_arena_alloc(ar, CF_VALUE_TEXT_MAX + 1);
                if (t == NULL) {
                    return (size_t)-1;
                }
                t[cf_value_to_text(was, t, CF_VALUE_TEXT_MAX + 1)] = '\0';
                pairs[n].value                                     = t;
            }
        }
        ++n;
    }
    return n;
}

/* The keys of `pairs`, a line each, from the allocator. */
static char* live_list(cf_live* l, const cf_pair* pairs, size_t n, size_t* len) {
    size_t i, need = 0;
    char*  out;
    char*  p;

    *len = 0;
    if (n == 0) {
        return NULL;
    }
    for (i = 0; i < n; ++i) {
        need += strlen(pairs[i].key) + 1;
    }
    out = (char*)l->a->alloc(l->a->ctx, need + 1);
    if (out == NULL) {
        return NULL;
    }
    p = out;
    for (i = 0; i < n; ++i) {
        size_t k = strlen(pairs[i].key);
        memcpy(p, pairs[i].key, k);
        p += k;
        *p++ = '\n';
    }
    *p   = '\0';
    *len = need;
    return out;
}

/* `next` with the running values of its CF_RESTART keys kept on top,
   as a source named "running"; `next` itself is consumed. */
static cf_config* live_keep(cf_live* l, const cf_config* running, cf_config* next,
                            char** list, size_t* list_len, cf_error* err) {
    cf_arena*  ar;
    cf_pair*   pairs;
    size_t     n;
    cf_config* kept;

    *list     = NULL;
    *list_len = 0;
    n         = live_pins(running, next, NULL, NULL, 0);
    if (n == 0) {
        return next;
    }
    ar    = cf_arena_new(l->a, 0);
    pairs = (ar != NULL) ? (cf_pair*)cf_arena_alloc(ar, n * sizeof *pairs) : NULL;
    if (pairs == NULL || live_pins(running, next, ar, pairs, n) != n) {
        cf_arena_free(ar);
        cf_config_free(next);
        cf_error_set(err, CF_E_MEMORY, NULL, NULL,
                     "no room to keep the keys that need a restart");
        return NULL;
    }
    kept = cf_derive(next, "running", pairs, n, err);
    if (kept != NULL) {
        *list = live_list(l, pairs, n, list_len);
        if (*list == NULL) {
            cf_config_free(kept);
            kept = NULL;
            cf_error_set(err, CF_E_MEMORY, NULL, NULL,
                         "no room to list the keys that need a restart");
        }
    }
    cf_arena_free(ar);
    cf_config_free(next);
    return kept;
}

/* ------------------------------------------------------------------ *
 * Building and swapping
 * ------------------------------------------------------------------ */

/* The next snapshot, built, kept and checked, or NULL with `err` set. */
static cf_config* live_build(cf_live* l, const cf_config* running, char** list,
                             size_t* list_len, cf_error* err) {
    cf_builder* b;
    cf_config*  next;

    *list     = NULL;
    *list_len = 0;
    b         = l->build(l->build_user);
    if (b != NULL && l->log != NULL) {
        cf_builder_log(b, l->log, l->log_user, l->log_min);
    }
    if (b == NULL) {
        cf_error_set(err, CF_E_SOURCE, NULL, NULL,
                     "the build function made no builder");
        return NULL;
    }
    next = cf_builder_create(b, err);
    if (next == NULL) {
        return NULL;
    }
    if (running != NULL) {
        next = live_keep(l, running, next, list, list_len, err);
        if (next == NULL) {
            return NULL;
        }
    }
    if (l->check != NULL) {
        cf_error mine = CF_ERROR_INIT;
        if (!l->check(l->check_user, running, next, &mine)) {
            if (mine.message[0] == '\0') {
                cf_error_set(&mine, CF_E_POLICY, NULL, NULL,
                             "the check refused the new configuration");
            }
            cf_error_copy(err, &mine);
            live_release(l, *list);
            *list     = NULL;
            *list_len = 0;
            cf_config_free(next);
            return NULL;
        }
    }
    return next;
}

/* A watcher on the running snapshot's files, or none: a platform
   without one, or a configuration read from no file, polls instead. */
static void live_watch(cf_live* l) {
    cf_watcher_close(l->watcher);
    l->watcher = cf_watcher_open(l->current, l->a, NULL);
}

/* Readers get `next` from now on; `quiet` says it is the same values
   (a neighbour woke the watcher, or a file was touched), so the stamps
   move on and the generation does not. */
static void live_swap(cf_live* l, cf_config* next, char* list, size_t list_len,
                      cf_bool quiet) {
    cf_config* old;
    char*      old_list;

    live_lock(l);
    old             = l->current;
    old_list        = l->restarts;
    l->current      = next;
    l->restarts     = list;
    l->restarts_len = list_len;
    l->generation += quiet ? 0u : 1u;
    live_unlock(l);
    cf_config_free(old); /* a reader holding it keeps it */
    live_release(l, old_list);
    live_watch(l);
}

static void live_drop_pending(cf_live* l) {
    cf_config_free(l->pending);
    live_release(l, l->pending_restarts);
    l->pending              = NULL;
    l->pending_restarts     = NULL;
    l->pending_restarts_len = 0;
}

static cf_live_result live_rebuild_once(cf_live* l, cf_error* err) {
    char*      list;
    size_t     list_len;
    cf_config* next = live_build(l, l->current, &list, &list_len, err);

    if (next == NULL) {
        return CF_LIVE_FAILED;
    }
    if (cf_digest(next) == cf_digest(l->current) && list_len == l->restarts_len &&
        (list_len == 0 || memcmp(list, l->restarts, list_len) == 0)) {
        live_drop_pending(l);
        live_swap(l, next, list, list_len, CF_TRUE);
        return CF_LIVE_UNCHANGED;
    }
    if (l->mode == CF_LIVE_APPROVE) {
        live_drop_pending(l);
        l->pending              = next;
        l->pending_restarts     = list;
        l->pending_restarts_len = list_len;
        return CF_LIVE_PENDING;
    }
    live_swap(l, next, list, list_len, CF_FALSE);
    return CF_LIVE_SWAPPED;
}

/* One event to the reloads' log, if there is one and it is loud enough. */
static void live_log(const cf_live* l, cf_log_level level, const char* event,
                     cf_code code, const char* key, const char* message) {
    cf_log_event e;

    if (l->log == NULL || level < l->log_min) {
        return;
    }
    e.size    = sizeof e;
    e.level   = level;
    e.event   = event;
    e.code    = code;
    e.key     = key;
    e.source  = NULL;
    e.message = message;
    l->log(l->log_user, &e);
}

/* Each key the swap kept at its running value: one line apiece, so a
   restart's reason is in the log as well as in cf_live_restarts. */
static void live_log_restarts(const cf_live* l) {
    char        msg[CF_MESSAGE_MAX];
    char        key[CF_KEY_MAX];
    const char* p = l->restarts;
    const char* nl;

    while (p != NULL && *p != '\0') {
        size_t n;
        nl = strchr(p, '\n');
        n  = (nl != NULL) ? (size_t)(nl - p) : strlen(p);
        if (n >= sizeof key) {
            n = sizeof key - 1;
        }
        memcpy(key, p, n);
        key[n] = '\0';
        cf_fmt(msg, sizeof msg,
               "'%s' changed and keeps its running value until a restart", key);
        live_log(l, CF_LOG_WARN, "restart-pending", CF_OK, key, msg);
        p = (nl != NULL) ? nl + 1 : NULL;
    }
}

/* What a rebuild or an approval did, for the status, and the error for
   the caller who asked for one. */
static cf_live_result live_record(cf_live* l, cf_live_result r, cf_bool rebuilt,
                                  cf_error* err) {
    live_lock(l);
    l->status.reloads += rebuilt ? 1u : 0u;
    l->status.last       = r;
    l->status.pending    = (l->pending != NULL) ? CF_TRUE : CF_FALSE;
    l->status.generation = l->generation;
    if (r == CF_LIVE_FAILED) {
        ++l->status.failures;
        cf_error_copy(&l->last_error, &l->scratch);
    }
    live_unlock(l);
    if (r == CF_LIVE_FAILED) {
        cf_error_copy(err, &l->scratch);
        live_log(l, CF_LOG_ERROR, "reload-failed", (cf_code)l->scratch.code,
                 (l->scratch.key[0] != '\0') ? l->scratch.key : NULL,
                 l->scratch.message);
    } else if (l->log != NULL) {
        char msg[64];
        if (r == CF_LIVE_SWAPPED) {
            cf_fmt(msg, sizeof msg, "generation %U serving", l->status.generation);
            live_log(l, CF_LOG_INFO, "reload", CF_OK, NULL, msg);
            live_log_restarts(l);
        } else if (r == CF_LIVE_PENDING) {
            live_log(l, CF_LOG_INFO, "reload-pending", CF_OK, NULL,
                     "a change waits for approval");
        } else {
            live_log(l, CF_LOG_DEBUG, "reload", CF_OK, NULL, "nothing changed");
        }
    }
    return r;
}

static cf_live_result live_rebuild(cf_live* l, cf_error* err) {
    cf_error_clear(&l->scratch);
    l->scratch.size = sizeof l->scratch;
    return live_record(l, live_rebuild_once(l, &l->scratch), CF_TRUE, err);
}

/* ------------------------------------------------------------------ *
 * The interface
 * ------------------------------------------------------------------ */

cf_live* cf_live_open(cf_live_mode mode, cf_live_build_fn build, cf_live_check_fn check,
                      void* user, const cf_allocator* a, cf_error* err) {
    cf_live_options o = CF_LIVE_OPTIONS_INIT;

    o.mode       = mode;
    o.build      = build;
    o.build_user = user;
    o.check      = check;
    o.check_user = user;
    o.allocator  = a;
    return cf_live_open_with(&o, err);
}

/* cf_live_options as 1.0 has it; a later member is read only where
   o->size reaches it. */
#define LIVE_OPTIONS_SIZE_1_0 sizeof(cf_live_options)

cf_live* cf_live_open_with(const cf_live_options* o, cf_error* err) {
    cf_live*            l;
    char*               list;
    size_t              list_len;
    const cf_allocator* a;
    cf_live_mode        mode;
    cf_live_build_fn    build;

    if (o == NULL || o->size < LIVE_OPTIONS_SIZE_1_0) {
        cf_error_set(err, CF_E_CAPABILITY, NULL, NULL,
                     "no reload options, or options without CF_LIVE_OPTIONS_INIT");
        return NULL;
    }
    a     = o->allocator;
    mode  = o->mode;
    build = o->build;
    if (a == NULL) {
        a = cf_default_allocator();
    }
    if (build == NULL || a == NULL) {
        cf_error_set(err, CF_E_CAPABILITY, NULL, NULL, "%s",
                     (build == NULL) ? "no build function to reload with"
                                     : "no allocator for the reloads");
        return NULL;
    }
    if (mode != CF_LIVE_MANUAL && mode != CF_LIVE_APPROVE && mode != CF_LIVE_AUTO) {
        cf_error_set(err, CF_E_CAPABILITY, NULL, NULL, "%d is not a reload mode",
                     (int)mode);
        return NULL;
    }
    l = (cf_live*)a->alloc(a->ctx, sizeof *l);
    if (l == NULL) {
        cf_error_set(err, CF_E_MEMORY, NULL, NULL, "no room for the reloads");
        return NULL;
    }
    memset(l, 0, sizeof *l);
    l->a          = a;
    l->mode       = mode;
    l->build      = build;
    l->build_user = o->build_user;
    l->check      = o->check;
    l->check_user = o->check_user;
    l->log        = o->log;
    l->log_user   = o->log_user;
    l->log_min    = o->log_min;
    cf_ref_init(&l->ticket_next, 0);
    cf_ref_init(&l->ticket_serving, 0);
    l->current = live_build(l, NULL, &list, &list_len, err);
    if (l->current == NULL) {
        a->release(a->ctx, l);
        return NULL;
    }
    l->generation        = 1;
    l->status.size       = sizeof l->status;
    l->status.generation = 1;
    l->status.last       = CF_LIVE_SWAPPED;
    cf_error_clear(&l->last_error);
    l->last_error.size = sizeof l->last_error;
    if (mode != CF_LIVE_MANUAL) {
        live_watch(l);
    }
    return l;
}

void cf_live_close(cf_live* l) {
    if (l == NULL) {
        return;
    }
    live_drop_pending(l);
    cf_watcher_close(l->watcher);
    cf_config_free(l->current);
    live_release(l, l->restarts);
    l->a->release(l->a->ctx, l);
}

cf_config* cf_live_get(cf_live* l) {
    cf_config* c;

    if (l == NULL) {
        return NULL;
    }
    live_lock(l);
    c = l->current;
    cf_config_ref(c);
    live_unlock(l);
    return c;
}

cf_live_result cf_live_poll(cf_live* l, cf_error* err) {
    const cf_config* watched;

    if (l == NULL) {
        cf_error_set(err, CF_E_CAPABILITY, NULL, NULL, "no reloads to poll");
        return CF_LIVE_FAILED;
    }
    if (l->mode == CF_LIVE_MANUAL) {
        return CF_LIVE_UNCHANGED;
    }
    /* A change waiting for approval is the newest the program knows of,
       so an edit after it replaces it rather than being lost behind it. */
    watched = (l->pending != NULL) ? l->pending : l->current;
    if (!cf_changed(watched)) {
        return CF_LIVE_UNCHANGED;
    }
    return live_rebuild(l, err);
}

cf_live_result cf_live_reload(cf_live* l, cf_error* err) {
    if (l == NULL) {
        cf_error_set(err, CF_E_CAPABILITY, NULL, NULL, "no reloads to reload");
        return CF_LIVE_FAILED;
    }
    return live_rebuild(l, err);
}

cf_live_result cf_live_wait(cf_live* l, long timeout_ms, cf_error* err) {
    if (l != NULL && l->watcher != NULL && l->mode != CF_LIVE_MANUAL) {
        (void)cf_watcher_wait(l->watcher, timeout_ms);
    }
    return cf_live_poll(l, err);
}

const cf_config* cf_live_pending(const cf_live* l) {
    return (l != NULL) ? l->pending : NULL;
}

cf_live_result cf_live_approve(cf_live* l, cf_error* err) {
    if (l == NULL) {
        cf_error_set(err, CF_E_CAPABILITY, NULL, NULL, "no reloads to approve");
        return CF_LIVE_FAILED;
    }
    if (l->pending == NULL) {
        return CF_LIVE_UNCHANGED;
    }
    live_swap(l, l->pending, l->pending_restarts, l->pending_restarts_len, CF_FALSE);
    l->pending              = NULL;
    l->pending_restarts     = NULL;
    l->pending_restarts_len = 0;
    return live_record(l, CF_LIVE_SWAPPED, CF_FALSE, err);
}

void cf_live_reject(cf_live* l) {
    if (l != NULL) {
        live_drop_pending(l);
        live_lock(l);
        l->status.pending = CF_FALSE;
        l->status.last    = CF_LIVE_UNCHANGED;
        live_unlock(l);
    }
}

/* cf_live_status as 1.0 has it: the smallest a caller may pass. When a
   field is added, this stays the offset just past `pending`. */
#define LIVE_STATUS_SIZE_1_0 sizeof(cf_live_status)

cf_live_result cf_live_commit(cf_live* l, cf_edit* e, const char* note, cf_error* err) {
    if (l == NULL) {
        cf_edit_abort(e);
        cf_error_set(err, CF_E_CAPABILITY, NULL, NULL, "no reloads to commit to");
        return CF_LIVE_FAILED;
    }
    if (!cf_edit_commit(e, note, err)) {
        live_log(l, CF_LOG_ERROR, "edit-refused",
                 (err != NULL) ? (cf_code)err->code : CF_E_POLICY,
                 (err != NULL && err->key[0] != 0) ? err->key : NULL,
                 (err != NULL) ? err->message : "an edit was refused");
        return CF_LIVE_FAILED;
    }
    return cf_live_reload(l, err);
}

cf_bool cf_live_status_get(cf_live* l, cf_live_status* out) {
    size_t n;

    if (l == NULL || out == NULL || out->size < LIVE_STATUS_SIZE_1_0) {
        return CF_FALSE;
    }
    n = (out->size < sizeof *out) ? out->size : sizeof *out;
    live_lock(l);
    /* Everything after `size`, as far as both structs reach. */
    memcpy((char*)out + sizeof out->size, (const char*)&l->status + sizeof out->size,
           n - sizeof out->size);
    live_unlock(l);
    return CF_TRUE;
}

cf_bool cf_live_last_error(cf_live* l, cf_error* err) {
    cf_bool failed;

    if (l == NULL) {
        return CF_FALSE;
    }
    live_lock(l);
    failed = (l->status.failures > 0) ? CF_TRUE : CF_FALSE;
    if (failed) {
        cf_error_copy(err, &l->last_error);
    }
    live_unlock(l);
    if (!failed && err != NULL) {
        cf_error_clear(err);
    }
    return failed;
}

cf_u64 cf_live_generation(cf_live* l) {
    cf_u64 g;

    if (l == NULL) {
        return 0;
    }
    live_lock(l);
    g = l->generation;
    live_unlock(l);
    return g;
}

size_t cf_live_restarts(const cf_live* l, char* buf, size_t cap) {
    size_t n;

    if (l == NULL || l->restarts == NULL) {
        if (buf != NULL && cap > 0) {
            buf[0] = '\0';
        }
        return 0;
    }
    n = l->restarts_len;
    if (buf != NULL && cap > 0) {
        size_t k = (n < cap - 1) ? n : cap - 1;
        memcpy(buf, l->restarts, k);
        buf[k] = '\0';
    }
    return n;
}
