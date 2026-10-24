/*
 * secrets.c - a directory of secrets, one file a key, as a source. C89
 * where it can be.
 *
 * Docker and Kubernetes mount secrets as files in a directory, and
 * systemd hands credentials over the same way ($CREDENTIALS_DIRECTORY):
 * the file's name says what it is, its content is the value, and the
 * value never passes through the environment, where every child process
 * and `ps e` can read it. So:
 *
 *     /run/secrets/db.password        db.password = <the file>
 *     /run/secrets/MYAPP_DB__PASSWORD db.password, with prefix MYAPP_
 *
 * A name that is a key is that key; any other is read as an environment
 * variable's name is, so the spellings people already use for secrets
 * work, and a name that is neither is an error, because the directory
 * is the program's and a file in it that configures nothing is a
 * mistake somebody would want to hear about. The one trailing newline
 * that `echo` and most editors leave is not part of the value. Values
 * are text, typed by the contract; marking them CF_SENSITIVE is the
 * contract's job too, and for anything read from here it should.
 *
 * Under CF_WITH_DIR, because the listing is the drop-in directory's.
 */

#include "configforge/cf_keyspace.h"
#include "configforge/cf_platform.h"
#include "configforge/cf_serialize.h"
#include "configforge/cf_source.h"

#include <string.h>

#define CF_SECRETS_MAX_FILES 256

typedef struct secrets_state {
    const char* path;
    const char* prefix;
    unsigned    flags;
    cf_error    err;
    cf_bool     failed;
} secrets_state;

#if CF_WITH_DIR

typedef char secrets_name[CF_PLATFORM_NAME_MAX];

static cf_bool secrets_load(const cf_source* s, cf_keyspace* ks) {
    secrets_state* st = (secrets_state*)s->ext;
    cf_arena*      ar = cf_ks_arena(ks);
    secrets_name*  names;
    char           full[CF_PLATFORM_NAME_MAX];
    char           key[CF_KEY_MAX];
    int            n, i;

    st->failed = CF_FALSE;
    cf_error_clear(&st->err);

    names = (secrets_name*)cf_arena_alloc(ar, (size_t)CF_SECRETS_MAX_FILES *
                                                  CF_PLATFORM_NAME_MAX);
    if (names == NULL) {
        st->failed = CF_TRUE;
        return cf_error_set(&st->err, CF_E_MEMORY, NULL, st->path,
                            "no room to list '%s'", st->path);
    }
    n = cf_platform_dir_list(st->path, names, CF_SECRETS_MAX_FILES, &st->err);
    if (n < 0) {
        if ((st->flags & CF_FILE_OPTIONAL) != 0) {
            cf_error_clear(&st->err);
            return CF_TRUE;
        }
        st->failed = CF_TRUE;
        return CF_FALSE;
    }

    /* No order is needed: every file is a different key. */
    for (i = 0; i < n; ++i) {
        const char* name = names[i];
        size_t      nlen = strlen(name);
        size_t      klen;
        cf_blob     blob;
        size_t      vlen;

        if (cf_fmt(full, sizeof full, "%s/%s", st->path, name) >= sizeof full) {
            st->failed = CF_TRUE;
            return cf_error_set(&st->err, CF_E_SOURCE, NULL, st->path,
                                "a secret's path is too long: '%s/%s'", st->path, name);
        }
        if (cf_key_valid(name, nlen) && nlen < sizeof key) {
            memcpy(key, name, nlen + 1);
            klen = nlen;
        } else {
            klen = cf_env_key((st->prefix != NULL) ? st->prefix : "", name, nlen, key,
                              sizeof key);
        }
        if (klen == 0) {
            st->failed = CF_TRUE;
            return cf_error_set(
                &st->err, CF_E_KEY, NULL, full,
                "'%s' names no key: a secret's file is named as the key "
                "(db.password) or as its environment variable would be%s%s",
                full, (st->prefix != NULL) ? ", with the prefix " : "",
                (st->prefix != NULL) ? st->prefix : "");
        }
        if (!cf_blob_file(&blob, ar, full, &st->err)) {
            st->failed = CF_TRUE;
            return CF_FALSE;
        }
        vlen = blob.n;
        if (vlen > 0 && blob.p[vlen - 1] == '\n') {
            --vlen;
            if (vlen > 0 && blob.p[vlen - 1] == '\r') {
                --vlen;
            }
        }
        if (!cf_ks_add_text(ks, key, klen, s->rank, blob.p, vlen, full)) {
            st->failed = CF_TRUE;
            return cf_error_set(&st->err, CF_E_MEMORY, NULL, full,
                                "no room to read '%s'", full);
        }
    }
    return CF_TRUE;
}

#else /* !CF_WITH_DIR */

static cf_bool secrets_load(const cf_source* s, cf_keyspace* ks) {
    secrets_state* st = (secrets_state*)s->ext;

    (void)ks;
    st->failed = CF_TRUE;
    return cf_error_set(&st->err, CF_E_CAPABILITY, NULL, st->path,
                        "'%s' is a secrets directory, and this build was compiled "
                        "without CF_WITH_DIR",
                        st->path);
}

#endif /* CF_WITH_DIR */

static void secrets_destroy(cf_source* s) {
    (void)s;
}

static const char* secrets_path(const cf_source* s) {
    const secrets_state* st = (const secrets_state*)s->ext;
    return (st != NULL) ? st->path : NULL;
}

static const cf_source_vtable cf_secrets_vt = {"secrets", secrets_load, secrets_destroy,
                                               secrets_path, NULL};

size_t cf_source_secrets_state_size(void) {
    return sizeof(secrets_state);
}

cf_bool cf_source_secrets(cf_source* s, const char* name, int rank, const char* path,
                          const char* prefix, unsigned flags, void* state,
                          size_t state_size) {
    secrets_state* st = (secrets_state*)state;

    if (s == NULL || path == NULL || st == NULL || state_size < sizeof(secrets_state)) {
        return CF_FALSE;
    }
    memset(st, 0, sizeof *st);
    st->path     = path;
    st->prefix   = prefix;
    st->flags    = flags;
    st->err.size = sizeof(cf_error);

    s->vt   = &cf_secrets_vt;
    s->name = (name != NULL) ? name : path;
    s->rank = rank;
    s->ext  = st;
    s->ref  = NULL;
    return CF_TRUE;
}

const cf_error* cf_source_secrets_error(const cf_source* s) {
    const secrets_state* st;

    if (s == NULL || s->vt != &cf_secrets_vt || s->ext == NULL) {
        return NULL;
    }
    st = (const secrets_state*)s->ext;
    return st->failed ? &st->err : NULL;
}
