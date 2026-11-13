/*
 * emit.c - writing a configuration out. C89.
 *
 * The specification restricts write support to a file ConfigForge owns
 * and rewrites in full, and that restriction is the whole design here.
 * Editing a hand-written file in place means preserving its comments,
 * its ordering and its whitespace, which is a separate problem: the
 * parser would have to keep a concrete syntax tree that every codec
 * then has to maintain, and getting it slightly wrong silently destroys
 * something a person wrote.
 *
 * What people actually need from "save settings" is an overrides file,
 * which is what git config, systemd drop-ins and sudoers.d all are.
 */

#include "configforge/configforge.h"

#include <string.h>

#if CF_WITH_EMIT

typedef struct emit_filter {
    cf_emit_what     what;
    int              rank;
    const cf_defset* defs;
} emit_filter;

static cf_bool keep(const void* ctx, const cf_entry* e) {
    const emit_filter* f = (const emit_filter*)ctx;

    if (e->selected == NULL) {
        return CF_FALSE;
    }
    /* A command is never written out. --cf.explain asks a question; a
       file that records it would ask the question again on every run
       that reads the file. */
    {
        const cf_def* d = cf_defset_find(f->defs, e->key, e->keylen);
        if (d != NULL && (d->flags & CF_TRANSIENT) != 0) {
            return CF_FALSE;
        }
    }
    switch (f->what) {
    case CF_EMIT_ALL:
        return CF_TRUE;
    case CF_EMIT_CONFIGURED:
        /* Not "differs from the default value" but "did not come from
           the defaults source". The distinction shows up when someone
           sets a value that happens to equal the default: comparing
           values would drop it and let a later release move it, while
           provenance keeps the intent that was expressed. This library
           is about provenance, so it keeps the intent. */
        return (e->selected->source != CF_RANK_DEFAULT) ? CF_TRUE : CF_FALSE;
    case CF_EMIT_FROM_RANK:
        return (e->selected->source == f->rank) ? CF_TRUE : CF_FALSE;
    }
    return CF_FALSE;
}

/* A sink that writes nothing and notes whether it was given an array.
   Asked before encoding rather than discovered during it, because a
   codec that meets an array it cannot write has already emitted half a
   document.

   It is the same walk with the same filter as the real one, so "has an
   array" means exactly "would write one". Looking for a key called
   `count` took a table with a retry count for an array, and an array
   that was not going to be written for one that was. */
typedef struct emit_probe {
    cf_bool array; /* an array would be written                       */
    /* and one of them is more than a list of values - it holds a table
       or an array, or nothing - which CF_CODEC_SCALAR_ARRAYS cannot
       write: INI's name[] lines have no form for any of the three */
    cf_bool       beyond_values;
    int           depth;
    int           arrays; /* arrays open around the current place */
    unsigned char kind[CF_MAX_DEPTH + 2];
    unsigned char items[CF_MAX_DEPTH + 2]; /* an array's elements so far, 0 or 1 */
} emit_probe;

static cf_bool probe_open(emit_probe* p, unsigned char kind) {
    if (p->depth >= (int)sizeof p->kind) {
        return CF_FALSE;
    }
    if (p->arrays > 0) {
        p->beyond_values = CF_TRUE; /* a table or an array in an array */
    }
    p->items[p->depth]  = 0;
    p->kind[p->depth++] = kind;
    if (kind == 'a') {
        p->array = CF_TRUE;
        ++p->arrays;
    }
    return CF_TRUE;
}

static cf_bool probe_table(void* ctx, const char* name, size_t n) {
    (void)name;
    (void)n;
    return probe_open((emit_probe*)ctx, 't');
}

static cf_bool probe_array(void* ctx, const char* name, size_t n) {
    (void)name;
    (void)n;
    return probe_open((emit_probe*)ctx, 'a');
}

static cf_bool probe_scalar(void* ctx, const char* name, size_t n, const cf_value* v,
                            cf_bool textual, const cf_loc* at) {
    emit_probe* p = (emit_probe*)ctx;
    (void)name;
    (void)n;
    (void)v;
    (void)textual;
    (void)at;
    if (p->depth > 0) {
        p->items[p->depth - 1] = 1;
    }
    return CF_TRUE;
}

static cf_bool probe_end(void* ctx) {
    emit_probe* p = (emit_probe*)ctx;
    if (p->depth <= 0) {
        return CF_TRUE;
    }
    --p->depth;
    if (p->kind[p->depth] == 'a') {
        --p->arrays;
        if (p->items[p->depth] == 0) {
            p->beyond_values = CF_TRUE; /* an empty array */
        }
    }
    return CF_TRUE;
}

/* CF_FALSE when the walk itself failed; `err` says why. */
static cf_bool find_array(const cf_keyspace* ks, const emit_filter* f,
                          emit_probe* found, cf_error* err) {
    cf_doc_sink probe;

    memset(found, 0, sizeof *found);
    memset(&probe, 0, sizeof probe);
    probe.size        = sizeof probe;
    probe.begin_table = probe_table;
    probe.begin_array = probe_array;
    probe.scalar      = probe_scalar;
    probe.end         = probe_end;
    probe.ctx         = found;
    return cf_doc_emit_where(ks, &probe, keep, f, err);
}

typedef struct emit_literals {
    cf_doc_sink* encoded;
    cf_sink*     out;
    /* A binary encoder (CF_CODEC_BINARY) doubles each ${ itself, as it
       writes the text's length: nothing is done after it. */
    cf_bool binary;
} emit_literals;

static cf_bool literal_table(void* ctx, const char* name, size_t n) {
    cf_doc_sink* encoded = ((emit_literals*)ctx)->encoded;
    return encoded->begin_table(encoded->ctx, name, n);
}

static cf_bool literal_array(void* ctx, const char* name, size_t n) {
    cf_doc_sink* encoded = ((emit_literals*)ctx)->encoded;
    return encoded->begin_array(encoded->ctx, name, n);
}

static cf_bool literal_end(void* ctx) {
    cf_doc_sink* encoded = ((emit_literals*)ctx)->encoded;
    return encoded->end(encoded->ctx);
}

/* Each built-in writer leaves ${ intact. Keys cannot contain $, so
   only the scalar's text needs escaping. Work backwards over stored
   bytes, not the measured length: even a truncated output must be the
   prefix of the full escaped document, without allocating a copy. */
static void escape_written(cf_sink* out, size_t from) {
    size_t limit, end, i, extra = 0;
    char   next = '\0';

    if (out->buf == NULL || out->cap == 0) {
        return;
    }
    limit = out->cap - 1;
    end   = out->len < limit ? out->len : limit;
    if (from >= end) {
        return;
    }
    for (i = from; i + 1 < end; ++i) {
        if (out->buf[i] == '$' && out->buf[i + 1] == '{') {
            ++extra;
        }
    }
    i = end;
    while (i > from) {
        char c = out->buf[--i];
        if (extra < limit - i) {
            out->buf[i + extra] = c;
        }
        if (c == '$' && next == '{') {
            --extra;
            if (extra < limit - i) {
                out->buf[i + extra] = '$';
            }
        }
        next = c;
    }
}

static cf_bool literal_scalar(void* ctx, const char* name, size_t n, const cf_value* v,
                              cf_bool textual, const cf_loc* at) {
    emit_literals* literals = (emit_literals*)ctx;
    cf_doc_sink*   encoded  = literals->encoded;
    cf_sink*       out      = literals->out;
    size_t         from     = out->len;
    size_t         i, extra = 0;

    if (!encoded->scalar(encoded->ctx, name, n, v, textual, at)) {
        return CF_FALSE;
    }
    if (literals->binary) {
        return CF_TRUE;
    }
    if (v->type == CF_TYPE_TEXT) {
        for (i = 0; i + 1 < v->u.t.n; ++i) {
            if (v->u.t.p[i] == '$' && v->u.t.p[i + 1] == '{') {
                ++extra;
            }
        }
    }
    if (extra > (size_t)-1 - out->len) {
        return CF_FALSE;
    }
    if (extra != 0) {
        escape_written(out, from);
        out->len += extra;
    }
    return CF_TRUE;
}

size_t cf_emit(const cf_config* c, const char* format, cf_emit_what what, int rank,
               char* buf, size_t cap, cf_error* err) {
    const cf_codec*    codec;
    const cf_keyspace* ks;
    cf_doc_sink        sink;
    cf_doc_sink        literal_sink;
    emit_literals      literals;
    cf_sink            out;
    emit_filter        f;
    /* The encoder's state, on the stack: CF_ENCODER_STATE_MAX, which
       follows the limits as the encoders' state does, and which each
       built-in encoder is asserted at compile time to fit. The check
       below is for a codec that does not. Aligned through the union
       because a char array is not aligned for a struct; a binary
       encoder's state begins with the head, which is named here so that
       its flags are set without a cast. */
    union {
        cf_i64          align;
        cf_encoder_head head;
        char            b[CF_ENCODER_STATE_MAX];
    } state;

    if (buf != NULL && cap > 0) {
        buf[0] = '\0';
    }
    if (c == NULL) {
        cf_error_set(err, CF_E_SOURCE, NULL, NULL, "nothing to write");
        return 0;
    }
    ks = cf_keys(c);

    codec = cf_config_codec(c, format);
    if (codec == NULL) {
        const char* option = cf_format_option(format);
        if (option != NULL) {
            cf_error_set(err, CF_E_CAPABILITY, NULL, format,
                         "format '%s' is not compiled into this build; turn "
                         "on %s",
                         format, option);
        } else {
            cf_error_set(err, CF_E_CAPABILITY, NULL, format, "no format called '%s'",
                         format);
        }
        return 0;
    }
    if ((codec->caps & CF_CODEC_WRITE) == 0) {
        cf_error_set(err, CF_E_CAPABILITY, NULL, format,
                     "format '%s' can be read but not written", format);
        return 0;
    }
    f.what = what;
    f.rank = rank;
    f.defs = cf_defs(c);

    if ((codec->caps & CF_CODEC_ARRAYS) == 0) {
        emit_probe probe;

        if (!find_array(ks, &f, &probe, err)) {
            return 0;
        }
        /* The promise the encoders make: the caller checks before
           encoding. Discovering it halfway through would leave a
           truncated file that looks like a complete one. */
        if (probe.array && (codec->caps & CF_CODEC_SCALAR_ARRAYS) == 0) {
            cf_error_set(err, CF_E_CAPABILITY, NULL, format,
                         "this configuration has an array and '%s' has no syntax "
                         "for one; use a format that does",
                         format);
            return 0;
        }
        if (probe.beyond_values) {
            cf_error_set(err, CF_E_CAPABILITY, NULL, format,
                         "this configuration has an array of tables or of arrays, "
                         "or an empty one, and '%s' writes arrays of values only; "
                         "use a format that does",
                         format);
            return 0;
        }
    }
    if (codec->encoder_state_size > sizeof state.b) {
        cf_error_set(err, CF_E_MEMORY, NULL, format, "no room for the '%s' encoder",
                     format);
        return 0;
    }

    cf_sink_init(&out, buf, cap);
    if (!codec->encoder(&out, state.b, &sink)) {
        cf_error_set(err, CF_E_MEMORY, NULL, format, "encoder refused");
        return 0;
    }
    literals.binary = ((codec->caps & CF_CODEC_BINARY) != 0) ? CF_TRUE : CF_FALSE;
    if (literals.binary) {
        state.head.flags |= CF_ENCODE_ESCAPE_REFS;
    }
    literals.encoded = &sink;
    literals.out     = &out;
    memset(&literal_sink, 0, sizeof literal_sink);
    literal_sink.size        = sizeof literal_sink;
    literal_sink.begin_table = literal_table;
    literal_sink.begin_array = literal_array;
    literal_sink.scalar      = literal_scalar;
    literal_sink.end         = literal_end;
    literal_sink.ctx         = &literals;
    if (!cf_doc_emit_where(ks, &literal_sink, keep, &f, err)) {
        /* The walk may have written part of the document before it failed,
           and half a file reads as a whole one to anything that looks no
           further than the buffer. */
        if (buf != NULL && cap > 0) {
            buf[0] = '\0';
        }
        return 0;
    }
    cf_sink_terminate(&out);
    return out.len;
}

#else /* !CF_WITH_EMIT */

size_t cf_emit(const cf_config* c, const char* format, cf_emit_what what, int rank,
               char* buf, size_t cap, cf_error* err) {
    (void)c;
    (void)what;
    (void)rank;
    if (buf != NULL && cap > 0) {
        buf[0] = '\0';
    }
    cf_error_set(err, CF_E_CAPABILITY, NULL, format,
                 "writing a configuration out needs CF_WITH_EMIT, and this build "
                 "was compiled without it");
    return 0;
}

#endif /* CF_WITH_EMIT */
