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
static cf_bool probe_table(void* ctx, const char* name, size_t n) {
    (void)ctx;
    (void)name;
    (void)n;
    return CF_TRUE;
}

static cf_bool probe_array(void* ctx, const char* name, size_t n) {
    (void)name;
    (void)n;
    *(cf_bool*)ctx = CF_TRUE;
    return CF_TRUE;
}

static cf_bool probe_scalar(void* ctx, const char* name, size_t n, const cf_value* v,
                            cf_bool textual, const cf_loc* at) {
    (void)ctx;
    (void)name;
    (void)n;
    (void)v;
    (void)textual;
    (void)at;
    return CF_TRUE;
}

static cf_bool probe_end(void* ctx) {
    (void)ctx;
    return CF_TRUE;
}

/* CF_FALSE when the walk itself failed; `err` says why. */
static cf_bool find_array(const cf_keyspace* ks, const emit_filter* f, cf_bool* found,
                          cf_error* err) {
    cf_doc_sink probe;

    *found = CF_FALSE;
    memset(&probe, 0, sizeof probe);
    probe.size        = sizeof probe;
    probe.begin_table = probe_table;
    probe.begin_array = probe_array;
    probe.scalar      = probe_scalar;
    probe.end         = probe_end;
    probe.ctx         = found;
    return cf_doc_emit_where(ks, &probe, keep, f, err);
}

size_t cf_emit(const cf_config* c, const char* format, cf_emit_what what, int rank,
               char* buf, size_t cap, cf_error* err) {
    const cf_codec*    codec;
    const cf_keyspace* ks;
    cf_doc_sink        sink;
    cf_sink            out;
    emit_filter        f;
    /* The encoder's state, on the stack: CF_ENCODER_STATE_MAX, which
       follows the limits as the encoders' state does, and which each
       built-in encoder is asserted at compile time to fit. The check
       below is for a codec that does not. Aligned through the union
       because a char array is not aligned for a struct. */
    union {
        cf_i64 align;
        char   b[CF_ENCODER_STATE_MAX];
    } state;

    if (buf != NULL && cap > 0) {
        buf[0] = '\0';
    }
    if (c == NULL) {
        cf_error_set(err, CF_E_SOURCE, NULL, NULL, "nothing to write");
        return 0;
    }
    ks = cf_keys(c);

    codec = cf_codec_by_name(format);
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
        cf_bool array;

        if (!find_array(ks, &f, &array, err)) {
            return 0;
        }
        if (array) {
            /* The promise the INI encoder makes: the caller checks before
               encoding. Discovering it halfway through would leave a
               truncated file that looks like a complete one. */
            cf_error_set(err, CF_E_CAPABILITY, NULL, format,
                         "this configuration has an array and '%s' has no syntax "
                         "for one; use a format that does",
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
    if (!cf_doc_emit_where(ks, &sink, keep, &f, err)) {
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
