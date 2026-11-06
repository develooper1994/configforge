/*
 * file.c - a configuration file as a source. C89.
 *
 * The source does not parse: it reads bytes, picks a codec by
 * extension, and hands the events to the flattener. A format that is
 * not compiled in fails loudly and names the option to turn on, which
 * is the rule that matters: a configuration system whose purpose is
 * explaining where values come from must not have a build option that
 * makes a file invisible.
 */

#include "configforge/cf_flatten.h"
#include "configforge/cf_define.h"
#include "configforge/cf_source.h"

#include <string.h>

typedef struct file_state {
    const char*     path;
    const char*     format; /* explicit override, or NULL for by-extension */
    const cf_codec* codec;  /* handed over by the caller, or NULL */
    unsigned        flags;
    cf_error        err;
    cf_bool         failed;
} file_state;

/* A document's bytes through its codec into the key space, at the
   source's rank. Shared with the drop-in directory (dir.c). */
cf_bool cf_source_read_blob(const cf_source* s, cf_keyspace* ks, const cf_codec* codec,
                            const cf_blob* blob, const char* path, cf_error* err) {
    cf_flatten fl;

    cfi_ks_expect_lines(ks, blob->p, blob->n); /* sizes the key space once; a guess */
    if (!cf_flatten_init(&fl, ks, s->rank)) {
        return cf_error_set(err, CF_E_MEMORY, NULL, path, "cannot start reading '%s'",
                            path);
    }
    if ((codec->caps & CF_CODEC_TYPED) == 0 && s->ref != NULL) {
        cf_flatten_lists(&fl, (const cf_defset*)s->ref); /* the definitions decide */
    }
    if (!codec->decode(blob, cf_flatten_sink(&fl), err)) {
        return CF_FALSE;
    }
    if (fl.failed || fl.overflow) {
        return cf_error_set(err, CF_E_KEY, NULL, path,
                            "'%s' contains a key this build cannot hold", path);
    }
    return CF_TRUE;
}

static cf_bool file_load(const cf_source* s, cf_keyspace* ks) {
    file_state*     st = (file_state*)s->ext;
    const cf_codec* codec;
    cf_blob         blob;
    cf_arena*       ar = cf_ks_arena(ks);
    cf_arena*       bytes;
    cf_bool         ok;

    st->failed = CF_FALSE;
    cf_error_clear(&st->err);

    if (st->codec != NULL) {
        codec = st->codec;
    } else if (st->format != NULL) {
        codec = cf_codec_by_name(st->format);
        if (codec == NULL) {
            st->failed = CF_TRUE;
            return cf_error_set(&st->err, CF_E_CAPABILITY, NULL, st->path,
                                "format '%s' is not compiled into this build",
                                st->format);
        }
    } else {
        codec = cf_codec_by_path(st->path);
        if (codec == NULL) {
            const char* fmt = cf_format_of_path(st->path);
            st->failed      = CF_TRUE;
            if (fmt != NULL) {
                return cf_error_set(&st->err, CF_E_CAPABILITY, NULL, st->path,
                                    "source '%s' requires %s support; this "
                                    "build was compiled without %s",
                                    st->path, fmt, cf_format_option(fmt));
            }
            return cf_error_set(&st->err, CF_E_CAPABILITY, NULL, st->path,
                                "cannot tell the format of '%s' from its "
                                "name; set one explicitly",
                                st->path);
        }
    }
    if ((codec->caps & CF_CODEC_READ) == 0) {
        st->failed = CF_TRUE;
        return cf_error_set(&st->err, CF_E_CAPABILITY, NULL, st->path,
                            "format '%s' can be written but not read", codec->name);
    }

    /* The file's bytes only while it is read (D-61). */
    bytes = cf_blob_arena(ar);
    if (bytes == NULL) {
        st->failed = CF_TRUE;
        return cf_error_set(&st->err, CF_E_MEMORY, NULL, st->path,
                            "no room to read '%s'", st->path);
    }
    if (!cf_blob_file(&blob, bytes, st->path, &st->err)) {
        cf_blob_arena_done(bytes, ar);
        if ((st->flags & CF_OPTIONAL) != 0) {
            /* A missing optional file is not an error, and it is also
               not silence: it shows up in explain() as a source that
               supplied nothing. */
            cf_error_clear(&st->err);
            return CF_TRUE;
        }
        st->failed = CF_TRUE;
        return CF_FALSE;
    }
    ok = cf_source_read_blob(s, ks, codec, &blob, st->path, &st->err);
    cf_blob_arena_done(bytes, ar);
    if (!ok) {
        st->failed = CF_TRUE;
    }
    return ok;
}

static void file_destroy(cf_source* s) {
    (void)s;
}

static const char* file_path(const cf_source* s) {
    const file_state* st = (const file_state*)s->ext;
    return (st != NULL) ? st->path : NULL;
}

static const cf_source_vtable cf_file_vt = {
    sizeof(cf_source_vtable), "file", file_load, file_destroy, file_path, NULL};

cf_bool cf_source_file(cf_source* s, const char* name, int rank, const char* path,
                       const char* format, unsigned flags, void* state,
                       size_t state_size) {
    file_state* st = (file_state*)state;

    if (s == NULL || path == NULL || st == NULL || state_size < sizeof(file_state)) {
        return CF_FALSE;
    }
    memset(st, 0, sizeof *st);
    st->path     = path;
    st->format   = format;
    st->flags    = flags;
    st->err.size = sizeof(cf_error);

    s->vt   = &cf_file_vt;
    s->name = (name != NULL) ? name : path;
    s->rank = rank;
    s->ext  = st;
    s->ref  = NULL;
    return CF_TRUE;
}

cf_bool cf_source_file_codec(cf_source* s, const char* name, int rank, const char* path,
                             const cf_codec* codec, unsigned flags, void* state,
                             size_t state_size) {
    if (codec == NULL || codec->decode == NULL ||
        !cf_source_file(s, name, rank, path, NULL, flags, state, state_size)) {
        return CF_FALSE;
    }
    ((file_state*)state)->codec = codec;
    return CF_TRUE;
}

/* Borrowed, as cf_source_env_defs's is: the definitions outlive the
   source, both being the configuration's. A directory source keeps
   them in the same place, so one setter serves both. */
void cf_source_file_defs(cf_source* s, const cf_defset* ds) {
    if (s != NULL) {
        s->ref = ds;
    }
}

size_t cf_source_file_state_size(void) {
    return sizeof(file_state);
}

const cf_error* cf_source_file_error(const cf_source* s) {
    const file_state* st;

    if (s == NULL || s->vt != &cf_file_vt || s->ext == NULL) {
        return NULL;
    }
    st = (const file_state*)s->ext;
    return st->failed ? &st->err : NULL;
}
