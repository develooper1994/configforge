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

#include "configforge/cf_error.h"
#include "configforge/cf_define.h"
#include "configforge/cf_encoding.h"
#include "configforge/cf_source.h"
#include "configforge/cf_build_options.h"
#include "configforge/cf_keyspace.h"
#include "configforge/cf_port.h"
#include "configforge/cf_value.h"

#include <string.h>
/* iwyu asks for this include and against it by turns (cf_keyspace.h
   and cf_error.h carry each other's names): it is kept. */
/* IWYU pragma: begin_keep */
#include "configforge/cf_keyspace.h"
/* IWYU pragma: end_keep */

/* A default the program wrote is UTF-8 too (D-108), named by its key; a value
   that is not text has no bytes to look at. */
static cf_bool defaults_utf8(cf_keyspace* ks, const char* key, const cf_value* v) {
    cf_error err = CF_ERROR_INIT;

    if (v->type != CF_TYPE_TEXT || cf_encoding_check(key, v->u.t.p, v->u.t.n, &err)) {
        return CF_TRUE;
    }
    cfi_ks_refuse(ks, &err);
    return CF_FALSE;
}

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
        if (d->list_mode != CF_LIST_NONE) {
            char     key[CF_KEY_MAX];
            cf_value count;
            size_t   j, n;
            for (j = 0; j < d->list_default_count; ++j) {
                n = cf_fmt(key, sizeof key, "%s.%zu", d->key, j);
                if (n >= sizeof key || !defaults_utf8(ks, key, &d->list_defaults[j]) ||
                    !cf_ks_add_default(ks, key, n, s->rank, &d->list_defaults[j])) {
                    return CF_FALSE;
                }
            }
            n = cf_fmt(key, sizeof key, "%s.count", d->key);
            cf_value_int(&count, (cf_i64)d->list_default_count);
            if (n >= sizeof key || !cf_ks_add_default(ks, key, n, s->rank, &count)) {
                return CF_FALSE;
            }
            continue;
        }
        /* No location: a value declared in code has no file, line or
           variable name to point at. */
        if (!defaults_utf8(ks, d->key, &d->default_value) ||
            !cf_ks_add_default(ks, d->key, strlen(d->key), s->rank,
                               &d->default_value)) {
            return CF_FALSE;
        }
    }
    return CF_TRUE;
}

static void cf_defaults_destroy(cf_source* s) {
    (void)s;
}

static const cf_source_vtable cf_defaults_vt = {
    sizeof(cf_source_vtable), "default", cf_defaults_load,
    cf_defaults_destroy,      NULL,      NULL};

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
