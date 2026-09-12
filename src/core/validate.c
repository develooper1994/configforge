/*
 * validate.c - applying the contract. C89.
 *
 * Conversion and validation happen here, once, at construction. That
 * is what lets every read afterwards be infallible: if resolution
 * succeeded, every key with a definition or a default holds a value of
 * the declared type, inside its declared bounds.
 */

#include "configforge/cf_define.h"

#include <string.h>

/* ------------------------------------------------------------------ *
 * Validation of one value against one definition
 * ------------------------------------------------------------------ */

static const char* validate_value(const cf_def* d, const cf_value* v, char* reason,
                                  size_t rcap) {
    if (d->has_range && d->type == CF_TYPE_INT) {
        if (v->u.i < d->imin || v->u.i > d->imax) {
            cf_fmt(reason, rcap, "%I is outside %I..%I", v->u.i, d->imin, d->imax);
            return reason;
        }
    }
#if CF_WITH_REAL
    if (d->has_range && d->type == CF_TYPE_REAL) {
        if (v->u.r < d->rmin || v->u.r > d->rmax) {
            char lo[CF_F64_DIGITS], hi[CF_F64_DIGITS], got[CF_F64_DIGITS];
            cf_f64_to_text(v->u.r, got, sizeof got);
            cf_f64_to_text(d->rmin, lo, sizeof lo);
            cf_f64_to_text(d->rmax, hi, sizeof hi);
            cf_fmt(reason, rcap, "%s is outside %s..%s", got, lo, hi);
            return reason;
        }
    }
#endif
    if (d->type == CF_TYPE_TEXT) {
        if (d->min_len != 0 && v->u.t.n < d->min_len) {
            cf_fmt(reason, rcap, "shorter than %zu characters", d->min_len);
            return reason;
        }
        if (d->max_len != 0 && v->u.t.n > d->max_len) {
            cf_fmt(reason, rcap, "longer than %zu characters", d->max_len);
            return reason;
        }
        if (d->allowed != NULL) {
            size_t i;
            for (i = 0; d->allowed[i] != NULL; ++i) {
                if (cf_cmp_bytes(v->u.t.p, v->u.t.n, d->allowed[i],
                                 strlen(d->allowed[i])) == 0) {
                    return NULL;
                }
            }
            /* Naming what was permitted matters more than naming what
               was given: the reader already knows what they wrote. */
            {
                size_t at = 0;
                size_t i2;
                at = cf_fmt_append(reason, rcap, at, "not one of");
                for (i2 = 0; d->allowed[i2] != NULL && i2 < 8; ++i2) {
                    at = cf_fmt_append(reason, rcap, at, " %s", d->allowed[i2]);
                }
                if (d->allowed[i2] != NULL) {
                    cf_fmt_append(reason, rcap, at, " ...");
                }
            }
            return reason;
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------ *
 * Applying the whole contract
 * ------------------------------------------------------------------ */

static cf_bool apply_entry(cf_keyspace* ks, const cf_def* d, cf_entry* e,
                           cf_apply_report* rep, cf_error* err) {
    cf_candidate* c;
    char          reason[CF_MESSAGE_MAX];

    for (c = e->chain; c != NULL; c = c->next) {
        const char* why = NULL;

        if (!cf_outcome_can_win(c->outcome)) {
            continue;
        }

        if ((c->flags & CF_CAND_TEXTUAL) != 0) {
            cf_value converted;
            if (!cf_value_from_text(d->type, c->value.u.t.p, c->value.u.t.n,
                                    &converted)) {
                cf_fmt(reason, sizeof reason, "\"%s\" is not a valid %s",
                       c->value.u.t.p, cf_type_name(d->type));
                why = reason;
            } else {
                /* The converted text is already in the arena when the
                   type is text; for the others the value is inline. */
                c->value = converted;
                c->flags &= ~(unsigned)CF_CAND_TEXTUAL;
                ++rep->converted;
            }
        } else if (c->value.type != d->type) {
            /* A typed source said something different from the
               declaration. Coercing here would undo the only reason to
               have a typed source at all. */
            cf_fmt(reason, sizeof reason, "source supplied %s where %s was declared",
                   cf_type_name(c->value.type), cf_type_name(d->type));
            why = reason;
        }

        if (why == NULL) {
            why = validate_value(d, &c->value, reason, sizeof reason);
        }
        if (why != NULL) {
            char* stored = cf_arena_dup(ks->arena, why, strlen(why));
            c->outcome   = CF_OUT_REJECTED;
            c->reason    = (stored != NULL) ? stored : "invalid";
            ++rep->rejected;

            if (d->on_invalid == CF_INVALID_STRICT) {
                return cf_error_set(err, CF_E_VALIDATE, d->key,
                                    (c->at != NULL) ? c->at : "", "%s: %s", d->key,
                                    c->reason);
            }
        }
    }
    return CF_TRUE;
}

cf_bool cf_defs_apply(cf_keyspace* ks, const cf_defset* ds, cf_unknown_policy unknown,
                      cf_apply_report* report, cf_error* err) {
    cf_apply_report local;
    size_t          i;

    if (report == NULL) {
        report = &local;
    }
    memset(report, 0, sizeof *report);

    if (ks == NULL || ds == NULL) {
        return cf_error_set(err, CF_E_MEMORY, NULL, NULL,
                            "apply called with nothing to apply");
    }

    for (i = 0; i < cf_ks_count(ks); ++i) {
        cf_entry*     e = cf_ks_at_mut(ks, i);
        const cf_def* d;

        if (e == NULL) {
            continue;
        }
        d = cf_defset_find(ds, e->key, e->keylen);

        if (d == NULL) {
            cf_candidate* c;
            ++report->unknown;
            /* No contract, so Tier 0 inference decides. Doing it here
               rather than in the source is what lets a declared type
               win over a guess. */
            for (c = e->chain; c != NULL; c = c->next) {
                cf_value v;
                if ((c->flags & CF_CAND_TEXTUAL) == 0) {
                    continue;
                }
                if (cf_value_infer(c->value.u.t.p, c->value.u.t.n, &v)) {
                    c->value = v;
                    c->flags &= ~(unsigned)CF_CAND_TEXTUAL;
                }
            }
            if (unknown == CF_UNKNOWN_REJECT) {
                size_t        dist = 0;
                const cf_def* near = cf_defset_nearest(ds, e->key, e->keylen, &dist);
                const char*   at =
                    (e->chain != NULL && e->chain->at != NULL) ? e->chain->at : "";
                if (near != NULL) {
                    return cf_error_set(err, CF_E_KEY, e->key, at,
                                        "unknown key '%s'; did you mean '%s'?", e->key,
                                        near->key);
                }
                return cf_error_set(err, CF_E_KEY, e->key, at, "unknown key '%s'",
                                    e->key);
            }
            continue;
        }
        if ((d->flags & CF_DEPRECATED) != 0) {
            ++report->deprecated;
        }
        if (!apply_entry(ks, d, e, report, err)) {
            return CF_FALSE;
        }
    }

    cf_ks_reselect(ks);

    /* Required parameters, and defaults that ended up being the
       answer. Both are counted after reselection, because a rejected
       winner can hand over to a default. */
    for (i = 0; i < ds->count; ++i) {
        const cf_def*   d = &ds->defs[i];
        const cf_entry* e = cf_ks_find(ks, d->key, strlen(d->key));

        if (e == NULL || e->selected == NULL) {
            if ((d->flags & CF_REQUIRED) != 0) {
                return cf_error_set(err, CF_E_REQUIRED, d->key, NULL,
                                    "'%s' is required and has no value", d->key);
            }
            continue;
        }
        if (d->has_default && e->selected->at == NULL &&
            cf_value_equal(&e->selected->value, &d->default_value)) {
            ++report->defaults_used;
        }
    }
    return CF_TRUE;
}
