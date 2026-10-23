/*
 * config.c - the builder and the configuration facade. C89.
 *
 * Everything here is assembly. The work is done by the key space, the
 * sources and the definition set; this file exists so that a caller
 * never has to know that.
 */

#include "configforge/configforge.h"
#include "configforge/cf_platform.h"

#include <string.h>

#define CF_BUILDER_SOURCES 16

struct cf_builder {
    cf_arena*         arena;
    cf_source         sources[CF_BUILDER_SOURCES];
    size_t            nsources;
    cf_defset         defs;
    cf_unknown_policy unknown;

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
};

/* ------------------------------------------------------------------ *
 * Builder
 * ------------------------------------------------------------------ */

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

cf_bool cf_builder_memory(cf_builder* b, const char* name, int rank,
                          const cf_pair* pairs, size_t count) {
    cf_source* s = builder_slot(b);
    if (s == NULL || !cf_source_memory(s, name, rank, pairs, count)) {
        return CF_FALSE;
    }
    ++b->nsources;
    return CF_TRUE;
}

cf_bool cf_builder_env(cf_builder* b, const char* name, int rank, const char* prefix,
                       char* const* environment) {
    cf_source* s = builder_slot(b);
    if (s == NULL || !cf_source_env(s, name, rank, prefix, environment)) {
        return CF_FALSE;
    }
    ++b->nsources;
    return CF_TRUE;
}

cf_bool cf_builder_cli(cf_builder* b, const char* name, int rank, int argc,
                       char* const* argv) {
    cf_source* s = builder_slot(b);
    if (s == NULL || !cf_source_cli(s, name, rank, argc, argv)) {
        return CF_FALSE;
    }
    ++b->nsources;
    return CF_TRUE;
}

cf_bool cf_builder_file(cf_builder* b, const char* name, int rank, const char* path,
                        const char* format, unsigned flags) {
    cf_source* s = builder_slot(b);
    void*      st;

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
    if (!cf_source_file(s, name, rank, path, format, flags, st,
                        cf_source_file_state_size())) {
        return CF_FALSE;
    }
    ++b->nsources;
    return CF_TRUE;
}

cf_bool cf_builder_file_codec(cf_builder* b, const char* name, int rank,
                              const char* path, const struct cf_codec* codec,
                              unsigned flags) {
    cf_source* s = builder_slot(b);
    void*      st;

    if (s == NULL) {
        return CF_FALSE;
    }
    if (codec == NULL) {
        cf_error e = CF_ERROR_INIT;
        cf_error_set(&e, CF_E_SOURCE, NULL, path, "no codec given for '%s'", path);
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
    if (!cf_source_file_codec(s, name, rank, path, codec, flags, st,
                              cf_source_file_state_size())) {
        return CF_FALSE;
    }
    ++b->nsources;
    return CF_TRUE;
}

cf_bool cf_builder_dotenv(cf_builder* b, const char* name, int rank, const char* path,
                          const char* prefix, unsigned flags) {
    cf_source* s = builder_slot(b);
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
    if (!cf_source_dotenv(s, name, rank, path, prefix, flags, st,
                          cf_source_dotenv_state_size())) {
        return CF_FALSE;
    }
    ++b->nsources;
    return CF_TRUE;
}

cf_bool cf_builder_dir(cf_builder* b, const char* name, int rank, const char* path,
                       const char* format, unsigned flags) {
    cf_source* s = builder_slot(b);
    void*      st;

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
    if (!cf_source_dir(s, name, rank, path, format, flags, st,
                       cf_source_dir_state_size())) {
        return CF_FALSE;
    }
    ++b->nsources;
    return CF_TRUE;
}

cf_bool cf_builder_replay(cf_builder* b, const char* name, int rank, const char* path) {
    cf_source* s = builder_slot(b);
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
    if (!cf_source_replay(s, name, rank, path, st, cf_source_replay_state_size())) {
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
    *slot = *src;
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

void cf_builder_unknown(cf_builder* b, cf_unknown_policy policy) {
    if (b != NULL) {
        b->unknown = policy;
    }
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
    if (err != NULL) {
        *err = *what;
    }
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
        if (err != NULL) {
            *err = e;
        }
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

    cf_defset_seal(&c->defs);

    /* Hand the key list to the sources that can use it: the
       environment gains its natural spelling, the command line gains
       --key value, --no-key and aliases. Neither can be parsed
       unambiguously without it, which is why both are Tier 1. */
    if (cf_defset_count(&c->defs) > 0) {
        for (i = 0; i < c->sv.count; ++i) {
            if (c->sources[i].vt == NULL) {
                continue;
            }
            if (strcmp(c->sources[i].vt->kind, "env") == 0) {
                cf_source_env_defs(&c->sources[i], &c->defs);
            } else if (strcmp(c->sources[i].vt->kind, "cli") == 0) {
                cf_source_cli_defs(&c->sources[i], &c->defs);
            }
        }
    }

    /* Defaults are a source like any other, at the lowest rank. They
       are appended after the caller's sources so that a caller who
       registered their own default source still gets theirs. */
    if (cf_defset_count(&c->defs) > 0 && c->sv.count < CF_BUILDER_SOURCES) {
        size_t at = c->sv.count;
        cf_source_defaults(&c->sources[at], "default", CF_RANK_DEFAULT, &c->defs);
        c->view[at] = &c->sources[at];
        c->sv.count = at + 1;
    }

    if (!cf_ks_init(&c->keys, c->arena, 64)) {
        cf_error_set(&e, CF_E_MEMORY, NULL, NULL, "no room for the key space");
        return create_failed(b, err, &e);
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
            if (p == NULL) {
                continue;
            }
            c->watch_paths[c->watch_count] = p;
            cf_platform_stamp(p, &c->watch_stamps[c->watch_count]);
            ++c->watch_count;
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

static cf_config* derive_onto(const cf_config* base, cf_arena* ar, cf_bool owns,
                              const char* source_name, const cf_pair* pairs,
                              size_t count, cf_error* err) {
    cf_builder* b;
    size_t      i;

    b = builder_on(ar, owns);
    if (b == NULL) {
        cf_error_set(err, CF_E_MEMORY, NULL, NULL, "no room to derive a configuration");
        return NULL;
    }

    /* Every candidate the base holds, not only the winners: dropping
       the losers would make explain() on the derived configuration say
       less than on the one it came from, which is the wrong direction
       for this library to move in. */
    {
        cf_source* s = builder_slot(b);
        if (s == NULL || !cf_source_snapshot(s, "inherited", &base->keys)) {
            cf_error_set(err, CF_E_MEMORY, NULL, NULL,
                         "no room for the inherited configuration");
            cf_builder_abort(b);
            return NULL;
        }
        ++b->nsources;
    }

    /* The overrides go in after the base, not before. Rank already puts
       them above everything, so the order looks irrelevant - and is
       not, because deriving twice gives two candidates at the same
       override rank and the later one has to win. Adding them first
       made the *inherited* override the later of the two, so a second
       change to the same key did nothing. */
    if (count > 0) {
        cf_builder_memory(b, (source_name != NULL) ? source_name : "set",
                          CF_RANK_OVERRIDE, pairs, count);
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
                       err);
}

cf_config* cf_derive_in(const cf_config* base, cf_arena* arena, const char* source_name,
                        const cf_pair* pairs, size_t count, cf_error* err) {
    if (base == NULL || arena == NULL) {
        cf_error_set(err, CF_E_SOURCE, NULL, NULL, "nothing to derive from");
        return NULL;
    }
    return derive_onto(base, arena, CF_FALSE, source_name, pairs, count, err);
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
    if (c == NULL || key == NULL) {
        return NULL;
    }
    return cf_ks_get(&c->keys, key, strlen(key));
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
    return cf_ks_explain(&c->keys, &c->sv, key, strlen(key), buf, cap);
}

size_t cf_inspect(const cf_config* c, char* buf, size_t cap) {
    if (c == NULL) {
        if (buf != NULL && cap > 0) {
            buf[0] = '\0';
        }
        return 0;
    }
    return cf_ks_inspect(&c->keys, &c->sv, buf, cap);
}

cf_u64 cf_digest(const cf_config* c) {
    return (c != NULL) ? cf_ks_digest(&c->keys) : 0;
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

size_t cf_report(const cf_config* c, const cf_report_info* info, char* buf,
                 size_t cap) {
    if (c == NULL) {
        if (buf != NULL && cap > 0) {
            buf[0] = '\0';
        }
        return 0;
    }
    return cf_report_write(&c->keys, &c->defs, &c->sv, info, buf, cap);
}

const cf_keyspace* cf_keys(const cf_config* c) {
    return (c != NULL) ? &c->keys : NULL;
}
