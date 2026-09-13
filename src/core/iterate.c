/*
 * iterate.c - walking a prefix. C89.
 *
 * The questions a program actually asks of a configuration are rarely
 * "what is server.port". They are "what are the elements of this array"
 * and "which plugins are configured", and both were unanswerable
 * without the caller building keys by hand and guessing where to stop.
 *
 * Both are a scan from one binary search, because the key space is
 * sorted. Nothing is allocated: the iterator is the caller's, which
 * matters on the targets where that is the only kind there is.
 */

#include "configforge/configforge.h"

#include <string.h>

static const cf_keyspace* keys_of(const cf_config* c) {
    return cf_keys(c);
}

/* Does `key` lie under `prefix`? An empty prefix is the whole space.
   A prefix matches at a segment boundary only: "server" must not match
   "servername", which is the bug every hand-written prefix check
   has. */
static cf_bool under(const char* key, size_t klen, const char* prefix, size_t plen) {
    if (plen == 0) {
        return CF_TRUE;
    }
    if (klen <= plen) {
        return CF_FALSE;
    }
    if (memcmp(key, prefix, plen) != 0) {
        return CF_FALSE;
    }
    return (key[plen] == '.') ? CF_TRUE : CF_FALSE;
}

static cf_bool iter_start(cf_iter* it, const cf_config* c, const char* prefix,
                          cf_bool children) {
    const cf_keyspace* ks = keys_of(c);

    if (it == NULL) {
        return CF_FALSE;
    }
    memset(it, 0, sizeof *it);
    if (ks == NULL) {
        return CF_FALSE;
    }
    it->cfg      = c;
    it->prefix   = (prefix != NULL) ? prefix : "";
    it->plen     = strlen(it->prefix);
    it->children = children;
    it->pos      = cf_ks_lower_bound(ks, it->prefix, it->plen);
    return CF_TRUE;
}

cf_bool cf_iter_keys(cf_iter* it, const cf_config* c, const char* prefix) {
    return iter_start(it, c, prefix, CF_FALSE);
}

cf_bool cf_iter_children(cf_iter* it, const cf_config* c, const char* prefix) {
    return iter_start(it, c, prefix, CF_TRUE);
}

cf_bool cf_iter_next(cf_iter* it) {
    const cf_keyspace* ks;
    size_t             skip = (it != NULL && it->plen > 0) ? it->plen + 1 : 0;

    if (it == NULL || it->cfg == NULL) {
        return CF_FALSE;
    }
    ks = keys_of(it->cfg);

    while (it->pos < cf_ks_count(ks)) {
        const cf_entry* e = cf_ks_at(ks, it->pos);

        if (!under(e->key, e->keylen, it->prefix, it->plen)) {
            /* Sorted, so the first key outside the prefix ends it. */
            it->pos = cf_ks_count(ks);
            return CF_FALSE;
        }
        ++it->pos;
        if (e->selected == NULL) {
            continue;
        }

        it->key    = e->key;
        it->keylen = e->keylen;

        if (!it->children) {
            it->name    = e->key + skip;
            it->namelen = e->keylen - skip;
            it->value   = &e->selected->value;
            return CF_TRUE;
        }

        /* One entry per distinct immediate child. The child's name runs
           to the next dot; a child with a value of its own reports it,
           a child that is a table reports NULL. */
        {
            const char* name = e->key + skip;
            size_t      n    = 0;
            while (skip + n < e->keylen && name[n] != '.') {
                ++n;
            }
            if (it->name != NULL && it->namelen == n &&
                memcmp(it->name, name, n) == 0) {
                continue; /* same child as last time */
            }
            it->name    = name;
            it->namelen = n;
            it->value   = (skip + n == e->keylen) ? &e->selected->value : NULL;
            return CF_TRUE;
        }
    }
    return CF_FALSE;
}

size_t cf_array_count(const cf_config* c, const char* key) {
    char   probe[CF_KEY_MAX];
    size_t n;
    cf_i64 v;

    if (key == NULL) {
        return 0;
    }
    n = cf_fmt(probe, sizeof probe, "%s.count", key);
    if (n >= sizeof probe) {
        return 0;
    }
    /* The length the flattener recorded, not a count of the index keys
       that happen to be present. A high-precedence source that shortens
       an array leaves the longer one's extra elements behind in the key
       space; counting them would report the union of two arrays, which
       is the defect the length key exists to prevent. */
    v = cf_get_int(c, probe, -1);
    return (v > 0) ? (size_t)v : 0;
}

size_t cf_array_key(char* buf, size_t cap, const char* key, size_t index) {
    return cf_fmt(buf, cap, "%s.%zu", key, index);
}
