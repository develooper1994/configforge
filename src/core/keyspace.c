/*
 * keyspace.c - the flat key space and the provenance chain. C89.
 */

#include "configforge/cf_keyspace.h"

#include <string.h>

/* ================================================================== *
 * Keys
 * ================================================================== */

cf_bool cf_key_valid(const char* key, size_t len) {
    size_t i;
    int    seg = 0;

    if (key == NULL || len == 0 || len > CF_KEY_MAX) {
        return CF_FALSE;
    }
    if (key[0] == '.' || key[len - 1] == '.') {
        return CF_FALSE;
    }
    for (i = 0; i < len; ++i) {
        char c = key[i];
        if (c == '.') {
            if (seg == 0) {
                return CF_FALSE; /* empty segment */
            }
            seg = 0;
            continue;
        }
        /* Canonical form is lowercase ASCII. Uppercase is rejected
           rather than folded, because folding is where two distinct
           keys quietly become one. */
        if (!cf_is_lower((unsigned char)c) && !cf_is_digit((unsigned char)c) &&
            c != '_' && c != '-') {
            return CF_FALSE;
        }
        ++seg;
    }
    return (seg > 0) ? CF_TRUE : CF_FALSE;
}

size_t cf_key_segments(const char* key, size_t len) {
    size_t i, n = 1;

    if (!cf_key_valid(key, len)) {
        return 0;
    }
    for (i = 0; i < len; ++i) {
        if (key[i] == '.') {
            ++n;
        }
    }
    return n;
}

int cf_key_cmp(const char* a, size_t alen, const char* b, size_t blen) {
    return cf_cmp_bytes(a, alen, b, blen);
}

const char* cf_outcome_name(cf_outcome o) {
    switch (o) {
#define CF_OUTCOME_X(sym, name, wins)                                                  \
    case sym:                                                                          \
        return name;
#include "configforge/cf_outcome.def"
#undef CF_OUTCOME_X
    }
    return "?";
}

cf_bool cf_outcome_can_win(cf_outcome o) {
    switch (o) {
#define CF_OUTCOME_X(sym, name, wins)                                                  \
    case sym:                                                                          \
        return wins ? CF_TRUE : CF_FALSE;
#include "configforge/cf_outcome.def"
#undef CF_OUTCOME_X
    }
    return CF_FALSE;
}

/* ================================================================== *
 * Key space
 * ================================================================== */

struct cf_cand_rec {
    const char*  key;
    size_t       keylen;
    size_t       seq; /* insertion order, part of the sort key */
    cf_candidate cand;
};

cf_bool cf_ks_init(cf_keyspace* ks, cf_arena* arena, size_t hint) {
    if (ks == NULL || arena == NULL) {
        return CF_FALSE;
    }
    if (hint < 16) {
        hint = 16;
    }
    ks->arena    = arena;
    ks->ncand    = 0;
    ks->ccand    = hint;
    ks->entries  = NULL;
    ks->count    = 0;
    ks->resolved = CF_FALSE;
    ks->cands    = (cf_cand_rec*)cf_arena_alloc(arena, hint * sizeof(cf_cand_rec));
    return (ks->cands != NULL) ? CF_TRUE : CF_FALSE;
}

cf_arena* cf_ks_arena(const cf_keyspace* ks) {
    return (ks != NULL) ? ks->arena : NULL;
}

/* Doubling. The abandoned block stays in the arena, which costs at
   most the same again and buys a single release for the whole space. */
static cf_bool cf_ks_grow(cf_keyspace* ks) {
    size_t       ncap = ks->ccand * 2;
    cf_cand_rec* nc =
        (cf_cand_rec*)cf_arena_alloc(ks->arena, ncap * sizeof(cf_cand_rec));
    if (nc == NULL) {
        return CF_FALSE;
    }
    memcpy(nc, ks->cands, ks->ncand * sizeof(cf_cand_rec));
    ks->cands = nc;
    ks->ccand = ncap;
    return CF_TRUE;
}

static cf_bool cf_ks_push(cf_keyspace* ks, const char* key, size_t keylen, int source,
                          const cf_value* value, const char* at, cf_outcome outcome,
                          const char* reason, unsigned flags) {
    cf_cand_rec* r;
    char*        k;

    /* cands is NULL when init could not allocate, which happens in a
       fixed arena that is simply too small. A caller that ignored the
       init result must not be rewarded with a crash. */
    if (ks == NULL || value == NULL || ks->resolved || ks->cands == NULL) {
        return CF_FALSE;
    }
    if (!cf_key_valid(key, keylen)) {
        return CF_FALSE;
    }
    if (ks->ncand == ks->ccand && !cf_ks_grow(ks)) {
        return CF_FALSE;
    }
    k = cf_arena_dup(ks->arena, key, keylen);
    if (k == NULL) {
        return CF_FALSE;
    }

    r         = &ks->cands[ks->ncand];
    r->key    = k;
    r->keylen = keylen;
    r->seq    = ks->ncand;

    r->cand.source  = source;
    r->cand.flags   = flags;
    r->cand.value   = *value;
    r->cand.at      = NULL;
    r->cand.outcome = outcome;
    r->cand.reason  = reason;
    r->cand.next    = NULL;

    /* Text is copied so the caller's buffer need not outlive the call. */
    if (value->type == CF_TYPE_TEXT) {
        char* p = cf_arena_dup(ks->arena, value->u.t.p, value->u.t.n);
        if (p == NULL) {
            return CF_FALSE;
        }
        r->cand.value.u.t.p = p;
    }
    if (at != NULL) {
        r->cand.at = cf_arena_dup(ks->arena, at, strlen(at));
        if (r->cand.at == NULL) {
            return CF_FALSE;
        }
    }
    ++ks->ncand;
    return CF_TRUE;
}

cf_bool cf_ks_add(cf_keyspace* ks, const char* key, size_t keylen, int source,
                  const cf_value* value, const char* at) {
    return cf_ks_push(ks, key, keylen, source, value, at, CF_OUT_PENDING, NULL, 0u);
}

cf_bool cf_ks_add_text(cf_keyspace* ks, const char* key, size_t keylen, int source,
                       const char* text, size_t textlen, const char* at) {
    cf_value v;

    if (text == NULL) {
        return CF_FALSE;
    }
    cf_value_text(&v, text, textlen);
    return cf_ks_push(ks, key, keylen, source, &v, at, CF_OUT_PENDING, NULL,
                      CF_CAND_TEXTUAL);
}

cf_bool cf_ks_reject(cf_keyspace* ks, const char* key, size_t keylen, int source,
                     const cf_value* value, const char* at, const char* reason) {
    return cf_ks_push(ks, key, keylen, source, value, at, CF_OUT_REJECTED, reason, 0u);
}

/* ------------------------------------------------------------------ *
 * Sorting
 *
 * Bottom-up merge sort over an array of pointers.
 *
 * Merge sort rather than quicksort because the worst case is
 * O(n log n) rather than O(n squared), and a configuration file is
 * exactly the kind of input that arrives already sorted or reversed.
 * Bottom-up rather than recursive because there is no stack depth to
 * bound. Pointers rather than records because the scratch array is
 * then one word per candidate instead of a full record.
 *
 * The sequence number is part of the comparison, so the order is
 * total and the sort does not have to be stable to be reproducible.
 * ------------------------------------------------------------------ */

static int cand_cmp(const cf_cand_rec* a, const cf_cand_rec* b) {
    int r = cf_cmp_bytes(a->key, a->keylen, b->key, b->keylen);

    if (r != 0) {
        return r;
    }
    if (a->cand.source != b->cand.source) {
        return (a->cand.source < b->cand.source) ? -1 : 1;
    }
    /* Descending, so that within one source the *later* occurrence wins.
       That is the convention everywhere a source can repeat a key: a
       command line where --port=1 --port=2 gives 2, an INI file where
       the second assignment wins, a drop-in directory where 90-site
       overrides 10-base. Ascending looked natural and made a response
       file beat the arguments typed after it. */
    if (a->seq != b->seq) {
        return (a->seq > b->seq) ? -1 : 1;
    }
    return 0;
}

static void cand_merge(cf_cand_rec** src, cf_cand_rec** dst, size_t lo, size_t mid,
                       size_t hi) {
    size_t i = lo, j = mid, k = lo;

    while (i < mid && j < hi) {
        if (cand_cmp(src[i], src[j]) <= 0) {
            dst[k++] = src[i++];
        } else {
            dst[k++] = src[j++];
        }
    }
    while (i < mid) {
        dst[k++] = src[i++];
    }
    while (j < hi) {
        dst[k++] = src[j++];
    }
}

static cf_cand_rec** cand_sort(cf_arena* ar, cf_cand_rec* recs, size_t n) {
    cf_cand_rec** a;
    cf_cand_rec** b;
    size_t        width, i;

    a = (cf_cand_rec**)cf_arena_alloc(ar, n * sizeof(cf_cand_rec*));
    b = (cf_cand_rec**)cf_arena_alloc(ar, n * sizeof(cf_cand_rec*));
    if (a == NULL || b == NULL) {
        return NULL;
    }
    for (i = 0; i < n; ++i) {
        a[i] = &recs[i];
    }
    for (width = 1; width < n; width *= 2) {
        cf_cand_rec** tmp;
        for (i = 0; i < n; i += 2 * width) {
            size_t mid = (i + width < n) ? i + width : n;
            size_t hi  = (i + 2 * width < n) ? i + 2 * width : n;
            cand_merge(a, b, i, mid, hi);
        }
        tmp = a;
        a   = b;
        b   = tmp;
    }
    return a;
}

/* ------------------------------------------------------------------ *
 * Resolution
 * ------------------------------------------------------------------ */

void cf_ks_resolve(cf_keyspace* ks) {
    cf_cand_rec** sorted;
    size_t        i, distinct;

    if (ks == NULL || ks->resolved) {
        return;
    }
    ks->resolved = CF_TRUE;
    if (ks->ncand == 0) {
        ks->entries = NULL;
        ks->count   = 0;
        return;
    }

    sorted = cand_sort(ks->arena, ks->cands, ks->ncand);
    if (sorted == NULL) {
        ks->count = 0;
        return;
    }

    distinct = 1;
    for (i = 1; i < ks->ncand; ++i) {
        if (cf_cmp_bytes(sorted[i - 1]->key, sorted[i - 1]->keylen, sorted[i]->key,
                         sorted[i]->keylen) != 0) {
            ++distinct;
        }
    }

    /* Allocated at exactly the size needed, because the count is known
       before anything is written. */
    ks->entries = (cf_entry*)cf_arena_alloc(ks->arena, distinct * sizeof(cf_entry));
    if (ks->entries == NULL) {
        ks->count = 0;
        return;
    }
    ks->count = distinct;

    {
        size_t e = 0;
        i        = 0;
        while (i < ks->ncand) {
            size_t        j      = i;
            cf_candidate* head   = NULL;
            cf_candidate* tail   = NULL;
            cf_candidate* chosen = NULL;

            while (j < ks->ncand &&
                   cf_cmp_bytes(sorted[i]->key, sorted[i]->keylen, sorted[j]->key,
                                sorted[j]->keylen) == 0) {
                cf_candidate* c = &sorted[j]->cand;
                c->next         = NULL;
                if (head == NULL) {
                    head = c;
                } else {
                    tail->next = c;
                }
                tail = c;

                if (cf_outcome_can_win(c->outcome)) {
                    if (chosen == NULL) {
                        c->outcome = CF_OUT_SELECTED;
                        chosen     = c;
                    } else {
                        c->outcome = CF_OUT_SHADOWED;
                    }
                }
                ++j;
            }
            ks->entries[e].key      = sorted[i]->key;
            ks->entries[e].keylen   = sorted[i]->keylen;
            ks->entries[e].chain    = head;
            ks->entries[e].selected = chosen;
            ++e;
            i = j;
        }
    }
}

void cf_ks_infer_untyped(cf_keyspace* ks) {
    size_t i;

    if (ks == NULL || !ks->resolved) {
        return;
    }
    for (i = 0; i < ks->count; ++i) {
        cf_candidate* c;
        for (c = ks->entries[i].chain; c != NULL; c = c->next) {
            cf_value v;
            if ((c->flags & CF_CAND_TEXTUAL) == 0) {
                continue;
            }
            if (cf_value_infer(c->value.u.t.p, c->value.u.t.n, &v)) {
                c->value = v;
                c->flags &= ~(unsigned)CF_CAND_TEXTUAL;
            }
        }
    }
}

cf_entry* cf_ks_at_mut(cf_keyspace* ks, size_t i) {
    if (ks == NULL || !ks->resolved || i >= ks->count) {
        return NULL;
    }
    return &ks->entries[i];
}

void cf_ks_reselect(cf_keyspace* ks) {
    size_t i;

    if (ks == NULL || !ks->resolved) {
        return;
    }
    for (i = 0; i < ks->count; ++i) {
        cf_entry*     e = &ks->entries[i];
        cf_candidate* c;
        cf_candidate* chosen = NULL;

        for (c = e->chain; c != NULL; c = c->next) {
            if (!cf_outcome_can_win(c->outcome)) {
                continue;
            }
            if (chosen == NULL) {
                c->outcome = CF_OUT_SELECTED;
                chosen     = c;
            } else {
                c->outcome = CF_OUT_SHADOWED;
            }
        }
        e->selected = chosen;
    }
}

cf_bool cf_ks_is_sorted(const cf_keyspace* ks) {
    size_t i;

    if (ks == NULL || !ks->resolved) {
        return CF_TRUE;
    }
    for (i = 1; i < ks->count; ++i) {
        if (cf_key_cmp(ks->entries[i - 1].key, ks->entries[i - 1].keylen,
                       ks->entries[i].key, ks->entries[i].keylen) >= 0) {
            return CF_FALSE;
        }
    }
    return CF_TRUE;
}

size_t cf_ks_lower_bound(const cf_keyspace* ks, const char* key, size_t keylen) {
    size_t lo = 0;
    size_t hi;

    if (ks == NULL || key == NULL) {
        return 0;
    }
    hi = ks->count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (cf_cmp_bytes(ks->entries[mid].key, ks->entries[mid].keylen, key, keylen) <
            0) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo;
}

const cf_entry* cf_ks_find(const cf_keyspace* ks, const char* key, size_t keylen) {
    size_t lo, hi;

    if (ks == NULL || !ks->resolved || ks->count == 0 || key == NULL) {
        return NULL;
    }
    lo = 0;
    hi = ks->count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int r = cf_key_cmp(ks->entries[mid].key, ks->entries[mid].keylen, key, keylen);
        if (r == 0) {
            return &ks->entries[mid];
        }
        if (r < 0) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return NULL;
}

const cf_value* cf_ks_get(const cf_keyspace* ks, const char* key, size_t keylen) {
    const cf_entry* e = cf_ks_find(ks, key, keylen);
    if (e == NULL || e->selected == NULL) {
        return NULL;
    }
    return &e->selected->value;
}

size_t cf_ks_count(const cf_keyspace* ks) {
    return (ks != NULL && ks->resolved) ? ks->count : 0;
}

const cf_entry* cf_ks_at(const cf_keyspace* ks, size_t i) {
    if (ks == NULL || !ks->resolved || i >= ks->count) {
        return NULL;
    }
    return &ks->entries[i];
}

/* ------------------------------------------------------------------ *
 * Digest
 *
 * FNV-1a over key, type and canonical value text of every selected
 * entry, in sorted key order. Built from the canonical text rather
 * than from the raw bytes so that the digest cannot depend on padding,
 * pointer values or endianness.
 * ------------------------------------------------------------------ */

static cf_u64 fnv1a(cf_u64 h, const char* p, size_t n) {
    static const cf_u64 prime = (cf_u64)1099511628211U;
    size_t              i;

    for (i = 0; i < n; ++i) {
        h ^= (cf_u64)(unsigned char)p[i];
        h *= prime;
    }
    return h;
}

cf_u64 cf_ks_digest(const cf_keyspace* ks) {
    /* 14695981039346656037 written as an expression: a 64-bit literal
       would need the LL suffix, which -std=c89 -pedantic rejects. */
    cf_u64 h = ((cf_u64)3421674724U << 32) | (cf_u64)2216829733U;
    size_t i;
    char   buf[CF_KEY_MAX > 64 ? CF_KEY_MAX : 64];

    if (ks == NULL) {
        return h;
    }
    for (i = 0; i < ks->count; ++i) {
        const cf_entry* e = &ks->entries[i];
        const char*     tn;
        size_t          n;

        if (e->selected == NULL) {
            continue;
        }
        h = fnv1a(h, e->key, e->keylen);
        h = fnv1a(h, "\037", 1);

        tn = cf_type_name(e->selected->value.type);
        h  = fnv1a(h, tn, strlen(tn));
        h  = fnv1a(h, "\037", 1);

        if (e->selected->value.type == CF_TYPE_TEXT) {
            h = fnv1a(h, e->selected->value.u.t.p, e->selected->value.u.t.n);
        } else {
            n = cf_value_to_text(&e->selected->value, buf, sizeof buf);
            h = fnv1a(h, buf, n);
        }
        h = fnv1a(h, "\036", 1);
    }
    return h;
}
