/*
 * defaults.c - definition defaults as an ordinary source. C89.
 *
 * The specification claims that a default is a value from the
 * lowest-precedence source rather than a separate mechanism. Seeding
 * them outside the source list made that claim false in one visible
 * place: explain() had no name to print for them and showed "?".
 *
 * Making defaults a real source removes the special case instead of
 * papering over it, and the diagnostics get the right word for free.
 */

#include "configforge/cf_define.h"
#include "configforge/cf_source.h"

#include <string.h>

static cf_bool cf_defaults_load(const cf_source* s, cf_keyspace* ks) {
    const cf_defset* ds = (const cf_defset*)s->ref;
    size_t           i;

    if (ds == NULL) {
        return CF_TRUE;
    }
    for (i = 0; i < cf_defset_count(ds); ++i) {
        const cf_def* d = cf_defset_at(ds, i);
        if (d == NULL || !d->has_default) {
            continue;
        }
        /* No location: a value declared in code has no file, line or
           variable name to point at. */
        if (!cf_ks_add(ks, d->key, strlen(d->key), s->rank, &d->default_value, NULL)) {
            return CF_FALSE;
        }
    }
    return CF_TRUE;
}

static void cf_defaults_destroy(cf_source* s) {
    (void)s;
}

static const cf_source_vtable cf_defaults_vt = {"default", cf_defaults_load,
                                                cf_defaults_destroy, NULL, NULL};

cf_bool cf_source_defaults(cf_source* s, const char* name, int rank,
                           const cf_defset* ds) {
    if (s == NULL) {
        return CF_FALSE;
    }
    s->vt   = &cf_defaults_vt;
    s->name = (name != NULL) ? name : "default";
    s->rank = rank;
    s->ext  = NULL;
    /* Borrowed, not owned: the definition set outlives the source
       because both live in the configuration's arena. */
    s->ref = ds;
    return CF_TRUE;
}
