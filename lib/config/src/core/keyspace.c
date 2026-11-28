/*
 * keyspace.c - the flat key space and the provenance chain. C89.
 */

#include "configforge/cf_keyspace.h"
#include "configforge/cf_alloc.h"
#include "configforge/cf_build_options.h"
#include "configforge/cf_error.h"
#include "configforge/cf_port.h"
#include "configforge/cf_value.h"

#include <string.h>

/* ================================================================== *
 * Keys
 * ================================================================== */

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

/* One row per kind, in the enum's order: the name the report writes and
   the wording a sensitive key's reason takes. */
static const struct {
    const char* name;
    const char* text;
} refusal_rows[] = {{"", ""},
                    {"type", "wrong type"},
                    {"range", "out of range"},
                    {"allowed", "not one of the permitted values"},
                    {"too-short", "too short"},
                    {"too-long", "too long"},
                    {"reference", "a malformed reference"},
                    {"no-value", "a reference names a key with no value"}};

const char* cf_refusal_name(cf_refusal k) {
    return ((unsigned)k < sizeof refusal_rows / sizeof refusal_rows[0])
               ? refusal_rows[k].name
               : "";
}

const char* cf_refusal_text(cf_refusal k) {
    return ((unsigned)k < sizeof refusal_rows / sizeof refusal_rows[0])
               ? refusal_rows[k].text
               : "";
}

cf_bool cf_outcome_can_win(cf_outcome o) {
    switch (o) {
#define CF_OUTCOME_X(sym, name, wins)                                                  \
    case sym:                                                                          \
        return (wins) ? CF_TRUE : CF_FALSE;
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
    if (ks == NULL) {
        return CF_FALSE;
    }
    /* Every member defined before anything can fail: a key space whose
       init failed is an empty one that refuses work, never one whose
       fields are whatever the stack held (Valgrind found a refusal that
       read `resolved` uninitialised). */
    memset(ks, 0, sizeof *ks);
    if (arena == NULL) {
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

size_t cfi_ks_key_limit(const cf_keyspace* ks) {
    return (ks != NULL && ks->key_limit != 0) ? ks->key_limit : (size_t)CF_KEY_MAX - 1u;
}

void cfi_ks_limits(cf_keyspace* ks, size_t key_limit, size_t text_limit) {
    if (ks != NULL) {
        ks->key_limit  = key_limit;
        ks->text_limit = text_limit;
    }
}

void cfi_ks_refuse(cf_keyspace* ks, const cf_error* err) {
    cf_error* copy;

    if (ks == NULL || err == NULL || err->code != CF_E_SOURCE || ks->refusal != NULL ||
        ks->arena == NULL) {
        return;
    }
    copy = (cf_error*)cf_arena_alloc(ks->arena, sizeof(cf_error));
    if (copy == NULL) {
        return;
    }
    memset(copy, 0, sizeof *copy);
    copy->size = sizeof(cf_error);
    cf_error_copy(copy, err);
    ks->refusal = copy;
}

const cf_error* cfi_ks_refusal(const cf_keyspace* ks) {
    return (ks != NULL) ? ks->refusal : NULL;
}

cf_arena* cf_ks_arena(const cf_keyspace* ks) {
    return (ks != NULL) ? ks->arena : NULL;
}

/* The array moved to room for `ncap`. The abandoned block stays in the
   arena, which costs at most the same again and buys a single release
   for the whole space. */
static cf_bool cf_ks_resize(cf_keyspace* ks, size_t ncap) {
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

/* Doubling. */
static cf_bool cf_ks_grow(cf_keyspace* ks) {
    return cf_ks_resize(ks, ks->ccand * 2);
}

/* The most a guess may ask for at once: 2^20 candidates, a few tens of
   megabytes. Beyond it, doubling. */
#define CF_KS_EXPECT_MAX ((size_t)1 << 20)

void cfi_ks_expect(cf_keyspace* ks, size_t more) {
    size_t want;

    if (ks == NULL || ks->cands == NULL || ks->resolved || ks->arena == NULL ||
        cf_arena_is_fixed(ks->arena) || more == 0) {
        return;
    }
    if (more > CF_KS_EXPECT_MAX) {
        more = CF_KS_EXPECT_MAX;
    }
    want = ks->ncand + more;
    if (want > CF_KS_EXPECT_MAX) {
        want = CF_KS_EXPECT_MAX;
    }
    if (want <= ks->ccand) {
        return;
    }
    (void)cf_ks_resize(ks, want); /* refused: the doubling still works */
}

/* About a key a line: the lines that start like a key - not blank, not
   a comment (# ; ! /), not a section header or a brace or bracket on
   its own - are the guess. A count of bytes divided by a typical line
   (32) was tried first and was off by a factor of 5 either way for short
   or long values; every line, comments too, reserved 350 KB for a
   200 KB file holding one key. Minified text is one line and gets the
   doubling it always had. A line count alone is fooled by one multi-line
   string, whose every line starts like a key: a million short lines in
   2 MB reserved 2^20 records, some 90 MB, for one key. No record comes
   from fewer bytes than `k=v` and its line break, so the guess is held
   to one record per CF_KS_MIN_KEY_BYTES of input; a file denser than
   that only doubles a little at the end. */
#define CF_KS_MIN_KEY_BYTES 4

void cfi_ks_expect_lines(cf_keyspace* ks, const char* text, size_t n) {
    size_t  keys = 0;
    size_t  i;
    cf_bool start = CF_TRUE;

    if (ks == NULL || text == NULL || n == 0) {
        return;
    }
    for (i = 0; i < n; ++i) {
        char c = text[i];
        if (c == '\n') {
            start = CF_TRUE;
        } else if (start && c != ' ' && c != '\t' && c != '\r') {
            start = CF_FALSE;
            if (c != '#' && c != ';' && c != '!' && c != '/' && c != '[' && c != ']' &&
                c != '{' && c != '}') {
                ++keys;
            }
        }
    }
    if (keys > n / CF_KS_MIN_KEY_BYTES) {
        keys = n / CF_KS_MIN_KEY_BYTES;
    }
    cfi_ks_expect(ks, keys);
}

static cf_bool cf_ks_push(cf_keyspace* ks, const char* key, size_t keylen, int source,
                          const cf_value* value, const char* at, cf_outcome outcome,
                          const char* reason, unsigned flags) {
    cf_cand_rec* r;
    const char*  k;

    /* cands is NULL when init could not allocate, which happens in a
       fixed arena that is simply too small. A caller that ignored the
       init result must not be rewarded with a crash. */
    if (ks == NULL || value == NULL || ks->resolved || ks->cands == NULL) {
        return CF_FALSE;
    }
    if (!cfi_key_valid_n(key, keylen, cfi_ks_key_limit(ks))) {
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
    r->cand.written = NULL;
    r->cand.next    = NULL;

    /* Text is copied so the caller's buffer need not outlive the call. */
    if (value->type == CF_TYPE_TEXT) {
        const char* p = cf_arena_dup(ks->arena, value->u.t.p, value->u.t.n);
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
                      CF_CAND_TEXTUAL | CF_CAND_AUTHORED);
}

cf_bool cf_ks_add_authored(cf_keyspace* ks, const char* key, size_t keylen, int source,
                           const cf_value* value, const char* at) {
    return cf_ks_push(ks, key, keylen, source, value, at, CF_OUT_PENDING, NULL,
                      CF_CAND_AUTHORED);
}

cf_bool cf_ks_add_default(cf_keyspace* ks, const char* key, size_t keylen, int source,
                          const cf_value* value) {
    return cf_ks_push(ks, key, keylen, source, value, NULL, CF_OUT_PENDING, NULL,
                      CF_CAND_DEFAULT);
}

static cf_bool cf_ks_remember_original(cf_keyspace* ks, size_t index, const char* key) {
    if (index >= ks->original_capacity) {
        const char** originals;
        if (ks->ccand > (size_t)-1 / sizeof *originals) {
            return CF_FALSE;
        }
        originals =
            (const char**)cf_arena_calloc(ks->arena, ks->ccand * sizeof *originals);
        if (originals == NULL) {
            return CF_FALSE;
        }
        if (ks->original_capacity != 0) {
            memcpy((void*)originals, (const void*)ks->original_keys,
                   ks->original_capacity * sizeof *originals);
        }
        ks->original_keys     = originals;
        ks->original_capacity = ks->ccand;
    }
    if (ks->original_keys[index] == NULL) {
        ks->original_keys[index] = key;
    }
    return CF_TRUE;
}

static cf_bool cf_ks_rename_impl(cf_keyspace* ks, const char* from, size_t fromlen,
                                 const char* to, size_t tolen, cf_bool members) {
    size_t i, n;

    if (ks == NULL || ks->resolved || ks->cands == NULL ||
        !cfi_key_valid_n(to, tolen, cfi_ks_key_limit(ks))) {
        return CF_FALSE;
    }
    n = ks->ncand; /* what is there now: the copies are not copied again */
    for (i = 0; i < n; ++i) {
        cf_candidate c;
        size_t       len;
        char*        at;
        char         stack[CF_KEY_MAX];
        char*        target       = stack;
        const char*  original     = ks->cands[i].key;
        size_t       original_len = ks->cands[i].keylen;
        size_t       target_len;

        if (original_len < fromlen || memcmp(original, from, fromlen) != 0 ||
            (original_len != fromlen && (!members || original[fromlen] != '.')) ||
            (ks->cands[i].cand.flags & CF_CAND_RENAMED) != 0) {
            continue;
        }
        target_len = tolen + (original_len - fromlen);
        if (target_len > cfi_ks_key_limit(ks)) {
            return CF_FALSE;
        }
        if (target_len >= sizeof stack) {
            /* Only where the mode lets a key outgrow the build's buffer. */
            target = (char*)cf_arena_alloc(ks->arena, target_len + 1);
            if (target == NULL) {
                return CF_FALSE;
            }
        }
        cf_fmt(target, target_len + 1, "%S%S", to, tolen, original + fromlen,
               original_len - fromlen);
        c   = ks->cands[i].cand; /* a copy: pushing may move the array */
        len = cf_fmt(NULL, 0, "%s%s(as %S)", (c.at != NULL) ? c.at : "",
                     (c.at != NULL) ? " " : "", original, original_len);
        at  = (char*)cf_arena_alloc(ks->arena, len + 1);
        if (at == NULL) {
            return CF_FALSE;
        }
        cf_fmt(at, len + 1, "%s%s(as %S)", (c.at != NULL) ? c.at : "",
               (c.at != NULL) ? " " : "", original, original_len);
        if (!cf_ks_push(ks, target, target_len, c.source, &c.value, at, c.outcome,
                        c.reason, c.flags | CF_CAND_RENAMED)) {
            return CF_FALSE;
        }
        if (members && !cf_ks_remember_original(ks, ks->ncand - 1, original)) {
            return CF_FALSE;
        }
        ks->cands[ks->ncand - 1].cand.written = c.written;
    }
    return CF_TRUE;
}

cf_bool cf_ks_rename(cf_keyspace* ks, const char* from, size_t fromlen, const char* to,
                     size_t tolen) {
    return cf_ks_rename_impl(ks, from, fromlen, to, tolen, CF_FALSE);
}

cf_bool cf_ks_rename_list(cf_keyspace* ks, const char* from, size_t fromlen,
                          const char* to, size_t tolen) {
    return cf_ks_rename_impl(ks, from, fromlen, to, tolen, CF_TRUE);
}

cf_bool cf_ks_add_candidate(cf_keyspace* ks, const char* key, size_t keylen,
                            const cf_candidate* c) {
    const char* written = NULL;

    if (c == NULL || ks == NULL) {
        return CF_FALSE;
    }
    /* The text first: the candidate is added only once everything it
       carries has room, so a refusal leaves the key space as it was. */
    if (c->written != NULL) {
        written = cf_arena_dup(ks->arena, c->written, strlen(c->written));
        if (written == NULL) {
            return CF_FALSE;
        }
    }
    if (!cf_ks_push(ks, key, keylen, c->source, &c->value, c->at, CF_OUT_PENDING, NULL,
                    c->flags & (CF_CAND_TEXTUAL | CF_CAND_DEFAULT | CF_CAND_RESOLVED |
                                CF_CAND_PINNED))) {
        return CF_FALSE;
    }
    ks->cands[ks->ncand - 1].cand.written = written;
    return CF_TRUE;
}

cf_bool cf_ks_add_resolved(cf_keyspace* ks, const char* key, size_t keylen, int source,
                           const cf_value* value, const char* at) {
    return cf_ks_push(ks, key, keylen, source, value, at, CF_OUT_PENDING, NULL,
                      CF_CAND_RESOLVED);
}

cf_bool cfi_ks_add_pinned(cf_keyspace* ks, const char* key, size_t keylen, int source,
                          const cf_value* value) {
    return cf_ks_push(ks, key, keylen, source, value, NULL, CF_OUT_PENDING, NULL,
                      CF_CAND_RESOLVED | CF_CAND_PINNED);
}

const char* cf_ks_original_key(const cf_keyspace* ks, const cf_candidate* c) {
    const char*        first;
    const char*        at;
    size_t             off;
    const cf_cand_rec* r;
    if (ks == NULL || c == NULL || ks->original_keys == NULL || ks->cands == NULL ||
        ks->ncand == 0) {
        return NULL;
    }
    /* c is the member of one of this space's records: found as an
       element of the array - its distance from the first record's member
       a whole number of records, and inside the array - rather than
       computed back from the member, so a candidate of another space is
       refused instead of read through. In constant time: report.c asks
       for every candidate. */
    first = (const char*)&ks->cands[0].cand;
    at    = (const char*)c;
    if (at < first) {
        return NULL;
    }
    off = (size_t)(at - first);
    if (off % sizeof(cf_cand_rec) != 0 || off / sizeof(cf_cand_rec) >= ks->ncand) {
        return NULL;
    }
    r = &ks->cands[off / sizeof(cf_cand_rec)];
    return r->seq < ks->original_capacity ? ks->original_keys[r->seq] : NULL;
}

cf_bool cf_ks_replay(cf_keyspace* ks, const cf_keyspace* base) {
    size_t i;
    if (ks == NULL || base == NULL || !base->resolved) {
        return CF_FALSE;
    }
    for (i = 0; i < base->ncand; ++i) {
        const cf_cand_rec* r        = &base->cands[i];
        const char*        original = cf_ks_original_key(base, &r->cand);
        const char*        key      = original != NULL ? original : r->key;
        if ((r->cand.flags & (CF_CAND_RENAMED | CF_CAND_LIST_RESULT)) != 0) {
            continue;
        }
        if (!cf_ks_add_candidate(ks, key, strlen(key), &r->cand)) {
            return CF_FALSE;
        }
        /* A losing template was never expanded. Only resolved values
           are frozen; a template that becomes selected must still run. */
        if ((r->cand.flags & CF_CAND_TEMPLATE) != 0) {
            ks->cands[ks->ncand - 1].cand.flags |= CF_CAND_AUTHORED | CF_CAND_TEMPLATE;
        }
    }
    return CF_TRUE;
}

cf_bool cf_ks_append_list(cf_keyspace* ks, const char* key, size_t keylen,
                          const cf_list_part* parts, size_t count,
                          const cf_candidate* result, cf_error* err) {
    size_t i, j;
    char   stack[CF_KEY_MAX];
    char*  renamed = stack;
    size_t room    = sizeof stack;
    size_t n;
    /* ".", an index of up to twenty digits, or ".count": where the mode
       lets a key outgrow the build's buffer, the room for it. */
    if (keylen + 22u >= room && cfi_ks_key_limit(ks) >= room) {
        renamed = (char*)cf_arena_alloc(ks->arena, keylen + 23u);
        if (renamed == NULL) {
            return cf_error_set(err, CF_E_MEMORY, key, NULL,
                                "no room for appended list keys");
        }
        room = keylen + 23u;
    }
    for (i = 0; i < ks->ncand; ++i) {
        cf_cand_rec* r = &ks->cands[i];
        cf_i64       index;
        cf_value     number;
        if (r->keylen <= keylen + 1 || r->key[keylen] != '.' ||
            memcmp(r->key, key, keylen) != 0 ||
            !cf_is_digit((unsigned char)r->key[keylen + 1])) {
            continue;
        }
        for (j = 0; j < count && parts[j].source != r->cand.source; ++j) {
        }
        if (j == count) {
            continue; /* no local count: an explicit final-index patch */
        }
        if (!cf_value_from_text(CF_TYPE_INT, r->key + keylen + 1,
                                r->keylen - keylen - 1, &number)) {
            return cf_error_set(err, CF_E_KEY, r->key, r->cand.at,
                                "list index is not a nonnegative integer");
        }
        index = number.u.i;
        if (!parts[j].included || index >= parts[j].length) {
            if (cf_outcome_can_win(r->cand.outcome)) {
                r->cand.outcome = CF_OUT_TRUNCATED;
                r->cand.reason =
                    "outside the source-local list or below a resolved snapshot";
            }
        }
        if (parts[j].included && index < parts[j].length && parts[j].offset != 0) {
            if (index > CF_I64_MAX - parts[j].offset) {
                return cf_error_set(err, CF_E_KEY, r->key, r->cand.at,
                                    "appended list index overflows");
            }
            n = cf_fmt(renamed, room, "%S.%I", key, keylen, index + parts[j].offset);
            if (n >= room || n > cfi_ks_key_limit(ks)) {
                return cf_error_set(err, CF_E_KEY, r->key, r->cand.at,
                                    "appended list index exceeds the key limit");
            }
            if (!cf_ks_remember_original(ks, i, r->key)) {
                return cf_error_set(err, CF_E_MEMORY, key, NULL,
                                    "no room for original list keys");
            }
            r->key = cf_arena_dup(ks->arena, renamed, n);
            if (r->key == NULL) {
                return cf_error_set(err, CF_E_MEMORY, key, NULL,
                                    "no room for appended list keys");
            }
            r->keylen = n;
        }
    }
    ks->resolved = CF_FALSE;
    if (result == NULL) {
        return CF_TRUE;
    }
    n = cf_fmt(renamed, room, "%S.count", key, keylen);
    if (n >= room || n > cfi_ks_key_limit(ks) ||
        !cf_ks_push(ks, renamed, n, result->source, &result->value, result->at,
                    CF_OUT_PENDING,
                    "appended permitted source lists, lowest precedence first",
                    (result->flags & (CF_CAND_DEFAULT | CF_CAND_PINNED)) |
                        CF_CAND_LIST_RESULT)) {
        return cf_error_set(err, CF_E_MEMORY, key, NULL,
                            "no room for appended list count");
    }
    return CF_TRUE;
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
    if (((a->cand.flags ^ b->cand.flags) & CF_CAND_PINNED) != 0) {
        return (a->cand.flags & CF_CAND_PINNED) != 0 ? -1 : 1;
    }
    if (a->cand.source != b->cand.source) {
        return (a->cand.source < b->cand.source) ? -1 : 1;
    }
    /* Within one source a value written under a key's old name loses to
       one written under the new, wherever each appears. */
    if (((a->cand.flags ^ b->cand.flags) & CF_CAND_RENAMED) != 0) {
        return ((a->cand.flags & CF_CAND_RENAMED) != 0) ? 1 : -1;
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

size_t cf_ks_candidate_count(const cf_keyspace* ks) {
    return (ks != NULL) ? ks->ncand : 0;
}

cf_bool cf_ks_resolve(cf_keyspace* ks) {
    cf_cand_rec** sorted;
    size_t        i, distinct;

    if (ks == NULL || ks->resolved) {
        return (ks != NULL) ? CF_TRUE : CF_FALSE;
    }
    ks->resolved = CF_TRUE;
    if (ks->ncand == 0) {
        ks->entries = NULL;
        ks->count   = 0;
        return CF_TRUE;
    }

    sorted = cand_sort(ks->arena, ks->cands, ks->ncand);
    if (sorted == NULL) {
        /* Reported rather than swallowed. This used to set the count to
           zero and return void, so a caller out of arena got a
           configuration with no keys in it and no way to find out - a
           device would have booted with every value at its default and
           nothing to say why. A function that can fail does not return
           void. */
        ks->count = 0;
        return CF_FALSE;
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
        return CF_FALSE;
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
    return CF_TRUE;
}

/* public signature (and it writes through ks) */
/* cppcheck-suppress constParameterPointer */
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
    /* 1099511628211, as an expression for the reason the basis below is:
       on a 32-bit long the literal would be a long long, which C89 has
       not (wasm32 found it). */
    static const cf_u64 prime = ((cf_u64)256U << 32) | (cf_u64)435U;
    size_t              i;

    for (i = 0; i < n; ++i) {
        h ^= (cf_u64)(unsigned char)p[i];
        h *= prime;
    }
    return h;
}

/* The canonical bytes of a value, as every digest of it takes them: a
   text as it is, anything else as its canonical text in `tmp`. */
static const char* ks_value_bytes(const cf_value* v, char* tmp, size_t cap, size_t* n) {
    if (v == NULL) {
        *n = 0;
        return "";
    }
    *n = (v->type == CF_TYPE_TEXT) ? v->u.t.n : cf_value_to_text(v, tmp, cap);
    return (v->type == CF_TYPE_TEXT) ? v->u.t.p : tmp;
}

cf_u64 cf_ks_value_digest(const cf_value* v) {
    char        tmp[CF_VALUE_TEXT_MAX];
    size_t      n;
    const char* bytes = ks_value_bytes(v, tmp, sizeof tmp, &n);

    return fnv1a(((cf_u64)3421674724U << 32) | (cf_u64)2216829733U, bytes, n);
}

/* Sixteen hex digits written by hand: the library's formatter has no
   width specifiers on purpose (report.c says how "%08x" once went). */
size_t cf_ks_digest_text(cf_u64 h, char* buf, size_t cap) {
    static const char hex[] = "0123456789abcdef";
    char              t[22] = "fnv1a:"; /* the rest zero, then the digits */
    int               k;

    for (k = 21; k >= 6; --k) {
        t[k] = hex[(size_t)(h & (cf_u64)0xF)];
        h >>= 4;
    }
    if (buf != NULL && cap > 0) {
        size_t n = (sizeof t < cap - 1) ? sizeof t : cap - 1;
        memcpy(buf, t, n);
        buf[n] = '\0';
    }
    return sizeof t;
}

/* SipHash-2-4 (Aumasson and Bernstein, 2012), written from the paper:
   the key as two little-endian 64-bit words, the state initialised
   from "somepseudorandomlygeneratedbytes", two rounds a message word,
   the last word carrying the length in its top byte, then four rounds.
   Every 64-bit constant is two 32-bit halves, as fnv1a's are. */
#define SIP_ROTL(x, b) (((x) << (b)) | ((x) >> (64 - (b))))

typedef struct sip_state {
    cf_u64 v0, v1, v2, v3;
} sip_state;

static void sip_round(sip_state* s) {
    s->v0 += s->v1;
    s->v1 = SIP_ROTL(s->v1, 13);
    s->v1 ^= s->v0;
    s->v0 = SIP_ROTL(s->v0, 32);
    s->v2 += s->v3;
    s->v3 = SIP_ROTL(s->v3, 16);
    s->v3 ^= s->v2;
    s->v0 += s->v3;
    s->v3 = SIP_ROTL(s->v3, 21);
    s->v3 ^= s->v0;
    s->v2 += s->v1;
    s->v1 = SIP_ROTL(s->v1, 17);
    s->v1 ^= s->v2;
    s->v2 = SIP_ROTL(s->v2, 32);
}

/* Eight bytes, little-endian, from `p` (at most `n` of them are read;
   the rest of the word is zero). */
static cf_u64 sip_word(const unsigned char* p, size_t n) {
    cf_u64 w = 0;
    size_t i;

    if (n > 8) {
        n = 8;
    }
    for (i = 0; i < n; ++i) {
        w |= (cf_u64)p[i] << (8 * i);
    }
    return w;
}

cf_u64 cf_siphash24(const unsigned char* key, const void* p, size_t n) {
    const unsigned char* in = (const unsigned char*)p;
    sip_state            s;
    cf_u64               k0 = sip_word(key, 8);
    cf_u64               k1 = sip_word(key + 8, 8);
    cf_u64               m;
    size_t               left = n;

    s.v0 = k0 ^ (((cf_u64)0x736f6d65UL << 32) | (cf_u64)0x70736575UL);
    s.v1 = k1 ^ (((cf_u64)0x646f7261UL << 32) | (cf_u64)0x6e646f6dUL);
    s.v2 = k0 ^ (((cf_u64)0x6c796765UL << 32) | (cf_u64)0x6e657261UL);
    s.v3 = k1 ^ (((cf_u64)0x74656462UL << 32) | (cf_u64)0x79746573UL);

    while (left >= 8) {
        m = sip_word(in, 8);
        s.v3 ^= m;
        sip_round(&s);
        sip_round(&s);
        s.v0 ^= m;
        in += 8;
        left -= 8;
    }
    m = sip_word(in, left) | ((cf_u64)(n & 0xffU) << 56);
    s.v3 ^= m;
    sip_round(&s);
    sip_round(&s);
    s.v0 ^= m;

    s.v2 ^= (cf_u64)0xffU;
    sip_round(&s);
    sip_round(&s);
    sip_round(&s);
    sip_round(&s);
    return s.v0 ^ s.v1 ^ s.v2 ^ s.v3;
}

/* `digits` hex digits of `h`, the last of them from the low nibble, into
   `out`; the cursor is the caller's. */
static void ks_hex_digits(char* out, int digits, cf_u64 h) {
    static const char hex[] = "0123456789abcdef";
    int               k;

    for (k = digits - 1; k >= 0; --k) {
        out[k] = hex[(size_t)(h & (cf_u64)0xF)];
        h >>= 4;
    }
}

size_t cf_ks_secret_digest_text(const cf_value* v, const unsigned char* key, char* buf,
                                size_t cap) {
    static const char id_text[] = "configforge digest key id"; /* 25 bytes */
    char              t[31]     = "sip24:";
    char              tmp[CF_VALUE_TEXT_MAX];
    const char*       bytes;
    size_t            n;

    if (key == NULL) {
        return cf_ks_digest_text(cf_ks_value_digest(v), buf, cap);
    }
    bytes = ks_value_bytes(v, tmp, sizeof tmp, &n);
    ks_hex_digits(t + 6, 8, cf_siphash24(key, id_text, sizeof id_text - 1) >> 32);
    t[14] = ':';
    ks_hex_digits(t + 15, 16, cf_siphash24(key, bytes, n));
    if (buf != NULL && cap > 0) {
        size_t len = (sizeof t < cap - 1) ? sizeof t : cap - 1;
        memcpy(buf, t, len);
        buf[len] = '\0';
    }
    return sizeof t;
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
            size_t n = cf_value_to_text(&e->selected->value, buf, sizeof buf);
            h        = fnv1a(h, buf, n);
        }
        h = fnv1a(h, "\036", 1);
    }
    return h;
}
