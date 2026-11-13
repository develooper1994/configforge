/*
 * yaml.c - YAML, read through the built-in parser, libfyaml or libyaml,
 * and written by hand. C89, but for the libfyaml half, whose header needs
 * C11.
 *
 * Reading is a parser's events mapped onto the document events, one to
 * one: a mapping is a table, a sequence an array, a scalar a scalar.
 * Three parsers can supply them (D-62, D-69), and only the last part of
 * the reader knows which: the built-in one (yamlparse.c), which reads all
 * of yaml-test-suite as YAML 1.2 says and needs nothing; libfyaml, which
 * does too; and libyaml, which follows YAML 1.1. Writing is a few dozen
 * lines of block style here, because
 * every value this library holds can be written in a handful of forms
 * and a parser's emitter would bring an allocation and a second state
 * machine for them.
 *
 * Anchors and aliases are read (D-63): an anchored node's events are
 * recorded in the sink's workspace and an alias replays them, each value
 * keeping the place it was written, so explain() names the one line it
 * came from; an alias expands to a bounded multiple of the document, in
 * events and in bytes of text, so a "billion laughs" is refused in linear
 * time and memory. The core schema's tags
 * (!!str, !!int, !!float, !!bool, !!null, !!map, !!seq) are honoured.
 * What a configuration has no place for is refused by name:
 *
 *   another tag (!custom) a type of the writing application's
 *   several documents     one file is one configuration
 *   a key that is not a   a configuration key is text
 *   scalar
 *   a top level that is   a configuration is a mapping
 *   not a mapping
 *
 * The decision that matters is about types (D-39). A plain scalar is
 * handed over as text, to be typed by its definition, exactly as an INI
 * value is; a quoted one is text, and says so. YAML 1.1 reads
 * "country: no" as false and "version: 3.10" as 3.1, and which of 1.1
 * and 1.2 a reader follows decides it - the problem every YAML
 * configuration has met. Here the program's contract decides, and the
 * library's one spelling of a boolean (D-24, "true" and "false" only)
 * means "no" is the text "no" under either version and under a
 * definition too. Without a contract, Tier 0 inference applies, as it
 * does to INI, and "3.10" written plain is the number it looks like:
 * quote what is meant as text. The one plain
 * scalar given a type of its own is null - "~", "null", or nothing -
 * which is a value here, as JSON's null is.
 */

#include "configforge/cf_serialize.h"

/* Which parser: CF_YAML_LIBFYAML, CF_YAML_LIBYAML, or the built-in one. */
#include "configforge/cf_yamlparse.h"

#include <string.h>

#if CF_WITH_YAML

/* Which parser reads (D-62, D-69): libfyaml or libyaml when the build
   chose one, the built-in one otherwise. */
#if CF_YAML_LIBFYAML
#include <libfyaml.h>
#elif CF_YAML_LIBYAML
#include <yaml.h>
#endif

/* ================================================================== *
 * Decoding
 * ================================================================== */

/* The node a mapping, a sequence or a scalar is, and how many levels are
   open around it. */
typedef struct yaml_level {
    unsigned char is_map;   /* a mapping, or else a sequence           */
    unsigned char want_key; /* in a mapping: the next scalar is a key  */
} yaml_level;

/* One event as this reader needs it, from libyaml or from a recording
   being replayed for an alias. Text points at libyaml's buffer or at the
   recording's pool, and lives until the event is handled. */
enum { YAML_EV_MAP, YAML_EV_SEQ, YAML_EV_SCALAR, YAML_EV_END };

typedef struct yaml_ev {
    int         kind;
    const char* text; /* a scalar's value */
    size_t      n;
    const char* tag; /* resolved: NULL, "!" or a full tag */
    cf_bool     plain;
    unsigned    line, col; /* 1-based */
} yaml_ev;

/* A recorded event: what an alias replays. */
typedef struct yaml_rec {
    cf_u32 text, n;   /* in the pool */
    cf_u32 tag, ntag; /* in the pool; ntag 0: none */
    cf_u32 line, col;
    cf_u8  kind;
    cf_u8  plain;
} yaml_rec;

/* An anchor: its name in the pool, and the events its node recorded. */
typedef struct yaml_anchor {
    cf_u32 name, nlen;
    cf_u32 first, count;
    cf_u32 chain; /* the next anchor in the same bucket, + 1 */
    int    depth; /* the depth its node opened at */
    cf_u8  done;
} yaml_anchor;

/* An alias expands to at most this many events per event read, with a
   floor, so a document of aliases of aliases ("a billion laughs") is
   refused in linear time rather than read in exponential. */
#define YAML_ALIAS_FACTOR 64u
#define YAML_ALIAS_FLOOR 4096u

/* And to at most this many bytes of scalar text per byte of the document,
   with a floor. Counting events alone lets one large scalar aliased a few
   thousand times copy gigabytes into the configuration from a megabyte of
   input; every replayed scalar is copied, so its bytes are what memory
   follows. 64 times matches the event rule and leaves room for what
   anchors are for: an anchor of a few kilobytes of shared defaults
   reused dozens of times stays well under it, and the 1 MiB floor lets a
   small document reuse a block freely. A megabyte of input can then
   become at most about 65 MiB of text, not 4 GiB. */
#define YAML_ALIAS_BYTES_FACTOR 64u
#define YAML_ALIAS_BYTES_FLOOR ((size_t)1024u * 1024u)

typedef struct yaml_scan {
    const char*        file;
    const cf_doc_sink* out;
    cf_error*          err;
    yaml_level         level[CF_MAX_DEPTH];
    int                depth; /* -1 before the top-level mapping */
    char               key[CF_KEY_MAX];
    size_t             klen;
    int                documents;

    /* Anchors, when the sink lends workspace (cf_doc_sink.reserve_work):
       events are recorded while an anchored node is open, and an alias
       replays its anchor's. */
    cf_bool       work;
    yaml_rec*     recs;
    size_t        nrecs, crecs;
    char*         pool;
    size_t        npool, cpool;
    yaml_anchor*  anchors;
    size_t        nanchors, canchors;
    cf_u32*       buckets;
    size_t        nbuckets;
    cf_u32        seed; /* what every hash of an anchor name starts from */
    int           open[CF_MAX_DEPTH + 1]; /* anchors whose node is open */
    int           nopen;
    unsigned long read, replayed;
    size_t        bytes, max_bytes; /* scalar text replayed, and its bound */
} yaml_scan;

static cf_bool yaml_fail_at(yaml_scan* s, unsigned line, unsigned col,
                            const char* what) {
    return cf_error_set(s->err, CF_E_SOURCE, NULL, s->file, "%s:%u:%u: %s", s->file,
                        line, col, what);
}

static cf_bool yaml_no_room(yaml_scan* s) {
    return cf_error_set(s->err, CF_E_MEMORY, NULL, s->file,
                        "no room to record what a YAML anchor names");
}

/* A larger block for one of the recording's arrays, the old one copied.
   What was outgrown stays where the sink put it until decode returns. */
static void* yaml_grow(yaml_scan* s, void* old, size_t used, size_t* cap, size_t size,
                       size_t need) {
    size_t c = (*cap == 0) ? 64u : *cap;
    void*  p;
    while (c < need) {
        if (c > ((size_t)-1 / size) / 2u) {
            return NULL;
        }
        c *= 2u;
    }
    p = s->out->reserve_work(s->out->ctx, c * size);
    if (p != NULL && used > 0) {
        memcpy(p, old, used * size);
    }
    if (p != NULL) {
        *cap = c;
    }
    return p;
}

static cf_bool yaml_keep(yaml_scan* s, const char* p, size_t n, cf_u32* at) {
    char* pool;
    if (s->npool + n > s->cpool) {
        pool = (char*)yaml_grow(s, s->pool, s->npool, &s->cpool, 1, s->npool + n);
        if (pool == NULL || s->npool + n > (cf_u32)-1) {
            return yaml_no_room(s);
        }
        s->pool = pool;
    }
    if (n > 0 && p != NULL) {
        memcpy(s->pool + s->npool, p, n);
    }
    *at = (cf_u32)s->npool;
    s->npool += n;
    return CF_TRUE;
}

/* The event, recorded while an anchored node is open. */
static cf_bool yaml_record(yaml_scan* s, const yaml_ev* e) {
    yaml_rec* r;
    if (s->nopen == 0) {
        return CF_TRUE;
    }
    if (s->nrecs + 1 > s->crecs) {
        yaml_rec* recs = (yaml_rec*)yaml_grow(s, s->recs, s->nrecs, &s->crecs,
                                              sizeof *recs, s->nrecs + 1);
        if (recs == NULL || s->nrecs + 1 > (cf_u32)-1) {
            return yaml_no_room(s);
        }
        s->recs = recs;
    }
    r        = &s->recs[s->nrecs];
    r->kind  = (cf_u8)e->kind;
    r->plain = (cf_u8)(e->plain ? 1 : 0);
    r->line  = e->line;
    r->col   = e->col;
    r->n     = (cf_u32)e->n;
    r->ntag  = (e->tag != NULL) ? (cf_u32)strlen(e->tag) : 0u;
    if (!yaml_keep(s, e->text, e->n, &r->text) ||
        !yaml_keep(s, e->tag, r->ntag, &r->tag)) {
        return CF_FALSE;
    }
    ++s->nrecs;
    return CF_TRUE;
}

/* The seed every hash of a name in this decode starts from, so that a
   document's author cannot choose names that all land in one bucket: the
   hash was FNV-1a from its fixed offset basis, which anyone can compute
   against. The library has no clock and no random source, so the seed is
   made of two addresses, the scan's and a local's, which differ from run to
   run where the system randomises addresses and which a document cannot
   know. Where it does not (no address randomisation), the seed is the
   same every run: no worse than the unseeded hash was, and no better.
   Nothing observable may depend on it: no table is walked in bucket
   order. The high bits of an address are the random ones and a bucket
   index is the low bits of a hash, so they are folded down first. The
   two addresses are often both on the stack, sharing their random high
   bits, so the second is multiplied before the XOR lest they cancel. */
static size_t yaml_seed(const void* where) {
    char   here;
    size_t x = (size_t)where ^ ((size_t)&here * 2654435761u);

    x ^= x >> 12;
    x ^= x >> 24;
    x *= 16777619u;
    x = (size_t)(cf_u32)(x ^ (x >> 16 >> 16));
    return (x == 0) ? (size_t)2166136261u : x;
}

static cf_u32 yaml_hash(cf_u32 seed, const char* p, size_t n) {
    cf_u32 h = seed;
    size_t i;
    for (i = 0; i < n; ++i) {
        h = (h ^ (cf_u32)(unsigned char)p[i]) * 16777619u;
    }
    return h;
}

/* The latest anchor of that name, + 1; 0 when there is none. YAML lets a
   name be anchored again, and an alias means the one before it. */
static cf_u32 yaml_find(const yaml_scan* s, const char* p, size_t n) {
    cf_u32 i;
    if (s->nbuckets == 0) {
        return 0;
    }
    i = s->buckets[yaml_hash(s->seed, p, n) & (s->nbuckets - 1u)];
    while (i != 0) {
        const yaml_anchor* a = &s->anchors[i - 1u];
        if ((size_t)a->nlen == n && memcmp(s->pool + a->name, p, n) == 0) {
            return i;
        }
        i = a->chain;
    }
    return 0;
}

/* An anchor on the node about to open: from here its events are kept. */
static cf_bool yaml_anchor_open(yaml_scan* s, unsigned line, unsigned col,
                                const char* name, size_t n) {
    size_t       i;
    yaml_anchor* a;
    cf_u32       h;

    if (!s->work) {
        return yaml_fail_at(
            s, line, col,
            "an anchor needs reader workspace, which this sink does not "
            "lend (cf_doc_sink.reserve_work)");
    }
    if (s->nopen > CF_MAX_DEPTH) {
        return yaml_fail_at(s, line, col, "nesting is too deep");
    }
    /* A name anchored again takes over its entry: an alias means the
       latest definition, so the older one can never be found again, and
       an entry per definition would make every lookup in that bucket walk
       them all. An entry whose node is still open is kept - its recording
       is not finished - and the new one goes in front of it; there are no
       more of those than levels of nesting. */
    i = yaml_find(s, name, n);
    if (i != 0 && s->anchors[i - 1u].done) {
        a                 = &s->anchors[i - 1u];
        a->first          = (cf_u32)s->nrecs;
        a->count          = 0;
        a->done           = 0;
        a->depth          = s->depth + 1;
        s->open[s->nopen] = (int)(i - 1u);
        ++s->nopen;
        return CF_TRUE;
    }
    if (s->nanchors + 1 > s->canchors) {
        size_t       old     = s->canchors;
        yaml_anchor* anchors = (yaml_anchor*)yaml_grow(
            s, s->anchors, s->nanchors, &s->canchors, sizeof *anchors, s->nanchors + 1);
        cf_u32* buckets;
        if (anchors == NULL) {
            return yaml_no_room(s);
        }
        s->anchors = anchors;
        if (s->canchors != old) {
            /* Rehashed into twice as many buckets as anchors, newest first
               in each chain, so a name anchored again is found first. */
            size_t nb = s->canchors * 2u;
            buckets = (cf_u32*)s->out->reserve_work(s->out->ctx, nb * sizeof *buckets);
            if (buckets == NULL) {
                return yaml_no_room(s);
            }
            memset(buckets, 0, nb * sizeof *buckets);
            for (i = 0; i < s->nanchors; ++i) {
                yaml_anchor* x = &s->anchors[i];
                cf_u32       b =
                    yaml_hash(s->seed, s->pool + x->name, x->nlen) & (cf_u32)(nb - 1u);
                x->chain   = buckets[b];
                buckets[b] = (cf_u32)(i + 1u);
            }
            s->buckets  = buckets;
            s->nbuckets = nb;
        }
    }
    a = &s->anchors[s->nanchors];
    if (!yaml_keep(s, name, n, &a->name)) {
        return CF_FALSE;
    }
    a->nlen           = (cf_u32)n;
    a->first          = (cf_u32)s->nrecs;
    a->count          = 0;
    a->done           = 0;
    a->depth          = s->depth + 1;
    h                 = yaml_hash(s->seed, name, n) & (cf_u32)(s->nbuckets - 1u);
    a->chain          = s->buckets[h];
    s->buckets[h]     = (cf_u32)(s->nanchors + 1u);
    s->open[s->nopen] = (int)s->nanchors;
    ++s->nopen;
    ++s->nanchors;
    return CF_TRUE;
}

/* After an event: each anchored node that just ended has all its events. */
static void yaml_anchor_close(yaml_scan* s) {
    while (s->nopen > 0) {
        yaml_anchor* a = &s->anchors[s->open[s->nopen - 1]];
        if (s->depth >= a->depth) {
            return;
        }
        a->count = (cf_u32)(s->nrecs - a->first);
        a->done  = 1;
        --s->nopen;
    }
}

/* The name a node at the current position goes by: the pending key in
   a mapping, nothing in a sequence. */
static cf_bool yaml_name(yaml_scan* s, const yaml_ev* e, const char** name, size_t* n) {
    yaml_level* l = &s->level[s->depth];

    if (!l->is_map) {
        *name = NULL;
        *n    = 0;
        return CF_TRUE;
    }
    if (l->want_key) {
        return yaml_fail_at(
            s, e->line, e->col,
            "a key has to be a scalar: a mapping or a sequence cannot be "
            "a configuration key");
    }
    *name       = s->key;
    *n          = s->klen;
    l->want_key = 1; /* this value answers the pending key */
    return CF_TRUE;
}

#define YAML_CORE "tag:yaml.org,2002:"

/* A tag this reader resolves: none, the non-specific "!", or one of the
   core schema's (YAML 1.2, 10.3). Any other names a type of the writing
   application's, which a configuration has no way to honour. */
static cf_bool yaml_known_tag(yaml_scan* s, const yaml_ev* e, const char* want) {
    char what[160];
    if (e->tag == NULL || strcmp(e->tag, "!") == 0) {
        return CF_TRUE;
    }
    if (strncmp(e->tag, YAML_CORE, sizeof YAML_CORE - 1) == 0 && want != NULL) {
        const char* t = e->tag + sizeof YAML_CORE - 1;
        if (strcmp(t, want) == 0 ||
            (want[0] == '\0' && (strcmp(t, "str") == 0 || strcmp(t, "int") == 0 ||
                                 strcmp(t, "float") == 0 || strcmp(t, "bool") == 0 ||
                                 strcmp(t, "null") == 0))) {
            return CF_TRUE;
        }
    }
    cf_fmt(what, sizeof what,
           "the tag %s is not one this library resolves: the core schema's !!str, "
           "!!int, !!float, !!bool, !!null, !!map and !!seq are",
           e->tag);
    return yaml_fail_at(s, e->line, e->col, what);
}

static cf_bool yaml_open(yaml_scan* s, const yaml_ev* e) {
    cf_bool     is_map = (e->kind == YAML_EV_MAP) ? CF_TRUE : CF_FALSE;
    const char* name   = NULL;
    size_t      n      = 0;
    cf_bool     ok;

    if (!yaml_known_tag(s, e, is_map ? "map" : "seq")) {
        return CF_FALSE;
    }
    if (s->depth < 0) {
        if (!is_map) {
            return yaml_fail_at(
                s, e->line, e->col,
                "a configuration is a mapping at the top, not a sequence");
        }
        ok = s->out->begin_table(s->out->ctx, NULL, 0);
    } else {
        if (!yaml_name(s, e, &name, &n)) {
            return CF_FALSE;
        }
        if (s->depth + 1 >= CF_MAX_DEPTH) {
            return yaml_fail_at(s, e->line, e->col, "nesting is too deep");
        }
        ok = is_map ? s->out->begin_table(s->out->ctx, name, n)
                    : s->out->begin_array(s->out->ctx, name, n);
    }
    if (!ok) {
        return yaml_fail_at(s, e->line, e->col, "not a valid key");
    }
    ++s->depth;
    s->level[s->depth].is_map   = (unsigned char)(is_map ? 1 : 0);
    s->level[s->depth].want_key = 1;
    return CF_TRUE;
}

/* "~", "null" in any of YAML's three spellings, or nothing at all. */
static cf_bool yaml_is_null(const char* p, size_t n) {
    return (n == 0 || (n == 1 && p[0] == '~') ||
            (n == 4 && (memcmp(p, "null", 4) == 0 || memcmp(p, "Null", 4) == 0 ||
                        memcmp(p, "NULL", 4) == 0)))
               ? CF_TRUE
               : CF_FALSE;
}

/* A scalar tagged with a core type is that type, as YAML 1.2's core
   schema spells it; a value that is not one is refused, by name. */
static cf_bool yaml_tagged(yaml_scan* s, const yaml_ev* e, cf_value* v,
                           cf_bool* plain) {
    const char* t = e->tag + sizeof YAML_CORE - 1;
    *plain        = CF_FALSE;
    if (strcmp(t, "str") == 0) {
        cf_value_text(v, e->text, e->n);
        return CF_TRUE;
    }
    if (strcmp(t, "null") == 0 && yaml_is_null(e->text, e->n)) {
        cf_value_null(v);
        return CF_TRUE;
    }
    if (strcmp(t, "bool") == 0) {
        static const char* const yes[] = {"true", "True", "TRUE"};
        static const char* const no[]  = {"false", "False", "FALSE"};
        int                      i;
        for (i = 0; i < 3; ++i) {
            if (e->n == strlen(yes[i]) && memcmp(e->text, yes[i], e->n) == 0) {
                cf_value_bool(v, CF_TRUE);
                return CF_TRUE;
            }
            if (e->n == strlen(no[i]) && memcmp(e->text, no[i], e->n) == 0) {
                cf_value_bool(v, CF_FALSE);
                return CF_TRUE;
            }
        }
    }
    if (strcmp(t, "int") == 0) {
        cf_i64 i;
        if (cf_text_to_i64(e->text, e->n, &i, NULL)) {
            cf_value_int(v, i);
            return CF_TRUE;
        }
    }
    if (strcmp(t, "float") == 0) {
#if CF_WITH_REAL
        double d;
        if (cf_text_to_f64(e->text, e->n, &d, NULL)) {
            cf_value_real(v, d);
            return CF_TRUE;
        }
#endif
        /* .inf and .nan: the library keeps reals finite, so the text, for
           a definition to refuse or keep (as TOML's inf is, D-62). */
        {
            static const char* const odd[] = {".inf", ".Inf", ".INF",
                                              ".nan", ".NaN", ".NAN"};
            size_t k = (e->n > 0 && (e->text[0] == '+' || e->text[0] == '-')) ? 1u : 0u;
            int    i;
            for (i = 0; i < 6; ++i) {
                if (e->n - k == 4 && memcmp(e->text + k, odd[i], 4) == 0 &&
                    (k == 0 || i < 3)) {
                    cf_value_text(v, e->text, e->n);
                    *plain = CF_TRUE;
                    return CF_TRUE;
                }
            }
        }
    }
    {
        char what[160];
        cf_fmt(what, sizeof what, "the value is not a !!%s", t);
        return yaml_fail_at(s, e->line, e->col, what);
    }
}

static cf_bool yaml_scalar(yaml_scan* s, const yaml_ev* e) {
    cf_bool     plain = e->plain;
    yaml_level* l;
    const char* name;
    size_t      nlen;
    cf_value    v;
    cf_loc      at;

    if (!yaml_known_tag(s, e, "")) {
        return CF_FALSE;
    }
    if (s->depth < 0) {
        /* A document that is one scalar - or empty, which libyaml reads
           as a single empty one. */
        if (plain && e->n == 0 && e->tag == NULL) {
            return CF_TRUE;
        }
        return yaml_fail_at(
            s, e->line, e->col,
            "a configuration is a mapping at the top, not a single value");
    }
    l = &s->level[s->depth];
    if (l->is_map && l->want_key) {
        if (e->n == 0 || e->n >= sizeof s->key) {
            return yaml_fail_at(s, e->line, e->col,
                                (e->n == 0) ? "an empty key" : "the key is too long");
        }
        memcpy(s->key, e->text, e->n);
        s->key[e->n] = '\0';
        s->klen      = e->n;
        l->want_key  = 0;
        return CF_TRUE;
    }
    if (!yaml_name(s, e, &name, &nlen)) {
        return CF_FALSE;
    }
    if (e->tag != NULL && strcmp(e->tag, "!") != 0) {
        if (!yaml_tagged(s, e, &v, &plain)) {
            return CF_FALSE;
        }
    } else if (plain && e->tag == NULL && yaml_is_null(e->text, e->n)) {
        cf_value_null(&v);
        plain = CF_FALSE;
    } else {
        /* "!" is the non-specific tag of a quoted scalar: text, typed. */
        cf_value_text(&v, e->text, e->n);
        if (e->tag != NULL) {
            plain = CF_FALSE;
        }
    }
    at.file = s->file;
    at.line = e->line;
    at.col  = e->col;
    if (!s->out->scalar(s->out->ctx, name, nlen, &v, plain, &at)) {
        return yaml_fail_at(s, e->line, e->col, "not a valid key");
    }
    return CF_TRUE;
}

/* One node event, read or replayed: recorded for any open anchor, then
   handed to the document. */
static cf_bool yaml_handle(yaml_scan* s, const yaml_ev* e) {
    cf_bool ok;
    if (!yaml_record(s, e)) {
        return CF_FALSE;
    }
    switch (e->kind) {
    case YAML_EV_MAP:
    case YAML_EV_SEQ:
        ok = yaml_open(s, e);
        break;
    case YAML_EV_SCALAR:
        ok = yaml_scalar(s, e);
        break;
    default:
        --s->depth;
        ok = s->out->end(s->out->ctx)
                 ? CF_TRUE
                 : yaml_fail_at(s, e->line, e->col,
                                "the document could not be recorded");
        break;
    }
    if (ok) {
        yaml_anchor_close(s);
    }
    return ok;
}

/* An alias: its anchor's events again, where the alias stands. Each value
   keeps the place it was written at, which is the anchor's: explain()
   names the one line the value came from. */
static cf_bool yaml_alias(yaml_scan* s, const char* name, size_t n, unsigned line,
                          unsigned col) {
    cf_u32      i = yaml_find(s, name, n), k;
    yaml_anchor a;
    size_t      bytes = 0;

    ++s->read;
    if (i == 0) {
        return yaml_fail_at(s, line, col,
                            "an alias of an anchor not defined before it");
    }
    a = s->anchors[i - 1u];
    if (!a.done) {
        return yaml_fail_at(s, line, col, "an alias inside the node its anchor names");
    }
    if (s->replayed + a.count > s->read * YAML_ALIAS_FACTOR + YAML_ALIAS_FLOOR) {
        return yaml_fail_at(s, line, col,
                            "aliases expand to far more than the document holds");
    }
    /* The text the replay copies, keys included, counted before any of it
       is: an anchor's recordings sit in a pool of at most 4 GiB, so the
       sum does not wrap, and the comparison is written not to. */
    for (k = 0; k < a.count; ++k) {
        bytes += s->recs[a.first + k].n;
    }
    if (bytes > s->max_bytes - s->bytes) {
        return yaml_fail_at(s, line, col,
                            "aliases expand to far more than the document holds");
    }
    s->bytes += bytes;
    s->replayed += a.count;
    for (k = 0; k < a.count; ++k) {
        /* Copied out: replaying records into the same array, which may
           move it. */
        yaml_rec r = s->recs[a.first + k];
        yaml_ev  e;
        char     tag[256];
        /* The copy below, alive until the event is handled. */
        char local[256];
        e.kind  = r.kind;
        e.n     = r.n;
        e.plain = r.plain ? CF_TRUE : CF_FALSE;
        e.line  = r.line;
        e.col   = r.col;
        e.tag   = NULL;
        if (r.ntag > 0) {
            size_t t = (r.ntag < sizeof tag) ? r.ntag : sizeof tag - 1u;
            memcpy(tag, s->pool + r.tag, t);
            tag[t] = '\0';
            e.tag  = tag;
        }
        e.text = s->pool + r.text;
        if (e.kind == YAML_EV_SCALAR && s->nopen > 0) {
            /* Recording this scalar again copies its text into the pool,
               which may move as it grows: read it from a copy. */
            char* text = local;
            if (r.n >= sizeof local) {
                text = s->out->reserve_text != NULL &&
                               s->out->size >= offsetof(cf_doc_sink, reserve_text) +
                                                   sizeof s->out->reserve_text
                           ? s->out->reserve_text(s->out->ctx, r.n + 1u)
                           : NULL;
                if (text == NULL) {
                    return yaml_no_room(s);
                }
            }
            memcpy(text, s->pool + r.text, r.n);
            e.text = text;
        }
        if (!yaml_handle(s, &e)) {
            return CF_FALSE;
        }
    }
    return CF_TRUE;
}

/* A node event from the parser, with the anchor on it if any. */
static cf_bool yaml_node(yaml_scan* s, const yaml_ev* e, const char* anchor, size_t n) {
    ++s->read;
    if (anchor != NULL && !yaml_anchor_open(s, e->line, e->col, anchor, n)) {
        return CF_FALSE;
    }
    return yaml_handle(s, e);
}

static cf_bool yaml_document(yaml_scan* s, unsigned line, unsigned col) {
    if (++s->documents > 1) {
        return yaml_fail_at(s, line, col,
                            "a second document: one file is one configuration");
    }
    return CF_TRUE;
}

/* The end of the stream. Nothing in the file is an empty configuration,
   not an error, which is what an empty INI or TOML file is too. */
static cf_bool yaml_stream_end(yaml_scan* s, unsigned line, unsigned col) {
    if (s->depth < 0) {
        return (s->out->begin_table(s->out->ctx, NULL, 0) && s->out->end(s->out->ctx))
                   ? CF_TRUE
                   : yaml_fail_at(s, line, col, "the document could not be recorded");
    }
    return CF_TRUE;
}

static void yaml_scan_init(yaml_scan* s, const cf_blob* in, const cf_doc_sink* out,
                           cf_error* err) {
    memset(s, 0, sizeof *s);
    s->seed  = (cf_u32)yaml_seed(s);
    s->file  = (in->name != NULL) ? in->name : "<yaml>";
    s->out   = out;
    s->err   = err;
    s->depth = -1;
    /* Saturated rather than wrapped, for a document near SIZE_MAX. */
    s->max_bytes =
        (in->n > ((size_t)-1 - YAML_ALIAS_BYTES_FLOOR) / YAML_ALIAS_BYTES_FACTOR)
            ? (size_t)-1
            : in->n * YAML_ALIAS_BYTES_FACTOR + YAML_ALIAS_BYTES_FLOOR;
    s->work =
        (out->size >= offsetof(cf_doc_sink, reserve_work) + sizeof out->reserve_work &&
         out->reserve_work != NULL)
            ? CF_TRUE
            : CF_FALSE;
}

/* A node event's common fields: the tag as written, or none. */
static void yaml_ev_init(yaml_ev* e, int kind, const char* tag, unsigned line,
                         unsigned col) {
    e->kind  = kind;
    e->text  = NULL;
    e->n     = 0;
    e->plain = CF_FALSE;
    e->tag   = tag;
    e->line  = line;
    e->col   = col;
}

#if CF_YAML_LIBFYAML

/* ------------------------------------------------------------------ *
 * Through libfyaml (YAML 1.2, complete on yaml-test-suite)
 * ------------------------------------------------------------------ */

/* A token's text, or "" for none (libfyaml gives an empty plain scalar
   no token). */
static const char* yaml_fy_text(struct fy_token* t, size_t* n) {
    const char* p = (t != NULL) ? fy_token_get_text(t, n) : NULL;
    if (p == NULL) {
        *n = 0;
        return "";
    }
    return p;
}

/* A tag token's tag, NUL-terminated into buf, or NULL for none. libfyaml
   resolves the handle, so !!str arrives as tag:yaml.org,2002:str. */
static const char* yaml_fy_tag(struct fy_token* t, char* buf, size_t cap) {
    size_t      n;
    const char* p = (t != NULL) ? fy_token_get_text(t, &n) : NULL;
    if (p == NULL) {
        return NULL;
    }
    if (n >= cap) {
        n = cap - 1u;
    }
    memcpy(buf, p, n);
    buf[n] = '\0';
    return buf;
}

static cf_bool yaml_fy_event(yaml_scan* s, struct fy_event* ev, cf_bool* done) {
    const struct fy_mark* m    = fy_event_start_mark(ev);
    unsigned              line = (m != NULL) ? (unsigned)m->line + 1u : 1u;
    unsigned              col  = (m != NULL) ? (unsigned)m->column + 1u : 1u;
    yaml_ev               e;
    char                  tag[256];
    struct fy_token*      anchor = NULL;
    const char*           a      = NULL;
    size_t                an     = 0;

    switch (ev->type) {
    case FYET_NONE:
    case FYET_STREAM_START:
    case FYET_DOCUMENT_END:
        return CF_TRUE;
    case FYET_DOCUMENT_START:
        return yaml_document(s, line, col);
    case FYET_STREAM_END:
        *done = CF_TRUE;
        return yaml_stream_end(s, line, col);
    case FYET_ALIAS: {
        size_t      n;
        const char* p = yaml_fy_text(ev->alias.anchor, &n);
        return yaml_alias(s, p, n, line, col);
    }
    case FYET_SCALAR:
        yaml_ev_init(&e, YAML_EV_SCALAR, yaml_fy_tag(ev->scalar.tag, tag, sizeof tag),
                     line, col);
        e.text  = yaml_fy_text(ev->scalar.value, &e.n);
        e.plain = (ev->scalar.value == NULL ||
                   fy_token_scalar_style(ev->scalar.value) == FYSS_PLAIN)
                      ? CF_TRUE
                      : CF_FALSE;
        if (e.tag == NULL && !e.plain) {
            e.tag = "!"; /* a quoted scalar's non-specific tag: text */
        }
        anchor = ev->scalar.anchor;
        break;
    case FYET_SEQUENCE_START:
        yaml_ev_init(&e, YAML_EV_SEQ,
                     yaml_fy_tag(ev->sequence_start.tag, tag, sizeof tag), line, col);
        anchor = ev->sequence_start.anchor;
        break;
    case FYET_MAPPING_START:
        yaml_ev_init(&e, YAML_EV_MAP,
                     yaml_fy_tag(ev->mapping_start.tag, tag, sizeof tag), line, col);
        anchor = ev->mapping_start.anchor;
        break;
    case FYET_SEQUENCE_END:
    case FYET_MAPPING_END:
        yaml_ev_init(&e, YAML_EV_END, NULL, line, col);
        break;
    default:
        return CF_TRUE;
    }
    if (anchor != NULL) {
        a = yaml_fy_text(anchor, &an);
    }
    return yaml_node(s, &e, a, an);
}

/* libfyaml's first collected error, as this library words a place. */
static cf_bool yaml_fy_fail(yaml_scan* s, struct fy_diag* diag) {
    void*                 iter = NULL;
    struct fy_diag_error* d =
        (diag != NULL) ? fy_diag_errors_iterate(diag, &iter) : NULL;
    if (d == NULL) {
        return yaml_fail_at(s, 1u, 1u, "not YAML");
    }
    return yaml_fail_at(s, (unsigned)d->line + 1u, (unsigned)d->column + 1u,
                        (d->msg != NULL) ? d->msg : "not YAML");
}

/* The build's least libfyaml (src/serialize/CMakeLists.txt, D-62). */
#ifndef CF_FYAML_MIN_MAJOR
#define CF_FYAML_MIN_MAJOR 0
#define CF_FYAML_MIN_MINOR 9
#define CF_FYAML_MIN_PATCH 1
#endif

/* Whether the libfyaml this program runs against is at least the build's
   least one. Asked of the library at run time, not of the header: 0.8 and
   0.9 share the soname libfyaml.so.0, so a program built against 0.9.6
   loads Ubuntu 24.04's 0.8 just as well - which hangs on malformed input
   (libfyaml issue 101). A version that cannot be read is not taken. */
static cf_bool yaml_fy_version_ok(const char** have) {
    const char* p    = fy_library_version();
    long        v[3] = {0, 0, 0};
    int         i;

    *have = (p != NULL) ? p : "unknown";
    if (p == NULL) {
        return CF_FALSE;
    }
    for (i = 0; i < 3; ++i) {
        if (!cf_is_digit((unsigned char)*p)) {
            return CF_FALSE;
        }
        while (cf_is_digit((unsigned char)*p) && v[i] < 100000L) {
            v[i] = v[i] * 10 + (*p++ - '0');
        }
        if (i < 2 && *p++ != '.') {
            return CF_FALSE;
        }
    }
    if (v[0] != CF_FYAML_MIN_MAJOR) {
        return (v[0] > CF_FYAML_MIN_MAJOR) ? CF_TRUE : CF_FALSE;
    }
    if (v[1] != CF_FYAML_MIN_MINOR) {
        return (v[1] > CF_FYAML_MIN_MINOR) ? CF_TRUE : CF_FALSE;
    }
    return (v[2] >= CF_FYAML_MIN_PATCH) ? CF_TRUE : CF_FALSE;
}

static cf_bool yaml_decode(const cf_blob* in, const cf_doc_sink* out, cf_error* err) {
    struct fy_parse_cfg cfg;
    struct fy_diag_cfg  dcfg;
    struct fy_diag*     diag;
    struct fy_parser*   fyp;
    struct fy_event*    ev;
    yaml_scan           s;
    cf_bool             ok   = CF_TRUE;
    cf_bool             done = CF_FALSE;
    const char*         have;

    yaml_scan_init(&s, in, out, err);
    if (!yaml_fy_version_ok(&have)) {
        return cf_error_set(
            err, CF_E_CAPABILITY, NULL, s.file,
            "YAML is read through libfyaml %d.%d.%d or later, and the "
            "one loaded is %s, which hangs on malformed input (libfyaml "
            "issue 101): install a newer libfyaml",
            CF_FYAML_MIN_MAJOR, CF_FYAML_MIN_MINOR, CF_FYAML_MIN_PATCH, have);
    }
    fy_diag_cfg_default(&dcfg);
    dcfg.fp = NULL; /* nothing on stderr: the error is reported here */
    diag    = fy_diag_create(&dcfg);
    if (diag == NULL) {
        return cf_error_set(err, CF_E_MEMORY, NULL, s.file,
                            "no room for a YAML parser");
    }
    fy_diag_set_collect_errors(diag, true);
    memset(&cfg, 0, sizeof cfg);
    cfg.flags = (enum fy_parse_cfg_flags)(FYPCF_QUIET | FYPCF_COLLECT_DIAG);
    cfg.diag  = diag;
    fyp       = fy_parser_create(&cfg);
    if (fyp == NULL || fy_parser_set_string(fyp, in->p, in->n) != 0) {
        if (fyp != NULL) {
            fy_parser_destroy(fyp);
        }
        fy_diag_destroy(diag);
        return cf_error_set(err, CF_E_MEMORY, NULL, s.file,
                            "no room for a YAML parser");
    }
    while (ok && !done) {
        ev = fy_parser_parse(fyp);
        if (ev == NULL) {
            /* The end without a stream end is an error, collected. */
            ok = fy_parser_get_stream_error(fyp) ? yaml_fy_fail(&s, diag)
                                                 : yaml_stream_end(&s, 1u, 1u);
            break;
        }
        ok = yaml_fy_event(&s, ev, &done);
        fy_parser_event_free(fyp, ev);
    }
    if (ok && fy_parser_get_stream_error(fyp)) {
        ok = yaml_fy_fail(&s, diag);
    }
    fy_parser_destroy(fyp);
    fy_diag_destroy(diag);
    return ok;
}

#elif CF_YAML_LIBYAML

/* ------------------------------------------------------------------ *
 * Through libyaml (YAML 1.1; builds everywhere, MSVC included)
 * ------------------------------------------------------------------ */

static unsigned yaml_line(const yaml_mark_t* m) {
    return (unsigned)m->line + 1u;
}
static unsigned yaml_col(const yaml_mark_t* m) {
    return (unsigned)m->column + 1u;
}

/* One event. Sets *done at the end of the stream. The tag is the one
   written, or none: libyaml gives no tag to a node without one, and
   marks a "!" as implicit, so its implicit flags are no guide. */
static cf_bool yaml_event(yaml_scan* s, const yaml_event_t* ev, cf_bool* done) {
    unsigned           line = yaml_line(&ev->start_mark);
    unsigned           col  = yaml_col(&ev->start_mark);
    yaml_ev            e;
    const yaml_char_t* anchor = NULL;

    switch (ev->type) {
    case YAML_NO_EVENT:
    case YAML_STREAM_START_EVENT:
    case YAML_DOCUMENT_END_EVENT:
        return CF_TRUE;
    case YAML_DOCUMENT_START_EVENT:
        return yaml_document(s, line, col);
    case YAML_STREAM_END_EVENT:
        *done = CF_TRUE;
        return yaml_stream_end(s, line, col);
    case YAML_ALIAS_EVENT: {
        const char* name = (const char*)ev->data.alias.anchor;
        return yaml_alias(s, name, strlen(name), line, col);
    }
    case YAML_SCALAR_EVENT:
        yaml_ev_init(&e, YAML_EV_SCALAR, (const char*)ev->data.scalar.tag, line, col);
        e.text = (const char*)ev->data.scalar.value;
        e.n    = ev->data.scalar.length;
        e.plain =
            (ev->data.scalar.style == YAML_PLAIN_SCALAR_STYLE) ? CF_TRUE : CF_FALSE;
        if (e.tag == NULL && !e.plain) {
            e.tag = "!"; /* a quoted scalar's non-specific tag: text */
        }
        anchor = ev->data.scalar.anchor;
        break;
    case YAML_SEQUENCE_START_EVENT:
        yaml_ev_init(&e, YAML_EV_SEQ, (const char*)ev->data.sequence_start.tag, line,
                     col);
        anchor = ev->data.sequence_start.anchor;
        break;
    case YAML_MAPPING_START_EVENT:
        yaml_ev_init(&e, YAML_EV_MAP, (const char*)ev->data.mapping_start.tag, line,
                     col);
        anchor = ev->data.mapping_start.anchor;
        break;
    case YAML_SEQUENCE_END_EVENT:
    case YAML_MAPPING_END_EVENT:
        yaml_ev_init(&e, YAML_EV_END, NULL, line, col);
        break;
    default:
        return CF_TRUE;
    }
    return yaml_node(s, &e, (const char*)anchor,
                     (anchor != NULL) ? strlen((const char*)anchor) : 0u);
}

static cf_bool yaml_decode(const cf_blob* in, const cf_doc_sink* out, cf_error* err) {
    yaml_parser_t parser;
    yaml_event_t  ev;
    yaml_scan     s;
    cf_bool       ok   = CF_TRUE;
    cf_bool       done = CF_FALSE;

    yaml_scan_init(&s, in, out, err);
    if (!yaml_parser_initialize(&parser)) {
        return cf_error_set(err, CF_E_MEMORY, NULL, s.file,
                            "no room for a YAML parser");
    }
    yaml_parser_set_input_string(&parser, (const unsigned char*)in->p, in->n);
    yaml_parser_set_encoding(&parser, YAML_UTF8_ENCODING);
    while (ok && !done) {
        if (!yaml_parser_parse(&parser, &ev)) {
            ok = yaml_fail_at(&s, yaml_line(&parser.problem_mark),
                              yaml_col(&parser.problem_mark),
                              (parser.problem != NULL) ? parser.problem : "not YAML");
            break;
        }
        ok = yaml_event(&s, &ev, &done);
        yaml_event_delete(&ev);
    }
    yaml_parser_delete(&parser);
    return ok;
}

#else /* the built-in parser */

/* ------------------------------------------------------------------ *
 * Through the built-in parser (yamlparse.c, D-69): YAML 1.2, all of
 * yaml-test-suite, no dependency
 * ------------------------------------------------------------------ */

static cf_bool yaml_b_document(void* ctx, unsigned line, unsigned col) {
    return yaml_document((yaml_scan*)ctx, line, col);
}

static cf_bool yaml_b_stream_end(void* ctx, unsigned line, unsigned col) {
    return yaml_stream_end((yaml_scan*)ctx, line, col);
}

static cf_bool yaml_b_alias(void* ctx, const char* name, size_t n, unsigned line,
                            unsigned col) {
    return yaml_alias((yaml_scan*)ctx, name, n, line, col);
}

static cf_bool yaml_b_node(void* ctx, const cf_yp_node* nd) {
    yaml_ev e;
    int     kind = (nd->kind == CF_YP_MAP)      ? YAML_EV_MAP
                   : (nd->kind == CF_YP_SEQ)    ? YAML_EV_SEQ
                   : (nd->kind == CF_YP_SCALAR) ? YAML_EV_SCALAR
                                                : YAML_EV_END;

    yaml_ev_init(&e, kind, nd->tag, nd->line, nd->col);
    e.text  = nd->text;
    e.n     = nd->n;
    e.plain = nd->plain;
    if (kind == YAML_EV_SCALAR && e.tag == NULL && !e.plain) {
        e.tag = "!"; /* a quoted or block scalar's non-specific tag: text */
    }
    return yaml_node((yaml_scan*)ctx, &e, nd->anchor, nd->alen);
}

/* The parser decodes each scalar into room the sink lends, as the
   anchors' recordings do. */
static void* yaml_b_work(void* ctx, size_t n) {
    yaml_scan* s = (yaml_scan*)ctx;
    return s->work ? s->out->reserve_work(s->out->ctx, n) : NULL;
}

static cf_bool yaml_decode(const cf_blob* in, const cf_doc_sink* out, cf_error* err) {
    yaml_scan    s;
    cf_yp_events ev;

    yaml_scan_init(&s, in, out, err);
    ev.ctx        = &s;
    ev.document   = yaml_b_document;
    ev.node       = yaml_b_node;
    ev.alias      = yaml_b_alias;
    ev.stream_end = yaml_b_stream_end;
    ev.work       = yaml_b_work;
    return cf_yaml_parse(in, s.file, &ev, err);
}

#endif /* the YAML parser */

/* ================================================================== *
 * Encoding
 *
 * Block style, two spaces a level, every string double-quoted - which
 * is also how JSON writes one, and JSON's escapes are a subset of
 * YAML's - so that nothing written here reads back as anything else
 * under 1.1 or 1.2. A key is written bare when it is a word no YAML
 * reader could take for a value, and quoted otherwise.
 *
 * A string is written as it is, but for what a reader would not take
 * as it stands (see yaml_quote), and a string that is not UTF-8 is not
 * written, because a YAML reader refuses the bytes and the file would
 * be one this library could not read back.
 * ================================================================== */

#if CF_WITH_EMIT

typedef struct yaml_enc {
    cf_sink*      out;
    int           depth; /* containers open, the top-level one included */
    unsigned char is_seq[CF_MAX_DEPTH + 1];
    unsigned char empty[CF_MAX_DEPTH + 1]; /* opened, nothing in it yet */
} yaml_enc;

/* cf_emit keeps this on its own frame, in CF_ENCODER_STATE_MAX bytes. */
typedef char yaml_enc_fits[(sizeof(yaml_enc) <= CF_ENCODER_STATE_MAX) ? 1 : -1];

/* The code point at the front of p[0..n) and how many bytes it takes;
   0 when they are not UTF-8 as a YAML reader takes it: the shortest form
   only, no surrogate, nothing past U+10FFFF. */
static size_t yaml_utf8(const unsigned char* p, size_t n, cf_u32* cp) {
    size_t len;
    size_t i;
    cf_u32 v;

    if (p[0] < 0x80u) {
        *cp = p[0];
        return 1;
    }
    if (p[0] >= 0xC2u && p[0] <= 0xDFu) {
        len = 2;
        v   = p[0] & 0x1Fu;
    } else if (p[0] >= 0xE0u && p[0] <= 0xEFu) {
        len = 3;
        v   = p[0] & 0x0Fu;
    } else if (p[0] >= 0xF0u && p[0] <= 0xF4u) {
        len = 4;
        v   = p[0] & 0x07u;
    } else {
        return 0;
    }
    if (n < len) {
        return 0;
    }
    for (i = 1; i < len; ++i) {
        if ((p[i] & 0xC0u) != 0x80u) {
            return 0;
        }
        v = (v << 6) | (p[i] & 0x3Fu);
    }
    if ((len == 3 && v < 0x800u) || (len == 4 && v < 0x10000u) ||
        (v >= 0xD800u && v <= 0xDFFFu) || v > 0x10FFFFu) {
        return 0;
    }
    *cp = v;
    return len;
}

/* One double-quoted scalar. Escaped is what a reader would not give
   back as written: the C0 controls, DEL, and the C1 controls, which
   libyaml refuses outright - but for U+0085, which it takes for a line
   break and folds into a space - and U+FFFE and U+FFFF, which it refuses
   as well. False for a string that is not UTF-8, with the scalar left
   unfinished. */
static cf_bool yaml_quote(cf_sink* out, const char* p, size_t n) {
    static const char hex[] = "0123456789abcdef";
    size_t            i     = 0;

    cf_sink_putc(out, '"');
    while (i < n) {
        cf_u32 cp;
        size_t len = yaml_utf8((const unsigned char*)p + i, n - i, &cp);

        if (len == 0) {
            return CF_FALSE;
        }
        if (cp == '"' || cp == '\\') {
            cf_sink_putc(out, '\\');
            cf_sink_putc(out, (char)cp);
        } else if (cp == '\n') {
            cf_sink_puts(out, "\\n");
        } else if (cp == '\t') {
            cf_sink_puts(out, "\\t");
        } else if (cp < 0x20u || (cp >= 0x7Fu && cp <= 0x9Fu)) {
            cf_sink_fmt(out, "\\x%c%c", hex[(cp >> 4) & 0xFu], hex[cp & 0xFu]);
        } else if (cp == 0xFFFEu || cp == 0xFFFFu) {
            cf_sink_fmt(out, "\\ufff%c", hex[cp & 0xFu]);
        } else {
            cf_sink_write(out, p + i, len);
        }
        i += len;
    }
    cf_sink_putc(out, '"');
    return CF_TRUE;
}

/* Words a YAML reader of either version takes for something other than
   a string, as a key or as a value. */
static cf_bool yaml_reserved(const char* p, size_t n) {
    static const char* const words[] = {"y",    "n",     "yes",  "no", "on", "off",
                                        "true", "false", "null", "~",  NULL};
    size_t                   i;

    for (i = 0; words[i] != NULL; ++i) {
        if (cf_cmp_ascii_ci(p, n, words[i], strlen(words[i])) == 0) {
            return CF_TRUE;
        }
    }
    return CF_FALSE;
}

static void yaml_key(cf_sink* out, const char* p, size_t n) {
    size_t  i;
    cf_bool bare = (n > 0 && !yaml_reserved(p, n) && cf_is_alpha((unsigned char)p[0]))
                       ? CF_TRUE
                       : CF_FALSE;

    for (i = 0; bare && i < n; ++i) {
        unsigned char c = (unsigned char)p[i];
        if (!cf_is_alnum(c) && c != '_' && c != '-') {
            bare = CF_FALSE;
        }
    }
    if (bare) {
        cf_sink_write(out, p, n);
    } else {
        (void)yaml_quote(out, p, n); /* a key is ASCII, and ASCII is UTF-8 */
    }
}

/* Where a member starts: its indentation, its name or its dash. A
   container that had nothing in it yet is given its line break first,
   since until now it might have been written as {} or []. */
static void yaml_lead(yaml_enc* e, const char* name, size_t n) {
    int i;

    if (e->depth > 1 && e->empty[e->depth]) {
        cf_sink_putc(e->out, '\n');
    }
    e->empty[e->depth] = 0;
    for (i = 1; i < e->depth; ++i) {
        cf_sink_puts(e->out, "  ");
    }
    if (e->is_seq[e->depth]) {
        cf_sink_putc(e->out, '-');
    } else {
        yaml_key(e->out, name, n);
        cf_sink_putc(e->out, ':');
    }
}

static cf_bool yaml_enc_open(yaml_enc* e, const char* name, size_t n, cf_bool seq) {
    if (e->depth >= CF_MAX_DEPTH) {
        return CF_FALSE;
    }
    if (e->depth > 0) {
        yaml_lead(e, name, n);
    }
    ++e->depth;
    e->is_seq[e->depth] = (unsigned char)(seq ? 1 : 0);
    e->empty[e->depth]  = 1;
    return CF_TRUE;
}

static cf_bool yaml_enc_table(void* ctx, const char* name, size_t n) {
    return yaml_enc_open((yaml_enc*)ctx, name, n, CF_FALSE);
}

static cf_bool yaml_enc_array(void* ctx, const char* name, size_t n) {
    return yaml_enc_open((yaml_enc*)ctx, name, n, CF_TRUE);
}

static cf_bool yaml_enc_scalar(void* ctx, const char* name, size_t n, const cf_value* v,
                               cf_bool textual, const cf_loc* at) {
    yaml_enc* e = (yaml_enc*)ctx;
    char      tmp[CF_VALUE_TEXT_MAX];

    (void)textual;
    (void)at;
    yaml_lead(e, name, n);
    cf_sink_putc(e->out, ' ');
    switch (v->type) {
    case CF_TYPE_NULL:
        cf_sink_puts(e->out, "null");
        break;
    case CF_TYPE_BOOL:
        cf_sink_puts(e->out, v->u.b ? "true" : "false");
        break;
    case CF_TYPE_INT:
    case CF_TYPE_REAL:
        cf_sink_write(e->out, tmp, cf_value_to_text(v, tmp, sizeof tmp));
        break;
    case CF_TYPE_TEXT:
        if (!yaml_quote(e->out, v->u.t.p, v->u.t.n)) {
            return CF_FALSE;
        }
        break;
    }
    cf_sink_putc(e->out, '\n');
    return CF_TRUE;
}

static cf_bool yaml_enc_end(void* ctx) {
    yaml_enc* e = (yaml_enc*)ctx;

    if (e->depth <= 0) {
        return CF_FALSE;
    }
    if (e->depth > 1 && e->empty[e->depth]) {
        cf_sink_puts(e->out, e->is_seq[e->depth] ? " []\n" : " {}\n");
    } else if (e->depth == 1 && e->empty[e->depth]) {
        cf_sink_puts(e->out, "{}\n");
    }
    --e->depth;
    return CF_TRUE;
}

static cf_bool yaml_encoder(cf_sink* out, void* state, cf_doc_sink* sink) {
    yaml_enc* e = (yaml_enc*)state;

    if (e == NULL || sink == NULL) {
        return CF_FALSE;
    }
    memset(e, 0, sizeof *e);
    e->out            = out;
    sink->size        = offsetof(cf_doc_sink, reserve_text);
    sink->begin_table = yaml_enc_table;
    sink->begin_array = yaml_enc_array;
    sink->scalar      = yaml_enc_scalar;
    sink->end         = yaml_enc_end;
    sink->ctx         = e;
    return CF_TRUE;
}

const cf_codec cf_codec_yaml = {sizeof(cf_codec),
                                "yaml",
                                ".yaml;.yml",
                                CF_CODEC_READ | CF_CODEC_WRITE | CF_CODEC_ARRAYS,
                                yaml_decode,
                                yaml_encoder,
                                sizeof(yaml_enc),
                                NULL,
                                NULL};

#else /* !CF_WITH_EMIT: read only */

const cf_codec cf_codec_yaml = {sizeof(cf_codec),
                                "yaml",
                                ".yaml;.yml",
                                CF_CODEC_READ | CF_CODEC_ARRAYS,
                                yaml_decode,
                                NULL,
                                0,
                                NULL,
                                NULL};

#endif /* CF_WITH_EMIT */

#endif /* CF_WITH_YAML */
