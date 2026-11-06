/*
 * file.c - a configuration file as a source. C89.
 *
 * The source does not parse: it reads bytes, picks a codec - the format
 * named, else the extension's, else the content's (D-73) - and hands
 * the events to the flattener. A format that is not compiled in fails
 * loudly and names the option to turn on, which is the rule that
 * matters: a configuration system whose purpose is explaining where
 * values come from must not have a build option that makes a file
 * invisible.
 *
 * A document the program holds in memory is the same source with its
 * bytes given instead of a path (cf_source_document), so there is one
 * way of choosing a format and one of reading it.
 */

#include "configforge/cf_flatten.h"
#include "configforge/cf_define.h"
#include "configforge/cf_entry.h"
#include "configforge/cf_platform.h"
#include "configforge/cf_source.h"

#include <string.h>

typedef struct file_state {
    const char*            path;   /* a document's name */
    const char*            format; /* explicit override, or NULL */
    const cf_codec*        codec;  /* handed over by the caller, or NULL */
    const cf_codec* const* codecs; /* the program's own, looked at first */
    size_t                 ncodecs;
    const char*            bytes; /* a document's bytes */
    size_t                 nbytes;
    cf_bool                document;
    const char*            envvar; /* a document held in this variable, or NULL */
    unsigned               flags;
    cf_error               err;
    cf_bool                failed;
    const cf_codec*        used; /* what the last load read with */
    cf_detect_how          how;  /* and how it was chosen */
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

/* The codec the name or the program chooses, and how: the codec handed
   over, the format named, or the extension's - the program's own first
   in each. CF_TRUE with *codec NULL when the name gives none, which is
   for the content to decide; CF_FALSE with the error set when it names a
   format this build lacks. */
static cf_bool file_named(file_state* st, const cf_codec** codec) {
    *codec = NULL;
    if (st->codec != NULL) {
        *codec  = st->codec;
        st->how = CF_DETECT_NAMED;
    } else if (st->format != NULL) {
        *codec = cf_codec_among(st->codecs, st->ncodecs, st->format, NULL);
        if (*codec == NULL) {
            *codec = cf_codec_by_name(st->format);
        }
        if (*codec == NULL) {
            return cf_error_set(&st->err, CF_E_CAPABILITY, NULL, st->path,
                                "format '%s' is not compiled into this build",
                                st->format);
        }
        st->how = CF_DETECT_NAMED;
    } else {
        *codec = cf_codec_among(st->codecs, st->ncodecs, NULL, st->path);
        if (*codec == NULL) {
            *codec = cf_codec_by_path(st->path);
        }
        if (*codec != NULL) {
            st->how = CF_DETECT_EXTENSION;
        } else if (cf_format_of_path(st->path) != NULL) {
            const char* fmt = cf_format_of_path(st->path);
            return cf_error_set(&st->err, CF_E_CAPABILITY, NULL, st->path,
                                "source '%s' requires %s support; this "
                                "build was compiled without %s",
                                st->path, fmt, cf_format_option(fmt));
        }
    }
    return CF_TRUE;
}

/* The name gave no format: the content decides (D-73), or the load fails
   saying what was tried. `scratch` is the trial readers' work space.
   Shared with the drop-in directory (dir.c), whose files are chosen the
   same way. */
const cf_codec* cf_source_by_content(const cf_codec* const* own, size_t nown,
                                     const cf_blob* blob, const char* path,
                                     cf_arena* scratch, cf_error* err) {
    cf_detect       d;
    const cf_codec* codec = cf_codec_detect(own, nown, blob, NULL, scratch, &d);

    if (codec != NULL) {
        return codec;
    }
    if (d.first != NULL) {
        cf_error_set(err, CF_E_CAPABILITY, NULL, path,
                     "cannot tell the format of '%s': %s and %s both read it, and "
                     "differently; set one explicitly",
                     path, d.first->name, d.second->name);
    } else if (blob->n == 0) {
        cf_error_set(err, CF_E_CAPABILITY, NULL, path,
                     "cannot tell the format of '%s': it is empty; set one explicitly",
                     path);
    } else {
        cf_error_set(err, CF_E_CAPABILITY, NULL, path,
                     "cannot tell the format of '%s' from its name or its content; "
                     "set one explicitly",
                     path);
    }
    return NULL;
}

/* A format the name chose could not read the bytes: the error stays as it
   was, and adds the format the content looks like - a renamed file is the
   commonest cause. */
static void file_hint(file_state* st, const cf_codec* tried, const cf_blob* blob,
                      cf_arena* scratch) {
    cf_detect       d;
    const cf_codec* like;
    char            message[CF_MESSAGE_MAX];
    char            key[CF_KEY_MAX];
    char            source[CF_SOURCE_MAX];
    int             code = st->err.code;

    if (st->codec != NULL) {
        return; /* the program handed that codec over: no second opinion */
    }
    like = cf_codec_detect(st->codecs, st->ncodecs, blob, NULL, scratch, &d);
    if (like == NULL || like == tried) {
        return;
    }
    memcpy(message, st->err.message, sizeof message);
    memcpy(key, st->err.key, sizeof key);
    memcpy(source, st->err.source, sizeof source);
    cf_error_set(&st->err, (cf_code)code, key[0] != '\0' ? key : NULL,
                 source[0] != '\0' ? source : NULL, "%s (the content looks like %s)",
                 message, like->name);
}

static cf_bool file_load(const cf_source* s, cf_keyspace* ks) {
    file_state*     st = (file_state*)s->ext;
    const cf_codec* codec;
    cf_blob         blob;
    cf_arena*       ar = cf_ks_arena(ks);
    cf_arena*       bytes;
    cf_bool         ok;

    st->failed = CF_TRUE; /* until it is read */
    st->used   = NULL;
    st->how    = CF_DETECT_NONE;
    cf_error_clear(&st->err);

    if (!file_named(st, &codec)) {
        return CF_FALSE;
    }
    if (codec != NULL && (codec->caps & CF_CODEC_READ) == 0) {
        return cf_error_set(&st->err, CF_E_CAPABILITY, NULL, st->path,
                            "format '%s' can be written but not read", codec->name);
    }

    /* The file's bytes only while it is read (D-61). */
    bytes = cf_blob_arena(ar);
    if (bytes == NULL) {
        return cf_error_set(&st->err, CF_E_MEMORY, NULL, st->path,
                            "no room to read '%s'", st->path);
    }
    if (st->envvar != NULL) {
        /* Looked up at every load: a reload sees what the variable holds
           now. The value is the process's own, and only read here. */
        size_t      n     = 0;
        const char* value = cf_env_value(cf_platform_environ(), st->envvar, &n);

        if (value == NULL) {
            cf_blob_arena_done(bytes, ar);
            if ((st->flags & CF_OPTIONAL) != 0) {
                st->failed = CF_FALSE; /* not set, and allowed not to be */
                return CF_TRUE;
            }
            return cf_error_set(&st->err, CF_E_SOURCE, NULL, st->path,
                                "the environment variable '%s' is not set", st->path);
        }
        cf_blob_mem(&blob, value, n, st->path);
    } else if (st->document) {
        cf_blob_mem(&blob, st->bytes, st->nbytes, st->path);
    } else if (!cf_blob_file(&blob, bytes, st->path, &st->err)) {
        cf_blob_arena_done(bytes, ar);
        if ((st->flags & CF_OPTIONAL) != 0) {
            /* A missing optional file is not an error, and it is also
               not silence: it shows up in explain() as a source that
               supplied nothing. */
            cf_error_clear(&st->err);
            st->failed = CF_FALSE;
            return CF_TRUE;
        }
        return CF_FALSE;
    }
    if (codec == NULL) {
        codec = cf_source_by_content(st->codecs, st->ncodecs, &blob, st->path, bytes,
                                     &st->err);
        if (codec == NULL) {
            cf_blob_arena_done(bytes, ar);
            return CF_FALSE;
        }
        st->how = CF_DETECT_CONTENT;
    }
    st->used = codec;
    ok       = cf_source_read_blob(s, ks, codec, &blob, st->path, &st->err);
    if (!ok && st->how != CF_DETECT_CONTENT) {
        file_hint(st, codec, &blob, bytes);
    }
    cf_blob_arena_done(bytes, ar);
    st->failed = ok ? CF_FALSE : CF_TRUE;
    return ok;
}

static void file_destroy(cf_source* s) {
    (void)s;
}

static const char* file_path(const cf_source* s) {
    const file_state* st = (const file_state*)s->ext;
    return (st != NULL && !st->document) ? st->path : NULL;
}

static const cf_source_vtable cf_file_vt = {
    sizeof(cf_source_vtable), "file", file_load, file_destroy, file_path, NULL};

/* The same reading, from bytes held in memory: there is no file to stamp
   or to watch, so no path. */
static const cf_source_vtable cf_document_vt = {
    sizeof(cf_source_vtable), "document", file_load, file_destroy, file_path, NULL};

/* The state of a file or a document source, or NULL for any other. */
static file_state* file_state_of(const cf_source* s) {
    if (s == NULL || s->ext == NULL ||
        (s->vt != &cf_file_vt && s->vt != &cf_document_vt)) {
        return NULL;
    }
    return (file_state*)s->ext;
}

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

cf_bool cf_source_document(cf_source* s, const char* name, int rank, const char* bytes,
                           size_t n, const char* format, unsigned flags, void* state,
                           size_t state_size) {
    file_state* st = (file_state*)state;

    if (name == NULL || (bytes == NULL && n > 0) ||
        !cf_source_file(s, name, rank, name, format, flags, state, state_size)) {
        return CF_FALSE;
    }
    st->bytes    = bytes;
    st->nbytes   = n;
    st->document = CF_TRUE;
    s->vt        = &cf_document_vt;
    return CF_TRUE;
}

cf_bool cf_source_env_document(cf_source* s, const char* name, int rank,
                               const char* variable, const char* format, unsigned flags,
                               void* state, size_t state_size) {
    if (variable == NULL || variable[0] == '\0' || strchr(variable, '=') != NULL ||
        !cf_source_document(s, (name != NULL) ? name : variable, rank, "", 0, format,
                            flags, state, state_size)) {
        return CF_FALSE;
    }
    /* What explain() shows for the document is the variable: what a
       reader would have to change. */
    ((file_state*)state)->path   = variable;
    ((file_state*)state)->envvar = variable;
    return CF_TRUE;
}

void cf_source_file_codecs(cf_source* s, const struct cf_codec* const* codecs,
                           size_t n) {
    file_state* st = file_state_of(s);

    if (st != NULL) {
        st->codecs  = codecs;
        st->ncodecs = (codecs != NULL) ? n : 0;
    }
}

const char* cf_source_file_format(const cf_source* s, cf_detect_how* how) {
    const file_state* st = file_state_of(s);

    if (how != NULL) {
        *how = (st != NULL) ? st->how : CF_DETECT_NONE;
    }
    return (st != NULL && st->used != NULL) ? st->used->name : NULL;
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
    const file_state* st = file_state_of(s);

    return (st != NULL && st->failed) ? &st->err : NULL;
}
