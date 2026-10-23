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

/* A reason never quotes a sensitive value: it is stored on the candidate,
   shown by explain() and carried in the error a strict definition
   raises, and an out-of-range PIN is still a PIN. */
static const char* validate_value(const cf_def* d, const cf_value* v, char* reason,
                                  size_t rcap) {
    cf_bool secret = ((d->flags & CF_SENSITIVE) != 0) ? CF_TRUE : CF_FALSE;

    if (d->has_range && d->type == CF_TYPE_INT) {
        if (v->u.i < d->imin || v->u.i > d->imax) {
            if (secret) {
                cf_fmt(reason, rcap, "the value is outside %I..%I", d->imin, d->imax);
            } else {
                cf_fmt(reason, rcap, "%I is outside %I..%I", v->u.i, d->imin, d->imax);
            }
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
            cf_fmt(reason, rcap, "%s is outside %s..%s", secret ? "the value" : got, lo,
                   hi);
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
 * The reserved syntax (D-10)
 *
 * ${...} will one day refer to another value. Until it does, a value
 * containing it is refused rather than taken literally: taken literally,
 * cache.dir = ${HOME}/cache creates a directory called "${HOME}", and
 * the day references arrive every such configuration would change
 * meaning without a word. Refusing it now is what makes adding them
 * later an addition rather than a break.
 *
 * $${ is the escape, and means a literal ${. It is unescaped here, once,
 * so the value a program reads - and the one a report records - is what
 * was meant.
 * ------------------------------------------------------------------ */

static const char* reserved_syntax(cf_arena* ar, cf_candidate* c, char* reason,
                                   size_t rcap) {
    const char* p;
    size_t      n, i, escapes = 0;

    if ((c->flags & CF_CAND_AUTHORED) == 0 || c->value.type != CF_TYPE_TEXT) {
        return NULL;
    }
    c->flags &= ~(unsigned)CF_CAND_AUTHORED; /* checked once */
    p = c->value.u.t.p;
    n = c->value.u.t.n;
    for (i = 0; i + 1 < n; ++i) {
        if (p[i] != '$') {
            continue;
        }
        if (p[i + 1] == '$' && i + 2 < n && p[i + 2] == '{') {
            ++escapes;
            i += 2; /* "$${" is the escape; its "{" opens nothing */
        } else if (p[i + 1] == '{') {
            cf_fmt(reason, rcap,
                   "'${' is reserved for references to other values, which are not "
                   "supported yet; write '$${' for a literal '${'");
            return reason;
        }
    }
    if (escapes > 0) {
        char*  out = (char*)cf_arena_alloc(ar, n - escapes + 1);
        size_t j   = 0;
        if (out == NULL) {
            cf_fmt(reason, rcap, "no room to unescape '$${'");
            return reason;
        }
        for (i = 0; i < n; ++i) {
            if (p[i] == '$' && i + 2 < n && p[i + 1] == '$' && p[i + 2] == '{') {
                ++i; /* drop one '$' of the pair */
            }
            out[j++] = p[i];
        }
        out[j]         = '\0';
        c->value.u.t.p = out;
        c->value.u.t.n = j;
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

        why = reserved_syntax(ks->arena, c, reason, sizeof reason);
        if (why == NULL && (c->flags & CF_CAND_TEXTUAL) != 0) {
            cf_value converted;
            if (!cf_value_from_text(d->type, c->value.u.t.p, c->value.u.t.n,
                                    &converted)) {
                if ((d->flags & CF_SENSITIVE) != 0) {
                    cf_fmt(reason, sizeof reason, "the value is not a valid %s",
                           cf_type_name(d->type));
                } else {
                    cf_fmt(reason, sizeof reason, "\"%s\" is not a valid %s",
                           c->value.u.t.p, cf_type_name(d->type));
                }
                why = reason;
            } else {
                /* The converted text is already in the arena when the
                   type is text; for the others the value is inline. */
                c->value = converted;
                c->flags &= ~(unsigned)CF_CAND_TEXTUAL;
                ++rep->converted;
            }
        } else if (why == NULL && c->value.type != d->type) {
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

/* ------------------------------------------------------------------ *
 * Per-parameter policy
 *
 * The specification writes it as "database.password: Environment >
 * Local", and the definition carries exactly that: the source names
 * permitted to supply the key, in decreasing precedence. One list does
 * both jobs - a source not named may not set the key, and the named
 * ones are tried in this order rather than the global one, which is how
 * an operator-controlled setting survives a command line that tries to
 * override it.
 *
 * A violation is a rejected candidate with a reason, not a failed
 * construction. Two arguments for that. Refusing to start would let any
 * user stop the program by passing a locked option, which turns a
 * policy into a denial of service. And for the secret case the damage
 * is already done: a password on the command line is in `ps` whether or
 * not this library uses it, so the useful response is to say so loudly
 * - in explain(), in the report, and at error severity in Doctor -
 * rather than to refuse to run.
 * ------------------------------------------------------------------ */

static const char* source_name(const cf_rank_names* names, int rank) {
    if (names == NULL || names->name_of == NULL) {
        return NULL;
    }
    return names->name_of(names->ctx, rank);
}

/* Position in the policy list, or -1 when the source is not permitted. */
static int policy_index(const char* const* policy, const char* name) {
    int i;

    if (policy == NULL || name == NULL) {
        return -1;
    }
    for (i = 0; policy[i] != NULL; ++i) {
        if (strcmp(policy[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

void cf_policy_text(const char* const* policy, char* buf, size_t cap) {
    size_t at = 0;
    int    i;

    for (i = 0; policy != NULL && policy[i] != NULL; ++i) {
        if (i > 0) {
            at = cf_fmt_append(buf, cap, at, " > ");
        }
        at = cf_fmt_append(buf, cap, at, "%s", policy[i]);
    }
    if (at == 0 && cap > 0) {
        buf[0] = '\0';
    }
}

/* Re-selects one entry under its policy. The chain keeps its rank order
   so explain() shows what actually happened; the reason on each loser
   says why the usual answer is not the answer. */
static void apply_policy(cf_entry* e, const cf_def* d, const cf_rank_names* names,
                         cf_arena* arena) {
    char          list[CF_MESSAGE_MAX];
    cf_candidate* c;
    cf_candidate* best    = NULL;
    int           best_at = -1;

    cf_policy_text(d->policy, list, sizeof list);

    for (c = e->chain; c != NULL; c = c->next) {
        const char* name = source_name(names, c->source);
        int         at;

        if (!cf_outcome_can_win(c->outcome)) {
            continue;
        }
        at = policy_index(d->policy, name);
        if (at < 0) {
            char msg[CF_MESSAGE_MAX];
            c->outcome = CF_OUT_REJECTED;
            cf_fmt(msg, sizeof msg, "policy: '%s' may only be set from %s", e->key,
                   list);
            c->reason = cf_arena_dup(arena, msg, cf_fmt(NULL, 0, "%s", msg));
            continue;
        }
        if (best == NULL || at < best_at) {
            best    = c;
            best_at = at;
        }
    }

    for (c = e->chain; c != NULL; c = c->next) {
        if (c == best) {
            c->outcome = CF_OUT_SELECTED;
        } else if (c->outcome == CF_OUT_SELECTED) {
            char msg[CF_MESSAGE_MAX];
            c->outcome = CF_OUT_SHADOWED;
            cf_fmt(msg, sizeof msg, "policy puts %s first for this key",
                   (best != NULL) ? source_name(names, best->source) : "nothing");
            c->reason = cf_arena_dup(arena, msg, cf_fmt(NULL, 0, "%s", msg));
        }
    }
    e->selected = best;
}

cf_bool cf_defs_apply(cf_keyspace* ks, const cf_defset* ds, const cf_rank_names* names,
                      cf_unknown_policy unknown, cf_apply_report* report,
                      cf_error* err) {
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
                char     why[CF_MESSAGE_MAX];
                /* No definition, so no fallback to ask for: the
                   reserved syntax fails construction, as any invalid
                   value does by default (D-05). */
                if (cf_outcome_can_win(c->outcome) &&
                    reserved_syntax(cf_ks_arena(ks), c, why, sizeof why) != NULL) {
                    return cf_error_set(err, CF_E_VALIDATE, e->key,
                                        (c->at != NULL) ? c->at : "", "%s: %s", e->key,
                                        why);
                }
                if ((c->flags & CF_CAND_TEXTUAL) == 0) {
                    continue;
                }
                if (cf_value_infer(c->value.u.t.p, c->value.u.t.n, &v)) {
                    c->value = v;
                    c->flags &= ~(unsigned)CF_CAND_TEXTUAL;
                }
            }
            if (unknown == CF_UNKNOWN_REJECT &&
                !cf_defset_is_open(ds, e->key, e->keylen)) {
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

    /* Policy after reselection, because it re-selects: a candidate
       rejected for its type is out of the running before policy looks
       at the rest, and a policy that picked a rejected candidate would
       hand back a value the contract already refused. */
    for (i = 0; i < cf_ks_count(ks); ++i) {
        cf_entry*     e = cf_ks_at_mut(ks, i);
        const cf_def* d;

        if (e == NULL) {
            continue;
        }
        d = cf_defset_find(ds, e->key, e->keylen);
        if (d == NULL || d->policy == NULL) {
            continue;
        }
        apply_policy(e, d, names, cf_ks_arena(ks));
    }

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
