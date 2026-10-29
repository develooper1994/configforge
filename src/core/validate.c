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
 * References (D-10)
 *
 * ${key} in a value somebody wrote is the value of another key, after
 * precedence has chosen it - url = http://${server.host}:${server.port}.
 * Three decisions make it explainable rather than a second resolver:
 *
 *   order    precedence first, references after. Each key's winner is
 *            chosen as if nothing referred to anything; then a winner
 *            with references has them replaced, a referenced key being
 *            finished first. So a reference reads exactly what a program
 *            reading that key would get, and explain() of the referenced
 *            key is the whole story of the part it supplied.
 *   typing   the result is text, typed by the contract like a value from
 *            the environment, so port = ${base.port} is an integer.
 *   names    only keys: ${env:HOME} would be a second environment source
 *            with no name in explain(), and the environment is already
 *            a source. A name that is not a key is refused, which keeps
 *            ${HOME}/cache from becoming a directory called "${HOME}".
 *
 * Refused, each naming the key: a reference to a key with no value; a
 * cycle; and a key that is not sensitive referring to one that is,
 * because its value would show the secret. $${ is a literal ${.
 * ------------------------------------------------------------------ */

/* The next ${name} at or after *i: 1 with its name and *i past the '}';
   0 at the end; -1 for a "${" that names no key. "$${" is skipped. */
static int ref_next(const char* p, size_t n, size_t* i, const char** name,
                    size_t* nlen) {
    while (*i + 1 < n) {
        size_t k = *i;

        if (p[k] == '$' && p[k + 1] == '$' && k + 2 < n && p[k + 2] == '{') {
            *i = k + 3;
        } else if (p[k] == '$' && p[k + 1] == '{') {
            const char* close = (const char*)memchr(p + k + 2, '}', n - k - 2);
            *name             = p + k + 2;
            *nlen             = (close != NULL) ? (size_t)(close - *name) : n - k - 2;
            if (close == NULL || *nlen == 0 || !cf_key_valid(*name, *nlen)) {
                return -1;
            }
            *i = (size_t)(close - p) + 1;
            return 1;
        } else {
            ++*i; /* any other character, a lone '$' among them */
        }
    }
    return 0;
}

/* $${ undone, once, into the arena. */
static char* unescape(cf_arena* ar, const char* p, size_t n, size_t* out_n) {
    char*  out = (char*)cf_arena_alloc(ar, n + 1);
    size_t i, j = 0;

    if (out == NULL) {
        return NULL;
    }
    for (i = 0; i < n; ++i) {
        if (p[i] == '$' && i + 2 < n && p[i + 1] == '$' && p[i + 2] == '{') {
            ++i; /* drop one '$' of the pair */
        }
        out[j++] = p[i];
    }
    out[j] = '\0';
    *out_n = j;
    return out;
}

/* Whether a value somebody wrote has references, and whether they are
   well formed. One with none is unescaped now; one with some waits, as
   written, for the keys it names. */
static const char* reference_syntax(cf_arena* ar, cf_candidate* c, char* reason,
                                    size_t rcap) {
    const char* p;
    const char* name = NULL;
    size_t      n, i = 0, nlen = 0, refs = 0;
    int         r;

    if ((c->flags & CF_CAND_AUTHORED) == 0 || c->value.type != CF_TYPE_TEXT) {
        return NULL;
    }
    c->flags &= ~(unsigned)CF_CAND_AUTHORED; /* read once */
    p = c->value.u.t.p;
    n = c->value.u.t.n;
    while ((r = ref_next(p, n, &i, &name, &nlen)) == 1) {
        ++refs;
    }
    if (r < 0) {
        cf_fmt(reason, rcap,
               "'${%S' refers to no key: a reference is ${key}, the key as the "
               "program names it; write '$${' for a literal '${'",
               name, (nlen > 40) ? (size_t)40 : nlen);
        return reason;
    }
    if (refs > 0) {
        c->flags |= CF_CAND_TEMPLATE;
        c->written = p;
        return NULL;
    }
    if (memchr(p, '$', n) != NULL) {
        size_t m   = 0;
        char*  out = unescape(ar, p, n, &m);
        if (out == NULL) {
            cf_fmt(reason, rcap, "no room to unescape '$${'");
            return reason;
        }
        c->value.u.t.p = out;
        c->value.u.t.n = m;
    }
    return NULL;
}

/* ------------------------------------------------------------------ *
 * Applying the whole contract
 * ------------------------------------------------------------------ */

/* One candidate against its definition: converted, type-checked and
   validated. NULL when it passes; a template passes until it is one. */
static const char* apply_candidate(const cf_def* d, cf_candidate* c,
                                   cf_apply_report* rep, char* reason, size_t rcap) {
    if ((c->flags & CF_CAND_TEMPLATE) != 0) {
        return NULL;
    }
    if ((c->flags & CF_CAND_TEXTUAL) != 0) {
        cf_value converted;
        if (!cf_value_from_text(d->type, c->value.u.t.p, c->value.u.t.n, &converted)) {
            if ((d->flags & CF_SENSITIVE) != 0) {
                cf_fmt(reason, rcap, "the value is not a valid %s",
                       cf_type_name(d->type));
            } else {
                cf_fmt(reason, rcap, "\"%s\" is not a valid %s", c->value.u.t.p,
                       cf_type_name(d->type));
            }
            return reason;
        }
        /* The converted text is already in the arena when the type is
           text; for the others the value is inline. */
        c->value = converted;
        c->flags &= ~(unsigned)CF_CAND_TEXTUAL;
        ++rep->converted;
    } else if (c->value.type != d->type) {
        /* A typed source said something different from the declaration.
           Coercing here would undo the only reason to have a typed
           source at all. */
        cf_fmt(reason, rcap, "source supplied %s where %s was declared",
               cf_type_name(c->value.type), cf_type_name(d->type));
        return reason;
    }
    return validate_value(d, &c->value, reason, rcap);
}

/* Refuse a candidate: rejected, with its reason, and the whole
   construction failed when the definition is strict. */
static cf_bool reject(cf_keyspace* ks, const cf_def* d, cf_candidate* c,
                      const char* why, cf_apply_report* rep, cf_error* err) {
    char* stored = cf_arena_dup(ks->arena, why, strlen(why));
    c->outcome   = CF_OUT_REJECTED;
    c->reason    = (stored != NULL) ? stored : "invalid";
    ++rep->rejected;
    if (d == NULL || d->on_invalid == CF_INVALID_STRICT) {
        return cf_error_set(err, CF_E_VALIDATE, (d != NULL) ? d->key : NULL,
                            (c->at != NULL) ? c->at : "", "%s: %s",
                            (d != NULL) ? d->key : "", c->reason);
    }
    return CF_TRUE;
}

static cf_bool apply_entry(cf_keyspace* ks, const cf_def* d, cf_entry* e,
                           cf_apply_report* rep, cf_error* err) {
    cf_candidate* c;
    char          reason[CF_MESSAGE_MAX];

    for (c = e->chain; c != NULL; c = c->next) {
        const char* why;

        if (!cf_outcome_can_win(c->outcome)) {
            continue;
        }
        why = reference_syntax(ks->arena, c, reason, sizeof reason);
        if (why == NULL) {
            why = apply_candidate(d, c, rep, reason, sizeof reason);
        }
        if (why != NULL && !reject(ks, d, c, why, rep, err)) {
            return CF_FALSE;
        }
    }
    return CF_TRUE;
}

/* ------------------------------------------------------------------ *
 * Replacing references
 * ------------------------------------------------------------------ */

/* The entry for a key, mutable: this pass changes outcomes. */
static cf_entry* find_mut(cf_keyspace* ks, const char* key, size_t n) {
    size_t    i = cf_ks_lower_bound(ks, key, n);
    cf_entry* e = (i < cf_ks_count(ks)) ? cf_ks_at_mut(ks, i) : NULL;

    return (e != NULL && e->keylen == n && memcmp(e->key, key, n) == 0) ? e : NULL;
}

/* A referenced key's value as text: one append to the result. */
static size_t ref_text(const cf_entry* t, char* tmp, size_t cap, const char** out) {
    const cf_value* v = &t->selected->value;

    if (v->type == CF_TYPE_TEXT) {
        *out = v->u.t.p;
        return v->u.t.n;
    }
    *out = tmp;
    return cf_value_to_text(v, tmp, cap);
}

/* The template with each reference replaced, measured and then written,
   into the arena. Every key it names is finished by now. */
static cf_bool expand_build(cf_keyspace* ks, cf_candidate* c) {
    const char* p   = c->written;
    size_t      n   = strlen(p);
    char*       out = NULL;
    size_t      len = 0;
    int         pass;

    for (pass = 0; pass < 2; ++pass) {
        size_t i = 0, j = 0;
        while (i < n) {
            if (p[i] == '$' && i + 2 < n && p[i + 1] == '$' && p[i + 2] == '{') {
                if (out != NULL) {
                    out[j]     = '$';
                    out[j + 1] = '{';
                }
                j += 2;
                i += 3;
            } else if (p[i] == '$' && i + 1 < n && p[i + 1] == '{') {
                const char*     name  = p + i + 2;
                const char*     close = (const char*)memchr(name, '}', n - i - 2);
                const cf_entry* t;
                const char*     s;
                char            tmp[CF_VALUE_TEXT_MAX];
                size_t          sn;

                t  = cf_ks_find(ks, name, (size_t)(close - name));
                sn = ref_text(t, tmp, sizeof tmp, &s);
                if (out != NULL) {
                    memcpy(out + j, s, sn);
                }
                j += sn;
                i = (size_t)(close - p) + 1;
            } else {
                if (out != NULL) {
                    out[j] = p[i];
                }
                ++j;
                ++i;
            }
        }
        if (pass == 0) {
            len = j;
            out = (char*)cf_arena_alloc(ks->arena, len + 1);
            if (out == NULL) {
                return CF_FALSE;
            }
        } else {
            out[len] = '\0';
        }
    }
    cf_value_text(&c->value, out, len);
    c->flags = (c->flags & ~(unsigned)CF_CAND_TEMPLATE) | CF_CAND_TEXTUAL;
    return CF_TRUE;
}

typedef struct expand_ctx {
    cf_keyspace*     ks;
    const cf_defset* ds;
    cf_apply_report* rep;
    cf_error*        err;
    char             reason[CF_MESSAGE_MAX]; /* one, not one a level */
} expand_ctx;

/* Finish `e`: its winner's references replaced, each key they name
   finished first, and the result typed and validated. A result the
   contract refuses hands over to the next candidate, as any refused
   value does under fallback. */
static cf_bool expand_entry(expand_ctx* x, cf_entry* e, int depth) {
    const cf_def* d = cf_defset_find(x->ds, e->key, e->keylen);

    for (;;) {
        cf_candidate* c = e->selected;
        const char*   name;
        const char*   why;
        size_t        i = 0, nlen = 0;
        cf_candidate* k;

        if (c == NULL || (c->flags & CF_CAND_TEMPLATE) == 0) {
            return CF_TRUE;
        }
        if ((c->flags & CF_CAND_EXPANDING) != 0) {
            return cf_error_set(x->err, CF_E_VALIDATE, e->key,
                                (c->at != NULL) ? c->at : "",
                                "%s: its references lead back to it", e->key);
        }
        if (depth >= CF_MAX_DEPTH) {
            return cf_error_set(
                x->err, CF_E_VALIDATE, e->key, (c->at != NULL) ? c->at : "",
                "%s: references nest deeper than %d", e->key, CF_MAX_DEPTH);
        }
        c->flags |= CF_CAND_EXPANDING;
        while (ref_next(c->written, strlen(c->written), &i, &name, &nlen) == 1) {
            cf_entry*     t = find_mut(x->ks, name, nlen);
            const cf_def* td =
                (t != NULL) ? cf_defset_find(x->ds, t->key, t->keylen) : NULL;

            if (t == NULL || t->selected == NULL) {
                return cf_error_set(
                    x->err, CF_E_VALIDATE, e->key, (c->at != NULL) ? c->at : "",
                    "%s: ${%S} names a key with no value", e->key, name, nlen);
            }
            if (td != NULL && (td->flags & CF_SENSITIVE) != 0 &&
                (d == NULL || (d->flags & CF_SENSITIVE) == 0)) {
                return cf_error_set(
                    x->err, CF_E_VALIDATE, e->key, (c->at != NULL) ? c->at : "",
                    "%s refers to the sensitive %S and is not sensitive "
                    "itself, so it would show the secret",
                    e->key, name, nlen);
            }
            if (!expand_entry(x, t, depth + 1)) {
                return CF_FALSE;
            }
        }
        c->flags &= ~(unsigned)CF_CAND_EXPANDING;

        if (!expand_build(x->ks, c)) {
            return cf_error_set(x->err, CF_E_MEMORY, e->key, NULL,
                                "no room to replace the references in '%s'", e->key);
        }
        if (d == NULL) {
            /* No contract: Tier 0 inference, as for any text. */
            cf_value v;
            if (cf_value_infer(c->value.u.t.p, c->value.u.t.n, &v)) {
                c->value = v;
            }
            c->flags &= ~(unsigned)CF_CAND_TEXTUAL;
            return CF_TRUE;
        }
        why = apply_candidate(d, c, x->rep, x->reason, sizeof x->reason);
        if (why == NULL) {
            return CF_TRUE;
        }
        if (!reject(x->ks, d, c, why, x->rep, x->err)) {
            return CF_FALSE;
        }
        /* Fallback: the next candidate down that may still win. */
        for (k = c->next; k != NULL && !cf_outcome_can_win(k->outcome); k = k->next) {
        }
        if (k != NULL) {
            k->outcome = CF_OUT_SELECTED;
        }
        e->selected = k;
    }
}

static cf_bool expand_all(cf_keyspace* ks, const cf_defset* ds, cf_apply_report* rep,
                          cf_error* err) {
    expand_ctx* x = (expand_ctx*)cf_arena_calloc(ks->arena, sizeof *x);
    size_t      i;

    if (x == NULL) {
        return cf_error_set(err, CF_E_MEMORY, NULL, NULL,
                            "no room to replace references");
    }
    x->ks  = ks;
    x->ds  = ds;
    x->rep = rep;
    x->err = err;
    for (i = 0; i < cf_ks_count(ks); ++i) {
        cf_entry* e = cf_ks_at_mut(ks, i);
        if (e != NULL && !expand_entry(x, e, 0)) {
            return CF_FALSE;
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
        if ((c->flags & CF_CAND_DEFAULT) != 0) {
            /* Not set by anyone, so not refused: the answer when no
               permitted source has one, below every one that has. */
            if (best == NULL) {
                best    = c;
                best_at = 1 << 14;
            }
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
                    reference_syntax(cf_ks_arena(ks), c, why, sizeof why) != NULL) {
                    return cf_error_set(err, CF_E_VALIDATE, e->key,
                                        (c->at != NULL) ? c->at : "", "%s: %s", e->key,
                                        why);
                }
                /* A template is inferred once its references are in. */
                if ((c->flags & CF_CAND_TEXTUAL) == 0 ||
                    (c->flags & CF_CAND_TEMPLATE) != 0) {
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

    /* References last: every winner is chosen, so a reference reads what
       a program reading that key would. */
    if (!expand_all(ks, ds, report, err)) {
        return CF_FALSE;
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
