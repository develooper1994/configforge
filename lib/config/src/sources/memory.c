/*
 * memory.c - explicit key/value pairs. C89.
 *
 * Also the defaults source: a default is a value from the
 * lowest-precedence source rather than a separate mechanism, which is
 * why explain() can show a default losing to a file with no special
 * case anywhere.
 */

#include "configforge/cf_error.h"
#include "configforge/cf_encoding.h"
#include "configforge/cf_source.h"
#include "configforge/cf_keyspace.h"
#include "configforge/cf_port.h"

#include <string.h>
/* iwyu asks for this include and against it by turns (cf_keyspace.h
   and cf_error.h carry each other's names): it is kept. */
/* IWYU pragma: begin_keep */
#include "configforge/cf_keyspace.h"
/* IWYU pragma: end_keep */

static cf_bool cf_memory_load(const cf_source* s, cf_keyspace* ks) {
    size_t   i;
    cf_error err = CF_ERROR_INIT;

    for (i = 0; i < s->u.mem.count; ++i) {
        const cf_pair* p = &s->u.mem.pairs[i];
        size_t         klen;

        if (p->key == NULL || p->value == NULL) {
            continue;
        }
        klen = strlen(p->key);
        if (!cf_key_valid(p->key, klen)) {
            return CF_FALSE;
        }
        /* The program's own text is UTF-8 too (D-108), named by its key. */
        if (!cf_encoding_check(p->key, p->value, strlen(p->value), &err)) {
            cfi_ks_refuse(ks, &err);
            return CF_FALSE;
        }
        /* Recorded as written. The type is decided later, by a
           definition if one exists and by Tier 0 inference otherwise.
           No location: a value set in code has no file, line or
           variable name to point at. */
        if (!cf_ks_add_text(ks, p->key, klen, s->rank, p->value, strlen(p->value),
                            NULL)) {
            return CF_FALSE;
        }
    }
    return CF_TRUE;
}

static void cf_memory_destroy(cf_source* s) {
    (void)s;
}

static const cf_source_vtable cf_memory_vt = {
    sizeof(cf_source_vtable), "memory", cf_memory_load, cf_memory_destroy, NULL, NULL};

/* The pair array is borrowed, not copied. Values are duplicated into
   the key space arena when added, so the caller's array only has to
   survive the load call. */
cf_bool cf_source_memory(cf_source* s, const char* name, int rank, const cf_pair* pairs,
                         size_t count) {
    if (s == NULL || pairs == NULL) {
        return CF_FALSE;
    }
    s->vt          = &cf_memory_vt;
    s->name        = (name != NULL) ? name : "memory";
    s->rank        = rank;
    s->ext         = NULL;
    s->ref         = NULL;
    s->u.mem.pairs = pairs;
    s->u.mem.count = count;
    return CF_TRUE;
}

/* ------------------------------------------------------------------ *
 * Dispatch
 * ------------------------------------------------------------------ */

cf_bool cf_source_load(const cf_source* s, cf_keyspace* ks) {
    if (s == NULL || s->vt == NULL || s->vt->load == NULL || ks == NULL) {
        return CF_FALSE;
    }
    return s->vt->load(s, ks);
}

void cf_source_destroy(cf_source* s) {
    if (s != NULL && s->vt != NULL && s->vt->destroy != NULL) {
        s->vt->destroy(s);
    }
}
