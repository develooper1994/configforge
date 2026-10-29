/*
 * define.c - definitions and the definition set. C89.
 */

#include "configforge/cf_define.h"

#include <string.h>

void cf_def_init(cf_def* d, cf_type type, const char* key) {
    if (d == NULL) {
        return;
    }
    memset(d, 0, sizeof *d);
    d->size       = sizeof *d;
    d->type       = type;
    d->key        = key;
    d->on_invalid = CF_INVALID_STRICT;
}

cf_bool cf_defset_init(cf_defset* ds, cf_arena* arena, size_t hint) {
    if (ds == NULL || arena == NULL) {
        return CF_FALSE;
    }
    if (hint < 8) {
        hint = 8;
    }
    /* Zeroed whole rather than field by field. Adding open_prefixes and
       forgetting to initialise open_count segfaulted every caller that
       built a defset on the stack, and the next field added would do
       the same. */
    memset(ds, 0, sizeof *ds);
    ds->arena    = arena;
    ds->count    = 0;
    ds->capacity = hint;
    ds->sealed   = CF_FALSE;
    ds->defs     = (cf_def*)cf_arena_alloc(arena, hint * sizeof(cf_def));
    return (ds->defs != NULL) ? CF_TRUE : CF_FALSE;
}

static cf_bool defset_grow(cf_defset* ds) {
    size_t  ncap = ds->capacity * 2;
    cf_def* nd   = (cf_def*)cf_arena_alloc(ds->arena, ncap * sizeof(cf_def));

    if (nd == NULL) {
        return CF_FALSE;
    }
    memcpy(nd, ds->defs, ds->count * sizeof(cf_def));
    ds->defs     = nd;
    ds->capacity = ncap;
    return CF_TRUE;
}

/* ------------------------------------------------------------------ *
 * Self-consistency
 *
 * A definition that contradicts itself is a programming error, and the
 * right moment to refuse it is where it was written rather than when a
 * value happens to exercise it.
 * ------------------------------------------------------------------ */

static cf_bool def_check(const cf_def* d, cf_error* err) {
    size_t klen;

    if (d == NULL || d->key == NULL) {
        return cf_error_set(err, CF_E_DEFINITION, NULL, NULL, "definition has no key");
    }
    klen = strlen(d->key);
    if (!cf_key_valid(d->key, klen)) {
        return cf_error_set(err, CF_E_DEFINITION, d->key, NULL,
                            "'%s' is not a valid key: canonical keys are "
                            "dotted lowercase ASCII",
                            d->key);
    }
    if (d->size < sizeof(cf_def)) {
        return cf_error_set(err, CF_E_DEFINITION, d->key, NULL,
                            "descriptor was not initialised with cf_def_init");
    }
    if (d->replaced_by != NULL) {
        size_t rlen = strlen(d->replaced_by);
        if ((d->flags & CF_DEPRECATED) == 0) {
            return cf_error_set(err, CF_E_DEFINITION, d->key, NULL,
                                "'%s' names a replacement and is not CF_DEPRECATED",
                                d->key);
        }
        if (!cf_key_valid(d->replaced_by, rlen) ||
            strcmp(d->replaced_by, d->key) == 0) {
            return cf_error_set(
                err, CF_E_DEFINITION, d->key, NULL,
                "'%s' is replaced by '%s', which is not another valid key", d->key,
                d->replaced_by);
        }
    }
    if (d->has_range && d->type != CF_TYPE_INT && d->type != CF_TYPE_REAL) {
        return cf_error_set(err, CF_E_DEFINITION, d->key, NULL,
                            "a range is meaningless on type %s", cf_type_name(d->type));
    }
    if (d->has_range && d->type == CF_TYPE_INT && d->imin > d->imax) {
        return cf_error_set(err, CF_E_DEFINITION, d->key, NULL,
                            "range is inverted: %I > %I", d->imin, d->imax);
    }
    if (d->allowed != NULL && d->type != CF_TYPE_TEXT) {
        return cf_error_set(err, CF_E_DEFINITION, d->key, NULL,
                            "a permitted-value list applies only to text");
    }
    if (d->allowed != NULL && d->allowed[0] == NULL) {
        return cf_error_set(err, CF_E_DEFINITION, d->key, NULL,
                            "permitted-value list is empty, so nothing "
                            "could ever be accepted");
    }
    if (d->max_len != 0 && d->min_len > d->max_len) {
        return cf_error_set(err, CF_E_DEFINITION, d->key, NULL,
                            "length bounds are inverted");
    }
    if ((d->flags & CF_REQUIRED) != 0 && d->has_default) {
        return cf_error_set(err, CF_E_DEFINITION, d->key, NULL,
                            "required and defaulted at once: a default "
                            "means it can never be absent");
    }
    if (d->has_default) {
        if (d->default_value.type != d->type) {
            return cf_error_set(err, CF_E_DEFINITION, d->key, NULL,
                                "default is %s but the parameter is %s",
                                cf_type_name(d->default_value.type),
                                cf_type_name(d->type));
        }
        if (d->has_range && d->type == CF_TYPE_INT &&
            (d->default_value.u.i < d->imin || d->default_value.u.i > d->imax)) {
            return cf_error_set(err, CF_E_DEFINITION, d->key, NULL,
                                "default %I is outside its own range %I..%I",
                                d->default_value.u.i, d->imin, d->imax);
        }
#if CF_WITH_REAL
        if (d->has_range && d->type == CF_TYPE_REAL &&
            (d->default_value.u.r < d->rmin || d->default_value.u.r > d->rmax)) {
            return cf_error_set(err, CF_E_DEFINITION, d->key, NULL,
                                "default is outside its own range");
        }
#endif
    }
    return CF_TRUE;
}

cf_bool cf_define(cf_defset* ds, const cf_def* d, cf_error* err) {
    size_t klen;
    char*  key;
    size_t i;

    if (ds == NULL || ds->defs == NULL) {
        return cf_error_set(err, CF_E_MEMORY, NULL, NULL,
                            "definition set was not initialised");
    }
    if (ds->sealed) {
        return cf_error_set(err, CF_E_DEFINITION, d ? d->key : NULL, NULL,
                            "the definition set is sealed");
    }
    if (!def_check(d, err)) {
        return CF_FALSE;
    }
    klen = strlen(d->key);

    for (i = 0; i < ds->count; ++i) {
        if (cf_key_cmp(ds->defs[i].key, strlen(ds->defs[i].key), d->key, klen) == 0) {
            return cf_error_set(err, CF_E_DEFINITION, d->key, NULL,
                                "'%s' is defined twice", d->key);
        }
    }
    if (ds->count == ds->capacity && !defset_grow(ds)) {
        return cf_error_set(err, CF_E_MEMORY, d->key, NULL,
                            "no room for another definition");
    }
    key = cf_arena_dup(ds->arena, d->key, klen);
    if (key == NULL) {
        return cf_error_set(err, CF_E_MEMORY, d->key, NULL, "no room to copy the key");
    }

    ds->defs[ds->count]     = *d;
    ds->defs[ds->count].key = key;

    if (d->has_default && d->type == CF_TYPE_TEXT) {
        char* t =
            cf_arena_dup(ds->arena, d->default_value.u.t.p, d->default_value.u.t.n);
        if (t == NULL) {
            return cf_error_set(err, CF_E_MEMORY, d->key, NULL,
                                "no room to copy the default");
        }
        ds->defs[ds->count].default_value.u.t.p = t;
    }
    ++ds->count;
    return CF_TRUE;
}

/* ------------------------------------------------------------------ *
 * Convenience
 * ------------------------------------------------------------------ */

cf_bool cf_def_bool(cf_defset* ds, const char* key, cf_bool dflt, unsigned flags,
                    cf_error* err) {
    cf_def d;
    cf_def_init(&d, CF_TYPE_BOOL, key);
    d.flags = flags;
    if ((flags & CF_REQUIRED) == 0) {
        d.has_default = CF_TRUE;
        cf_value_bool(&d.default_value, dflt);
    }
    return cf_define(ds, &d, err);
}

cf_bool cf_def_int(cf_defset* ds, const char* key, cf_i64 dflt, cf_i64 min, cf_i64 max,
                   unsigned flags, cf_error* err) {
    cf_def d;
    cf_def_init(&d, CF_TYPE_INT, key);
    d.flags = flags;
    if (min != max) {
        d.has_range = CF_TRUE;
        d.imin      = min;
        d.imax      = max;
    }
    if ((flags & CF_REQUIRED) == 0) {
        d.has_default = CF_TRUE;
        cf_value_int(&d.default_value, dflt);
    }
    return cf_define(ds, &d, err);
}

cf_bool cf_def_text(cf_defset* ds, const char* key, const char* dflt, unsigned flags,
                    cf_error* err) {
    cf_def d;
    cf_def_init(&d, CF_TYPE_TEXT, key);
    d.flags = flags;
    if ((flags & CF_REQUIRED) == 0 && dflt != NULL) {
        d.has_default = CF_TRUE;
        cf_value_text(&d.default_value, dflt, strlen(dflt));
    }
    return cf_define(ds, &d, err);
}

#if CF_WITH_REAL
cf_bool cf_def_real(cf_defset* ds, const char* key, double dflt, double min, double max,
                    unsigned flags, cf_error* err) {
    cf_def d;
    cf_def_init(&d, CF_TYPE_REAL, key);
    d.flags = flags;
    if (min != max) {
        d.has_range = CF_TRUE;
        d.rmin      = min;
        d.rmax      = max;
    }
    if ((flags & CF_REQUIRED) == 0) {
        d.has_default = CF_TRUE;
        cf_value_real(&d.default_value, dflt);
    }
    return cf_define(ds, &d, err);
}
#endif

/* ------------------------------------------------------------------ *
 * Sealing and lookup
 * ------------------------------------------------------------------ */

static void def_swap(cf_def* a, cf_def* b) {
    cf_def t = *a;
    *a       = *b;
    *b       = t;
}

/* Definition counts are in the tens, and the set is built once and
   never appended to afterwards, so insertion sort is both the fastest
   and the least code here. Nothing larger would change that. */
cf_bool cf_defset_open(cf_defset* ds, const char* prefix, cf_error* err) {
    if (ds == NULL || prefix == NULL || prefix[0] == '\0') {
        return cf_error_set(err, CF_E_DEFINITION, NULL, NULL,
                            "an open prefix must name something");
    }
    if (ds->open_count >= CF_MAX_OPEN_PREFIXES) {
        return cf_error_set(err, CF_E_DEFINITION, prefix, NULL,
                            "more than %d open prefixes", CF_MAX_OPEN_PREFIXES);
    }
    ds->open_prefixes[ds->open_count++] = prefix;
    return CF_TRUE;
}

cf_bool cf_defset_is_open(const cf_defset* ds, const char* key, size_t keylen) {
    size_t i;

    if (ds == NULL || key == NULL) {
        return CF_FALSE;
    }
    for (i = 0; i < ds->open_count; ++i) {
        const char* p = ds->open_prefixes[i];
        size_t      n = strlen(p);
        /* At a segment boundary only: opening "plugins" must not open
           "pluginsomething", which is the bug every prefix check
           has. */
        if (keylen > n && memcmp(key, p, n) == 0 && key[n] == '.') {
            return CF_TRUE;
        }
    }
    return CF_FALSE;
}

void cf_defset_seal(cf_defset* ds) {
    size_t i, j;

    if (ds == NULL || ds->sealed) {
        return;
    }
    for (i = 1; i < ds->count; ++i) {
        for (j = i; j > 0; --j) {
            if (cf_key_cmp(ds->defs[j - 1].key, strlen(ds->defs[j - 1].key),
                           ds->defs[j].key, strlen(ds->defs[j].key)) <= 0) {
                break;
            }
            def_swap(&ds->defs[j - 1], &ds->defs[j]);
        }
    }
    ds->sealed = CF_TRUE;
}

const cf_def* cf_defset_find(const cf_defset* ds, const char* key, size_t keylen) {
    size_t lo, hi;

    if (ds == NULL || ds->count == 0 || key == NULL) {
        return NULL;
    }
    if (!ds->sealed) {
        size_t i;
        for (i = 0; i < ds->count; ++i) {
            if (cf_key_cmp(ds->defs[i].key, strlen(ds->defs[i].key), key, keylen) ==
                0) {
                return &ds->defs[i];
            }
        }
        return NULL;
    }
    lo = 0;
    hi = ds->count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int r = cf_key_cmp(ds->defs[mid].key, strlen(ds->defs[mid].key), key, keylen);
        if (r == 0) {
            return &ds->defs[mid];
        }
        if (r < 0) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return NULL;
}

size_t cf_defset_count(const cf_defset* ds) {
    return (ds != NULL) ? ds->count : 0;
}

const cf_def* cf_defset_at(const cf_defset* ds, size_t i) {
    if (ds == NULL || i >= ds->count) {
        return NULL;
    }
    return &ds->defs[i];
}

/* ------------------------------------------------------------------ *
 * Nearest key
 *
 * Levenshtein distance over two rolling rows rather than a full
 * matrix: O(n*m) time, O(m) memory.
 *
 * The rows hold cf_u16 rather than size_t. Two size_t rows of 257
 * entries are four kilobytes of stack, which broke the 2 KB budget the
 * build enforces - and that budget exists precisely so that a
 * diagnostic helper cannot quietly make the library unusable on a
 * small target. A distance can never exceed the key length, so 16 bits
 * is not a compromise.
 * ------------------------------------------------------------------ */

#define CF_EDIT_MAX (CF_KEY_MAX + 1)

static size_t edit_distance(const char* a, size_t alen, const char* b, size_t blen,
                            size_t cutoff) {
    cf_u16 prev[CF_EDIT_MAX];
    cf_u16 cur[CF_EDIT_MAX];
    size_t i, j;

    if (alen >= CF_EDIT_MAX || blen >= CF_EDIT_MAX) {
        return cutoff + 1;
    }
    /* A length difference alone already exceeds the cutoff. */
    if ((alen > blen ? alen - blen : blen - alen) > cutoff) {
        return cutoff + 1;
    }
    for (j = 0; j <= blen; ++j) {
        prev[j] = (cf_u16)j;
    }
    for (i = 1; i <= alen; ++i) {
        cf_u16 best = (cf_u16)i;
        cur[0]      = (cf_u16)i;
        for (j = 1; j <= blen; ++j) {
            cf_u16 del = (cf_u16)(prev[j] + 1);
            cf_u16 ins = (cf_u16)(cur[j - 1] + 1);
            cf_u16 sub = (cf_u16)(prev[j - 1] + ((a[i - 1] == b[j - 1]) ? 0 : 1));
            cf_u16 v   = del;
            if (ins < v) {
                v = ins;
            }
            if (sub < v) {
                v = sub;
            }
            cur[j] = v;
            if (v < best) {
                best = v;
            }
        }
        /* Every remaining row can only grow, so bail out early. */
        if (best > cutoff) {
            return cutoff + 1;
        }
        for (j = 0; j <= blen; ++j) {
            prev[j] = cur[j];
        }
    }
    return prev[blen];
}

const cf_def* cf_defset_nearest(const cf_defset* ds, const char* key, size_t keylen,
                                size_t* distance) {
    const cf_def* best   = NULL;
    size_t        best_d = 0;
    size_t        cutoff;
    size_t        i;

    if (ds == NULL || key == NULL || keylen == 0) {
        return NULL;
    }
    /* A third of the key, capped. Suggesting something four edits away
       is not a suggestion, it is noise. */
    cutoff = keylen / 3;
    if (cutoff < 1) {
        cutoff = 1;
    }
    if (cutoff > 4) {
        cutoff = 4;
    }
    best_d = cutoff + 1;

    for (i = 0; i < ds->count; ++i) {
        const char* k = ds->defs[i].key;
        size_t      d = edit_distance(key, keylen, k, strlen(k), cutoff);
        if (d < best_d) {
            best_d = d;
            best   = &ds->defs[i];
        }
    }
    if (best != NULL && distance != NULL) {
        *distance = best_d;
    }
    return best;
}
