/*
 * config.c - the builder and the configuration facade. C89.
 *
 * Everything here is assembly. The work is done by the key space, the
 * sources and the definition set; this file exists so that a caller
 * never has to know that.
 */

#include "configforge/configforge.h"
#include "configforge/cf_platform.h"

#include <stddef.h>
#include <string.h>

/* What D-51 rests on, checked by the compiler rather than by a reader:
   every sized struct begins with its size, so a library and a program
   of different 1.x releases agree where to find it; and the integer
   types are the widths the interface says. */
CF_STATIC_ASSERT(offsetof(cf_error, size) == 0, error_size_first);
CF_STATIC_ASSERT(offsetof(cf_def, size) == 0, def_size_first);
CF_STATIC_ASSERT(offsetof(cf_source_vtable, size) == 0, vtable_size_first);
CF_STATIC_ASSERT(offsetof(cf_codec, size) == 0, codec_size_first);
CF_STATIC_ASSERT(offsetof(cf_doc_sink, size) == 0, sink_size_first);
CF_STATIC_ASSERT(offsetof(cf_live_status, size) == 0, status_size_first);
CF_STATIC_ASSERT(offsetof(cf_live_options, size) == 0, options_size_first);
CF_STATIC_ASSERT(offsetof(cf_log_event, size) == 0, log_size_first);
CF_STATIC_ASSERT(offsetof(cf_report_info, size) == 0, report_info_size_first);
CF_STATIC_ASSERT(sizeof(cf_i64) == 8 && sizeof(cf_u64) == 8, i64_is_64_bits);
CF_STATIC_ASSERT(sizeof(cf_i32) == 4 && sizeof(cf_u32) == 4, i32_is_32_bits);

#define CF_BUILDER_SOURCES 16

struct cf_builder {
    cf_arena*         arena;
    cf_source         sources[CF_BUILDER_SOURCES];
    size_t            nsources;
    cf_defset         defs;
    cf_unknown_policy unknown;
    cf_bool           track_reads;

    /* The first failure, held until create reports it. A definition
       block should read as a block rather than as an error check after
       every line. */
    cf_error pending;
    cf_bool  failed;
    char     prefix[CF_SOURCE_MAX];

    /* Whether the arena is ours to free. A caller who built inside
       their own buffer keeps it; freeing it would be freeing memory
       this library never allocated. */
    cf_bool owns_arena;

    /* The configuration being derived from, while cf_derive builds:
       its candidates are loaded before any source, and its files'
       stamps carried over. NULL for an ordinary build. */
    const struct cf_config* inherit;

    /* The sources the program allows to be written, by name, checked
       against what each can do when the configuration is created. */
    const char* writable[CF_BUILDER_SOURCES];
    size_t      nwritable;

    /* Where what is worth a log line goes (cf_log.h); NULL for nowhere. */
    cf_log_fn    log;
    void*        log_user;
    cf_log_level log_min;

    /* The program's own codecs (cf_builder_codec, D-68), borrowed. */
    const cf_codec* codecs[CF_BUILDER_CODECS];
    size_t          ncodecs;
};

struct cf_config {
    cf_arena*        arena;
    cf_keyspace      keys;
    cf_defset        defs;
    cf_source        sources[CF_BUILDER_SOURCES];
    const cf_source* view[CF_BUILDER_SOURCES];
    cf_source_view   sv;
    cf_apply_report  report;

    /* Shared ownership, so a reader can hold a snapshot while another
       is built. Atomic because the threading contract (D-14) says reads
       are safe concurrently, and a release is a read of the count
       followed by at most one free. */
    cf_refcount refs;
    cf_bool     owns_arena;

    /* One flag per key, by its index, set the first time a read asks
       for it; NULL unless the builder asked for cf_builder_track_reads.
       The one thing a read writes, so each flag is atomic and written
       only while clear: a key read on every request costs a load. */
    cf_refcount* reads;

    /* Kept so that deriving inherits it: a derived configuration that
       accepted unknown keys the original refused would be a different
       contract wearing the same name. */
    cf_unknown_policy unknown;

    /* The files this configuration was read from, stamped as they were
       when it was built. Kept so a caller can ask whether any of them
       has moved on without holding the paths itself. */
    const char** watch_paths;
    cf_stamp*    watch_stamps;
    size_t       watch_count;

    /* Which sources cf_write may write, by slot: granted by the program
       and able to be written, both checked when this was created. */
    cf_bool writable[CF_BUILDER_SOURCES];

    /* The builder's log, kept so that a derived configuration logs where
       its original did. */
    cf_log_fn    log;
    void*        log_user;
    cf_log_level log_min;

    /* The builder's codecs, for cf_emit and cf_config_codec, and for a
       derived configuration to read with the same ones. */
    const cf_codec* codecs[CF_BUILDER_CODECS];
    size_t          ncodecs;
};

/* ------------------------------------------------------------------ *
 * Builder
 * ------------------------------------------------------------------ */

/* A source of the configuration a derived one came from: its name,
   rank, kind and file, and nothing to load, because its candidates
   arrive with the rest of the original's. Without these, a derived
   configuration knew only "inherited" and the new source, so explain()
   said "?" for every rank in between, the default appeared twice, and
   a policy naming the environment could not find it. */
static cf_bool config_ghost_load(const cf_source* s, cf_keyspace* ks) {
    (void)s;
    (void)ks;
    return CF_TRUE;
}

static const char* config_ghost_path(const cf_source* s) {
    return (const char*)s->ref;
}

static cf_bool config_is_ghost(const cf_source* s) {
    return (s->vt != NULL && s->vt->load == config_ghost_load) ? CF_TRUE : CF_FALSE;
}

/* A list of borrowed codecs, element by element. */
static void codecs_copy(const cf_codec** to, const cf_codec* const* from, size_t n) {
    size_t i;
    for (i = 0; i < n; ++i) {
        to[i] = from[i];
    }
}

static void builder_fail(cf_builder* b, const cf_error* e) {
    if (b->failed) {
        return; /* the first failure is the useful one */
    }
    b->failed  = CF_TRUE;
    b->pending = *e;
}

static cf_builder* builder_on(cf_arena* ar, cf_bool owns) {
    cf_builder* b;

    if (ar == NULL) {
        return NULL;
    }
    b = (cf_builder*)cf_arena_calloc(ar, sizeof(cf_builder));
    if (b == NULL) {
        if (owns) {
            cf_arena_free(ar);
        }
        return NULL;
    }
    b->owns_arena = owns;
    b->arena      = ar;
    b->unknown    = CF_UNKNOWN_IGNORE;
    cf_error_clear(&b->pending);
    b->pending.size = sizeof(cf_error);

    if (!cf_defset_init(&b->defs, ar, 16)) {
        if (owns) {
            cf_arena_free(ar);
        }
        return NULL;
    }
    return b;
}

cf_builder* cf_build(const cf_allocator* alloc) {
    return builder_on(cf_arena_new(alloc, 0), CF_TRUE);
}

cf_builder* cf_build_in(cf_arena* arena) {
    /* Borrowed. The caller's buffer holds everything the configuration
       points at, so it has to outlive the configuration - on the target
       this exists for it is a static array and outlives everything. */
    return builder_on(arena, CF_FALSE);
}

void cf_builder_abort(cf_builder* b) {
    if (b != NULL && b->owns_arena) {
        cf_arena_free(b->arena);
    }
}

/* MYAPP_ from "myapp". ASCII folding only: cf_to_upper is the
   library's own, because toupper is locale-dependent and in a Turkish
   locale would turn the prefix of an application called "minio" into
   something no environment variable matches. */
static void derive_prefix(const char* name, char* out, size_t cap) {
    size_t i, n = 0;

    if (name == NULL || cap < 2) {
        if (cap > 0) {
            out[0] = '\0';
        }
        return;
    }
    for (i = 0; name[i] != '\0' && n + 2 < cap; ++i) {
        int c = (unsigned char)name[i];
        /* One cast over the whole choice: '_' is an int in C, so a
           cast on one arm alone left the other arm narrowing. */
        out[n++] = (char)(cf_is_alnum(c) ? cf_to_upper(c) : '_');
    }
    out[n++] = '_';
    out[n]   = '\0';
}

cf_builder* cf_app_builder(const char* name, int argc, char* const* argv) {
    cf_builder* b        = cf_build(NULL);
    cf_error    e        = CF_ERROR_INIT;
    int         n        = argc;
    char**      expanded = NULL;

    if (b == NULL) {
        return NULL;
    }
    derive_prefix(name, b->prefix, sizeof b->prefix);

    /* UTF-8 first, then response files: the file's name may itself
       contain a character the code page dropped. Both are identity on
       platforms that need neither, so the conventional path pays
       nothing for them. */
    argv = cf_cli_args(argc, argv, b->arena, &n);
    if (cf_cli_expand(n, argv, b->arena, &n, &expanded, &e)) {
        argv = expanded;
    } else {
        /* A response file that cannot be read is a command line the
           user meant and did not get, so it is held and reported by
           create() rather than quietly ignored. */
        builder_fail(b, &e);
    }
    cf_builder_cli(b, "cli", CF_RANK_CLI, n, argv);
    cf_builder_env(b, "env", CF_RANK_ENV, b->prefix, cf_environ());
    return b;
}

/* A copy of a string the caller handed over, in the builder's arena: no
   name, path or prefix is borrowed past the call that took it, so a
   buffer the caller frees afterwards is never read - by create, by
   explain(), or by a watcher. On no memory the builder fails, and create
   says so. */
static const char* builder_dup(cf_builder* b, const char* s) {
    char* out;

    if (b == NULL || s == NULL) {
        return s;
    }
    out = cf_arena_dup(b->arena, s, strlen(s));
    if (out == NULL && !b->failed) {
        cf_error e = CF_ERROR_INIT;
        cf_error_set(&e, CF_E_MEMORY, NULL, s, "no room to copy '%s'", s);
        builder_fail(b, &e);
    }
    return (out != NULL) ? out : s;
}

static cf_source* builder_slot(cf_builder* b) {
    if (b == NULL || b->failed) {
        return NULL;
    }
    if (b->nsources >= CF_BUILDER_SOURCES) {
        cf_error e = CF_ERROR_INIT;
        cf_error_set(&e, CF_E_POLICY, NULL, NULL,
                     "more than %zu sources: a precedence chain that long "
                     "is the problem this library exists to prevent",
                     (size_t)CF_BUILDER_SOURCES);
        builder_fail(b, &e);
        return NULL;
    }
    return &b->sources[b->nsources];
}

/* The codecs registered so far, copied into the arena: the builder goes
   when it creates, and a file, a document or a directory is read again
   on every reload. *out is the copy, or NULL when there are none; false,
   the builder failed, when there is no room for it. */
static cf_bool builder_codecs_copy(cf_builder* b, const char* what,
                                   const cf_codec* const** out) {
    const cf_codec** own;

    *out = NULL;
    if (b->ncodecs == 0) {
        return CF_TRUE;
    }
    own = (const cf_codec**)cf_arena_alloc(b->arena, b->ncodecs * sizeof(cf_codec*));
    if (own == NULL) {
        cf_error e = CF_ERROR_INIT;
        cf_error_set(&e, CF_E_MEMORY, NULL, what, "no room for the codecs of '%s'",
                     what);
        builder_fail(b, &e);
        return CF_FALSE;
    }
    codecs_copy(own, b->codecs, b->ncodecs);
    *out = own;
    return CF_TRUE;
}

/* A slot for a source that takes flags, refusing a bit that is not a
   source flag: a definition flag passed here by mistake, or one from a
   later 1.x this library does not know. */
static cf_source* builder_slot_flags(cf_builder* b, const char* name, unsigned flags) {
    if (b != NULL && !b->failed && (flags & ~(unsigned)CF_SOURCE_FLAGS) != 0) {
        cf_error e = CF_ERROR_INIT;
        cf_error_set(&e, CF_E_POLICY, NULL, name,
                     "flags 0x%x are not source flags this library knows (CF_OPTIONAL "
                     "is); a definition flag such as CF_REQUIRED belongs on the key",
                     flags & ~(unsigned)CF_SOURCE_FLAGS);
        builder_fail(b, &e);
        return NULL;
    }
    return builder_slot(b);
}

cf_bool cf_builder_memory(cf_builder* b, const char* name, int rank,
                          const cf_pair* pairs, size_t count) {
    cf_source* s = builder_slot(b);
    if (s == NULL || !cf_source_memory(s, builder_dup(b, name), rank, pairs, count)) {
        return CF_FALSE;
    }
    ++b->nsources;
    return CF_TRUE;
}

cf_bool cf_builder_env(cf_builder* b, const char* name, int rank, const char* prefix,
                       char* const* environment) {
    cf_source* s = builder_slot(b);
    if (s == NULL || !cf_source_env(s, builder_dup(b, name), rank,
                                    builder_dup(b, prefix), environment)) {
        return CF_FALSE;
    }
    ++b->nsources;
    return CF_TRUE;
}

cf_bool cf_builder_cli(cf_builder* b, const char* name, int rank, int argc,
                       char* const* argv) {
    cf_source* s = builder_slot(b);
    if (s == NULL || !cf_source_cli(s, builder_dup(b, name), rank, argc, argv)) {
        return CF_FALSE;
    }
    ++b->nsources;
    return CF_TRUE;
}

cf_bool cf_builder_file(cf_builder* b, const char* name, int rank, const char* path,
                        const char* format, unsigned flags) {
    cf_source*             s;
    void*                  st;
    const cf_codec*        own;
    const cf_codec* const* list;

    if (b == NULL) {
        return CF_FALSE;
    }
    /* The program's own codec, by the name given or the file's: ahead
       of the built-in one, which is what registering it asks for. */
    own = cf_codec_among(b->codecs, b->ncodecs, format, path);
    if (own != NULL) {
        return cf_builder_file_codec(b, name, rank, path, own, flags);
    }
    s = builder_slot_flags(b, name, flags);
    if (s == NULL) {
        return CF_FALSE;
    }
    /* The file source keeps real state, so it comes from the arena
       rather than the inline union the simple sources use. */
    st = cf_arena_calloc(b->arena, cf_source_file_state_size());
    if (st == NULL) {
        cf_error e = CF_ERROR_INIT;
        cf_error_set(&e, CF_E_MEMORY, NULL, path, "no room for a file source");
        builder_fail(b, &e);
        return CF_FALSE;
    }
    if (!cf_source_file(s, builder_dup(b, name), rank, builder_dup(b, path),
                        builder_dup(b, format), flags, st,
                        cf_source_file_state_size())) {
        return CF_FALSE;
    }
    /* For a file the extension does not name: the content decides, the
       program's own codecs first. */
    if (!builder_codecs_copy(b, path, &list)) {
        return CF_FALSE;
    }
    cf_source_file_codecs(s, list, b->ncodecs);
    ++b->nsources;
    return CF_TRUE;
}

/* A document as a source: the `n` bytes given, or - with `variable` - the
   value of that environment variable at every load. One path for both, so
   the format is chosen in one way. */
static cf_bool builder_document(cf_builder* b, const char* name, int rank,
                                const char* bytes, size_t n, const char* variable,
                                const char* format, unsigned flags) {
    cf_source*             s = builder_slot_flags(b, name, flags);
    void*                  st;
    char*                  copy;
    const cf_codec* const* list;
    cf_bool                ok;
    /* One error for every refusal below: a cf_error a block would put
       three on the frame, past the stack budget. */
    cf_error e = CF_ERROR_INIT;

    if (s == NULL) {
        return CF_FALSE;
    }
    if (variable != NULL && (variable[0] == '\0' || strchr(variable, '=') != NULL)) {
        cf_error_set(&e, CF_E_SOURCE, NULL, name,
                     "'%s' is not the name of an environment variable", variable);
        builder_fail(b, &e);
        return CF_FALSE;
    }
    if ((variable == NULL && name == NULL) || (bytes == NULL && n > 0)) {
        cf_error_set(&e, CF_E_SOURCE, NULL, name,
                     "a document needs a name, and bytes where it has a length");
        builder_fail(b, &e);
        return CF_FALSE;
    }
    /* Copied, like a name or a path: a buffer the program frees after the
       call is never read, and the configuration reads the document again
       on every reload. */
    copy = (n > 0) ? cf_arena_dup(b->arena, bytes, n) : NULL;
    st   = cf_arena_calloc(b->arena, cf_source_file_state_size());
    if ((n > 0 && copy == NULL) || st == NULL) {
        cf_error_set(&e, CF_E_MEMORY, NULL, name, "no room for the document '%s'",
                     (name != NULL) ? name : variable);
        builder_fail(b, &e);
        return CF_FALSE;
    }
    if (variable != NULL) {
        ok = cf_source_env_document(s, builder_dup(b, name), rank,
                                    builder_dup(b, variable), builder_dup(b, format),
                                    flags, st, cf_source_file_state_size());
    } else {
        ok = cf_source_document(s, builder_dup(b, name), rank,
                                (copy != NULL) ? copy : "", n, builder_dup(b, format),
                                flags, st, cf_source_file_state_size());
    }
    if (!ok || !builder_codecs_copy(b, (name != NULL) ? name : variable, &list)) {
        return CF_FALSE;
    }
    cf_source_file_codecs(s, list, b->ncodecs);
    ++b->nsources;
    return CF_TRUE;
}

cf_bool cf_builder_document(cf_builder* b, const char* name, int rank,
                            const char* bytes, size_t n, const char* format,
                            unsigned flags) {
    return builder_document(b, name, rank, bytes, n, NULL, format, flags);
}

cf_bool cf_builder_env_document(cf_builder* b, const char* name, int rank,
                                const char* variable, const char* format,
                                unsigned flags) {
    return builder_document(b, name, rank, NULL, 0, (variable != NULL) ? variable : "",
                            format, flags);
}

cf_bool cf_builder_file_codec(cf_builder* b, const char* name, int rank,
                              const char* path, const struct cf_codec* codec,
                              unsigned flags) {
    cf_source* s = builder_slot_flags(b, name, flags);
    void*      st;

    if (s == NULL) {
        return CF_FALSE;
    }
    if (codec == NULL || codec->size < sizeof(cf_codec)) {
        cf_error e = CF_ERROR_INIT;
        cf_error_set(&e, CF_E_SOURCE, NULL, path,
                     (codec == NULL)
                         ? "no codec given for '%s'"
                         : "the codec for '%s' has no size: its first member is "
                           "sizeof(cf_codec) (D-51)",
                     path);
        builder_fail(b, &e);
        return CF_FALSE;
    }
    st = cf_arena_calloc(b->arena, cf_source_file_state_size());
    if (st == NULL) {
        cf_error e = CF_ERROR_INIT;
        cf_error_set(&e, CF_E_MEMORY, NULL, path, "no room for a file source");
        builder_fail(b, &e);
        return CF_FALSE;
    }
    if (!cf_source_file_codec(s, builder_dup(b, name), rank, builder_dup(b, path),
                              codec, flags, st, cf_source_file_state_size())) {
        return CF_FALSE;
    }
    ++b->nsources;
    return CF_TRUE;
}

cf_bool cf_builder_dotenv(cf_builder* b, const char* name, int rank, const char* path,
                          const char* prefix, unsigned flags) {
    cf_source* s = builder_slot_flags(b, name, flags);
    void*      st;

    if (s == NULL) {
        return CF_FALSE;
    }
    st = cf_arena_calloc(b->arena, cf_source_dotenv_state_size());
    if (st == NULL) {
        cf_error e = CF_ERROR_INIT;
        cf_error_set(&e, CF_E_MEMORY, NULL, path, "no room for a .env source");
        builder_fail(b, &e);
        return CF_FALSE;
    }
    if (!cf_source_dotenv(s, builder_dup(b, name), rank, builder_dup(b, path),
                          builder_dup(b, prefix), flags, st,
                          cf_source_dotenv_state_size())) {
        return CF_FALSE;
    }
    ++b->nsources;
    return CF_TRUE;
}

cf_bool cf_builder_secrets(cf_builder* b, const char* name, int rank, const char* path,
                           const char* prefix, unsigned flags) {
    cf_source* s = builder_slot_flags(b, name, flags);
    void*      st;

    if (s == NULL) {
        return CF_FALSE;
    }
    st = cf_arena_calloc(b->arena, cf_source_secrets_state_size());
    if (st == NULL) {
        cf_error e = CF_ERROR_INIT;
        cf_error_set(&e, CF_E_MEMORY, NULL, path, "no room for a secrets source");
        builder_fail(b, &e);
        return CF_FALSE;
    }
    if (!cf_source_secrets(s, builder_dup(b, name), rank, builder_dup(b, path),
                           builder_dup(b, prefix), flags, st,
                           cf_source_secrets_state_size())) {
        return CF_FALSE;
    }
    ++b->nsources;
    return CF_TRUE;
}

cf_bool cf_builder_registry(cf_builder* b, const char* name, int rank, const char* root,
                            unsigned flags) {
    cf_source* s = builder_slot_flags(b, name, flags);
    void*      st;

    if (s == NULL) {
        return CF_FALSE;
    }
    st = cf_arena_calloc(b->arena, cf_source_registry_state_size());
    if (st == NULL) {
        cf_error e = CF_ERROR_INIT;
        cf_error_set(&e, CF_E_MEMORY, NULL, root, "no room for a registry source");
        builder_fail(b, &e);
        return CF_FALSE;
    }
    if (!cf_source_registry(s, builder_dup(b, name), rank, builder_dup(b, root), flags,
                            st, cf_source_registry_state_size())) {
        return CF_FALSE;
    }
    ++b->nsources;
    return CF_TRUE;
}

cf_bool cf_builder_store(cf_builder* b, const char* name, int rank, const char* path,
                         unsigned flags) {
    cf_source* s = builder_slot_flags(b, name, flags);
    void*      st;

    if (s == NULL) {
        return CF_FALSE;
    }
    st = cf_arena_calloc(b->arena, cf_source_store_state_size());
    if (st == NULL) {
        cf_error e = CF_ERROR_INIT;
        cf_error_set(&e, CF_E_MEMORY, NULL, path, "no room for a store source");
        builder_fail(b, &e);
        return CF_FALSE;
    }
    if (!cf_source_store(s, builder_dup(b, name), rank, builder_dup(b, path), flags, st,
                         cf_source_store_state_size())) {
        return CF_FALSE;
    }
    ++b->nsources;
    return CF_TRUE;
}

cf_bool cf_builder_dir(cf_builder* b, const char* name, int rank, const char* path,
                       const char* format, unsigned flags) {
    cf_source*             s = builder_slot_flags(b, name, flags);
    void*                  st;
    const cf_codec* const* own = NULL;

    if (s == NULL) {
        return CF_FALSE;
    }
    st = cf_arena_calloc(b->arena, cf_source_dir_state_size());
    if (st == NULL) {
        cf_error e = CF_ERROR_INIT;
        cf_error_set(&e, CF_E_MEMORY, NULL, path, "no room for a directory source");
        builder_fail(b, &e);
        return CF_FALSE;
    }
    if (!cf_source_dir(s, builder_dup(b, name), rank, builder_dup(b, path),
                       builder_dup(b, format), flags, st, cf_source_dir_state_size())) {
        return CF_FALSE;
    }
    if (!builder_codecs_copy(b, path, &own)) {
        return CF_FALSE;
    }
    if (own != NULL) {
        cf_source_dir_codecs(s, own, b->ncodecs);
    }
    ++b->nsources;
    return CF_TRUE;
}

cf_bool cf_builder_codec(cf_builder* b, const struct cf_codec* codec) {
    cf_error e = CF_ERROR_INIT;

    if (b == NULL || b->failed) {
        return CF_FALSE;
    }
    if (codec == NULL || codec->size < sizeof(cf_codec) || codec->name == NULL ||
        codec->name[0] == '\0' ||
        (codec->caps & (CF_CODEC_READ | CF_CODEC_WRITE)) == 0 ||
        ((codec->caps & CF_CODEC_READ) != 0 && codec->decode == NULL) ||
        ((codec->caps & CF_CODEC_WRITE) != 0 && codec->encoder == NULL)) {
        cf_error_set(&e, CF_E_SOURCE, NULL, NULL,
                     "a codec needs its size, a name, and a way to read or to write "
                     "(D-68)");
    } else if (cf_codec_among(b->codecs, b->ncodecs, codec->name, NULL) != NULL) {
        cf_error_set(&e, CF_E_SOURCE, NULL, codec->name,
                     "this builder already has a codec called '%s'", codec->name);
    } else if (b->ncodecs >= CF_BUILDER_CODECS) {
        cf_error_set(&e, CF_E_CAPABILITY, NULL, codec->name,
                     "a builder takes %d codecs of its own", CF_BUILDER_CODECS);
    } else {
        b->codecs[b->ncodecs++] = codec;
        return CF_TRUE;
    }
    builder_fail(b, &e);
    return CF_FALSE;
}

const struct cf_codec* cf_config_codec(const cf_config* c, const char* format) {
    const cf_codec* own;

    if (format == NULL) {
        return NULL;
    }
    own = (c != NULL) ? cf_codec_among(c->codecs, c->ncodecs, format, NULL) : NULL;
    return (own != NULL) ? own : cf_codec_by_name(format);
}

cf_bool cf_builder_replay(cf_builder* b, const char* name, int rank, const char* path,
                          unsigned flags) {
    cf_source* s = builder_slot_flags(b, name, flags);
    void*      st;

    if (s == NULL) {
        return CF_FALSE;
    }
    st = cf_arena_calloc(b->arena, cf_source_replay_state_size());
    if (st == NULL) {
        cf_error e = CF_ERROR_INIT;
        cf_error_set(&e, CF_E_MEMORY, NULL, path, "no room for a replay source");
        builder_fail(b, &e);
        return CF_FALSE;
    }
    if (!cf_source_replay(s, builder_dup(b, name), rank, builder_dup(b, path), flags,
                          st, cf_source_replay_state_size())) {
        return CF_FALSE;
    }
    ++b->nsources;
    return CF_TRUE;
}

cf_bool cf_builder_source(cf_builder* b, const cf_source* src) {
    cf_source* slot = builder_slot(b);

    if (slot == NULL || src == NULL || src->vt == NULL) {
        return CF_FALSE;
    }
    if (src->vt->size < sizeof(cf_source_vtable)) {
        cf_error e = CF_ERROR_INIT;
        cf_error_set(&e, CF_E_SOURCE, NULL, src->name,
                     "source '%s' has a table with no size: its first member is "
                     "sizeof(cf_source_vtable) (D-51)",
                     (src->name != NULL) ? src->name : "?");
        builder_fail(b, &e);
        return CF_FALSE;
    }
    /* 1.0's members one by one, never the struct whole: a later 1.x
       that adds one would read past a 1.0 caller's (D-51). */
    memset(slot, 0, sizeof *slot);
    slot->vt   = src->vt;
    slot->name = builder_dup(b, src->name);
    slot->rank = src->rank;
    slot->ext  = src->ext;
    slot->ref  = src->ref;
    slot->u    = src->u;
    ++b->nsources;
    return CF_TRUE;
}

cf_bool cf_builder_open(cf_builder* b, const char* prefix) {
    cf_error e = CF_ERROR_INIT;

    if (b == NULL) {
        return CF_FALSE;
    }
    if (!cf_defset_open(&b->defs, prefix, &e)) {
        builder_fail(b, &e);
        return CF_FALSE;
    }
    return CF_TRUE;
}

cf_bool cf_builder_schema(cf_builder* b, const char* text, size_t n, const char* name) {
    cf_error e = CF_ERROR_INIT;

    if (b == NULL || b->failed) {
        return CF_FALSE;
    }
    if (!cf_defset_schema(&b->defs, text, n, name, &e)) {
        builder_fail(b, &e);
        return CF_FALSE;
    }
    return CF_TRUE;
}

cf_bool cf_builder_schema_file(cf_builder* b, const char* path) {
    cf_error e = CF_ERROR_INIT;

    if (b == NULL || b->failed) {
        return CF_FALSE;
    }
    if (!cf_defset_schema_file(&b->defs, path, &e)) {
        builder_fail(b, &e);
        return CF_FALSE;
    }
    return CF_TRUE;
}

cf_bool cf_builder_define(cf_builder* b, const cf_def* d) {
    cf_error e = CF_ERROR_INIT;

    if (b == NULL || b->failed) {
        return CF_FALSE;
    }
    if (!cf_define(&b->defs, d, &e)) {
        builder_fail(b, &e);
        return CF_FALSE;
    }
    return CF_TRUE;
}

cf_bool cf_builder_bool(cf_builder* b, const char* key, cf_bool dflt, unsigned flags) {
    cf_error e = CF_ERROR_INIT;
    if (b == NULL || b->failed) {
        return CF_FALSE;
    }
    if (!cf_def_bool(&b->defs, key, dflt, flags, &e)) {
        builder_fail(b, &e);
        return CF_FALSE;
    }
    return CF_TRUE;
}

cf_bool cf_builder_int(cf_builder* b, const char* key, cf_i64 dflt, cf_i64 min,
                       cf_i64 max, unsigned flags) {
    cf_error e = CF_ERROR_INIT;
    if (b == NULL || b->failed) {
        return CF_FALSE;
    }
    if (!cf_def_int(&b->defs, key, dflt, min, max, flags, &e)) {
        builder_fail(b, &e);
        return CF_FALSE;
    }
    return CF_TRUE;
}

cf_bool cf_builder_text(cf_builder* b, const char* key, const char* dflt,
                        unsigned flags) {
    cf_error e = CF_ERROR_INIT;
    if (b == NULL || b->failed) {
        return CF_FALSE;
    }
    if (!cf_def_text(&b->defs, key, dflt, flags, &e)) {
        builder_fail(b, &e);
        return CF_FALSE;
    }
    return CF_TRUE;
}

#if CF_WITH_REAL
cf_bool cf_builder_real(cf_builder* b, const char* key, double dflt, double min,
                        double max, unsigned flags) {
    cf_error e = CF_ERROR_INIT;
    if (b == NULL || b->failed) {
        return CF_FALSE;
    }
    if (!cf_def_real(&b->defs, key, dflt, min, max, flags, &e)) {
        builder_fail(b, &e);
        return CF_FALSE;
    }
    return CF_TRUE;
}
#endif

/* The type the default's text reads as, tried from the narrowest:
   "true" a bool, "8080" an int, "0.5" a real, anything else text. */
static cf_type key_infer(const char* s, size_t n) {
    cf_value v;
    if (cf_value_from_text(CF_TYPE_BOOL, s, n, &v)) {
        return CF_TYPE_BOOL;
    }
    if (cf_value_from_text(CF_TYPE_INT, s, n, &v)) {
        return CF_TYPE_INT;
    }
#if CF_WITH_REAL
    if (cf_value_from_text(CF_TYPE_REAL, s, n, &v)) {
        return CF_TYPE_REAL;
    }
#endif
    return CF_TYPE_TEXT;
}

/* A bound, read as the key's type, into the definition's range. */
static cf_bool key_bound(cf_def* d, const char* s, cf_bool upper) {
    cf_value v;
    if (s == NULL) {
        return CF_TRUE;
    }
    if (!cf_value_from_text(d->type, s, strlen(s), &v)) {
        return CF_FALSE;
    }
    d->has_range = CF_TRUE;
    if (d->type == CF_TYPE_INT) {
        *(upper ? &d->imax : &d->imin) = v.u.i;
        return CF_TRUE;
    }
#if CF_WITH_REAL
    if (d->type == CF_TYPE_REAL) {
        *(upper ? &d->rmax : &d->rmin) = v.u.r;
        return CF_TRUE;
    }
#endif
    return CF_FALSE;
}

cf_bool cf_builder_key(cf_builder* b, const char* key, cf_type type, const char* dflt,
                       const char* min, const char* max, unsigned flags) {
    cf_error e = CF_ERROR_INIT;
    cf_def   d;
    size_t   n;

    if (b == NULL || b->failed) {
        return CF_FALSE;
    }
    if (key == NULL) {
        key = "";
    }
    n = (dflt != NULL) ? strlen(dflt) : 0;
    if (type == CF_TYPE_NULL) {
        if (dflt == NULL) {
            cf_error_set(&e, CF_E_DEFINITION, key, NULL,
                         "'%s' has neither a type nor a default", key);
            builder_fail(b, &e);
            return CF_FALSE;
        }
        type = key_infer(dflt, n);
    }
    cf_def_init(&d, type, key);
    d.flags = flags;
    if (dflt != NULL && (flags & CF_REQUIRED) == 0) {
        if (!cf_value_from_text(type, dflt, n, &d.default_value)) {
            cf_error_set(&e, CF_E_DEFINITION, key, NULL,
                         "the default of '%s', '%s', is not a %s", key, dflt,
                         cf_type_name(type));
            builder_fail(b, &e);
            return CF_FALSE;
        }
        d.has_default = CF_TRUE;
    }
    if ((min != NULL) != (max != NULL) || !key_bound(&d, min, CF_FALSE) ||
        !key_bound(&d, max, CF_TRUE)) {
        cf_error_set(&e, CF_E_DEFINITION, key, NULL,
                     "the range of '%s' is not two bounds of its type, %s", key,
                     cf_type_name(type));
        builder_fail(b, &e);
        return CF_FALSE;
    }
    if (!cf_define(&b->defs, &d, &e)) {
        builder_fail(b, &e);
        return CF_FALSE;
    }
    return CF_TRUE;
}

void cf_builder_unknown(cf_builder* b, cf_unknown_policy policy) {
    if (b != NULL) {
        b->unknown = policy;
    }
}

void cf_builder_track_reads(cf_builder* b) {
    if (b != NULL) {
        b->track_reads = CF_TRUE;
    }
}

void cf_builder_log(cf_builder* b, cf_log_fn fn, void* user, cf_log_level min) {
    if (b != NULL) {
        b->log      = fn;
        b->log_user = user;
        b->log_min  = min;
    }
}

static const char* config_name_of_rank(const void* ctx, int rank);

/* One event to the configuration's log, if it has one and the level
   reaches the floor it asked for. */
static void config_log(const cf_config* c, cf_log_level level, const char* event,
                       cf_code code, const char* key, const char* source,
                       const char* message) {
    cf_log_event e;

    if (c->log == NULL || level < c->log_min) {
        return;
    }
    e.size        = sizeof e;
    e.level       = level;
    e.event       = event;
    e.code        = code;
    e.key         = key;
    e.source      = source;
    e.message     = message;
    e.previous    = 0;
    e.fingerprint = 0;
    c->log(c->log_user, &e);
}

/* What a created configuration has to say that is not an error: keys
   nobody defined (under CF_UNKNOWN_WARN), deprecated keys in use, and
   values refused and fallen back from. Read from the resolved key space
   after the fact, so the resolver itself stays free of side effects. */
static void config_log_events(const cf_config* c) {
    size_t i;
    char   msg[CF_MESSAGE_MAX];

    if (c->log == NULL || c->log_min > CF_LOG_WARN) {
        return;
    }
    for (i = 0; i < cf_ks_count(&c->keys); ++i) {
        const cf_entry*     e = cf_ks_at(&c->keys, i);
        const cf_def*       d;
        const cf_candidate* k;
        const char*         from;

        if (e == NULL || e->selected == NULL) {
            continue;
        }
        from = config_name_of_rank(&c->sv, e->selected->source);
        d    = cf_defset_find(&c->defs, e->key, e->keylen);
        if (d == NULL) {
            if (c->unknown == CF_UNKNOWN_WARN &&
                !cf_defset_is_open(&c->defs, e->key, e->keylen)) {
                cf_fmt(msg, sizeof msg, "unknown key '%s', set by %s", e->key,
                       (from != NULL) ? from : "?");
                config_log(c, CF_LOG_WARN, "unknown-key", CF_E_KEY, e->key,
                           (e->selected->at != NULL) ? e->selected->at : from, msg);
            }
            continue;
        }
        if ((d->flags & CF_DEPRECATED) != 0 && e->selected->source != CF_RANK_DEFAULT) {
            if (d->replaced_by != NULL) {
                cf_fmt(msg, sizeof msg, "'%s' is deprecated: use '%s'", e->key,
                       d->replaced_by);
            } else {
                cf_fmt(msg, sizeof msg, "'%s' is deprecated", e->key);
            }
            config_log(c, CF_LOG_WARN, "deprecated-key", CF_OK, e->key,
                       (e->selected->at != NULL) ? e->selected->at : from, msg);
        }
        for (k = e->chain; k != NULL; k = k->next) {
            if (k->outcome == CF_OUT_REJECTED && d->on_invalid == CF_INVALID_FALLBACK) {
                const char* by = config_name_of_rank(&c->sv, k->source);
                cf_fmt(msg, sizeof msg, "%s: %s from %s refused (%s); %s used instead",
                       e->key, (by != NULL) ? by : "?", (k->at != NULL) ? k->at : "-",
                       /* A reason can quote the value; a secret's never
                          reaches a log line. */
                       ((d->flags & CF_SENSITIVE) != 0 || k->reason == NULL)
                           ? "invalid"
                           : k->reason,
                       (from != NULL) ? from : "?");
                config_log(c, CF_LOG_WARN, "fallback", CF_E_VALIDATE, e->key,
                           (k->at != NULL) ? k->at : by, msg);
            }
        }
    }
}

cf_bool cf_builder_writable(cf_builder* b, const char* source_name) {
    const char* name;

    if (b == NULL || source_name == NULL) {
        return CF_FALSE;
    }
    name = cf_arena_dup(b->arena, source_name, strlen(source_name));
    if (name == NULL || b->nwritable >= CF_BUILDER_SOURCES) {
        cf_error e = CF_ERROR_INIT;
        cf_error_set(&e, CF_E_MEMORY, NULL, source_name,
                     "no room to grant '%s' writing", source_name);
        builder_fail(b, &e);
        return CF_FALSE;
    }
    b->writable[b->nwritable++] = builder_dup(b, name);
    return CF_TRUE;
}

/* ------------------------------------------------------------------ *
 * Creation
 *
 * The whole error surface of the library is this function.
 * ------------------------------------------------------------------ */

/* The one thing the apply pass asks of the world outside the core: what
   is this rank called. Supplied here because this is where the sources
   are; the core never sees a cf_source. */
static const char* config_name_of_rank(const void* ctx, int rank) {
    const cf_source_view* sv = (const cf_source_view*)ctx;
    size_t                i;

    if (sv == NULL) {
        return NULL;
    }
    for (i = 0; i < sv->count; ++i) {
        if (sv->items[i] != NULL && sv->items[i]->rank == rank) {
            return sv->items[i]->name;
        }
    }
    return NULL;
}

static cf_config* create_failed(cf_builder* b, cf_error* err, const cf_error* what) {
    cf_error_copy(err, what);
    if (b->owns_arena) {
        cf_arena_free(b->arena);
    }
    return NULL;
}

cf_config* cf_builder_create(cf_builder* b, cf_error* err) {
    cf_error   e = CF_ERROR_INIT;
    cf_config* c;
    size_t     i;

    if (err != NULL) {
        cf_error_clear(err);
    }
    if (b == NULL) {
        cf_error_set(&e, CF_E_MEMORY, NULL, NULL, "the builder could not be created");
        cf_error_copy(err, &e);
        return NULL;
    }
    if (b->failed) {
        return create_failed(b, err, &b->pending);
    }

    c = (cf_config*)cf_arena_calloc(b->arena, sizeof(cf_config));
    if (c != NULL) {
        /* One owner to start with. calloc left it at zero, which would
           make the first release free a configuration nobody had taken
           a second reference to - correct today and wrong the moment
           anyone does. */
        cf_ref_init(&c->refs, 1);
        c->owns_arena = b->owns_arena;
        c->unknown    = b->unknown;
        c->log        = b->log;
        c->log_user   = b->log_user;
        c->log_min    = b->log_min;
        codecs_copy(c->codecs, b->codecs, b->ncodecs);
        c->ncodecs = b->ncodecs;
        /* One slot per source; the ones that read no file stay unused,
           which costs a pointer each and saves a second pass. */
        c->watch_paths =
            (const char**)cf_arena_calloc(b->arena, (b->nsources + 1) * sizeof(char*));
        c->watch_stamps =
            (cf_stamp*)cf_arena_calloc(b->arena, (b->nsources + 1) * sizeof(cf_stamp));
    }
    if (c == NULL) {
        cf_error_set(&e, CF_E_MEMORY, NULL, NULL, "no room for the config");
        return create_failed(b, err, &e);
    }
    c->arena = b->arena;
    c->defs  = b->defs;

    for (i = 0; i < b->nsources; ++i) {
        c->sources[i] = b->sources[i];
        c->view[i]    = &c->sources[i];
    }
    c->sv.items = c->view;
    c->sv.count = b->nsources;

    /* What the program grants, against what each source can do, before
       anything is read: a grant to a source that cannot be written, or
       to no source at all, is a mistake in the program, and the time to
       say so is now rather than at the first write. */
    for (i = 0; i < b->nwritable; ++i) {
        size_t  k;
        cf_bool found = CF_FALSE;

        for (k = 0; k < b->nsources; ++k) {
            const cf_source* s = &c->sources[k];
            if (s->name == NULL || strcmp(s->name, b->writable[i]) != 0) {
                continue;
            }
            found = CF_TRUE;
            if (s->vt == NULL || s->vt->write == NULL) {
                cf_error_set(&e, CF_E_CAPABILITY, NULL, s->name,
                             "'%s' is granted writing, and a %s source cannot be "
                             "written: a running program's changes go to a store",
                             s->name, (s->vt != NULL) ? s->vt->kind : "?");
                return create_failed(b, err, &e);
            }
            c->writable[k] = CF_TRUE;
        }
        if (!found) {
            cf_error_set(&e, CF_E_POLICY, NULL, b->writable[i],
                         "'%s' is granted writing, and no source has that name",
                         b->writable[i]);
            return create_failed(b, err, &e);
        }
    }

    cf_defset_seal(&c->defs);

    /* Hand the key list to the sources that can use it: the
       environment gains its natural spelling, the command line gains
       --key value, --no-key and aliases. Neither can be parsed
       unambiguously without it, which is why both are Tier 1. */
    if (cf_defset_count(&c->defs) > 0) {
        for (i = 0; i < c->sv.count; ++i) {
            if (c->sources[i].vt == NULL || config_is_ghost(&c->sources[i])) {
                continue;
            }
            if (strcmp(c->sources[i].vt->kind, "env") == 0) {
                cf_source_env_defs(&c->sources[i], &c->defs);
            } else if (strcmp(c->sources[i].vt->kind, "cli") == 0) {
                cf_source_cli_defs(&c->sources[i], &c->defs);
            } else if (strcmp(c->sources[i].vt->kind, "dotenv") == 0) {
                cf_source_dotenv_defs(&c->sources[i], &c->defs);
            } else if (strcmp(c->sources[i].vt->kind, "file") == 0 ||
                       strcmp(c->sources[i].vt->kind, "document") == 0 ||
                       strcmp(c->sources[i].vt->kind, "dir") == 0) {
                cf_source_file_defs(&c->sources[i], &c->defs);
            }
        }
    }

    /* Defaults are a source like any other, at the lowest rank. They
       are appended after the caller's sources so that a caller who
       registered their own default source still gets theirs. */
    /* Not when deriving: the original's defaults arrive with its other
       candidates, from the same definitions, and seeding them again put
       every default in the chain twice. */
    if (cf_defset_count(&c->defs) > 0 && c->sv.count < CF_BUILDER_SOURCES &&
        b->inherit == NULL) {
        size_t at = c->sv.count;
        cf_source_defaults(&c->sources[at], "default", CF_RANK_DEFAULT, &c->defs);
        c->view[at] = &c->sources[at];
        c->sv.count = at + 1;
    }

    if (!cf_ks_init(&c->keys, c->arena, 64)) {
        cf_error_set(&e, CF_E_MEMORY, NULL, NULL, "no room for the key space");
        return create_failed(b, err, &e);
    }

    /* A derived configuration's inheritance: every candidate the
       original held, at the rank it had. Loaded before the sources, not
       after - a second change to one key lands at the same override
       rank as the first, and the later of two equal ranks wins. And not
       as a source: its candidates belong to the sources their ranks
       name, which the ghosts stand in for. */
    if (b->inherit != NULL) {
        cf_source snap;
        if (!cf_source_snapshot(&snap, "inherited", &b->inherit->keys) ||
            !cf_source_load(&snap, &c->keys)) {
            cf_error_set(&e, CF_E_MEMORY, NULL, NULL,
                         "no room for the inherited configuration");
            return create_failed(b, err, &e);
        }
    }

    for (i = 0; i < c->sv.count; ++i) {
        if (!cf_source_load(&c->sources[i], &c->keys)) {
            /* A file source knows a dozen ways to fail and keeps the
               detail; everything else fails in one way. Reporting the
               generic message over the specific one would throw away
               the line number the reader needs. */
            const cf_error* detail = cf_source_file_error(&c->sources[i]);
            if (detail == NULL) {
                detail = cf_source_replay_error(&c->sources[i]);
            }
            if (detail == NULL) {
                detail = cf_source_dotenv_error(&c->sources[i]);
            }
            if (detail == NULL) {
                detail = cf_source_image_error(&c->sources[i]);
            }
            if (detail == NULL) {
                detail = cf_source_dir_error(&c->sources[i]);
            }
            if (detail == NULL) {
                detail = cf_source_store_error(&c->sources[i]);
            }
            if (detail == NULL) {
                detail = cf_source_secrets_error(&c->sources[i]);
            }
            if (detail == NULL) {
                detail = cf_source_registry_error(&c->sources[i]);
            }
            if (detail != NULL) {
                return create_failed(b, err, detail);
            }
            cf_error_set(&e, CF_E_SOURCE, NULL, c->sources[i].name,
                         "source '%s' could not be read", c->sources[i].name);
            return create_failed(b, err, &e);
        }
    }

    /* Stamped after the sources were read, not before: a file that
       changed while it was being read has already produced a
       configuration nobody asked for, and the stamp taken afterwards is
       the one that makes the next check notice. */
    if (c->watch_paths != NULL && c->watch_stamps != NULL) {
        for (i = 0; i < c->sv.count; ++i) {
            const char* p = cf_source_path(&c->sources[i]);
            if (p == NULL || config_is_ghost(&c->sources[i])) {
                continue;
            }
            c->watch_paths[c->watch_count] = p;
            cf_platform_stamp(p, &c->watch_stamps[c->watch_count]);
            ++c->watch_count;
        }
        /* The original's files with the stamps they had when it was
           read, not now: a file that changed after the original was
           built and before the derive is still a change. There is room,
           as each of those files has a ghost among the sources. */
        if (b->inherit != NULL) {
            const cf_config* o = b->inherit;
            for (i = 0; i < o->watch_count && c->watch_count < c->sv.count + 1; ++i) {
                const char* p = cf_arena_dup(c->arena, o->watch_paths[i],
                                             strlen(o->watch_paths[i]));
                if (p == NULL) {
                    cf_error_set(&e, CF_E_MEMORY, NULL, NULL,
                                 "no room for the watched files");
                    return create_failed(b, err, &e);
                }
                c->watch_paths[c->watch_count]  = p;
                c->watch_stamps[c->watch_count] = o->watch_stamps[i];
                ++c->watch_count;
            }
        }
    }

    /* A renamed key's old spelling still configures it: every value
       written under the old name is offered to the new one, below the
       new spelling in the same source (CF_CAND_RENAMED). */
    for (i = 0; i < c->defs.count; ++i) {
        const cf_def* d = &c->defs.defs[i];
        cf_bool       renamed;
        if ((d->flags & CF_DEPRECATED) == 0 || d->replaced_by == NULL) {
            continue;
        }
        renamed = d->list_mode != CF_LIST_NONE
                      ? cf_ks_rename_list(&c->keys, d->key, strlen(d->key),
                                          d->replaced_by, strlen(d->replaced_by))
                      : cf_ks_rename(&c->keys, d->key, strlen(d->key), d->replaced_by,
                                     strlen(d->replaced_by));
        if (!renamed) {
            cf_error_set(&e, CF_E_MEMORY, d->key, NULL, "no room to rename '%s'",
                         d->key);
            return create_failed(b, err, &e);
        }
    }

    if (!cf_ks_resolve(&c->keys)) {
        cf_error_set(&e, CF_E_MEMORY, NULL, NULL, "no room to resolve %u candidates",
                     (unsigned)cf_ks_candidate_count(&c->keys));
        return create_failed(b, err, &e);
    }

    /* Conversion, validation and Tier 0 inference, in that order and
       in one place. Everything that can go wrong has gone wrong by the
       time this returns, which is what lets every read afterwards be
       infallible. */
    /* A policy that names no source this build actually has means the
       key can never be set by anything, which is a typo rather than an
       intention. Checked here because this is where the sources are,
       and only when *none* of the named ones exists: naming a source
       that is present on some deployments and absent on others is
       legitimate, and failing on that would make the feature unusable
       for the case it exists for. */
    for (i = 0; i < c->defs.count; ++i) {
        const cf_def* d = &c->defs.defs[i];
        size_t        j, k;
        cf_bool       any = CF_FALSE;

        if (d->policy == NULL) {
            continue;
        }
        for (j = 0; d->policy[j] != NULL && !any; ++j) {
            for (k = 0; k < c->sv.count; ++k) {
                if (c->sv.items[k] != NULL && c->sv.items[k]->name != NULL &&
                    strcmp(c->sv.items[k]->name, d->policy[j]) == 0) {
                    any = CF_TRUE;
                    break;
                }
            }
        }
        if (!any) {
            char list[CF_MESSAGE_MAX];
            cf_policy_text(d->policy, list, sizeof list);
            cf_error_set(&e, CF_E_POLICY, d->key, NULL,
                         "the policy for '%s' names only %s, and this build "
                         "has none of them, so nothing could ever set it",
                         d->key, list);
            return create_failed(b, err, &e);
        }
    }

    {
        cf_rank_names names;
        names.name_of = config_name_of_rank;
        names.ctx     = &c->sv;
        if (!cf_defs_apply(&c->keys, &c->defs, &names, b->unknown, &c->report, &e)) {
            return create_failed(b, err, &e);
        }
    }
    /* After the definitions, which may add keys. One more than the
       count, so an empty configuration still says it tracks. */
    if (b->track_reads) {
        c->reads = (cf_refcount*)cf_arena_calloc(c->arena, (cf_ks_count(&c->keys) + 1) *
                                                               sizeof(cf_refcount));
        if (c->reads == NULL) {
            cf_error_set(&e, CF_E_MEMORY, NULL, NULL, "no room to track reads");
            return create_failed(b, err, &e);
        }
    }
    config_log_events(c);
    return c;
}

cf_config* cf_app(const char* name, int argc, char* const* argv, cf_error* err) {
    return cf_builder_create(cf_app_builder(name, argc, argv), err);
}

/* ------------------------------------------------------------------ *
 * Deriving
 *
 * A snapshot is immutable, so setting a value means building another
 * one. The derived configuration copies what it needs rather than
 * pointing into the original, so neither has to outlive the other -
 * which costs a pass over the key space and saves the caller a lifetime
 * to reason about.
 * ------------------------------------------------------------------ */

/* A ghost of `o` in `b`: everything copied into the derived arena, so
   the original may be freed first. */
static cf_bool derive_ghost(cf_builder* b, const cf_source* o) {
    cf_source_vtable* vt   = (cf_source_vtable*)cf_arena_calloc(b->arena, sizeof *vt);
    const char*       path = cf_source_path(o);
    cf_source*        s;

    if (vt == NULL) {
        return CF_FALSE;
    }
    vt->size    = sizeof *vt;
    vt->kind    = cf_arena_dup(b->arena, o->vt->kind, strlen(o->vt->kind));
    vt->load    = config_ghost_load;
    vt->destroy = NULL;
    vt->path    = config_ghost_path;
    vt->write   = o->vt->write; /* through the path, which the ghost has */
    s           = builder_slot(b);
    if (s == NULL || vt->kind == NULL) {
        return CF_FALSE;
    }
    memset(s, 0, sizeof *s);
    s->vt   = vt;
    s->rank = o->rank;
    s->name = cf_arena_dup(b->arena, o->name, strlen(o->name));
    s->ref  = (path != NULL) ? cf_arena_dup(b->arena, path, strlen(path)) : NULL;
    if (s->name == NULL || (path != NULL && s->ref == NULL)) {
        return CF_FALSE;
    }
    ++b->nsources;
    return CF_TRUE;
}

static cf_config* derive_onto(const cf_config* base, cf_arena* ar, cf_bool owns,
                              const char* source_name, const cf_pair* pairs,
                              size_t count, const cf_source* overlay, cf_error* err) {
    cf_builder* b;
    size_t      i;
    const char* name;

    b = builder_on(ar, owns);
    if (b == NULL) {
        cf_error_set(err, CF_E_MEMORY, NULL, NULL, "no room to derive a configuration");
        return NULL;
    }

    /* Every candidate the base holds, not only the winners: dropping
       the losers would make explain() on the derived configuration say
       less than on the one it came from, which is the wrong direction
       for this library to move in. cf_builder_create loads them, before
       the sources, from here. */
    b->inherit = base;
    /* Copied: the change's source is named in every explain() of the
       derived configuration, which outlives the caller's string as it
       outlives the caller's pairs (whose values the load copies). */
    name = cf_arena_dup(ar, (source_name != NULL) ? source_name : "set",
                        strlen((source_name != NULL) ? source_name : "set"));
    if (name == NULL) {
        cf_error_set(err, CF_E_MEMORY, NULL, NULL, "no room to derive a configuration");
        cf_builder_abort(b);
        return NULL;
    }

    /* And the sources they came from, by name, rank, kind and file, so
       that the derived configuration explains itself as the original
       did. A source this derive is about to add again - a second change
       from "admin" - is left to the live one, or every key it does not
       set would be "admin: not present" twice. */
    for (i = 0; i < base->sv.count; ++i) {
        const cf_source* o = base->sv.items[i];
        if (o == NULL || o->vt == NULL) {
            continue;
        }
        if (count > 0 && o->rank == CF_RANK_OVERRIDE && strcmp(o->name, name) == 0) {
            continue;
        }
        if (!derive_ghost(b, o)) {
            cf_error_set(err, CF_E_MEMORY, NULL, NULL,
                         "no room for the inherited sources");
            cf_builder_abort(b);
            return NULL;
        }
    }

    /* And what it was allowed to write. A grant lost on deriving would
       make the change just made impossible to keep from the
       configuration that holds it. */
    for (i = 0; i < base->sv.count; ++i) {
        const cf_source* o = base->sv.items[i];
        if (o == NULL || !base->writable[i] ||
            (count > 0 && o->rank == CF_RANK_OVERRIDE && strcmp(o->name, name) == 0)) {
            continue;
        }
        cf_builder_writable(b, o->name);
    }

    /* The overrides go in after the base, not before. Rank already puts
       them above everything, so the order looks irrelevant - and is
       not, because deriving twice gives two candidates at the same
       override rank and the later one has to win. Adding them first
       made the *inherited* override the later of the two, so a second
       change to the same key did nothing. */
    if (count > 0) {
        if (overlay != NULL) {
            cf_builder_source(b, overlay);
        } else {
            cf_builder_memory(b, name, CF_RANK_OVERRIDE, pairs, count);
        }
    }

    /* The contract travels with it, or the derived configuration would
       accept values the original refused. */
    for (i = 0; i < base->defs.count; ++i) {
        if (!cf_builder_define(b, &base->defs.defs[i])) {
            cf_error_set(err, CF_E_MEMORY, base->defs.defs[i].key, NULL,
                         "no room for the inherited definitions");
            cf_builder_abort(b);
            return NULL;
        }
    }
    for (i = 0; i < base->defs.open_count; ++i) {
        cf_builder_open(b, base->defs.open_prefixes[i]);
    }
    cf_builder_unknown(b, base->unknown);
    cf_builder_log(b, base->log, base->log_user, base->log_min);
    codecs_copy(b->codecs, base->codecs, base->ncodecs);
    b->ncodecs = base->ncodecs;
    /* Tracking travels, the reads do not: each snapshot records what
       was read through it. */
    b->track_reads = (base->reads != NULL) ? CF_TRUE : CF_FALSE;

    return cf_builder_create(b, err);
}

cf_config* cf_derive(const cf_config* base, const char* source_name,
                     const cf_pair* pairs, size_t count, cf_error* err) {
    const cf_allocator* alloc;

    if (base == NULL) {
        cf_error_set(err, CF_E_SOURCE, NULL, NULL, "nothing to derive from");
        return NULL;
    }
    alloc = cf_arena_allocator(base->arena);
    if (alloc == NULL) {
        /* Built inside a caller's buffer, so there is no allocator to
           borrow and falling back to malloc would be the one thing a
           target with no heap cannot survive. */
        cf_error_set(err, CF_E_CAPABILITY, NULL, NULL,
                     "this configuration was built in a fixed arena; derive "
                     "into one with cf_derive_in");
        return NULL;
    }
    return derive_onto(base, cf_arena_new(alloc, 0), CF_TRUE, source_name, pairs, count,
                       NULL, err);
}

cf_config* cf_derive_in(const cf_config* base, cf_arena* arena, const char* source_name,
                        const cf_pair* pairs, size_t count, cf_error* err) {
    if (base == NULL || arena == NULL) {
        cf_error_set(err, CF_E_SOURCE, NULL, NULL, "nothing to derive from");
        return NULL;
    }
    return derive_onto(base, arena, CF_FALSE, source_name, pairs, count, NULL, err);
}

cf_config* cfi_config_keep_values(const cf_config* base, const cf_config* running,
                                  const char* const* keys, size_t count,
                                  cf_error* err) {
    const cf_allocator* alloc = cf_arena_allocator(base->arena);
    cf_arena*           ar;
    cf_keyspace*        values;
    cf_source           source;
    size_t              i;

    if (alloc == NULL) {
        cf_error_set(err, CF_E_CAPABILITY, NULL, NULL,
                     "cannot keep restart values in a fixed-arena configuration");
        return NULL;
    }
    ar     = cf_arena_new(alloc, 0);
    values = ar != NULL ? (cf_keyspace*)cf_arena_alloc(ar, sizeof *values) : NULL;
    if (values == NULL) {
        cf_arena_free(ar);
        cf_error_set(err, CF_E_MEMORY, NULL, NULL, "no room for running values");
        return NULL;
    }
    if (!cf_ks_init(values, ar, count)) {
        cf_arena_free(ar);
        cf_error_set(err, CF_E_MEMORY, NULL, NULL, "no room for running values");
        return NULL;
    }
    for (i = 0; i < count; ++i) {
        size_t          n = strlen(keys[i]);
        const cf_value* v = cf_ks_get(cf_keys(running), keys[i], n);
        cf_bool         added;
        if (v == NULL) {
            cf_error_set(err, CF_E_SOURCE, keys[i], NULL, "running value is absent");
            cf_arena_free(ar);
            return NULL;
        }
        added = cf_defset_list(&base->defs, keys[i], n) != NULL
                    ? cfi_ks_add_pinned(values, keys[i], n, CF_RANK_OVERRIDE, v)
                    : cf_ks_add_resolved(values, keys[i], n, CF_RANK_OVERRIDE, v, NULL);
        if (!added) {
            cf_error_set(err, CF_E_MEMORY, keys[i], NULL, "no room for running values");
            cf_arena_free(ar);
            return NULL;
        }
    }
    if (!cf_ks_resolve(values) || !cf_source_snapshot(&source, "running", values)) {
        cf_error_set(err, CF_E_MEMORY, NULL, NULL, "no room for running values");
        cf_arena_free(ar);
        return NULL;
    }
    source.rank = CF_RANK_OVERRIDE;
    return derive_onto(base, ar, CF_TRUE, "running", NULL, count, &source, err);
}

/* ------------------------------------------------------------------ *
 * Writing
 *
 * Three questions, in the order the specification asks them, and any
 * "no" refuses every item before one is written: can the source be
 * written, may this program write it, and would the contract accept
 * each value from it.
 *
 * The last is the read side's own answer, not a second set of rules:
 * the items become candidates from the target, at its rank, and the
 * contract is applied to them exactly as it is when the store is read
 * back. A value that would be rejected there is refused here, because
 * written it would sit in the store rejected on every start - and a
 * key whose policy leaves the store out is a lock an operator set,
 * which a program able to write past it would have lifted.
 * ------------------------------------------------------------------ */

/* Whether the key's policy leaves this source out: a lock, which is a
   policy refusal rather than a value the contract found wrong. */
static cf_bool write_locked(const cf_def* d, const char* name) {
    size_t j;

    if (d == NULL || d->policy == NULL) {
        return CF_FALSE;
    }
    for (j = 0; d->policy[j] != NULL; ++j) {
        if (strcmp(d->policy[j], name) == 0) {
            return CF_FALSE;
        }
    }
    return CF_TRUE;
}

/* The items as the read side would see them from `s`. Required keys
   and strict invalidity are left out of the copy of the contract: the
   items are a few keys, not a configuration, and a refusal is reported
   here with the reason, not as the first failure the pass meets. */
static cf_bool write_authorized(const cf_config* c, const cf_source* s,
                                const cf_write_item* items, size_t count,
                                cf_error* err) {
    const cf_allocator* alloc = cf_arena_allocator(c->arena);
    cf_arena*           ar;
    cf_keyspace*        ks;
    cf_defset*          ds;
    cf_rank_names       names;
    cf_bool             ok = CF_TRUE;
    size_t              i;

    /* The write path is cold, and the store's own library allocates
       whatever this does; a configuration built in a fixed arena
       borrows the default allocator rather than growing that arena,
       which readers may be using. */
    ar = cf_arena_new((alloc != NULL) ? alloc : cf_default_allocator(), 0);
    ks = (ar != NULL) ? (cf_keyspace*)cf_arena_calloc(ar, sizeof *ks) : NULL;
    ds = (ar != NULL) ? (cf_defset*)cf_arena_calloc(ar, sizeof *ds) : NULL;
    if (ks == NULL || ds == NULL || !cf_ks_init(ks, ar, count + 1) ||
        !cf_defset_init(ds, ar, c->defs.count + 1)) {
        cf_arena_free(ar);
        return cf_error_set(err, CF_E_MEMORY, NULL, NULL, "no room to check a write");
    }
    for (i = 0; ok && i < c->defs.count; ++i) {
        cf_def d = c->defs.defs[i];
        d.flags &= ~(unsigned)CF_REQUIRED;
        d.on_invalid = CF_INVALID_FALLBACK;
        ok           = cf_define(ds, &d, err);
    }
    for (i = 0; ok && i < c->defs.open_count; ++i) {
        ok = cf_defset_open(ds, c->defs.open_prefixes[i], err);
    }
    cf_defset_seal(ds);
    ds->partial = CF_TRUE;

    for (i = 0; ok && i < count; ++i) {
        const char*   key = items[i].key;
        size_t        n   = (key != NULL) ? strlen(key) : 0;
        const cf_def* d;

        if (key == NULL || !cf_key_valid(key, n)) {
            ok = cf_error_set(err, CF_E_KEY, key, s->name, "'%s' is not a valid key",
                              (key != NULL) ? key : "");
            break;
        }
        if (items[i].op != CF_WRITE_SET && items[i].op != CF_WRITE_DELETE) {
            ok = cf_error_set(err, CF_E_CAPABILITY, key, s->name,
                              "'%s': %u is not a write operation (CF_WRITE_SET, "
                              "CF_WRITE_DELETE)",
                              key, items[i].op);
            break;
        }
        if (items[i].op == CF_WRITE_DELETE) {
            continue; /* no value to hold to the contract */
        }
        d = cf_defset_find(&c->defs, key, n);
        if (d != NULL && (d->flags & CF_TRANSIENT) != 0) {
            /* A command, not a setting: written, it would run again on
               every start. */
            ok = cf_error_set(err, CF_E_VALIDATE, key, s->name,
                              "'%s' is a command, not a setting, and is not written",
                              key);
            break;
        }
        if (!cf_ks_add(ks, key, n, s->rank, &items[i].value, s->name)) {
            ok = cf_error_set(err, CF_E_MEMORY, key, s->name,
                              "no room to check a write");
        }
    }
    if (ok && !cf_ks_resolve(ks)) {
        ok = cf_error_set(err, CF_E_MEMORY, NULL, NULL, "no room to check a write");
    }
    if (ok) {
        names.name_of = config_name_of_rank;
        names.ctx     = &c->sv;
        ok            = cf_defs_apply(ks, ds, &names, c->unknown, NULL, err);
    }
    for (i = 0; ok && i < cf_ks_count(ks); ++i) {
        const cf_entry*     e = cf_ks_at(ks, i);
        const cf_candidate* k;

        for (k = e->chain; k != NULL; k = k->next) {
            if (k->outcome == CF_OUT_REJECTED) {
                const cf_def* d   = cf_defset_find(&c->defs, e->key, e->keylen);
                const char*   why = (k->reason != NULL) ? k->reason : "refused";
                cf_code code = write_locked(d, s->name) ? CF_E_POLICY : CF_E_VALIDATE;

                ok = cf_error_set(err, code, e->key, s->name,
                                  "'%s' is not written to '%s': %s", e->key, s->name,
                                  why);
                break;
            }
        }
    }
    cf_arena_free(ar);
    return ok;
}

/* Only an existing contributing element has an unambiguous local
   coordinate. A count-free source instead writes final-index patches.
   Item constraints are shared by every index, so mapping cannot change
   the authorization just checked. */
static cf_bool write_local_keys(const cf_config* c, const cf_source* s,
                                const cf_write_item* items, size_t count,
                                cf_arena** storage, const cf_write_item** local,
                                cf_error* err) {
    cf_write_item* mapped = NULL;
    size_t         i;
    for (i = 0; i < count; ++i) {
        const char*         key = items[i].key;
        size_t              n   = strlen(key);
        const cf_def*       d   = cf_defset_list(&c->defs, key, n);
        const cf_entry*     length;
        const cf_entry*     element;
        const cf_candidate* candidate;
        const char*         original;
        const char*         suffix;
        size_t              rootlen;
        char                physical[CF_KEY_MAX];
        cf_bool             contributes = CF_FALSE;
        if (d == NULL || d->list_mode != CF_LIST_APPEND) {
            continue;
        }
        rootlen = strlen(d->key);
        if (n <= rootlen + 1 || !cf_is_digit((unsigned char)key[rootlen + 1]) ||
            cf_defset_find(&c->defs, key, n) == NULL) {
            return cf_error_set(err, CF_E_CAPABILITY, key, s->name,
                                "append list structure requires a source-local edit; "
                                "cf_write addresses existing final indices");
        }
        element = cf_ks_find(&c->keys, key, n);
        if (element == NULL || element->selected == NULL) {
            return cf_error_set(err, CF_E_CAPABILITY, key, s->name,
                                "'%s' is not an existing appended element", key);
        }
        n      = cf_fmt(physical, sizeof physical, "%s.count", d->key);
        length = cf_ks_find(&c->keys, physical, n);
        for (candidate = length != NULL ? length->chain : NULL; candidate != NULL;
             candidate = candidate->next) {
            if (candidate->source == s->rank &&
                (candidate->flags & CF_CAND_LIST_RESULT) == 0) {
                contributes = CF_TRUE;
                break;
            }
        }
        if (!contributes) {
            continue;
        }
        if (items[i].op == CF_WRITE_DELETE) {
            return cf_error_set(err, CF_E_CAPABILITY, key, s->name,
                                "removing an appended element requires a source-local "
                                "edit of the list and its count");
        }
        for (candidate = element->chain; candidate != NULL;
             candidate = candidate->next) {
            if (candidate->source == s->rank &&
                cf_outcome_can_win(candidate->outcome)) {
                break;
            }
        }
        if (candidate == NULL) {
            return cf_error_set(err, CF_E_CAPABILITY, key, s->name,
                                "'%s' does not contribute this appended element",
                                s->name);
        }
        original = cf_ks_original_key(&c->keys, candidate);
        if (original == NULL) {
            continue;
        }
        suffix = strrchr(original, '.');
        if (suffix == NULL || cf_fmt(physical, sizeof physical, "%s%s", d->key,
                                     suffix) >= sizeof physical) {
            return cf_error_set(err, CF_E_KEY, key, s->name,
                                "the source-local list key exceeds the key limit");
        }
        if (strcmp(key, physical) == 0) {
            continue;
        }
        if (mapped == NULL) {
            const cf_allocator* alloc = cf_arena_allocator(c->arena);
            *storage = cf_arena_new(alloc != NULL ? alloc : cf_default_allocator(), 0);
            if (*storage != NULL && count <= (size_t)-1 / sizeof *mapped) {
                mapped =
                    (cf_write_item*)cf_arena_alloc(*storage, count * sizeof *mapped);
            }
            if (mapped == NULL) {
                return cf_error_set(err, CF_E_MEMORY, key, s->name,
                                    "no room to map appended list indices");
            }
            memcpy(mapped, items, count * sizeof *mapped);
            *local = mapped;
        }
        {
            cf_write_item* slot = mapped + i; /* mapped holds count, i < count */
            slot->key           = cf_arena_dup(*storage, physical, strlen(physical));
            if (slot->key == NULL) {
                return cf_error_set(err, CF_E_MEMORY, key, s->name,
                                    "no room to map appended list indices");
            }
        }
    }
    return CF_TRUE;
}

cf_bool cf_write(const cf_config* c, const char* target, const cf_write_item* items,
                 size_t count, const char* meta, cf_error* err) {
    const cf_source*     s       = NULL;
    cf_bool              granted = CF_FALSE;
    cf_bool              ok;
    cf_arena*            storage = NULL;
    const cf_write_item* local   = items;
    size_t               i;

    if (err != NULL) {
        cf_error_clear(err);
    }
    if (c == NULL || target == NULL || (items == NULL && count > 0)) {
        return cf_error_set(err, CF_E_SOURCE, NULL, NULL,
                            "nothing to write, or nowhere to write it");
    }
    for (i = 0; i < c->sv.count && !granted; ++i) {
        const cf_source* o = c->sv.items[i];
        if (o != NULL && o->name != NULL && strcmp(o->name, target) == 0) {
            s       = o;
            granted = c->writable[i];
        }
    }
    if (s == NULL) {
        return cf_error_set(err, CF_E_SOURCE, NULL, target,
                            "this configuration has no source named '%s'", target);
    }
    /* What it can do, then what it may. */
    if (s->vt == NULL || s->vt->write == NULL) {
        return cf_error_set(err, CF_E_CAPABILITY, NULL, target,
                            "'%s' is a %s source, which cannot be written: a running "
                            "program's changes go to a store",
                            target, (s->vt != NULL) ? s->vt->kind : "?");
    }
    if (!granted) {
        return cf_error_set(err, CF_E_POLICY, NULL, target,
                            "'%s' may not be written: nothing granted it "
                            "(cf_builder_writable)",
                            target);
    }
    if (!write_authorized(c, s, items, count, err)) {
        return CF_FALSE;
    }
    ok = write_local_keys(c, s, items, count, &storage, &local, err);
    if (ok) {
        ok = s->vt->write(s, local, count, meta, err);
    }
    cf_arena_free(storage);
    return ok;
}

/* ------------------------------------------------------------------ *
 * Change notification
 *
 * A pollable question, not a callback. A library that owns a thread and
 * calls you back has made a threading decision that belongs to the
 * caller, and calling back while that caller reads is the race the
 * threading contract exists to avoid.
 *
 * Reload is a rebuild: run the builder again and swap. The old snapshot
 * stays valid for as long as anybody holds it, so enabling this changes
 * no contract - which the design expected to be the one real conflict
 * between the simple and the capable tiers, and is not, because
 * snapshots became immutable and counted for other reasons.
 * ------------------------------------------------------------------ */

size_t cf_watch_count(const cf_config* c) {
    return (c != NULL) ? c->watch_count : 0;
}

const char* cf_watch_path(const cf_config* c, size_t i) {
    if (c == NULL || i >= c->watch_count) {
        return NULL;
    }
    return c->watch_paths[i];
}

cf_bool cf_changed(const cf_config* c) {
    size_t i;

    if (c == NULL) {
        return CF_FALSE;
    }
    for (i = 0; i < c->watch_count; ++i) {
        cf_stamp now;
        cf_platform_stamp(c->watch_paths[i], &now);
        /* Appearing and disappearing are both changes: an optional file
           that was missing and now is not is exactly the case somebody
           wants to be told about. */
        if (now.present != c->watch_stamps[i].present ||
            now.mtime != c->watch_stamps[i].mtime ||
            now.size != c->watch_stamps[i].size) {
            return CF_TRUE;
        }
    }
    return CF_FALSE;
}

/* The watcher is the platform's object under a public name. Nothing is
   added on top of it, so there is nothing here to keep in step. */
cf_watcher* cf_watcher_open(const cf_config* c, const cf_allocator* a, cf_error* err) {
    if (c == NULL) {
        cf_error_set(err, CF_E_CAPABILITY, NULL, NULL, "no configuration to watch");
        return NULL;
    }
    if (c->watch_count == 0) {
        /* A handle that can never be signalled would let a loop wait
           forever on it and look healthy doing so. */
        cf_error_set(err, CF_E_CAPABILITY, NULL, NULL,
                     "this configuration was read from no file, so nothing can change "
                     "under it");
        return NULL;
    }
    if (a == NULL) {
        a = cf_default_allocator();
    }
    return cf_platform_wake_open(c->watch_paths, c->watch_count, a, err);
}

int cf_watcher_fd(const cf_watcher* w) {
    return cf_platform_wake_fd(w);
}

void* cf_watcher_handle(const cf_watcher* w) {
    return cf_platform_wake_handle(w);
}

cf_bool cf_watcher_drain(cf_watcher* w) {
    return cf_platform_wake_drain(w);
}

int cf_watcher_wait(cf_watcher* w, long timeout_ms) {
    return cf_platform_wake_wait(w, timeout_ms);
}

void cf_watcher_close(cf_watcher* w) {
    cf_platform_wake_close(w);
}

void cf_config_ref(cf_config* c) {
    if (c != NULL) {
        cf_ref_inc(&c->refs);
    }
}

void cf_config_free(cf_config* c) {
    if (c == NULL) {
        return;
    }
    /* The config lives inside the arena it releases, so the count has
       to be decremented before anything is freed and its result read
       from a local. Touching c after the free is a use-after-free of
       the object doing the freeing. */
    if (cf_ref_dec(&c->refs) == 0 && c->owns_arena) {
        cf_arena_free(c->arena);
    }
}

/* ------------------------------------------------------------------ *
 * Reading
 * ------------------------------------------------------------------ */

static const cf_value* lookup(const cf_config* c, const char* key) {
    const cf_entry* e;

    if (c == NULL || key == NULL) {
        return NULL;
    }
    e = cf_ks_find(&c->keys, key, strlen(key));
    if (e == NULL) {
        return NULL;
    }
    if (c->reads != NULL) {
        cf_refcount* flag = &c->reads[e - c->keys.entries];
        if (cf_ref_get(flag) == 0) {
            cf_ref_inc(flag);
        }
    }
    return (e->selected != NULL) ? &e->selected->value : NULL;
}

cf_bool cf_get_bool(const cf_config* c, const char* key, cf_bool fallback) {
    const cf_value* v = lookup(c, key);
    return (v != NULL && v->type == CF_TYPE_BOOL) ? v->u.b : fallback;
}

cf_i64 cf_get_int(const cf_config* c, const char* key, cf_i64 fallback) {
    const cf_value* v = lookup(c, key);
    return (v != NULL && v->type == CF_TYPE_INT) ? v->u.i : fallback;
}

const char* cf_get_text(const cf_config* c, const char* key, const char* fallback) {
    const cf_value* v = lookup(c, key);
    return (v != NULL && v->type == CF_TYPE_TEXT) ? v->u.t.p : fallback;
}

const char* cf_get_text_n(const cf_config* c, const char* key, size_t* len,
                          const char* fallback) {
    const cf_value* v = lookup(c, key);

    if (v != NULL && v->type == CF_TYPE_TEXT) {
        if (len != NULL) {
            *len = v->u.t.n;
        }
        return v->u.t.p;
    }
    if (len != NULL) {
        *len = (fallback != NULL) ? strlen(fallback) : 0;
    }
    return fallback;
}

#if CF_WITH_REAL
double cf_get_real(const cf_config* c, const char* key, double fallback) {
    const cf_value* v = lookup(c, key);
    return (v != NULL && v->type == CF_TYPE_REAL) ? v->u.r : fallback;
}
#endif

static cf_code try_of(const cf_config* c, const char* key, cf_type want,
                      const cf_value** out) {
    const cf_value* v = lookup(c, key);

    if (v == NULL) {
        return CF_E_KEY;
    }
    if (v->type != want) {
        return CF_E_CONVERT;
    }
    *out = v;
    return CF_OK;
}

cf_code cf_try_bool(const cf_config* c, const char* key, cf_bool* out) {
    const cf_value* v = NULL;
    cf_code         r = try_of(c, key, CF_TYPE_BOOL, &v);
    if (r == CF_OK && out != NULL) {
        *out = v->u.b;
    }
    return r;
}

cf_code cf_try_int(const cf_config* c, const char* key, cf_i64* out) {
    const cf_value* v = NULL;
    cf_code         r = try_of(c, key, CF_TYPE_INT, &v);
    if (r == CF_OK && out != NULL) {
        *out = v->u.i;
    }
    return r;
}

cf_code cf_try_text(const cf_config* c, const char* key, const char** out,
                    size_t* len) {
    const cf_value* v = NULL;
    cf_code         r = try_of(c, key, CF_TYPE_TEXT, &v);
    if (r == CF_OK) {
        if (out != NULL) {
            *out = v->u.t.p;
        }
        if (len != NULL) {
            *len = v->u.t.n;
        }
    }
    return r;
}

#if CF_WITH_REAL
cf_code cf_try_real(const cf_config* c, const char* key, double* out) {
    const cf_value* v = NULL;
    cf_code         r = try_of(c, key, CF_TYPE_REAL, &v);
    if (r == CF_OK && out != NULL) {
        *out = v->u.r;
    }
    return r;
}
#endif

cf_type cf_type_of(const cf_config* c, const char* key) {
    const cf_value* v = lookup(c, key);
    return (v != NULL) ? v->type : CF_TYPE_NULL;
}

cf_bool cf_has(const cf_config* c, const char* key) {
    return (lookup(c, key) != NULL) ? CF_TRUE : CF_FALSE;
}

cf_bool cf_tracks_reads(const cf_config* c) {
    return (c != NULL && c->reads != NULL) ? CF_TRUE : CF_FALSE;
}

cf_bool cf_was_read(const cf_config* c, const char* key) {
    const cf_entry* e;

    if (c == NULL || c->reads == NULL || key == NULL) {
        return CF_FALSE;
    }
    e = cf_ks_find(&c->keys, key, strlen(key));
    return (e != NULL && cf_ref_get(&c->reads[e - c->keys.entries]) != 0) ? CF_TRUE
                                                                          : CF_FALSE;
}

size_t cf_read_count(const cf_config* c) {
    size_t i, n = 0;

    if (c == NULL || c->reads == NULL) {
        return 0;
    }
    for (i = 0; i < cf_ks_count(&c->keys); ++i) {
        if (cf_ref_get(&c->reads[i]) != 0) {
            ++n;
        }
    }
    return n;
}

/* ------------------------------------------------------------------ *
 * Diagnostics
 * ------------------------------------------------------------------ */

size_t cf_explain(const cf_config* c, const char* key, char* buf, size_t cap) {
    if (c == NULL || key == NULL) {
        if (buf != NULL && cap > 0) {
            buf[0] = '\0';
        }
        return 0;
    }
    return cf_ks_explain_redacted(&c->keys, &c->sv, &c->defs, key, strlen(key), buf,
                                  cap);
}

size_t cf_inspect(const cf_config* c, char* buf, size_t cap) {
    if (c == NULL) {
        if (buf != NULL && cap > 0) {
            buf[0] = '\0';
        }
        return 0;
    }
    return cf_ks_inspect_redacted(&c->keys, &c->sv, &c->defs, buf, cap);
}

size_t cf_schema(const cf_config* c, const char* title, char* buf, size_t cap) {
    cf_error e = CF_ERROR_INIT;

    if (c == NULL) {
        if (buf != NULL && cap > 0) {
            buf[0] = '\0';
        }
        return 0;
    }
    return cf_schema_write(&c->defs, title,
                           (c->unknown == CF_UNKNOWN_REJECT) ? CF_TRUE : CF_FALSE, buf,
                           cap, &e);
}

cf_u64 cf_digest(const cf_config* c) {
    return (c != NULL) ? cf_ks_digest(&c->keys) : 0;
}

cf_u64 cf_fingerprint(const cf_config* c) {
    return cf_defset_fingerprint((c != NULL) ? &c->defs : NULL,
                                 (c != NULL) ? &c->keys : NULL);
}

size_t cf_fingerprint_format(cf_u64 fp, char* buf, size_t cap) {
    return cf_ks_digest_text(fp, buf, cap);
}

size_t cf_count(const cf_config* c) {
    return (c != NULL) ? cf_ks_count(&c->keys) : 0;
}

const cf_apply_report* cf_apply_stats(const cf_config* c) {
    return (c != NULL) ? &c->report : NULL;
}

const cf_source_view* cf_sources(const cf_config* c) {
    return (c != NULL) ? &c->sv : NULL;
}

const cf_defset* cf_defs(const cf_config* c) {
    return (c != NULL) ? &c->defs : NULL;
}

size_t cf_explain_json(const cf_config* c, const char* key, cf_report_mode mode,
                       char* buf, size_t cap) {
    if (c == NULL || key == NULL) {
        if (buf != NULL && cap > 0) {
            buf[0] = '\0';
        }
        return 0;
    }
    return cf_report_key_write(&c->keys, &c->defs, &c->sv, key, strlen(key), mode, buf,
                               cap);
}

size_t cf_report(const cf_config* c, const cf_report_info* info, char* buf,
                 size_t cap) {
    if (c == NULL) {
        if (buf != NULL && cap > 0) {
            buf[0] = '\0';
        }
        return 0;
    }
    return cfi_report_reads(&c->keys, &c->defs, &c->sv, info, c->reads, buf, cap);
}

const cf_keyspace* cf_keys(const cf_config* c) {
    return (c != NULL) ? &c->keys : NULL;
}
