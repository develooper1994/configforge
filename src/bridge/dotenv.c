/*
 * dotenv.c - a .env file as a source. C89.
 *
 * A bridge component because it reads bytes and produces key space
 * entries; the lexer it calls is pure and lives in src/sources.
 */

#include "configforge/cf_entry.h"
#include "configforge/cf_serialize.h"
#include "configforge/cf_source.h"

#include <string.h>

typedef struct dotenv_state {
    const char* path;
    const char* prefix;
    unsigned    flags;
    cf_error    err;
    cf_bool     failed;
} dotenv_state;

#if CF_WITH_DOTENV

static cf_bool dotenv_load(const cf_source* s, cf_keyspace* ks) {
    dotenv_state* st = (dotenv_state*)s->ext;
    cf_keysink    out;
    cf_keymap     map;
    cf_blob       blob;

    st->failed = CF_FALSE;
    cf_error_clear(&st->err);

    if (!cf_blob_file(&blob, cf_ks_arena(ks), st->path, &st->err)) {
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
    if (!cf_keysink_init(&out, ks, s->rank)) {
        st->failed = CF_TRUE;
        return cf_error_set(&st->err, CF_E_MEMORY, NULL, st->path,
                            "no room to read '%s'", st->path);
    }
    cf_keymap_init(&map, CF_KEYMAP_ENV, st->prefix);
    if (!cf_lex_dotenv(blob.p, blob.n, st->path, &map, cf_keysink_of(&out), &st->err)) {
        st->failed = CF_TRUE;
        return CF_FALSE;
    }
    return CF_TRUE;
}

#else /* !CF_WITH_DOTENV */

/* As for the directory source: present, and failing by name. */
static cf_bool dotenv_load(const cf_source* s, cf_keyspace* ks) {
    dotenv_state* st = (dotenv_state*)s->ext;

    (void)ks;
    st->failed = CF_TRUE;
    return cf_error_set(&st->err, CF_E_CAPABILITY, NULL, st->path,
                        "'%s' is a .env file, and this build was compiled without "
                        "CF_WITH_DOTENV",
                        st->path);
}

#endif /* CF_WITH_DOTENV */

static void dotenv_destroy(cf_source* s) {
    (void)s;
}

static const char* dotenv_path(const cf_source* s) {
    const dotenv_state* st = (const dotenv_state*)s->ext;
    return (st != NULL) ? st->path : NULL;
}

static const cf_source_vtable cf_dotenv_vt = {"dotenv", dotenv_load, dotenv_destroy,
                                              dotenv_path, NULL};

size_t cf_source_dotenv_state_size(void) {
    return sizeof(dotenv_state);
}

cf_bool cf_source_dotenv(cf_source* s, const char* name, int rank, const char* path,
                         const char* prefix, unsigned flags, void* state,
                         size_t state_size) {
    dotenv_state* st = (dotenv_state*)state;

    if (s == NULL || path == NULL || st == NULL || state_size < sizeof(dotenv_state)) {
        return CF_FALSE;
    }
    memset(st, 0, sizeof *st);
    st->path     = path;
    st->prefix   = prefix;
    st->flags    = flags;
    st->err.size = sizeof(cf_error);

    s->vt   = &cf_dotenv_vt;
    s->name = (name != NULL) ? name : path;
    s->rank = rank;
    s->ext  = st;
    s->ref  = NULL;
    return CF_TRUE;
}

const cf_error* cf_source_dotenv_error(const cf_source* s) {
    const dotenv_state* st;

    if (s == NULL || s->vt != &cf_dotenv_vt || s->ext == NULL) {
        return NULL;
    }
    st = (const dotenv_state*)s->ext;
    return st->failed ? &st->err : NULL;
}
