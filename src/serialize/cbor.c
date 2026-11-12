/*
 * cbor.c - CBOR (RFC 8949), read and written. C89.
 *
 * The JSON data model in binary. A configuration is a map with text keys
 * at the top; inside it maps, arrays, integers, floats of every width,
 * text strings, booleans and null. Maps and arrays and text strings may
 * have their lengths or be streamed (an indefinite length); both are read.
 *
 * What a configuration cannot hold is refused by name, never guessed at:
 * byte strings, tags other than 0 (a date and time), 1 (seconds since
 * the epoch), 32 (a URI) and 55799 (self-described CBOR, read past),
 * undefined and the other simple values, a map key that is not text, an
 * integer past the 64-bit signed range. Infinity and NaN are the text
 * "inf", "-inf" and "nan", typed by the contract as TOML's are (D-62).
 *
 * Every value is placed by the byte it starts at, "<name>, byte <n>",
 * counted from 0 as dd and cmp count - the place a hex dump of the file
 * shows it. The map's keys are not copied: a definite key is a slice of
 * the document.
 *
 * Written here rather than taken from a library: the grammar is a page,
 * and a library would not say which byte a value came from.
 */

#include "configforge/cf_numeric.h"
#include "configforge/cf_serialize.h"

#include <float.h>
#include <stddef.h>
#include <string.h>

#if CF_WITH_CBOR

typedef struct cbor_scan {
    const unsigned char* base;
    const unsigned char* p;
    const unsigned char* end;
    const char*          name;
    const cf_doc_sink*   out;
    cf_error*            err;
    int                  depth;
    char                 where[CF_PATH_MAX + 32];
} cbor_scan;

/* One data item's head: its major type, the argument it carries, and
   whether it is a break (major 7, additional information 31) or has an
   indefinite length (the same information on major 2 to 5). */
typedef struct cbor_head {
    unsigned major;
    unsigned info;
    cf_u64   arg;
    cf_bool  indef;
    cf_bool  brk;
    size_t   off;
} cbor_head;

/* Where a text string's bytes go: the caller's local buffer or the
   sink's text storage for a value, the sink's work storage for a key. */
enum { CBOR_VALUE = 0, CBOR_KEY = 1 };

static size_t cbor_left(const cbor_scan* s) {
    return (size_t)(s->end - s->p);
}

static size_t cbor_at(const cbor_scan* s) {
    return (size_t)(s->p - s->base);
}

/* Whether the sink is at least as new as the member that gives text and
   work storage, and has it. */
static cf_bool cbor_has_text(const cf_doc_sink* o) {
    return (o->size >= offsetof(cf_doc_sink, reserve_text) + sizeof o->reserve_text &&
            o->reserve_text != NULL)
               ? CF_TRUE
               : CF_FALSE;
}

static cf_bool cbor_has_work(const cf_doc_sink* o) {
    return (o->size >= offsetof(cf_doc_sink, reserve_work) + sizeof o->reserve_work &&
            o->reserve_work != NULL)
               ? CF_TRUE
               : CF_FALSE;
}

/* "<name>, byte <n>: <what>" */
static cf_bool cbor_fail(cbor_scan* s, size_t off, const char* what) {
    return cf_error_set(s->err, CF_E_SOURCE, NULL, s->name, "%s, byte %zu: %s", s->name,
                        off, what);
}

/* What a head begins, for the messages that say what was found. */
static const char* cbor_kind(unsigned major, unsigned info) {
    switch (major) {
    case 0:
        return "an unsigned integer";
    case 1:
        return "a negative integer";
    case 2:
        return "a byte string";
    case 3:
        return "a text string";
    case 4:
        return "an array";
    case 5:
        return "a map";
    case 6:
        return "a tag";
    default:
        break;
    }
    return (info >= 25u && info <= 27u) ? "a float" : "a simple value";
}

/* One head, advancing past it, with everything RFC 8949 section 3 makes
   not well-formed refused here. */
static cf_bool cbor_head_read(cbor_scan* s, cbor_head* h) {
    unsigned b;
    size_t   nb, i;

    h->off   = cbor_at(s);
    h->arg   = 0;
    h->indef = CF_FALSE;
    h->brk   = CF_FALSE;
    if (s->p >= s->end) {
        return cbor_fail(s, h->off, "the document ends inside a data item");
    }
    b        = *s->p++;
    h->major = b >> 5;
    h->info  = b & 31u;
    if (h->info < 24u) {
        h->arg = h->info;
        return CF_TRUE;
    }
    if (h->info <= 27u) {
        nb = (size_t)1 << (h->info - 24u);
        if (cbor_left(s) < nb) {
            return cbor_fail(s, h->off, "the document ends inside a data item");
        }
        for (i = 0; i < nb; ++i) {
            h->arg = (h->arg << 8) | (cf_u64)*s->p++;
        }
        if (h->major == 7 && h->info == 24u && h->arg < 32u) {
            return cbor_fail(s, h->off,
                             "not well-formed: a two-byte simple value "
                             "below 32");
        }
        return CF_TRUE;
    }
    if (h->info != 31u) {
        return cf_error_set(s->err, CF_E_SOURCE, NULL, s->name,
                            "%s, byte %zu: not well-formed: additional information %u "
                            "is reserved",
                            s->name, h->off, h->info);
    }
    if (h->major == 7) {
        h->brk = CF_TRUE;
        return CF_TRUE;
    }
    if (h->major == 0 || h->major == 1 || h->major == 6) {
        return cf_error_set(s->err, CF_E_SOURCE, NULL, s->name,
                            "%s, byte %zu: not well-formed: major type %u has no "
                            "indefinite length",
                            s->name, h->off, h->major);
    }
    h->indef = CF_TRUE;
    return CF_TRUE;
}

/* The place a scalar is explained by. */
static void cbor_place(cbor_scan* s, size_t off, cf_loc* at) {
    cf_fmt(s->where, sizeof s->where, "%s, byte %zu", s->name, off);
    at->file = s->where;
    at->line = 0;
    at->col  = 0;
}

/* The sink refused something named, or not named. */
static cf_bool cbor_refused(cbor_scan* s, size_t off, const char* name, size_t nlen) {
    if (name == NULL) {
        return cbor_fail(s, off, "the value could not be recorded");
    }
    return cf_error_set(s->err, CF_E_KEY, NULL, s->name,
                        "%s, byte %zu: '%S' is not a valid key", s->name, off, name,
                        nlen);
}

static cf_bool cbor_emit(cbor_scan* s, size_t start, const char* name, size_t nlen,
                         const cf_value* v, cf_bool textual) {
    cf_loc at;

    cbor_place(s, start, &at);
    if (!s->out->scalar(s->out->ctx, name, nlen, v, textual, &at)) {
        return cbor_refused(s, start, name, nlen);
    }
    return CF_TRUE;
}

/* A text string's head is read; its definite length is checked against
   what is left before anything is read, and its bytes are UTF-8. */
static cf_bool cbor_definite(cbor_scan* s, const cbor_head* h, const char** p,
                             size_t* n) {
    if (h->arg > (cf_u64)cbor_left(s)) {
        return cf_error_set(s->err, CF_E_SOURCE, NULL, s->name,
                            "%s, byte %zu: a length of %U, and %zu bytes are left",
                            s->name, h->off, h->arg, cbor_left(s));
    }
    *n = (size_t)h->arg;
    *p = (const char*)s->p;
    if (!cf_text_is_utf8(*p, *n)) {
        return cbor_fail(s, h->off, "a text string that is not UTF-8");
    }
    s->p += *n;
    return CF_TRUE;
}

/* The chunks of an indefinite-length text string, to the break: each is
   a definite-length text string, UTF-8 by itself (RFC 8949 section
   3.2.3). `total` is their length together; with `into` not NULL they
   are copied there as well, and s->p is left past the break either way. */
static cf_bool cbor_chunks(cbor_scan* s, size_t* total, char* into) {
    size_t sum = 0;

    for (;;) {
        cbor_head   c;
        const char* p;
        size_t      n;

        if (!cbor_head_read(s, &c)) {
            return CF_FALSE;
        }
        if (c.brk) {
            break;
        }
        if (c.major != 3 || c.indef) {
            return cbor_fail(s, c.off,
                             "not well-formed: a chunk of an indefinite-length "
                             "string is a definite-length string of its type");
        }
        if (!cbor_definite(s, &c, &p, &n)) {
            return CF_FALSE;
        }
        if (into != NULL) {
            memcpy(into + sum, p, n);
        }
        sum += n;
    }
    *total = sum;
    return CF_TRUE;
}

/* A text string whose head is read: a slice of the document when it has
   a length, else gathered. `local` (cap bytes, with a terminator) takes
   a short value; anything longer, and every key, goes to the sink. */
static cf_bool cbor_string(cbor_scan* s, const cbor_head* h, int which, char* local,
                           size_t cap, const char** p, size_t* n) {
    const unsigned char* start;
    size_t               total = 0;
    char*                buf;

    if (!h->indef) {
        return cbor_definite(s, h, p, n);
    }
    start = s->p;
    if (!cbor_chunks(s, &total, NULL)) {
        return CF_FALSE;
    }
    if (which == CBOR_VALUE && total < cap) {
        buf = local;
    } else if (which == CBOR_VALUE) {
        if (!cbor_has_text(s->out)) {
            return cbor_fail(s, h->off,
                             "an indefinite-length text string needs sink "
                             "text storage");
        }
        buf = s->out->reserve_text(s->out->ctx, total + 1);
        if (buf == NULL) {
            return cf_error_set(s->err, CF_E_MEMORY, NULL, s->name,
                                "no room to decode a CBOR text string");
        }
    } else {
        /* A key must outlive the value's own parse, so it is kept where
           the decode keeps its work. */
        if (!cbor_has_work(s->out)) {
            return cbor_fail(s, h->off,
                             "an indefinite-length map key needs sink work "
                             "storage");
        }
        buf = (char*)s->out->reserve_work(s->out->ctx, total + 1);
        if (buf == NULL) {
            return cf_error_set(s->err, CF_E_MEMORY, NULL, s->name,
                                "no room to decode a CBOR text string");
        }
    }
    s->p = start;
    if (!cbor_chunks(s, &total, buf)) {
        return CF_FALSE;
    }
    buf[total] = '\0';
    *p         = buf;
    *n         = total;
    return CF_TRUE;
}

static cf_bool cbor_text_value(cbor_scan* s, const cbor_head* h, size_t start,
                               const char* name, size_t nlen) {
    char        local[256];
    const char* p = NULL;
    size_t      n = 0;
    cf_value    v;

    if (!cbor_string(s, h, CBOR_VALUE, local, sizeof local, &p, &n)) {
        return CF_FALSE;
    }
    cf_value_text(&v, p, n);
    /* Typed: CBOR said this is a text string, as JSON does. */
    return cbor_emit(s, start, name, nlen, &v, CF_FALSE);
}

#if CF_WITH_REAL

/* A float of any width, by its bit pattern. Infinity and NaN are text,
   as TOML's are. */
static cf_bool cbor_float(cbor_scan* s, const cbor_head* h, size_t start,
                          const char* name, size_t nlen) {
    double   d;
    cf_u64   bits;
    cf_value v;

    if (h->info == 25u) {
        d = cf_f64_from_f16_bits((cf_u32)h->arg);
    } else if (h->info == 26u) {
        d = cf_f64_from_f32_bits((cf_u32)h->arg);
    } else {
        d = cf_f64_from_bits(h->arg);
    }
    bits = cf_f64_bits(d);
    if (((bits >> 52) & 0x7FFu) == 0x7FFu) {
        const char* t = "nan";

        if ((bits & (((cf_u64)1 << 52) - 1u)) == 0) {
            t = ((bits >> 63) != 0) ? "-inf" : "inf";
        }
        cf_value_text(&v, t, strlen(t));
        return cbor_emit(s, start, name, nlen, &v, CF_TRUE);
    }
    cf_value_real(&v, d);
    return cbor_emit(s, start, name, nlen, &v, CF_FALSE);
}

#endif /* CF_WITH_REAL */

static cf_bool cbor_item(cbor_scan* s, const char* name, size_t nlen);

/* A map or an array's entry: one level deeper, as the flattener and every
   other reader count them. */
static cf_bool cbor_enter(cbor_scan* s, size_t start) {
    if (++s->depth > CF_MAX_DEPTH) {
        return cf_error_set(s->err, CF_E_SOURCE, NULL, s->name,
                            "%s, byte %zu: nests deeper than %d levels", s->name, start,
                            (int)CF_MAX_DEPTH);
    }
    return CF_TRUE;
}

static cf_bool cbor_close(cbor_scan* s) {
    --s->depth;
    if (!s->out->end(s->out->ctx)) {
        return cf_error_set(s->err, CF_E_SOURCE, NULL, s->name,
                            "%s: the document could not be recorded", s->name);
    }
    return CF_TRUE;
}

/* A map key: a text string, read as a slice of the document or gathered
   from chunks. Anything else is refused by what it is. */
static cf_bool cbor_key(cbor_scan* s, const char** key, size_t* klen) {
    cbor_head h;

    if (!cbor_head_read(s, &h)) {
        return CF_FALSE;
    }
    if (h.brk) {
        return cbor_fail(s, h.off,
                         "not well-formed: a break outside an "
                         "indefinite-length item");
    }
    if (h.major == 3) {
        return cbor_string(s, &h, CBOR_KEY, NULL, 0, key, klen);
    }
    return cf_error_set(s->err, CF_E_SOURCE, NULL, s->name,
                        "%s, byte %zu: a map key that is %s, which a configuration "
                        "does not hold",
                        s->name, h.off, cbor_kind(h.major, h.info));
}

static cf_bool cbor_map(cbor_scan* s, const cbor_head* h, size_t start,
                        const char* name, size_t nlen) {
    cf_u64  left = h->arg;
    cf_bool root = (s->depth == 0) ? CF_TRUE : CF_FALSE;

    if (!h->indef && h->arg > (cf_u64)(cbor_left(s) / 2u)) {
        return cf_error_set(s->err, CF_E_SOURCE, NULL, s->name,
                            "%s, byte %zu: a map of %U pairs, and %zu bytes are left",
                            s->name, h->off, h->arg, cbor_left(s));
    }
    if (!cbor_enter(s, start)) {
        return CF_FALSE;
    }
    if (!s->out->begin_table(s->out->ctx, name, nlen)) {
        if (root) {
            return cf_error_set(s->err, CF_E_SOURCE, NULL, s->name, "%s: sink refused",
                                s->name);
        }
        return cbor_refused(s, start, name, nlen);
    }
    for (;;) {
        const char* key  = NULL;
        size_t      klen = 0;

        if (h->indef) {
            if (s->p >= s->end) {
                return cbor_fail(s, cbor_at(s), "the document ends inside a data item");
            }
            if (*s->p == 0xFFu) {
                ++s->p;
                break;
            }
        } else {
            if (left == 0) {
                break;
            }
            --left;
        }
        if (!cbor_key(s, &key, &klen)) {
            return CF_FALSE;
        }
        if (h->indef && s->p < s->end && *s->p == 0xFFu) {
            return cbor_fail(s, cbor_at(s),
                             "not well-formed: a key with no value "
                             "before the break");
        }
        if (!cbor_item(s, key, klen)) {
            return CF_FALSE;
        }
    }
    return cbor_close(s);
}

static cf_bool cbor_array(cbor_scan* s, const cbor_head* h, size_t start,
                          const char* name, size_t nlen) {
    cf_u64 left = h->arg;

    if (!h->indef && h->arg > (cf_u64)cbor_left(s)) {
        return cf_error_set(
            s->err, CF_E_SOURCE, NULL, s->name,
            "%s, byte %zu: an array of %U items, and %zu bytes are left", s->name,
            h->off, h->arg, cbor_left(s));
    }
    if (!cbor_enter(s, start)) {
        return CF_FALSE;
    }
    if (!s->out->begin_array(s->out->ctx, name, nlen)) {
        return cbor_refused(s, start, name, nlen);
    }
    for (;;) {
        if (h->indef) {
            if (s->p >= s->end) {
                return cbor_fail(s, cbor_at(s), "the document ends inside a data item");
            }
            if (*s->p == 0xFFu) {
                ++s->p;
                break;
            }
        } else {
            if (left == 0) {
                break;
            }
            --left;
        }
        if (!cbor_item(s, NULL, 0)) {
            return CF_FALSE;
        }
    }
    return cbor_close(s);
}

/* What a head, with its tags behind it, is: the value. */
static cf_bool cbor_value(cbor_scan* s, const cbor_head* h, size_t start,
                          const char* name, size_t nlen) {
    cf_value v;

    switch (h->major) {
    case 0:
    case 1:
        if (h->arg > (cf_u64)CF_I64_MAX) {
            return cbor_fail(s, h->off,
                             "an integer past the 64-bit signed range, which "
                             "a configuration does not hold");
        }
        cf_value_int(&v, (h->major == 0) ? (cf_i64)h->arg : -(cf_i64)h->arg - 1);
        return cbor_emit(s, start, name, nlen, &v, CF_FALSE);
    case 2:
        return cbor_fail(s, h->off,
                         "a byte string, which a configuration does not hold");
    case 3:
        return cbor_text_value(s, h, start, name, nlen);
    case 4:
        return cbor_array(s, h, start, name, nlen);
    case 5:
        return cbor_map(s, h, start, name, nlen);
    default:
        break;
    }
    /* Major 7. */
    switch (h->info) {
    case 20:
        cf_value_bool(&v, CF_FALSE);
        return cbor_emit(s, start, name, nlen, &v, CF_FALSE);
    case 21:
        cf_value_bool(&v, CF_TRUE);
        return cbor_emit(s, start, name, nlen, &v, CF_FALSE);
    case 22:
        cf_value_null(&v);
        return cbor_emit(s, start, name, nlen, &v, CF_FALSE);
    case 23:
        return cbor_fail(s, h->off, "undefined, which a configuration does not hold");
    case 25:
    case 26:
    case 27:
#if CF_WITH_REAL
        return cbor_float(s, h, start, name, nlen);
#else
        return cbor_fail(s, h->off, "this build has no floating point support");
#endif
    default:
        break;
    }
    return cf_error_set(s->err, CF_E_SOURCE, NULL, s->name,
                        "%s, byte %zu: simple value %u, which a configuration does not "
                        "hold",
                        s->name, h->off, (unsigned)h->arg);
}

/* One data item: its tags, then what they tag. A run of tags is a loop,
   not recursion. 55799 is read past; 0, 1 and 32 are read as their
   content, which is checked against the tag; any other tag is refused by
   its number before its content is read. */
static cf_bool cbor_item(cbor_scan* s, const char* name, size_t nlen) {
    size_t    start = cbor_at(s);
    cbor_head h;
    cf_u64    tag    = 0;
    cf_bool   tagged = CF_FALSE;

    for (;;) {
        if (!cbor_head_read(s, &h)) {
            return CF_FALSE;
        }
        if (h.major != 6 || h.indef) {
            break;
        }
        if (h.arg == 55799u) {
            continue;
        }
        if (tagged) {
            return cf_error_set(s->err, CF_E_SOURCE, NULL, s->name,
                                "%s, byte %zu: tag %U holds a tag, and must hold %s",
                                s->name, h.off, tag,
                                (tag == 1u) ? "a number" : "a text string");
        }
        if (h.arg != 0 && h.arg != 1u && h.arg != 32u) {
            return cf_error_set(
                s->err, CF_E_SOURCE, NULL, s->name,
                "%s, byte %zu: tag %U, which this reader does not read: "
                "it reads tags 0, 1, 32 and 55799",
                s->name, h.off, h.arg);
        }
        tagged = CF_TRUE;
        tag    = h.arg;
    }
    if (h.brk) {
        return cbor_fail(s, h.off,
                         "not well-formed: a break outside an "
                         "indefinite-length item");
    }
    if (tagged) {
        cf_bool ok;

        if (tag == 1u) {
            ok = (h.major == 0 || h.major == 1 ||
                  (h.major == 7 && h.info >= 25u && h.info <= 27u))
                     ? CF_TRUE
                     : CF_FALSE;
        } else {
            ok = (h.major == 3) ? CF_TRUE : CF_FALSE;
        }
        if (!ok) {
            return cf_error_set(s->err, CF_E_SOURCE, NULL, s->name,
                                "%s, byte %zu: tag %U holds %s, and must hold %s",
                                s->name, h.off, tag, cbor_kind(h.major, h.info),
                                (tag == 1u) ? "a number" : "a text string");
        }
    }
    return cbor_value(s, &h, start, name, nlen);
}

static cf_bool cbor_decode(const cf_blob* in, const cf_doc_sink* out, cf_error* err) {
    cbor_scan s;
    cbor_head h;

    s.base  = (const unsigned char*)in->p;
    s.p     = s.base;
    s.end   = s.base + ((in->p != NULL) ? in->n : 0);
    s.name  = (in->name != NULL) ? in->name : "<cbor>";
    s.out   = out;
    s.err   = err;
    s.depth = 0;

    if (s.p == s.end) {
        return cf_error_set(err, CF_E_SOURCE, NULL, s.name, "%s: the document is empty",
                            s.name);
    }
    /* The top: a map, after any number of 55799 tags. No byte order mark
       is skipped: EF is a CBOR simple value, not a mark. */
    for (;;) {
        if (!cbor_head_read(&s, &h)) {
            return CF_FALSE;
        }
        if (h.major == 6 && !h.indef && h.arg == 55799u) {
            continue;
        }
        break;
    }
    if (h.brk) {
        return cbor_fail(&s, h.off,
                         "not well-formed: a break outside an "
                         "indefinite-length item");
    }
    if (h.major != 5) {
        return cf_error_set(err, CF_E_SOURCE, NULL, s.name,
                            "%s, byte %zu: a configuration is a map at the top, and "
                            "this is %s",
                            s.name, h.off, cbor_kind(h.major, h.info));
    }
    if (!cbor_map(&s, &h, 0, NULL, 0)) {
        return CF_FALSE;
    }
    if (s.p != s.end) {
        return cbor_fail(&s, cbor_at(&s), "bytes after the document");
    }
    return CF_TRUE;
}

#if CF_WITH_EMIT

/* ================================================================== *
 * Writing
 *
 * Maps and arrays have an indefinite length (BF ... FF, 9F ... FF): the
 * events do not say how many members a container has until its end, and
 * the sink measures or writes in one pass, so a count cannot be written
 * before the members without a second walk. Text strings and numbers
 * are definite and in RFC 8949's preferred serialization (section 4.1).
 * The keys are in the walk's order, not deterministic encoding's.
 * ================================================================== */

typedef struct cbor_enc {
    cf_encoder_head head; /* first: cf_emit sets CF_ENCODE_ESCAPE_REFS */
    cf_sink*        out;
    int             depth;
} cbor_enc;

/* cf_emit keeps this on its own frame, in CF_ENCODER_STATE_MAX bytes. */
typedef char cbor_enc_fits[(sizeof(cbor_enc) <= CF_ENCODER_STATE_MAX) ? 1 : -1];

/* An initial byte and the n bytes of `arg` after it, big-endian. */
static void cbor_put(cf_sink* out, unsigned first, cf_u64 arg, size_t n) {
    unsigned char b[9];
    size_t        i;

    b[0] = (unsigned char)first;
    for (i = 0; i < n; ++i) {
        b[1 + i] = (unsigned char)((arg >> (8u * (unsigned)(n - 1 - i))) & 0xFFu);
    }
    cf_sink_write(out, (const char*)b, n + 1);
}

/* A head in its shortest form: the argument in the initial byte below
   24, else in the fewest of 1, 2, 4 or 8 bytes after it. */
static void cbor_put_head(cf_sink* out, unsigned major, cf_u64 arg) {
    unsigned m = major << 5;

    if (arg < 24u) {
        cbor_put(out, m | (unsigned)arg, 0, 0);
    } else if (arg <= 0xFFu) {
        cbor_put(out, m | 24u, arg, 1);
    } else if (arg <= 0xFFFFu) {
        cbor_put(out, m | 25u, arg, 2);
    } else if (arg <= 0xFFFFFFFFu) {
        cbor_put(out, m | 26u, arg, 4);
    } else {
        cbor_put(out, m | 27u, arg, 8);
    }
}

/* A text string; with `escape`, each ${ as $${, which the length counts. */
static void cbor_put_text(cf_sink* out, const char* p, size_t n, cf_bool escape) {
    size_t i, from = 0, extra = 0;

    if (escape) {
        for (i = 0; i + 1 < n; ++i) {
            if (p[i] == '$' && p[i + 1] == '{') {
                ++extra;
            }
        }
    }
    cbor_put_head(out, 3, (cf_u64)n + (cf_u64)extra);
    if (extra != 0) {
        for (i = 0; i + 1 < n; ++i) {
            if (p[i] == '$' && p[i + 1] == '{') {
                cf_sink_write(out, p + from, i + 1 - from);
                cf_sink_putc(out, '$');
                from = i + 1;
            }
        }
    }
    cf_sink_write(out, p + from, n - from);
}

static void cbor_put_int(cf_sink* out, cf_i64 i) {
    if (i >= 0) {
        cbor_put_head(out, 0, (cf_u64)i);
    } else {
        cbor_put_head(out, 1, (cf_u64)(-(i + 1))); /* -1 - i, never overflowing */
    }
}

#if CF_WITH_REAL

/* A float's bits are taken by memcpy into a cf_u32. */
typedef char cbor_float_is_32[(sizeof(float) == 4) ? 1 : -1];

/* The half-precision bits that hold the single whose bits are `s`
   exactly, if any: zero; a normal half; or a subnormal one, when the
   bits shifted out are zero. */
static cf_bool cbor_half(cf_u32 s, cf_u32* half) {
    cf_u32 sign = (s >> 31) << 15;
    cf_u32 e    = (s >> 23) & 0xFFu;
    cf_u32 m    = s & 0x7FFFFFu;
    long   x    = (long)e - 127;

    if (e == 0 && m == 0) {
        *half = sign;
        return CF_TRUE;
    }
    if (e == 0) {
        return CF_FALSE; /* a subnormal single is below every half */
    }
    if (x >= -14 && x <= 15 && (m & 0x1FFFu) == 0) {
        *half = sign | ((cf_u32)(x + 15) << 10) | (m >> 13);
        return CF_TRUE;
    }
    if (x >= -24 && x <= -15) {
        cf_u32   full  = 0x800000u | m;
        unsigned shift = (unsigned)(-(x + 1));

        if ((full & (((cf_u32)1 << shift) - 1u)) == 0) {
            *half = sign | (full >> shift);
            return CF_TRUE;
        }
    }
    return CF_FALSE;
}

/* A real as the shortest of half, single and double precision that holds
   it exactly; NaN and the infinities as halves. */
static void cbor_put_real(cf_sink* out, double d) {
    cf_u64 bits = cf_f64_bits(d);
    cf_u32 single, half;
    float  f;

    if (((bits >> 52) & 0x7FFu) == 0x7FFu) {
        if ((bits & (((cf_u64)1 << 52) - 1u)) != 0) {
            half = 0x7E00u;
        } else {
            half = ((bits >> 63) != 0) ? 0xFC00u : 0x7C00u;
        }
        cbor_put(out, 0xF9u, half, 2);
        return;
    }
    /* A double past FLT_MAX is not converted: that is undefined. The
       single is judged by its stored bits, so that no excess precision
       of a register can make it look exact. */
    if (d <= (double)FLT_MAX && d >= -(double)FLT_MAX) {
        f = (float)d;
        memcpy(&single, &f, sizeof single);
        if (cf_f64_from_f32_bits(single) == d) {
            if (cbor_half(single, &half)) {
                cbor_put(out, 0xF9u, half, 2);
            } else {
                cbor_put(out, 0xFAu, single, 4);
            }
            return;
        }
    }
    cbor_put(out, 0xFBu, bits, 8);
}

#endif /* CF_WITH_REAL */

static void cbor_put_byte(cf_sink* out, unsigned b) {
    cbor_put(out, b, 0, 0);
}

/* A map or an array: the document's own map has no name. Refused at the
   depth json.c refuses, CF_MAX_DEPTH levels with the document's: the
   walk refuses first, and this is the last line of defence. */
static cf_bool cbor_enc_open(cbor_enc* e, const char* name, size_t n, unsigned opener) {
    if (e->depth < -1 || e->depth >= CF_MAX_DEPTH - 1) {
        return CF_FALSE;
    }
    if (e->depth >= 0 && name != NULL) {
        cbor_put_text(e->out, name, n, CF_FALSE);
    }
    cbor_put_byte(e->out, opener);
    ++e->depth;
    return CF_TRUE;
}

static cf_bool cbor_enc_table(void* ctx, const char* name, size_t n) {
    return cbor_enc_open((cbor_enc*)ctx, name, n, 0xBFu);
}

static cf_bool cbor_enc_array(void* ctx, const char* name, size_t n) {
    return cbor_enc_open((cbor_enc*)ctx, name, n, 0x9Fu);
}

static cf_bool cbor_enc_scalar(void* ctx, const char* name, size_t n, const cf_value* v,
                               cf_bool textual, const cf_loc* at) {
    cbor_enc* e = (cbor_enc*)ctx;

    (void)textual;
    (void)at;
    if (name != NULL) {
        /* A key holds no $, so there is nothing in it to double. */
        cbor_put_text(e->out, name, n, CF_FALSE);
    }
    switch (v->type) {
    case CF_TYPE_TEXT:
        cbor_put_text(e->out, v->u.t.p, v->u.t.n,
                      ((e->head.flags & CF_ENCODE_ESCAPE_REFS) != 0) ? CF_TRUE
                                                                     : CF_FALSE);
        break;
    case CF_TYPE_NULL:
        cbor_put_byte(e->out, 0xF6u);
        break;
    case CF_TYPE_BOOL:
        cbor_put_byte(e->out, v->u.b ? 0xF5u : 0xF4u);
        break;
    case CF_TYPE_INT:
        cbor_put_int(e->out, v->u.i);
        break;
    case CF_TYPE_REAL:
#if CF_WITH_REAL
        cbor_put_real(e->out, v->u.r);
        break;
#else
        return CF_FALSE;
#endif
    }
    return CF_TRUE;
}

static cf_bool cbor_enc_end(void* ctx) {
    cbor_enc* e = (cbor_enc*)ctx;

    if (e->depth < 0) {
        return CF_FALSE;
    }
    --e->depth;
    cbor_put_byte(e->out, 0xFFu);
    return CF_TRUE;
}

static cf_bool cbor_encoder(cf_sink* out, void* state, cf_doc_sink* sink) {
    cbor_enc* e = (cbor_enc*)state;

    if (e == NULL || sink == NULL) {
        return CF_FALSE;
    }
    memset(e, 0, sizeof *e);
    e->out   = out;
    e->depth = -1;

    sink->size        = offsetof(cf_doc_sink, reserve_text);
    sink->begin_table = cbor_enc_table;
    sink->begin_array = cbor_enc_array;
    sink->scalar      = cbor_enc_scalar;
    sink->end         = cbor_enc_end;
    sink->ctx         = e;
    return CF_TRUE;
}

const cf_codec cf_codec_cbor = {sizeof(cf_codec),
                                "cbor",
                                ".cbor",
                                CF_CODEC_READ | CF_CODEC_WRITE | CF_CODEC_TYPED |
                                    CF_CODEC_ARRAYS | CF_CODEC_BINARY,
                                cbor_decode,
                                cbor_encoder,
                                sizeof(cbor_enc)};

#else /* !CF_WITH_EMIT: read only */

const cf_codec cf_codec_cbor = {
    sizeof(cf_codec), "cbor", ".cbor", CF_CODEC_READ | CF_CODEC_TYPED | CF_CODEC_ARRAYS,
    cbor_decode,      NULL,   0};

#endif /* CF_WITH_EMIT */

#endif /* CF_WITH_CBOR */
