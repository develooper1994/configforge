/*
 * define.c - definitions and the definition set. C89.
 */

#include "configforge/cf_define.h"

#include <stdlib.h>
#include <string.h>

/* Do not read an extension from the tail padding of an older caller. */
#define DEF_SIZE_1_0 (offsetof(cf_def, policy) + sizeof(((cf_def*)0)->policy))
#define DEF_SIZE_LIST                                                                  \
    (offsetof(cf_def, list_default_count) + sizeof(((cf_def*)0)->list_default_count))

struct cf_list_projection {
    cf_def                     count;
    cf_def                     item;
    size_t                     keylen;
    struct cf_list_projection* next;
};

void cf_def_init_sized(cf_def* d, size_t size, cf_type type, const char* key) {
    if (d == NULL || size < DEF_SIZE_1_0) {
        if (d != NULL && size >= sizeof d->size) {
            d->size = size; /* refused by cf_builder_define, by name */
        }
        return;
    }
    memset(d, 0, (size < sizeof *d) ? size : sizeof *d);
    d->size       = size;
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
 * The index of an unsealed set
 *
 * Open addressing over FNV-1a of the key's bytes, which is the equality
 * cf_key_cmp has. Positions in `defs` do not move until the set is
 * sealed, so the index stores them; a set that outgrows half its index
 * gets one twice the size, rehashed, from the arena. Without memory for
 * it a set still works, by the linear search it used to do.
 * ------------------------------------------------------------------ */

static size_t index_hash(const char* key, size_t n) {
    cf_u64 h = ((cf_u64)3421674724U << 32) | (cf_u64)2216829733U;
    size_t i;
    for (i = 0; i < n; ++i) {
        h ^= (cf_u64)(unsigned char)key[i];
        h *= ((cf_u64)256U << 32) | (cf_u64)435U;
    }
    return (size_t)(h ^ (h >> 32));
}

static void index_put(size_t* slots, size_t cap, const cf_def* defs, size_t pos) {
    size_t mask = cap - 1;
    size_t at   = index_hash(defs[pos].key, strlen(defs[pos].key)) & mask;
    while (slots[at] != 0) {
        at = (at + 1) & mask;
    }
    slots[at] = pos + 1;
}

/* Room for `need` definitions at under half full, or false. */
static cf_bool index_reserve(cf_defset* ds, size_t need) {
    size_t  cap = (ds->index_cap != 0) ? ds->index_cap : 16;
    size_t* slots;
    size_t  i;

    if (ds->index != NULL && need * 2 <= ds->index_cap) {
        return CF_TRUE;
    }
    while (need * 2 > cap) {
        if (cap > (size_t)-1 / 4 / sizeof *slots) {
            return CF_FALSE;
        }
        cap *= 2;
    }
    slots = (size_t*)cf_arena_calloc(ds->arena, cap * sizeof *slots);
    if (slots == NULL) {
        return CF_FALSE;
    }
    for (i = 0; i < ds->count; ++i) {
        index_put(slots, cap, ds->defs, i);
    }
    ds->index     = slots;
    ds->index_cap = cap;
    return CF_TRUE;
}

static const cf_def* index_find(const cf_defset* ds, const char* key, size_t n) {
    size_t mask = ds->index_cap - 1;
    size_t at   = index_hash(key, n) & mask;
    while (ds->index[at] != 0) {
        const cf_def* d = &ds->defs[ds->index[at] - 1];
        if (cf_key_cmp(d->key, strlen(d->key), key, n) == 0) {
            return d;
        }
        at = (at + 1) & mask;
    }
    return NULL;
}

/* ------------------------------------------------------------------ *
 * Self-consistency
 *
 * A definition that contradicts itself is a programming error, and the
 * right moment to refuse it is where it was written rather than when a
 * value happens to exercise it.
 * ------------------------------------------------------------------ */

static cf_bool def_check_default(const cf_def* d, const cf_value* v, cf_error* err) {
    if (v->type != d->type) {
        return cf_error_set(err, CF_E_DEFINITION, d->key, NULL,
                            "default is %s but the parameter is %s",
                            cf_type_name(v->type), cf_type_name(d->type));
    }
    if (d->has_range && d->type == CF_TYPE_INT &&
        (v->u.i < d->imin || v->u.i > d->imax)) {
        return cf_error_set(err, CF_E_DEFINITION, d->key, NULL,
                            "default %I is outside its own range %I..%I", v->u.i,
                            d->imin, d->imax);
    }
#if CF_WITH_REAL
    if (d->has_range && d->type == CF_TYPE_REAL &&
        (v->u.r < d->rmin || v->u.r > d->rmax)) {
        return cf_error_set(err, CF_E_DEFINITION, d->key, NULL,
                            "default is outside its own range");
    }
#endif
    return CF_TRUE;
}

static cf_bool def_check(const cf_def* d, cf_error* err) {
    size_t klen, i;

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
    if (d->size < DEF_SIZE_1_0) {
        return cf_error_set(err, CF_E_DEFINITION, d->key, NULL,
                            "descriptor was not initialised with cf_def_init");
    }
    if ((d->flags & ~(unsigned)CF_DEF_FLAGS) != 0) {
        return cf_error_set(
            err, CF_E_DEFINITION, d->key, NULL,
            "'%s': flags 0x%x are not definition flags this library "
            "knows; a source flag such as CF_OPTIONAL belongs on the source",
            d->key, d->flags & ~(unsigned)CF_DEF_FLAGS);
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
    if (d->list_mode != CF_LIST_NONE && d->list_mode != CF_LIST_REPLACE &&
        d->list_mode != CF_LIST_APPEND) {
        return cf_error_set(err, CF_E_DEFINITION, d->key, NULL,
                            "unsupported list mode");
    }
    if (d->list_mode == CF_LIST_NONE) {
        if (d->list_separator != 0 || d->list_defaults != NULL ||
            d->list_default_count != 0) {
            return cf_error_set(err, CF_E_DEFINITION, d->key, NULL,
                                "list settings need an explicit list mode");
        }
        return !d->has_default || def_check_default(d, &d->default_value, err);
    }
    if (klen > CF_KEY_MAX - sizeof(".count")) {
        return cf_error_set(err, CF_E_DEFINITION, d->key, NULL,
                            "list key leaves no room for its .count member");
    }
    if (d->default_value.type != CF_TYPE_NULL) {
        return cf_error_set(
            err, CF_E_DEFINITION, d->key, NULL,
            "a list default belongs in list_defaults, not default_value");
    }
    if ((unsigned char)d->list_separator > 127) {
        return cf_error_set(err, CF_E_DEFINITION, d->key, NULL,
                            "list separator must be ASCII");
    }
    if ((!d->has_default && (d->list_defaults != NULL || d->list_default_count != 0)) ||
        (d->list_default_count != 0 && d->list_defaults == NULL)) {
        return cf_error_set(err, CF_E_DEFINITION, d->key, NULL,
                            "list default needs has_default and its element values");
    }
    if (d->list_default_count > (size_t)-1 / sizeof(cf_value) ||
        (cf_u64)d->list_default_count > (cf_u64)CF_I64_MAX) {
        return cf_error_set(err, CF_E_DEFINITION, d->key, NULL,
                            "list default is too large");
    }
    if (d->list_default_count != 0 &&
        cf_fmt(NULL, 0, "%s.%zu", d->key, d->list_default_count - 1) >= CF_KEY_MAX) {
        return cf_error_set(err, CF_E_DEFINITION, d->key, NULL,
                            "list default indices exceed the key limit");
    }
    for (i = 0; i < d->list_default_count; ++i) {
        if (!def_check_default(d, &d->list_defaults[i], err)) {
            return CF_FALSE;
        }
        if (d->type == CF_TYPE_TEXT &&
            (d->list_defaults[i].u.t.n < d->min_len ||
             (d->max_len != 0 && d->list_defaults[i].u.t.n > d->max_len))) {
            return cf_error_set(err, CF_E_DEFINITION, d->key, NULL,
                                "list default element %zu violates its length bounds",
                                i);
        }
        if (d->allowed != NULL) {
            size_t j;
            for (j = 0; d->allowed[j] != NULL; ++j) {
                if (strlen(d->allowed[j]) == d->list_defaults[i].u.t.n &&
                    memcmp(d->allowed[j], d->list_defaults[i].u.t.p,
                           d->list_defaults[i].u.t.n) == 0) {
                    break;
                }
            }
            if (d->allowed[j] == NULL) {
                return cf_error_set(err, CF_E_DEFINITION, d->key, NULL,
                                    "list default element %zu is not permitted", i);
            }
        }
    }
    return CF_TRUE;
}

/* A copy of a NULL-terminated list of strings in the set's arena, so the
   caller's may go as soon as cf_define returns. */
static const char* const* defset_dup_list(cf_arena* ar, const char* const* list,
                                          cf_bool* ok) {
    size_t       n = 0, i;
    const char** out;

    if (list == NULL) {
        return NULL;
    }
    while (list[n] != NULL) {
        ++n;
    }
    out = (const char**)cf_arena_calloc(ar, (n + 1) * sizeof *out);
    if (out == NULL) {
        *ok = CF_FALSE;
        return NULL;
    }
    for (i = 0; i < n; ++i) {
        out[i] = cf_arena_dup(ar, list[i], strlen(list[i]));
        if (out[i] == NULL) {
            *ok = CF_FALSE;
            return NULL;
        }
    }
    return out;
}

static const char* defset_dup(cf_arena* ar, const char* s, cf_bool* ok) {
    char* out;
    if (s == NULL) {
        return NULL;
    }
    out = cf_arena_dup(ar, s, strlen(s));
    if (out == NULL) {
        *ok = CF_FALSE;
    }
    return out;
}

static cf_bool defset_under(const char* key, size_t n, const char* parent, size_t pn) {
    return n > pn && key[pn] == '.' && memcmp(key, parent, pn) == 0;
}

static cf_bool defset_project_list(cf_defset* ds, const cf_def* d, size_t klen) {
    struct cf_list_projection* p;
    char*                      count_key;
    p         = (struct cf_list_projection*)cf_arena_alloc(ds->arena, sizeof *p);
    count_key = (char*)cf_arena_alloc(ds->arena, klen + sizeof(".count"));
    if (p == NULL || count_key == NULL) {
        return CF_FALSE;
    }
    memcpy(count_key, d->key, klen);
    memcpy(count_key + klen, ".count", sizeof(".count"));
    cf_def_init(&p->count, CF_TYPE_INT, count_key);
    p->count.flags       = d->flags;
    p->count.on_invalid  = d->on_invalid;
    p->count.description = d->description;
    p->count.policy      = d->policy;
    p->count.has_range   = CF_TRUE;
    p->count.imin        = 0;
    p->count.imax        = CF_I64_MAX;
    p->count.has_default = d->has_default;
    cf_value_int(&p->count.default_value, (cf_i64)d->list_default_count);
    p->item = *d;
    p->item.flags &= ~(unsigned)CF_REQUIRED;
    p->item.has_default = CF_FALSE;
    cf_value_null(&p->item.default_value);
    p->item.aliases            = NULL;
    p->item.replaced_by        = NULL;
    p->item.list_mode          = CF_LIST_NONE;
    p->item.list_separator     = 0;
    p->item.list_defaults      = NULL;
    p->item.list_default_count = 0;
    p->keylen                  = klen;
    p->next                    = ds->lists;
    ds->lists                  = p;
    return CF_TRUE;
}

cf_bool cf_define(cf_defset* ds, const cf_def* d, cf_error* err) {
    size_t  klen;
    char*   key;
    size_t  i;
    cf_bool copied = CF_TRUE;
    cf_def* kept;
    cf_def  normalized;

    if (ds == NULL || ds->defs == NULL) {
        return cf_error_set(err, CF_E_MEMORY, NULL, NULL,
                            "definition set was not initialised");
    }
    if (ds->sealed) {
        return cf_error_set(err, CF_E_DEFINITION, d ? d->key : NULL, NULL,
                            "the definition set is sealed");
    }
    if (d == NULL || d->size < DEF_SIZE_1_0) {
        return cf_error_set(err, CF_E_DEFINITION, NULL, NULL,
                            "descriptor was not initialised with cf_def_init");
    }
    memset(&normalized, 0, sizeof normalized);
    memcpy(&normalized, d, (d->size < sizeof normalized) ? d->size : sizeof normalized);
    if (d->size < DEF_SIZE_LIST) {
        normalized.list_mode          = CF_LIST_NONE;
        normalized.list_separator     = 0;
        normalized.list_defaults      = NULL;
        normalized.list_default_count = 0;
    }
    d = &normalized;
    if (!def_check(d, err)) {
        return CF_FALSE;
    }
    klen = strlen(d->key);

    if (index_reserve(ds, ds->count + 1)) {
        /* The same three refusals as the scan below, each without one:
           the key itself, a list above it - one look-up a segment - and,
           only for a new list, a key already under it. */
        const cf_def* above;
        if (index_find(ds, d->key, klen) != NULL) {
            return cf_error_set(err, CF_E_DEFINITION, d->key, NULL,
                                "'%s' is defined twice", d->key);
        }
        for (i = 0; i < klen; ++i) {
            if (d->key[i] != '.') {
                continue;
            }
            above = index_find(ds, d->key, i);
            if (above != NULL && above->list_mode != CF_LIST_NONE) {
                return cf_error_set(
                    err, CF_E_DEFINITION, d->key, NULL,
                    "a list owns its count and indexed member definitions");
            }
        }
        for (i = 0; d->list_mode != CF_LIST_NONE && i < ds->count; ++i) {
            if (defset_under(ds->defs[i].key, strlen(ds->defs[i].key), d->key, klen)) {
                return cf_error_set(
                    err, CF_E_DEFINITION, d->key, NULL,
                    "a list owns its count and indexed member definitions");
            }
        }
    } else {
        for (i = 0; i < ds->count; ++i) {
            size_t n = strlen(ds->defs[i].key);
            if (cf_key_cmp(ds->defs[i].key, n, d->key, klen) == 0) {
                return cf_error_set(err, CF_E_DEFINITION, d->key, NULL,
                                    "'%s' is defined twice", d->key);
            }
            if ((d->list_mode != CF_LIST_NONE &&
                 defset_under(ds->defs[i].key, n, d->key, klen)) ||
                (ds->defs[i].list_mode != CF_LIST_NONE &&
                 defset_under(d->key, klen, ds->defs[i].key, n))) {
                return cf_error_set(
                    err, CF_E_DEFINITION, d->key, NULL,
                    "a list owns its count and indexed member definitions");
            }
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

    /* As far as the caller's struct reaches and no further (D-51): a
       1.0 caller's cf_def is smaller than a later library's. */
    kept = &ds->defs[ds->count];
    memset(kept, 0, sizeof *kept);
    *kept      = *d;
    kept->size = sizeof *kept;
    kept->key  = key;

    /* Nothing the caller handed over is borrowed past this call: a
       description, a new name, the lists - each copied, so a buffer the
       caller frees afterwards is never read. */
    kept->description = defset_dup(ds->arena, d->description, &copied);
    kept->replaced_by = defset_dup(ds->arena, d->replaced_by, &copied);
    kept->allowed     = defset_dup_list(ds->arena, d->allowed, &copied);
    kept->aliases     = defset_dup_list(ds->arena, d->aliases, &copied);
    kept->policy      = defset_dup_list(ds->arena, d->policy, &copied);
    if (!copied) {
        return cf_error_set(err, CF_E_MEMORY, d->key, NULL,
                            "no room to copy the definition");
    }

    if (d->list_mode == CF_LIST_NONE && d->has_default && d->type == CF_TYPE_TEXT) {
        char* t =
            cf_arena_dup(ds->arena, d->default_value.u.t.p, d->default_value.u.t.n);
        if (t == NULL) {
            return cf_error_set(err, CF_E_MEMORY, d->key, NULL,
                                "no room to copy the default");
        }
        kept->default_value.u.t.p = t;
    }
    if (d->list_mode != CF_LIST_NONE) {
        if (d->list_default_count != 0) {
            cf_value* values = (cf_value*)cf_arena_alloc(
                ds->arena, d->list_default_count * sizeof *values);
            if (values == NULL) {
                copied = CF_FALSE;
            } else {
                memcpy(values, d->list_defaults,
                       d->list_default_count * sizeof *values);
                for (i = 0; i < d->list_default_count && copied; ++i) {
                    CF_ANALYZER_COUNTED; /* values holds that many, i below it */
                    if (values[i].type == CF_TYPE_TEXT) {
                        values[i].u.t.p =
                            cf_arena_dup(ds->arena, values[i].u.t.p, values[i].u.t.n);
                        if (values[i].u.t.p == NULL) {
                            copied = CF_FALSE;
                        }
                    }
                }
                kept->list_defaults = values;
            }
        } else {
            kept->list_defaults = NULL;
        }
        cf_value_null(&kept->default_value);
        if (!copied || !defset_project_list(ds, kept, klen)) {
            return cf_error_set(err, CF_E_MEMORY, d->key, NULL,
                                "no room to copy the list definition");
        }
    }
    if (ds->index != NULL) {
        index_put(ds->index, ds->index_cap, ds->defs, ds->count);
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

cf_bool cf_defset_open(cf_defset* ds, const char* prefix, cf_error* err) {
    if (ds == NULL || prefix == NULL || prefix[0] == '\0') {
        return cf_error_set(err, CF_E_DEFINITION, NULL, NULL,
                            "an open prefix must name something");
    }
    if (ds->open_count >= CF_MAX_OPEN_PREFIXES) {
        return cf_error_set(err, CF_E_DEFINITION, prefix, NULL,
                            "more than %d open prefixes", CF_MAX_OPEN_PREFIXES);
    }
    {
        cf_bool     ok   = CF_TRUE;
        const char* kept = defset_dup(ds->arena, prefix, &ok);
        if (!ok) {
            return cf_error_set(err, CF_E_MEMORY, prefix, NULL,
                                "no room to copy the prefix");
        }
        ds->open_prefixes[ds->open_count++] = kept;
    }
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

static int def_order(const void* a, const void* b) {
    const cf_def* x = (const cf_def*)a;
    const cf_def* y = (const cf_def*)b;
    return cf_key_cmp(x->key, strlen(x->key), y->key, strlen(y->key));
}

/* Sorted for the binary search lookups use from now on. qsort, not the
   insertion sort this was: "definition counts are in the tens" was the
   reason given for it, and the benchmark showed ten thousand. Keys are
   unique - cf_define refuses a second - so an unstable sort is exact. */
void cf_defset_seal(cf_defset* ds) {
    if (ds == NULL || ds->sealed) {
        return;
    }
    if (ds->count > 1) {
        qsort(ds->defs, ds->count, sizeof *ds->defs, def_order);
    }
    ds->index     = NULL; /* positions moved; the arena keeps the memory */
    ds->index_cap = 0;
    ds->sealed    = CF_TRUE;
}

static const cf_def* defset_exact(const cf_defset* ds, const char* key, size_t keylen) {
    size_t lo, hi;

    if (ds == NULL || ds->count == 0 || key == NULL) {
        return NULL;
    }
    if (!ds->sealed && ds->index != NULL) {
        return index_find(ds, key, keylen);
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

static const struct cf_list_projection*
defset_list_projection(const cf_defset* ds, const char* key, size_t keylen) {
    const struct cf_list_projection* p;
    if (ds == NULL || key == NULL) {
        return NULL;
    }
    for (p = ds->lists; p != NULL; p = p->next) {
        if ((keylen == p->keylen && memcmp(key, p->item.key, keylen) == 0) ||
            defset_under(key, keylen, p->item.key, p->keylen)) {
            return p;
        }
    }
    return NULL;
}

const cf_def* cf_defset_find(const cf_defset* ds, const char* key, size_t keylen) {
    const cf_def*                    d = defset_exact(ds, key, keylen);
    const struct cf_list_projection* p;
    size_t                           i;
    cf_u64                           index = 0;
    if (d != NULL) {
        return d;
    }
    p = defset_list_projection(ds, key, keylen);
    if (p == NULL || keylen <= p->keylen + 1) {
        return NULL;
    }
    i = p->keylen + 1;
    if (keylen - i == 5 && memcmp(key + i, "count", 5) == 0) {
        return &p->count;
    }
    if (key[i] == '0' && keylen - i != 1) {
        return NULL;
    }
    for (; i < keylen; ++i) {
        unsigned digit = (unsigned char)key[i] - (unsigned)'0';
        if (digit > 9 || index > ((cf_u64)CF_I64_MAX - digit) / 10) {
            return NULL;
        }
        index = index * 10 + digit;
    }
    return &p->item;
}

const cf_def* cf_defset_list(const cf_defset* ds, const char* key, size_t keylen) {
    const struct cf_list_projection* p = defset_list_projection(ds, key, keylen);
    return p != NULL ? defset_exact(ds, p->item.key, p->keylen) : NULL;
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

/* ------------------------------------------------------------------ *
 * Fingerprint
 *
 * cf_ks_digest's walk - key, type and canonical value text of every
 * selected entry, in key order, FNV-1a - with one difference: a secret
 * is hashed as the digest a value-blind report shows for it. The
 * fingerprint is meant for logs and traces, which are kept and read
 * more widely than a report; through it, a guess at a secret can be
 * checked no better than through a blind report.
 * ------------------------------------------------------------------ */

static cf_u64 fp_mix(cf_u64 h, const char* p, size_t n) {
    static const cf_u64 prime = ((cf_u64)256U << 32) | (cf_u64)435U;
    size_t              i;
    if (p == NULL) {
        return h; /* an empty text value has no bytes to point at */
    }
    for (i = 0; i < n; ++i) {
        h ^= (cf_u64)(unsigned char)p[i];
        h *= prime;
    }
    return h;
}

cf_u64 cf_fingerprint_seed(void) {
    return ((cf_u64)3421674724U << 32) | (cf_u64)2216829733U;
}

/* One entry's step, which Doctor takes as well: it holds a report's
   keys as text, and the algorithm stays here. */
cf_u64 cf_fingerprint_step(cf_u64 h, const char* key, size_t keylen, const char* type,
                           const char* text, size_t n) {
    h = fp_mix(h, key, keylen);
    h = fp_mix(h, "\037", 1);
    h = fp_mix(h, type, strlen(type));
    h = fp_mix(h, "\037", 1);
    h = fp_mix(h, text, n);
    return fp_mix(h, "\036", 1);
}

cf_u64 cf_defset_fingerprint(const cf_defset* ds, const cf_keyspace* ks) {
    cf_u64 h = cf_fingerprint_seed();
    size_t i;
    char   buf[CF_VALUE_TEXT_MAX > 64 ? CF_VALUE_TEXT_MAX : 64];

    for (i = 0; i < cf_ks_count(ks); ++i) {
        const cf_entry* e = cf_ks_at(ks, i);
        const cf_def*   d;
        const char*     text;
        size_t          n;

        if (e == NULL || e->selected == NULL) {
            continue;
        }
        d = (ds != NULL) ? cf_defset_find(ds, e->key, e->keylen) : NULL;
        if (d != NULL && (d->flags & CF_SENSITIVE) != 0) {
            n    = cf_ks_digest_text(cf_ks_value_digest(&e->selected->value), buf,
                                     sizeof buf);
            text = buf;
        } else if (e->selected->value.type == CF_TYPE_TEXT) {
            text = e->selected->value.u.t.p;
            n    = e->selected->value.u.t.n;
        } else {
            n    = cf_value_to_text(&e->selected->value, buf, sizeof buf);
            text = buf;
        }
        h = cf_fingerprint_step(h, e->key, e->keylen,
                                cf_type_name(e->selected->value.type), text, n);
    }
    return h;
}
