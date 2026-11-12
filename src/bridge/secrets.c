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
 * systemd's credentials are the same reading with a second vtable, kind
 * "credentials" (D-80): the directory is the one $CREDENTIALS_DIRECTORY
 * names when the source is loaded, not one the program names, and each
 * value is placed as "systemd credential <id> (<file>)", so explain()
 * and a report never pass one off as a plain file in a directory. The
 * variable unset or empty is nothing with CF_OPTIONAL and an error naming
 * it without; a set variable that names no directory is an error either
 * way, because systemd made the directory before it started the service.
 * Names, the trailing newline and sensitivity are as above, unchanged.
 * Nothing here knows the operating system: the variable is read through
 * the platform's environment, wherever it happens to be set.
 *
 * Under CF_WITH_DIR, because the listing is the drop-in directory's.
 */

#include "configforge/cf_entry.h"
#include "configforge/cf_keyspace.h"
#include "configforge/cf_platform.h"
#include "configforge/cf_serialize.h"
#include "configforge/cf_source.h"

#include <string.h>

#define CF_SECRETS_MAX_FILES 256

/* systemd's name for it (systemd.exec(5), "Credentials"). */
#define CF_SECRETS_CREDENTIALS_VAR "CREDENTIALS_DIRECTORY"

typedef struct secrets_state {
    const char* path; /* for credentials, `dir` once the variable is read */
    const char* prefix;
    const char* variable; /* the directory's variable; NULL for a path given */
    unsigned    flags;
    cf_error    err;
    cf_bool     failed;
    /* In the state rather than on the frame, which the listing's full
       path and key already take most of the stack budget of. */
    char dir[CF_PLATFORM_NAME_MAX];
    char at[2 * CF_PLATFORM_NAME_MAX + 32];
} secrets_state;

#if CF_WITH_DIR

typedef char secrets_name[CF_PLATFORM_NAME_MAX];

/* The directory $CREDENTIALS_DIRECTORY names, as it is now: a reload
   sees what it holds then. False with the error set when it cannot be
   used; true with st->path NULL when it is unset and may be. */
static cf_bool secrets_credentials_dir(secrets_state* st) {
    static const char secrets_unset[] =
        "the environment variable '%s' is not set: systemd sets it for a "
        "service given LoadCredential=, SetCredential= or ImportCredential=";
    size_t      n     = 0;
    const char* value = cf_env_value(cf_platform_environ(), st->variable, &n);

    st->path = NULL;
    if (value == NULL || n == 0) {
        if ((st->flags & CF_OPTIONAL) != 0) {
            return CF_TRUE; /* not run with credentials, and allowed not to be */
        }
        return cf_error_set(&st->err, CF_E_SOURCE, NULL, st->variable, secrets_unset,
                            st->variable);
    }
    if (n >= sizeof st->dir) {
        return cf_error_set(&st->err, CF_E_SOURCE, NULL, st->variable,
                            "'%s' names a directory too long to read", st->variable);
    }
    memcpy(st->dir, value, n);
    st->dir[n] = '\0';
    st->path   = st->dir;
    return CF_TRUE;
}

static cf_bool secrets_load(const cf_source* s, cf_keyspace* ks) {
    static const char secrets_nokey[] =
        "'%s' names no key: a %s's file is named as the key (db.password) or as "
        "its environment variable would be%s%s";
    secrets_state* st = (secrets_state*)s->ext;
    cf_arena*      ar = cf_ks_arena(ks);
    secrets_name*  names;
    char           full[CF_PLATFORM_NAME_MAX];
    char           key[CF_KEY_MAX];
    const char*    at;
    int            n, i;

    st->failed = CF_FALSE;
    cf_error_clear(&st->err);

    if (st->variable != NULL) {
        if (!secrets_credentials_dir(st)) {
            st->failed = CF_TRUE;
            return CF_FALSE;
        }
        if (st->path == NULL) {
            return CF_TRUE;
        }
    }
    names = (secrets_name*)cf_arena_alloc(ar, (size_t)CF_SECRETS_MAX_FILES *
                                                  CF_PLATFORM_NAME_MAX);
    if (names == NULL) {
        st->failed = CF_TRUE;
        return cf_error_set(&st->err, CF_E_MEMORY, NULL, st->path,
                            "no room to list '%s'", st->path);
    }
    n = cf_platform_dir_list(st->path, names, CF_SECRETS_MAX_FILES, &st->err);
    if (n < 0) {
        /* Only -1, a directory that is not there: -2 is one missing a
           secret, which optional does not cover (cf_platform.h). Nor
           does it cover credentials systemd said are in a directory
           that is not there: optional is for the variable. */
        if (n == -1 && (st->flags & CF_OPTIONAL) != 0 && st->variable == NULL) {
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
            return cf_error_set(&st->err, CF_E_KEY, NULL, full, secrets_nokey, full,
                                (st->variable != NULL) ? "systemd credential"
                                                       : "secret",
                                (st->prefix != NULL) ? ", with the prefix " : "",
                                (st->prefix != NULL) ? st->prefix : "");
        }
        /* A credential says it is one, by the id systemd knows it by; a
           secret is the file. */
        at = full;
        if (st->variable != NULL) {
            if (cf_fmt(st->at, sizeof st->at, "systemd credential %s (%s)", name,
                       full) >= sizeof st->at) {
                st->failed = CF_TRUE;
                return cf_error_set(&st->err, CF_E_SOURCE, NULL, full,
                                    "a credential's place is too long: '%s'", full);
            }
            at = st->at;
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
        if (!cf_ks_add_text(ks, key, klen, s->rank, blob.p, vlen, at)) {
            st->failed = CF_TRUE;
            return cf_error_set(&st->err, CF_E_MEMORY, NULL, full,
                                "no room to read '%s'", full);
        }
    }
    return CF_TRUE;
}

#else /* !CF_WITH_DIR */

static cf_bool secrets_load(const cf_source* s, cf_keyspace* ks) {
    static const char secrets_nodir[] =
        "'%s' is a %s, and this build was compiled without CF_WITH_DIR";
    secrets_state* st = (secrets_state*)s->ext;
    const char*    of = (st->variable != NULL) ? st->variable : st->path;

    (void)ks;
    st->failed = CF_TRUE;
    return cf_error_set(&st->err, CF_E_CAPABILITY, NULL, of, secrets_nodir, of,
                        (st->variable != NULL) ? "systemd credentials directory"
                                               : "secrets directory");
}

#endif /* CF_WITH_DIR */

static void secrets_destroy(cf_source* s) {
    (void)s;
}

static const char* secrets_path(const cf_source* s) {
    const secrets_state* st = (const secrets_state*)s->ext;
    return (st != NULL) ? st->path : NULL;
}

static const cf_source_vtable cf_secrets_vt = {
    sizeof(cf_source_vtable), "secrets",    secrets_load,
    secrets_destroy,          secrets_path, NULL};

/* The same reading under its own kind, so a report says which it was. */
static const cf_source_vtable cf_credentials_vt = {
    sizeof(cf_source_vtable), "credentials", secrets_load,
    secrets_destroy,          secrets_path,  NULL};

size_t cf_source_secrets_state_size(void) {
    return sizeof(secrets_state);
}

static cf_bool secrets_init(cf_source* s, const cf_source_vtable* vt, const char* name,
                            int rank, const char* path, const char* variable,
                            const char* prefix, unsigned flags, void* state,
                            size_t state_size) {
    secrets_state* st = (secrets_state*)state;

    if (s == NULL || st == NULL || state_size < sizeof(secrets_state)) {
        return CF_FALSE;
    }
    memset(st, 0, sizeof *st);
    st->path     = path;
    st->variable = variable;
    st->prefix   = prefix;
    st->flags    = flags;
    st->err.size = sizeof(cf_error);

    s->vt   = vt;
    s->name = name;
    s->rank = rank;
    s->ext  = st;
    s->ref  = NULL;
    return CF_TRUE;
}

cf_bool cf_source_secrets(cf_source* s, const char* name, int rank, const char* path,
                          const char* prefix, unsigned flags, void* state,
                          size_t state_size) {
    if (path == NULL) {
        return CF_FALSE;
    }
    return secrets_init(s, &cf_secrets_vt, (name != NULL) ? name : path, rank, path,
                        NULL, prefix, flags, state, state_size);
}

cf_bool cfi_source_credentials(cf_source* s, const char* name, int rank,
                               const char* prefix, unsigned flags, void* state,
                               size_t state_size) {
    return secrets_init(s, &cf_credentials_vt, (name != NULL) ? name : "credentials",
                        rank, NULL, CF_SECRETS_CREDENTIALS_VAR, prefix, flags, state,
                        state_size);
}

const cf_error* cf_source_secrets_error(const cf_source* s) {
    const secrets_state* st;

    if (s == NULL || s->ext == NULL) {
        return NULL;
    }
    if (s->vt != &cf_secrets_vt && s->vt != &cf_credentials_vt) {
        return NULL;
    }
    st = (const secrets_state*)s->ext;
    return st->failed ? &st->err : NULL;
}
