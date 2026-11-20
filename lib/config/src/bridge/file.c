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
#include "configforge/cf_encoding.h"
#include "configforge/cf_entry.h"
#include "configforge/cf_platform.h"
#include "configforge/cf_source.h"
#include "configforge/cf_alloc.h"
#include "configforge/cf_build_options.h"
#include "configforge/cf_error.h"
#include "configforge/cf_keyspace.h"
#include "configforge/cf_port.h"
#include "configforge/cf_serialize.h"
#include "configforge/cf_text.h"

#include <string.h>
/* iwyu asks for this include and against it by turns (cf_keyspace.h
   and cf_error.h carry each other's names): it is kept. */
/* IWYU pragma: begin_keep */
#include "configforge/cf_encoding.h"
/* IWYU pragma: end_keep */

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
    const cf_codec*        used;     /* what the last load read with */
    cf_detect_how          how;      /* and how it was chosen */
    const char*            encoding; /* the encoding it read, or NULL */
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
    if (!cf_flatten_decode(&fl, codec, blob, path, err)) {
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

/* ------------------------------------------------------------------ *
 * Another encoding (D-108)
 * ------------------------------------------------------------------ */

/* Whether a codec reads by an encoding of its own or has none: XML, which
   declares it, and the binary formats. Nothing is recoded for it. */
static cf_bool file_declares(const cf_codec* codec) {
    return (codec != NULL &&
            (codec->caps & (CF_CODEC_BINARY | CF_CODEC_SELF_ENCODED)) != 0)
               ? CF_TRUE
               : CF_FALSE;
}

/* `v`, `digits` hexadecimal digits in upper case: cf_fmt has no width. */
static void file_hex(unsigned v, size_t digits, char* out) {
    static const char hex[] = "0123456789ABCDEF";
    size_t            i;

    for (i = 0; i < digits; ++i) {
        out[i] = hex[(v >> (4 * (digits - 1 - i))) & 15u];
    }
    out[digits] = '\0';
}

/* The refusal of the byte at p[bad] in an encoding, as the readers write
   one: the origin, the line and the column in the original's units. */
static cf_bool file_refuse_at(cf_error* err, const char* path, unsigned enc,
                              const char* p, size_t n, size_t bad) {
    cf_u32   line = 1, col = 1;
    unsigned unit;
    char     hex[8];

    if (enc != (unsigned)CF_ENCODING_UTF16LE && enc != (unsigned)CF_ENCODING_UTF16BE) {
        cf_text_place(p, bad, 1, &line, &col);
        file_hex((unsigned)(unsigned char)p[bad], 2, hex);
        return cf_error_set(err, CF_E_SOURCE, NULL, path,
                            "%s:%u:%u: byte 0x%s has no character in %s", path,
                            (unsigned)line, (unsigned)col, hex, cf_encoding_name(enc));
    }
    if (n - bad < 2) {
        return cf_error_set(err, CF_E_SOURCE, NULL, path,
                            "%s: ends in half a UTF-16 code unit", path);
    }
    cf_text_place(p, bad, enc, &line, &col);
    if (enc == (unsigned)CF_ENCODING_UTF16BE) {
        unit = ((unsigned)(unsigned char)p[bad] << 8) |
               (unsigned)(unsigned char)p[bad + 1];
    } else {
        unit = ((unsigned)(unsigned char)p[bad + 1] << 8) |
               (unsigned)(unsigned char)p[bad];
    }
    file_hex(unit, 4, hex);
    return cf_error_set(err, CF_E_SOURCE, NULL, path,
                        "%s:%u:%u: an unpaired UTF-16 surrogate 0x%s", path,
                        (unsigned)line, (unsigned)col, hex);
}

/* The encoding the first bytes declare: UTF-16's byte-order mark, or
   none, with the bytes it takes; CF_FALSE when they are UTF-32's, which
   is refused by name. */
static cf_bool file_mark(const cf_blob* in, unsigned* enc, size_t* skip) {
    const unsigned char* u = (const unsigned char*)in->p;

    *enc  = CF_ENCODING_UTF8;
    *skip = 0;
    if (in->n >= 4 && ((u[0] == 0xFF && u[1] == 0xFE && u[2] == 0 && u[3] == 0) ||
                       (u[0] == 0 && u[1] == 0 && u[2] == 0xFE && u[3] == 0xFF))) {
        return CF_FALSE;
    }
    if (in->n >= 2 && u[0] == 0xFF && u[1] == 0xFE) {
        *enc  = CF_ENCODING_UTF16LE;
        *skip = 2;
    } else if (in->n >= 2 && u[0] == 0xFE && u[1] == 0xFF) {
        *enc  = CF_ENCODING_UTF16BE;
        *skip = 2;
    }
    return CF_TRUE;
}

cf_bool cfi_source_recode(unsigned flags, const cf_codec* codec, const cf_blob* in,
                          cf_blob* out, unsigned* used, cf_arena* arena,
                          cf_error* err) {
    unsigned    enc  = flags & CF_ENCODING_MASK;
    const char* path = (in->name != NULL) ? in->name : "";
    size_t      skip = 0;
    size_t      units, cap, wrote;
    size_t      bad = 0;
    char*       buf;

    *out  = *in;
    *used = CF_ENCODING_UTF8;
    if (file_declares(codec)) {
        if (enc != 0) {
            return cf_error_set(err, CF_E_SOURCE, NULL, path,
                                "'%s': %s declares its own encoding or has none; name "
                                "no encoding for it",
                                path, codec->name);
        }
        return CF_TRUE;
    }
    if (enc == 0) {
        if (!file_mark(in, &enc, &skip)) {
            return cf_error_set(err, CF_E_SOURCE, NULL, path,
                                "'%s' is UTF-32, which this library does not read; "
                                "convert it to UTF-8",
                                path);
        }
        if (enc == 0) {
            return CF_TRUE;
        }
    }
    units = in->n - skip;
    if (enc == (unsigned)CF_ENCODING_UTF16LE || enc == (unsigned)CF_ENCODING_UTF16BE) {
        units /= 2;
    }
    if (units > ((size_t)-1 - 1) / 3) {
        return cf_error_set(err, CF_E_MEMORY, NULL, path, "no room to read '%s'", path);
    }
    cap = units * 3;
    buf = (char*)cf_arena_alloc(arena, cap + 1);
    if (buf == NULL) {
        return cf_error_set(err, CF_E_MEMORY, NULL, path, "no room to read '%s'", path);
    }
    wrote = cf_encoding_recode(enc, in->p + skip, in->n - skip, buf, cap, &bad);
    if (wrote == (size_t)-1) {
        return file_refuse_at(err, path, enc, in->p + skip, in->n - skip, bad);
    }
    buf[wrote] = '\0';
    out->p     = buf;
    out->n     = wrote;
    *used      = enc;
    return CF_TRUE;
}

cf_bool cfi_source_text(unsigned flags, const cf_codec* const* own, size_t nown,
                        const cf_codec** codec, const cf_blob* raw, cf_blob* text,
                        unsigned* used, cf_bool* by_content, cf_arena* arena,
                        cf_error* err) {
    *by_content = CF_FALSE;
    if (!cfi_source_recode(flags, *codec, raw, text, used, arena, err)) {
        return CF_FALSE;
    }
    if (*codec == NULL) {
        /* Bytes that are not UTF-8 are refused before the content decides
           (D-108), unless a format that declares its own encoding or has
           none (XML, CBOR, TLV) reads them. */
        cf_bool utf8 = cf_encoding_check(raw->name, text->p, text->n, NULL);

        /* A recoded XML text still says encoding="UTF-16", which the
           reader refuses in UTF-8 bytes: XML is tried on what was given. */
        if (*used != CF_ENCODING_UTF8) {
            const cf_codec* xml = cf_codec_by_name("xml");
            cf_detect       d;

            if (cf_codec_can_read(xml) &&
                cf_codec_detect(&xml, 1, raw, NULL, arena, &d) == xml) {
                *codec = xml;
            }
        }
        if (*codec == NULL) {
            *codec = cf_source_by_content(own, nown, text, raw->name, arena, err);
        }
        if (!utf8 && !file_declares(*codec)) {
            return cf_encoding_check(raw->name, text->p, text->n, err);
        }
        if (*codec == NULL) {
            return CF_FALSE;
        }
        *by_content = CF_TRUE;
        /* The format declares its own encoding (XML in UTF-16): it reads
           what was given, as expat reads a mark itself, and an encoding
           the program named for it is refused. */
        if (*used != CF_ENCODING_UTF8 && file_declares(*codec)) {
            unsigned was = *used;

            if (!cfi_source_recode(flags, *codec, raw, text, used, arena, err)) {
                return CF_FALSE;
            }
            *used = was;
        }
    }
    return CF_TRUE;
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
    cf_blob         text; /* the same bytes as UTF-8 */
    cf_arena*       ar = cf_ks_arena(ks);
    cf_arena*       bytes;
    unsigned        enc;
    cf_bool         by_content;
    cf_bool         ok;

    st->failed   = CF_TRUE; /* until it is read */
    st->used     = NULL;
    st->encoding = NULL;
    st->how      = CF_DETECT_NONE;
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
    /* The bytes as UTF-8, and the format when the name gave none (D-108). */
    if (!cfi_source_text(st->flags, st->codecs, st->ncodecs, &codec, &blob, &text, &enc,
                         &by_content, bytes, &st->err)) {
        cf_blob_arena_done(bytes, ar);
        return CF_FALSE;
    }
    if (by_content) {
        st->how = CF_DETECT_CONTENT;
    }
    st->used     = codec;
    st->encoding = cf_encoding_name(enc);
    ok           = cf_source_read_blob(s, ks, codec, &text, st->path, &st->err);
    if (!ok && st->how != CF_DETECT_CONTENT) {
        file_hint(st, codec, &text, bytes);
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

    if (s == NULL || path == NULL || st == NULL || state_size < sizeof(file_state) ||
        (flags & CF_ENCODING_MASK) > (unsigned)CF_ENCODING_LAST) {
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
    if (!cf_codec_can_read(codec)) {
        return CF_FALSE;
    }
    if (!cf_source_file(s, name, rank, path, NULL, flags, state, state_size)) {
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

const char* cf_source_file_encoding(const cf_source* s) {
    const file_state* st = file_state_of(s);

    return (st != NULL) ? st->encoding : NULL;
}
