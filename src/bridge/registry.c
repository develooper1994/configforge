/*
 * registry.c - a key of the Windows Registry as a source. C89.
 *
 * Windows programs keep settings under HKEY_CURRENT_USER\Software\<App>
 * and HKEY_LOCAL_MACHINE\Software\<App>, an installer or Group Policy
 * writing them, and a subkey is a section:
 *
 *     HKCU\Software\MyApp\Server   Port  REG_DWORD  9443   server.port = 9443
 *     HKCU\Software\MyApp\Server   Host  REG_SZ     web1   server.host = "web1"
 *     HKCU\Software\MyApp          Hosts REG_MULTI_SZ a b  hosts.0, hosts.1,
 * hosts.count
 *
 * What it decides:
 *
 *   names     lowered, subkeys joined with '.': the Registry's names are
 *             case-insensitive and keep their case, and a report key is
 *             lowercase. A name that is still not a key is an error,
 *             naming the value, as a bad name in a file is.
 *   types     REG_DWORD and REG_QWORD are integers, typed as JSON's are.
 *             REG_SZ is text the contract types, as an INI value is:
 *             installers write numbers as strings more often than not.
 *             REG_EXPAND_SZ is not expanded - %VAR% in a value is the
 *             text the installer wrote, and expanding it would be a
 *             second environment source no explain() could name.
 *             REG_MULTI_SZ is a list, as a JSON array flattens. Anything
 *             else - REG_BINARY - is refused by name.
 *   unnamed   a key's unnamed ("(Default)") value has no name to be a
 *             key, so a set one is refused; regedit's unset default is
 *             not a value and is not seen.
 *   places    explain() shows the value's full path, hive first.
 *
 * Windows only, through the platform layer: elsewhere the source fails
 * and says so, even when optional, because reading nothing there would
 * look like an empty key.
 */

#include "configforge/cf_keyspace.h"
#include "configforge/cf_platform.h"
#include "configforge/cf_source.h"

#include <string.h>

typedef struct registry_state {
    const char* root;
    unsigned    flags;
    cf_error    err;
    cf_bool     failed;
} registry_state;

/* The walk's buffers live here, in the arena, and not on the stack of
   a callback the platform calls once per value: three of them are more
   than the minimal build's 1 KB frame. */
typedef struct registry_walk {
    const cf_source* s;
    registry_state*  st;
    cf_keyspace*     ks;
    char             key[CF_KEY_MAX];
    char             item[CF_KEY_MAX];
    char             at[CF_PATH_MAX + 64];
} registry_walk;

/* "Server" and "Port" as server.port; "" and "Port" as port. */
static size_t registry_key(const char* path, const char* name, char* out, size_t cap) {
    size_t k = 0;
    size_t i;

    for (i = 0; path[i] != '\0'; ++i) {
        if (k + 1 >= cap) {
            return 0;
        }
        out[k++] =
            (char)((path[i] == '\\') ? '.' : cf_to_lower((unsigned char)path[i]));
    }
    if (k > 0) {
        if (k + 1 >= cap) {
            return 0;
        }
        out[k++] = '.';
    }
    for (i = 0; name[i] != '\0'; ++i) {
        if (k + 1 >= cap) {
            return 0;
        }
        out[k++] = (char)cf_to_lower((unsigned char)name[i]);
    }
    out[k] = '\0';
    return k;
}

static cf_bool registry_value(void* ctx, const cf_platform_reg_value* v) {
    registry_walk* w    = (registry_walk*)ctx;
    const char*    root = w->st->root;
    char*          key  = w->key;
    char*          at   = w->at;
    size_t         n;
    cf_value       val;

    if (v->name[0] == '\0') {
        if (v->kind == CF_REG_TEXT && v->textlen == 0) {
            return CF_TRUE;
        }
        return cf_error_set(&w->st->err, CF_E_KEY, NULL, root,
                            "%s%s%s has an unnamed value, which no key can carry; "
                            "give it a name",
                            root, (v->path[0] != '\0') ? "\\" : "", v->path);
    }
    cf_fmt(at, sizeof w->at, "%s%s%s\\%s", root, (v->path[0] != '\0') ? "\\" : "",
           v->path, v->name);
    n = registry_key(v->path, v->name, key, sizeof w->key);
    if (n == 0 || !cf_key_valid(key, n)) {
        return cf_error_set(
            &w->st->err, CF_E_KEY, NULL, at,
            "%s: '%s' is not a valid key; name it in lowercase letters, "
            "digits, '_' and '-'",
            at, (n > 0) ? key : v->name);
    }
    switch (v->kind) {
    case CF_REG_TEXT:
        if (!cf_ks_add_text(w->ks, key, n, w->s->rank, v->text, v->textlen, at)) {
            break;
        }
        return CF_TRUE;
    case CF_REG_INT:
        cf_value_int(&val, v->i);
        if (!cf_ks_add(w->ks, key, n, w->s->rank, &val, at)) {
            break;
        }
        return CF_TRUE;
    case CF_REG_LIST: {
        /* key.0 .. key.(n-1) and key.count, as a JSON array flattens. */
        const char* p    = v->text;
        char*       item = w->item;
        size_t      i;

        for (i = 0; i < v->items; ++i) {
            size_t len = strlen(p);
            size_t in  = cf_fmt(item, sizeof w->item, "%s.%u", key, (unsigned)i);
            if (in + 1 >= sizeof w->item ||
                !cf_ks_add_text(w->ks, item, in, w->s->rank, p, len, at)) {
                return cf_error_set(&w->st->err, CF_E_MEMORY, key, at,
                                    "no room to read %s", at);
            }
            p += len + 1;
        }
        n = cf_fmt(item, sizeof w->item, "%s.count", key);
        cf_value_int(&val, (cf_i64)v->items);
        if (n + 1 >= sizeof w->item ||
            !cf_ks_add(w->ks, item, n, w->s->rank, &val, at)) {
            break;
        }
        return CF_TRUE;
    }
    default:
        return cf_error_set(&w->st->err, CF_E_CONVERT, key, at,
                            "%s: only strings, numbers and string lists are read; this "
                            "value is another Registry type",
                            at);
    }
    return cf_error_set(&w->st->err, CF_E_MEMORY, key, at, "no room to read %s", at);
}

static cf_bool registry_load(const cf_source* s, cf_keyspace* ks) {
    registry_state* st = (registry_state*)s->ext;
    registry_walk*  w  = (registry_walk*)cf_arena_calloc(cf_ks_arena(ks), sizeof *w);
    int             rc;

    st->failed = CF_FALSE;
    cf_error_clear(&st->err);
    st->err.size = sizeof(cf_error);
    if (w == NULL) {
        st->failed = CF_TRUE;
        return cf_error_set(&st->err, CF_E_MEMORY, NULL, st->root,
                            "no room to read '%s'", st->root);
    }
    w->s  = s;
    w->st = st;
    w->ks = ks;

    rc = cf_platform_registry_walk(st->root, cf_ks_arena(ks), registry_value, w,
                                   &st->err);
    if (rc == 1) {
        /* Missing is not an error when the program said so; on any
           platform without a Registry the walk has already failed. */
        if ((st->flags & CF_FILE_OPTIONAL) != 0) {
            return CF_TRUE;
        }
        cf_error_set(&st->err, CF_E_SOURCE, NULL, st->root,
                     "there is no Registry key '%s'", st->root);
        rc = -1;
    }
    st->failed = (rc != 0) ? CF_TRUE : CF_FALSE;
    return st->failed ? CF_FALSE : CF_TRUE;
}

static void registry_destroy(cf_source* s) {
    (void)s;
}

static const cf_source_vtable cf_registry_vt = {"registry", registry_load,
                                                registry_destroy, NULL, NULL};

size_t cf_source_registry_state_size(void) {
    return sizeof(registry_state);
}

cf_bool cf_source_registry(cf_source* s, const char* name, int rank, const char* root,
                           unsigned flags, void* state, size_t state_size) {
    registry_state* st = (registry_state*)state;

    if (s == NULL || root == NULL || st == NULL ||
        state_size < sizeof(registry_state)) {
        return CF_FALSE;
    }
    memset(st, 0, sizeof *st);
    st->root     = root;
    st->flags    = flags;
    st->err.size = sizeof(cf_error);

    s->vt   = &cf_registry_vt;
    s->name = (name != NULL) ? name : root;
    s->rank = rank;
    s->ext  = st;
    s->ref  = NULL;
    return CF_TRUE;
}

const cf_error* cf_source_registry_error(const cf_source* s) {
    const registry_state* st;

    if (s == NULL || s->vt != &cf_registry_vt || s->ext == NULL) {
        return NULL;
    }
    st = (const registry_state*)s->ext;
    return st->failed ? &st->err : NULL;
}
