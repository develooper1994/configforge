/*
 * flatten.c - document events to and from the flat key space. C89.
 *
 * This is the bridge, and it deliberately lives outside the
 * serialization layer: that layer may use the leaf types and may not
 * touch a key space. Keeping the bridge here is what makes the
 * boundary in D-27 real rather than aspirational.
 *
 * Every parser feeds this one flattener, so key construction, array
 * counting and location formatting exist in exactly one place. A
 * parser that also built keys would be a parser that drifts from its
 * siblings, which is the repetition this project exists to remove -
 * applied to its own internals.
 */

#include "configforge/cf_flatten.h"
#include "configforge/cf_define.h"

#include <string.h>

/* ------------------------------------------------------------------ *
 * Decoding: events into the key space
 * ------------------------------------------------------------------ */

/* Whether a name of n bytes can be added to the path: the dot before it,
   when there is a path, and the terminator after it. A key that fills
   the buffer but for its terminator is the longest key, and is read. */
static cf_bool fl_room(const cf_flatten* f, size_t n) {
    return (f->plen + ((f->plen > 0) ? 1u : 0u) + n < sizeof f->path) ? CF_TRUE
                                                                      : CF_FALSE;
}

static cf_bool fl_push(cf_flatten* f, const char* name, size_t n, cf_bool is_array) {
    cf_flatten_frame* fr;

    if (f->depth + 1 >= (int)(sizeof f->stack / sizeof f->stack[0])) {
        f->overflow = CF_TRUE;
        return CF_FALSE;
    }
    if (name != NULL) {
        /* Segment validity is checked here rather than at cf_ks_add so
           that the parser can turn a refusal into a message with a
           line and column in it. */
        if (!cf_key_valid(name, n)) {
            return CF_FALSE;
        }
        if (!fl_room(f, n)) {
            f->overflow = CF_TRUE;
            return CF_FALSE;
        }
    }

    fr            = &f->stack[++f->depth];
    fr->saved_len = (cf_u16)f->plen;
    fr->is_array  = (cf_u8)(is_array ? 1 : 0);
    fr->index     = 0;

    if (name != NULL) {
        if (f->plen > 0) {
            f->path[f->plen++] = '.';
        }
        memcpy(f->path + f->plen, name, n);
        f->plen += n;
    }
    return CF_TRUE;
}

/* Inside an array the position is the name, so the flattener supplies
   it. Every parser would otherwise have to, and they would disagree
   about whether the first element is 0 or 1. */
static cf_bool fl_enter_member(cf_flatten* f, const char* name, size_t n,
                               size_t* saved) {
    *saved = f->plen;

    if (f->depth >= 0 && f->stack[f->depth].is_array) {
        char   idx[CF_I64_DIGITS];
        size_t k;
        if (f->stack[f->depth].index == 0xFFFFFFFFu) {
            f->overflow = CF_TRUE;
            return CF_FALSE;
        }
        k = cf_u64_to_text((cf_u64)f->stack[f->depth].index, idx, sizeof idx);
        ++f->stack[f->depth].index;
        if (!fl_room(f, k)) {
            f->overflow = CF_TRUE;
            return CF_FALSE;
        }
        if (f->plen > 0) {
            f->path[f->plen++] = '.';
        }
        memcpy(f->path + f->plen, idx, k);
        f->plen += k;
        return CF_TRUE;
    }
    if (name == NULL) {
        return CF_FALSE;
    }
    if (!cf_key_valid(name, n)) {
        return CF_FALSE;
    }
    if (!fl_room(f, n)) {
        f->overflow = CF_TRUE;
        return CF_FALSE;
    }
    if (f->plen > 0) {
        f->path[f->plen++] = '.';
    }
    memcpy(f->path + f->plen, name, n);
    f->plen += n;
    return CF_TRUE;
}

static cf_bool fl_begin_table(void* ctx, const char* name, size_t n) {
    cf_flatten* f = (cf_flatten*)ctx;
    size_t      saved;

    if (f->depth >= 0) {
        if (!fl_enter_member(f, name, n, &saved)) {
            return CF_FALSE;
        }
        {
            cf_flatten_frame* fr;
            if (f->depth + 1 >= (int)(sizeof f->stack / sizeof f->stack[0])) {
                f->overflow = CF_TRUE;
                return CF_FALSE;
            }
            fr            = &f->stack[++f->depth];
            fr->saved_len = (cf_u16)saved;
            fr->is_array  = 0;
            fr->index     = 0;
            return CF_TRUE;
        }
    }
    return fl_push(f, name, n, CF_FALSE);
}

static cf_bool fl_begin_array(void* ctx, const char* name, size_t n) {
    cf_flatten*       f = (cf_flatten*)ctx;
    size_t            saved;
    cf_flatten_frame* fr;

    if (f->depth < 0) {
        return CF_FALSE; /* a document is never an array */
    }
    if (!fl_enter_member(f, name, n, &saved)) {
        return CF_FALSE;
    }
    if (f->depth + 1 >= (int)(sizeof f->stack / sizeof f->stack[0])) {
        f->overflow = CF_TRUE;
        return CF_FALSE;
    }
    fr            = &f->stack[++f->depth];
    fr->saved_len = (cf_u16)saved;
    fr->is_array  = 1;
    fr->index     = 0;
    return CF_TRUE;
}

/* D-82: whether this document may still add a key of `keylen` bytes
   holding `textlen` bytes of text. Once refused, every later key is. */
static cf_bool fl_charge(cf_flatten* f, size_t keylen, size_t textlen) {
    size_t cost = keylen + textlen + CF_DECODE_KEY_COST;
    if (f->budget == 0) {
        return CF_TRUE;
    }
    if (f->over_budget || cost < keylen || cost > f->budget - f->spent) {
        f->over_budget = CF_TRUE;
        return CF_FALSE;
    }
    f->spent += cost;
    return CF_TRUE;
}

/* A declared list's text, split at its separator into the elements and
   the count every array is: what fl_begin_array, fl_scalar and fl_end
   write for one, and what the environment's reader writes for the same
   text (entry.c). An empty value is an empty list, as it is there. */
static cf_bool fl_split(cf_flatten* f, const cf_def* d, const cf_value* v,
                        const char* at_text) {
    size_t      base = f->plen, i, start = 0;
    cf_u64      count = 0;
    const char* p     = v->u.t.p;
    size_t      n     = v->u.t.n;
    cf_value    total;

    for (i = 0; n != 0 && i <= n; ++i) {
        size_t klen;
        if (i != n && p[i] != d->list_separator) {
            continue;
        }
        klen = cf_fmt(f->path + base, sizeof f->path - base, ".%U", count);
        if (base + klen >= sizeof f->path) {
            f->overflow = CF_TRUE;
            f->plen     = base;
            return CF_FALSE;
        }
        if (!fl_charge(f, base + klen, i - start)) {
            f->plen = base;
            return CF_FALSE;
        }
        if (!cf_ks_add_text(f->ks, f->path, base + klen, f->rank, p + start, i - start,
                            at_text)) {
            f->failed = CF_TRUE;
            f->plen   = base;
            return CF_FALSE;
        }
        ++count;
        start = i + 1;
    }
    if (base + 7 > sizeof f->path) {
        f->overflow = CF_TRUE;
        f->plen     = base;
        return CF_FALSE;
    }
    memcpy(f->path + base, ".count", 7);
    cf_value_int(&total, (cf_i64)count);
    if (!fl_charge(f, base + 6, 0)) {
        f->path[base] = '\0';
        f->plen       = base;
        return CF_FALSE;
    }
    if (!cf_ks_add(f->ks, f->path, base + 6, f->rank, &total, NULL)) {
        f->failed = CF_TRUE;
    }
    f->path[base] = '\0';
    f->plen       = base;
    return f->failed ? CF_FALSE : CF_TRUE;
}

static cf_bool fl_scalar(void* ctx, const char* name, size_t n, const cf_value* v,
                         cf_bool textual, const cf_loc* at) {
    cf_flatten* f = (cf_flatten*)ctx;
    size_t      saved;
    char        where[CF_KEY_MAX];
    const char* at_text = NULL;
    cf_bool     ok;

    if (f->depth < 0) {
        return CF_FALSE;
    }
    if (!fl_enter_member(f, name, n, &saved)) {
        return CF_FALSE;
    }
    if (at != NULL && at->file != NULL) {
        cf_loc_text(at, where, sizeof where);
        at_text = where;
    }
    if (f->lists != NULL && textual && v->type == CF_TYPE_TEXT &&
        !f->stack[f->depth].is_array) {
        const cf_def* d = cf_defset_find(f->lists, f->path, f->plen);
        if (d != NULL && d->list_mode != CF_LIST_NONE && d->list_separator != 0 &&
            cf_key_cmp(d->key, strlen(d->key), f->path, f->plen) == 0) {
            ok      = fl_split(f, d, v, at_text);
            f->plen = saved;
            return ok;
        }
    }
    if (!fl_charge(f, f->plen, (textual || v->type == CF_TYPE_TEXT) ? v->u.t.n : 0)) {
        f->plen = saved;
        return CF_FALSE;
    }
    if (textual) {
        ok = cf_ks_add_text(f->ks, f->path, f->plen, f->rank, v->u.t.p, v->u.t.n,
                            at_text);
    } else {
        /* Typed, and still written by somebody: a JSON string can
           carry ${ as easily as an environment variable can. */
        ok = cf_ks_add_authored(f->ks, f->path, f->plen, f->rank, v, at_text);
    }
    f->plen = saved;
    if (!ok) {
        f->failed = CF_TRUE;
    }
    return ok;
}

static cf_bool fl_end(void* ctx) {
    cf_flatten*       f = (cf_flatten*)ctx;
    cf_flatten_frame* fr;

    if (f->depth < 0) {
        return CF_FALSE;
    }
    fr = &f->stack[f->depth];

    /* An array's length is a property of the whole array, so it is
       recorded as a key of its own. Without it a high-precedence
       source that shortens an array would leave the extra elements of
       a lower one behind as ghosts - the documented defect this model
       inherits from .NET, and the reason the resolver truncates. */
    if (fr->is_array) {
        char     idx[CF_KEY_MAX];
        cf_value count;
        size_t   plen = f->plen;

        if (plen + 7 <= sizeof idx) {
            memcpy(idx, f->path, plen);
            memcpy(idx + plen, ".count", 7); /* with its terminator: room was checked */
            cf_value_int(&count, (cf_i64)fr->index);
            if (!fl_charge(f, plen + 6, 0)) {
                return CF_FALSE;
            }
            if (!cf_ks_add(f->ks, idx, plen + 6, f->rank, &count, NULL)) {
                f->failed = CF_TRUE;
            }
        } else {
            /* Its elements fit and its length does not. Read without
               it, the array would be one no source could shorten. */
            f->overflow = CF_TRUE;
        }
    }
    f->plen = fr->saved_len;
    --f->depth;
    return CF_TRUE;
}

static char* fl_reserve_text(void* ctx, size_t n) {
    cf_flatten* f = (cf_flatten*)ctx;
    if (n > f->text_cap) {
        size_t cap = f->text_cap == 0 ? 256 : f->text_cap;
        char*  text;
        while (cap < n) {
            if (cap > (size_t)-1 / 2) {
                cap = n;
                break;
            }
            cap *= 2;
        }
        text = (char*)cf_arena_alloc(f->ks->arena, cap);
        if (text == NULL) {
            return NULL;
        }
        f->text     = text;
        f->text_cap = cap;
    }
    return f->text;
}

/* The reader's bookkeeping, from the arena the keys go to: a fixed arena
   bounds it with everything else, and it is released with them. */
static void* fl_reserve_work(void* ctx, size_t n) {
    return cf_arena_alloc(((cf_flatten*)ctx)->ks->arena, n);
}

cf_bool cf_flatten_init(cf_flatten* f, cf_keyspace* ks, int rank) {
    if (f == NULL || ks == NULL) {
        return CF_FALSE;
    }
    memset(f, 0, sizeof *f);
    f->ks    = ks;
    f->rank  = rank;
    f->depth = -1;

    f->sink.size         = sizeof(cf_doc_sink);
    f->sink.begin_table  = fl_begin_table;
    f->sink.begin_array  = fl_begin_array;
    f->sink.scalar       = fl_scalar;
    f->sink.end          = fl_end;
    f->sink.ctx          = f;
    f->sink.reserve_text = fl_reserve_text;
    f->sink.reserve_work = fl_reserve_work;
    return CF_TRUE;
}

const cf_doc_sink* cf_flatten_sink(cf_flatten* f) {
    return (f != NULL) ? &f->sink : NULL;
}

void cf_flatten_lists(cf_flatten* f, const struct cf_defset* ds) {
    if (f != NULL) {
        f->lists = ds;
    }
}

size_t cf_decode_budget(size_t n) {
    if (n > ((size_t)-1 - CF_DECODE_FLOOR) / CF_DECODE_FACTOR) {
        return (size_t)-1;
    }
    return n * CF_DECODE_FACTOR + CF_DECODE_FLOOR;
}

void cf_flatten_budget(cf_flatten* f, size_t bytes) {
    if (f != NULL) {
        f->budget = bytes;
        f->spent  = 0;
    }
}

static const char fl_over_budget[] =
    "'%s' makes more than %U bytes of keys and values from %U bytes of its "
    "own: a document may make %u times its size and 1 MiB more";

cf_bool cf_flatten_decode(cf_flatten* f, const cf_codec* codec, const cf_blob* blob,
                          const char* path, cf_error* err) {
    cf_bool ok;

    if (f->budget == 0) {
        cf_flatten_budget(f, cf_decode_budget(blob->n));
    }
    ok = cf_codec_decode(codec, blob, cf_flatten_sink(f), err);
    if (f->over_budget) {
        return cf_error_set(err, CF_E_SOURCE, NULL, path, fl_over_budget, path,
                            (cf_u64)f->budget, (cf_u64)blob->n,
                            (unsigned)CF_DECODE_FACTOR);
    }
    return ok;
}

#if CF_WITH_EMIT

/* ------------------------------------------------------------------ *
 * Encoding: the key space back into events
 *
 * The keys are sorted, so a single pass reconstructs the structure:
 * two adjacent keys share a prefix exactly as deep as the tables that
 * enclose them both. No tree is built, because the sort already is one.
 * ------------------------------------------------------------------ */

/* Emission is a recursive walk over ranges of the sorted key array
   rather than a linear pass, for one reason that only shows up in
   round trips: at every level the leaves must come out before the
   containers.
   An INI writer that emits a root key after the first [section] puts
   it inside that section, and reading the file back gives a different
   configuration than the one written. That is the classic way a
   hand-written INI writer corrupts its own output, and ordering the
   document properly fixes it for every format at once instead of
   making each codec buffer.

   Among the containers, arrays come before tables, for the same
   reason one level up. Every format that has arrays writes them inline
   - JSON, TOML - so to the format an array is a leaf, and TOML cannot
   go back to a parent once a sub-table's [header] is written: an array
   after it would land in the sub-table. Found by a TOML round trip that
   moved a top-level array into the last table before it.

   Inside an array the rule is the other way round: the members come out
   in index order, leaves and containers as they come, because there
   position is the name. The keys are in byte order, where "10" sorts
   before "2", so an array is walked by the length of the index first -
   digit strings of one length compare as numbers - and nothing is
   allocated for it. The index space itself is never counted up to
   `count`, which a source can set to anything. */

static cf_bool all_digits(const char* p, size_t n) {
    size_t i;
    if (n == 0) {
        return CF_FALSE;
    }
    for (i = 0; i < n; ++i) {
        if (!cf_is_digit((unsigned char)p[i])) {
            return CF_FALSE;
        }
    }
    return CF_TRUE;
}

/* Length of the segment starting at plen, and whether it is the last. */
static size_t seg_len(const cf_entry* e, size_t plen, cf_bool* is_leaf) {
    size_t j = plen;
    while (j < e->keylen && e->key[j] != '.') {
        ++j;
    }
    *is_leaf = (j >= e->keylen) ? CF_TRUE : CF_FALSE;
    return j - plen;
}

/* The length key's value, when it is a whole number that is not negative:
   an integer, or the text of one that nothing has typed yet, which is
   what it is in a keyspace read from a text format and not applied to
   definitions. A string somebody wrote is a string and not a length: as
   an array it would come back an integer. */
static cf_bool count_value(const cf_entry* e, cf_u64* out) {
    const cf_candidate* c = e->selected;
    const cf_value*     v = &c->value;
    cf_value            n;

    if (v->type == CF_TYPE_TEXT && (c->flags & CF_CAND_TEXTUAL) != 0 &&
        cf_value_from_text(CF_TYPE_INT, v->u.t.p, v->u.t.n, &n)) {
        v = &n;
    }
    if (v->type != CF_TYPE_INT || v->u.i < 0) {
        return CF_FALSE;
    }
    *out = (cf_u64)v->u.i;
    return CF_TRUE;
}

/* An index in the form the readers write it: digits, no leading zero,
   and few enough of them to be held. "007" is a name, not a position. */
#define INDEX_DIGITS_MAX 18

static cf_bool index_value(const char* d, size_t n, cf_u64* out) {
    cf_u64 v = 0;
    size_t i;

    if (n == 0 || n > INDEX_DIGITS_MAX || (n > 1 && d[0] == '0')) {
        return CF_FALSE;
    }
    for (i = 0; i < n; ++i) {
        if (!cf_is_digit((unsigned char)d[i])) {
            return CF_FALSE;
        }
        v = v * 10u + (cf_u64)(d[i] - '0');
    }
    *out = v;
    return CF_TRUE;
}

/* An array is a group of index keys and the length key the flattener
   synthesises, and nothing else; and a group that is written as one has
   to be one exactly, or what is written is not what was there. The
   indexes are 0, 1, 2 and so on with none left out, each as the readers
   write it and none of them both a value and a table, and the length is
   a whole number that is how many there are.

   Any other group is a table, with its index keys and its `count` as
   members, and that reads back as the same keys. Written as an array,
   {"count": 2, "0": "a"} was ["a"] and came back with a length of one;
   with an index missing, the ones after it moved up a place; and an
   index that was both a value and a table became two elements. A table
   that merely has a member called `count` - a pool size, a retry count -
   is a table as well, and taken for an array it came out as `[]` with
   everything else in it lost, and INI refused to write it at all.

   An array with no index key is the empty one, which is why a `count` of
   zero is enough to make one and any other count with no element is a
   table's member. Without that case an empty array would silently
   become a table.

   It is counted from what is there, in one pass and without a buffer:
   the keys are in byte order, so in a group of indexes and a length the
   keys of one index are together, each index is counted where it first
   appears, and the largest says whether any was left out. The length is
   never counted up to, which a source can set to anything. [lo, hi) are
   the keys under "<group>." and `glen` is the length of "<group>". */
static cf_bool group_is_array(const cf_keyspace* ks, size_t lo, size_t hi,
                              size_t glen) {
    const cf_entry* count  = NULL;
    const char*     last   = NULL; /* the index counted last */
    size_t          last_n = 0;
    cf_bool         value  = CF_FALSE; /* it has a value of its own */
    cf_bool         table  = CF_FALSE; /* and keys below it */
    cf_u64          have   = 0;        /* indexes counted */
    cf_u64          top    = 0;        /* the largest of them */
    cf_u64          want   = 0;        /* what the length says */
    size_t          i;

    for (i = lo; i < hi; ++i) {
        const cf_entry* e = cf_ks_at(ks, i);
        const char*     seg;
        cf_bool         leaf;
        size_t          n;
        cf_u64          v;

        if (e->selected == NULL) {
            continue;
        }
        seg = e->key + glen + 1;
        n   = seg_len(e, glen + 1, &leaf);
        if (leaf && n == 5 && memcmp(seg, "count", 5) == 0) {
            count = e;
            continue;
        }
        if (!index_value(seg, n, &v)) {
            return CF_FALSE;
        }
        if (last == NULL || n != last_n || memcmp(seg, last, n) != 0) {
            ++have;
            last   = seg;
            last_n = n;
            value  = CF_FALSE;
            table  = CF_FALSE;
        }
        if (leaf) {
            value = CF_TRUE;
        } else {
            table = CF_TRUE;
        }
        if (value && table) {
            return CF_FALSE;
        }
        if (v > top) {
            top = v;
        }
    }
    if (count == NULL || !count_value(count, &want) || want != have) {
        return CF_FALSE;
    }
    return (have == 0 || top + 1 == have) ? CF_TRUE : CF_FALSE;
}

typedef struct emit_ctx {
    cf_emit_pred pred;
    const void*  ctx;
} emit_ctx;

static cf_bool included(const emit_ctx* f, const cf_entry* e) {
    if (e->selected == NULL) {
        return CF_FALSE;
    }
    if (f == NULL || f->pred == NULL) {
        return CF_TRUE;
    }
    return f->pred(f->ctx, e);
}

/* Does anything in this range survive the filter? Asked before a group
   is opened, so an empty table is never written - and, for an array,
   because an array is all or nothing. */
static cf_bool any_included(const cf_keyspace* ks, const emit_ctx* f, size_t lo,
                            size_t hi) {
    size_t i;
    for (i = lo; i < hi; ++i) {
        if (included(f, cf_ks_at(ks, i))) {
            return CF_TRUE;
        }
    }
    return CF_FALSE;
}

/* The end of the run of keys that share, from `i`, the segment of
   length `n` at `plen`. The key at `i` is not a leaf at that level. */
static size_t run_end(const cf_keyspace* ks, size_t i, size_t hi, size_t plen,
                      size_t n) {
    const cf_entry* e = cf_ks_at(ks, i);
    size_t          j = i;

    while (j < hi) {
        const cf_entry* g = cf_ks_at(ks, j);
        cf_bool         gleaf;
        size_t          gn = seg_len(g, plen, &gleaf);
        if (gleaf || gn != n || memcmp(g->key + plen, e->key + plen, n) != 0) {
            break;
        }
        ++j;
    }
    return j;
}

static cf_bool emit_table(const cf_keyspace* ks, const cf_doc_sink* out, size_t lo,
                          size_t hi, size_t plen, int depth, const emit_ctx* f,
                          cf_error* err);
static cf_bool emit_array(const cf_keyspace* ks, const cf_doc_sink* out, size_t lo,
                          size_t hi, size_t plen, int depth, cf_error* err);

static cf_bool too_deep(cf_error* err) {
    return cf_error_set(err, CF_E_KEY, NULL, NULL,
                        "the document nests deeper than %d levels", CF_EMIT_MAX_DEPTH);
}

/* One container: [i, j) are its keys and `n` is the length of its
   segment at `plen`. Inside an array it has no name, because there its
   position is the name: handed "1", the JSON encoder wrote
   ["a", "1": {...}] - a key in the middle of a list.

   `depth` is the level of the container this one sits in, the document
   itself being 0. The readers take the document and CF_MAX_DEPTH - 1
   levels inside it, and it is the walk that holds the writers to that:
   left to each encoder it was whichever of them noticed first, which
   for JSON was one level too late to be read back, and the one that did
   notice refused the container with "sink refused", which says nothing
   about why. */
static cf_bool emit_group(const cf_keyspace* ks, const cf_doc_sink* out, size_t i,
                          size_t j, size_t plen, size_t n, cf_bool is_array,
                          cf_bool in_array, int depth, const emit_ctx* f,
                          cf_error* err) {
    const cf_entry* e    = cf_ks_at(ks, i);
    const char*     name = in_array ? NULL : e->key + plen;
    size_t          nlen = in_array ? 0 : n;

    if (depth + 1 >= CF_EMIT_MAX_DEPTH) {
        return too_deep(err);
    }
    if (!(is_array ? out->begin_array(out->ctx, name, nlen)
                   : out->begin_table(out->ctx, name, nlen))) {
        return cf_error_set(err, CF_E_SOURCE, NULL, NULL, "sink refused '%S'",
                            e->key + plen, n);
    }
    /* An array was asked about as a whole, so what is in it is not
       filtered again. */
    if (!(is_array ? emit_array(ks, out, i, j, plen + n + 1, depth + 1, err)
                   : emit_table(ks, out, i, j, plen + n + 1, depth + 1, f, err))) {
        return CF_FALSE;
    }
    if (!out->end(out->ctx)) {
        return cf_error_set(err, CF_E_SOURCE, NULL, NULL,
                            "sink refused the end of '%S'", e->key + plen, n);
    }
    return CF_TRUE;
}

static cf_bool emit_table(const cf_keyspace* ks, const cf_doc_sink* out, size_t lo,
                          size_t hi, size_t plen, int depth, const emit_ctx* f,
                          cf_error* err) {
    size_t i;
    int    pass;

    /* Leaves first. */
    for (i = lo; i < hi; ++i) {
        const cf_entry* e = cf_ks_at(ks, i);
        cf_bool         leaf;
        size_t          n;

        if (e->selected == NULL) {
            continue;
        }
        n = seg_len(e, plen, &leaf);
        if (!leaf || !included(f, e)) {
            continue;
        }
        if (!out->scalar(out->ctx, e->key + plen, n, &e->selected->value, CF_FALSE,
                         NULL)) {
            return cf_error_set(err, CF_E_SOURCE, NULL, NULL, "sink refused '%s'",
                                e->key);
        }
    }

    /* Then the containers, each a contiguous run because the keys are
       sorted: the arrays in one pass, then the tables. */
    for (pass = 0; pass < 2; ++pass) {
        i = lo;
        while (i < hi) {
            const cf_entry* e = cf_ks_at(ks, i);
            cf_bool         leaf;
            size_t          n = seg_len(e, plen, &leaf);
            size_t          j;
            cf_bool         is_array;

            if (leaf) {
                ++i;
                continue;
            }
            j        = run_end(ks, i, hi, plen, n);
            is_array = group_is_array(ks, i, j, plen + n);
            if ((pass == 0) != (is_array != CF_FALSE) || !any_included(ks, f, i, j)) {
                i = j;
                continue;
            }
            {
                const cf_entry* value = cf_ks_find(ks, e->key, plen + n);
                if (value != NULL && included(f, value)) {
                    return cf_error_set(err, CF_E_KEY, value->key, NULL,
                                        "key '%s' is both a value and a container",
                                        value->key);
                }
            }
            if (!emit_group(ks, out, i, j, plen, n, is_array, CF_FALSE, depth, f,
                            err)) {
                return CF_FALSE;
            }
            i = j;
        }
    }
    return CF_TRUE;
}

/* The members of an array, in index order. Inside an array only the
   index keys are members; the length key lives at the same level and is
   an artifact of the flat model rather than part of the document. */
static cf_bool emit_array(const cf_keyspace* ks, const cf_doc_sink* out, size_t lo,
                          size_t hi, size_t plen, int depth, cf_error* err) {
    size_t done = 0; /* indexes of this many digits are written */

    for (;;) {
        size_t want = 0;
        size_t i;

        /* The next length: the shortest index longer than the last. */
        for (i = lo; i < hi; ++i) {
            const cf_entry* e = cf_ks_at(ks, i);
            cf_bool         leaf;
            size_t          n = seg_len(e, plen, &leaf);

            if (n > done && (want == 0 || n < want) && all_digits(e->key + plen, n)) {
                want = n;
            }
        }
        if (want == 0) {
            return CF_TRUE;
        }

        i = lo;
        while (i < hi) {
            const cf_entry* e = cf_ks_at(ks, i);
            cf_bool         leaf;
            size_t          n = seg_len(e, plen, &leaf);
            size_t          j;

            if (n != want || !all_digits(e->key + plen, n)) {
                ++i;
                continue;
            }
            if (leaf) {
                if (e->selected != NULL &&
                    !out->scalar(out->ctx, NULL, 0, &e->selected->value, CF_FALSE,
                                 NULL)) {
                    return cf_error_set(err, CF_E_SOURCE, NULL, NULL,
                                        "sink refused '%s'", e->key);
                }
                ++i;
                continue;
            }
            j = run_end(ks, i, hi, plen, n);
            if (any_included(ks, NULL, i, j) &&
                !emit_group(ks, out, i, j, plen, n, group_is_array(ks, i, j, plen + n),
                            CF_TRUE, depth, NULL, err)) {
                return CF_FALSE;
            }
            i = j;
        }
        done = want;
    }
}

cf_bool cf_doc_emit(const cf_keyspace* ks, const cf_doc_sink* out, cf_error* err) {
    return cf_doc_emit_where(ks, out, NULL, NULL, err);
}

cf_bool cf_doc_emit_where(const cf_keyspace* ks, const cf_doc_sink* out,
                          cf_emit_pred pred, const void* ctx, cf_error* err) {
    emit_ctx f;

    f.pred = pred;
    f.ctx  = ctx;

    if (ks == NULL || out == NULL) {
        return cf_error_set(err, CF_E_SOURCE, NULL, NULL, "nothing to emit");
    }
    if (!out->begin_table(out->ctx, NULL, 0)) {
        return cf_error_set(err, CF_E_SOURCE, NULL, NULL, "sink refused");
    }
    if (!emit_table(ks, out, 0, cf_ks_count(ks), 0, 0, &f, err)) {
        return CF_FALSE;
    }
    return out->end(out->ctx)
               ? CF_TRUE
               : cf_error_set(err, CF_E_SOURCE, NULL, NULL, "sink refused the end");
}

#else /* !CF_WITH_EMIT */

cf_bool cf_doc_emit(const cf_keyspace* ks, const cf_doc_sink* out, cf_error* err) {
    return cf_doc_emit_where(ks, out, NULL, NULL, err);
}

cf_bool cf_doc_emit_where(const cf_keyspace* ks, const cf_doc_sink* out,
                          cf_emit_pred pred, const void* ctx, cf_error* err) {
    (void)ks;
    (void)out;
    (void)pred;
    (void)ctx;
    return cf_error_set(err, CF_E_CAPABILITY, NULL, NULL,
                        "this build was compiled without CF_WITH_EMIT");
}

#endif /* CF_WITH_EMIT */
