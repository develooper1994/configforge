/*
 * preferences.c - macOS's user defaults as a source. C89.
 *
 * A macOS program keeps its settings in the user defaults, a domain named
 * for the program ("com.example.app") that `defaults write` fills and
 * `defaults read` shows, and a dictionary inside it is a section:
 *
 *     defaults write com.example.app Server -dict Port -int 9443
 *     defaults write com.example.app Hosts -array web1 web2
 *
 * The values come from the platform walk as document events, and the
 * shared flattener turns them into keys, as it does for a JSON file:
 * Server = { Port = 9443; } is server.port, Hosts is hosts.0, hosts.1 and
 * hosts.count.
 *
 * What it decides:
 *
 *   scope     the current user's values, on any host: the ones `defaults
 *             read <domain>` shows. This host only, any user and a managed
 *             profile are other scopes and are not read (D-87); each of
 *             them would be a source of its own at its own rank, so the
 *             precedence between them stays the program's and explain()
 *             shows it.
 *   names     lowered: a defaults key keeps its case and a report key is
 *             lowercase. A name that is still not a key is an error naming
 *             the value, as a bad name in a file is.
 *   types     a string is text the contract types, as an INI value is:
 *             `defaults write` writes strings unless told -int. A number
 *             is an int or a real, a boolean a bool. A date and data
 *             have no meaning here and are refused by name.
 *   places    explain() shows "defaults <domain> <path>", the path with
 *             '/' between the names and an array's member by its index:
 *             "defaults com.example.app Server/Port". A place longer than
 *             CF_KEY_MAX is shown cut, as a file's place is.
 *   empty     a domain with no values is an error, and nothing with
 *             CF_OPTIONAL: reading nothing from a domain that is not
 *             there is not the program's mistake to hide.
 *
 * macOS only, under CF_WITH_PREFERENCES, through the platform layer:
 * elsewhere the source fails and says so, even when optional, because
 * reading nothing there would look like an empty domain.
 */

#include "configforge/cf_flatten.h"
#include "configforge/cf_keyspace.h"
#include "configforge/cf_platform.h"
#include "configforge/cf_source.h"

#include <string.h>

typedef struct prefs_state {
    const char* domain;
    unsigned    flags;
    cf_error    err;
    cf_bool     failed;
} prefs_state;

/* The walk's buffers live here, in the arena, and not on the stack of
   a callback the platform calls once per value: a flattener and two
   key buffers are far over the minimal build's frame. */
typedef struct prefs_walk {
    const cf_source* s;
    prefs_state*     st;
    cf_flatten       fl;
    char             name[CF_KEY_MAX];
    char             at[CF_PATH_MAX + CF_KEY_MAX];
} prefs_walk;

/* The member's name lowered into w->name; CF_FALSE with the error set
   when it does not fit or is not a key. */
static cf_bool prefs_name(prefs_walk* w, const cf_platform_pref_event* e) {
    size_t i;

    if (e->name == NULL) {
        return CF_TRUE;
    }
    if (e->namelen + 1 <= sizeof w->name) {
        for (i = 0; i < e->namelen; ++i) {
            w->name[i] = (char)cf_to_lower((unsigned char)e->name[i]);
        }
        w->name[e->namelen] = '\0';
        if (cf_key_valid(w->name, e->namelen)) {
            return CF_TRUE;
        }
    }
    cf_error_set(&w->st->err, CF_E_KEY, NULL, w->at,
                 "%s: '%s' is not a valid key; name it in lowercase letters, "
                 "digits, '_' and '-'",
                 w->at, e->name);
    return CF_FALSE;
}

static cf_bool prefs_event(void* ctx, const cf_platform_pref_event* e) {
    prefs_walk*        w    = (prefs_walk*)ctx;
    const cf_doc_sink* sink = cf_flatten_sink(&w->fl);
    const char*        dom  = w->st->domain;
    const char*        nm;
    size_t             n;
    cf_loc             loc;
    cf_value           val;
    cf_bool            textual = CF_FALSE;
    cf_bool            ok;

    if (e->kind == CF_PREF_END) {
        return sink->end(sink->ctx);
    }
    if (e->path[0] == '\0') {
        cf_fmt(w->at, sizeof w->at, "defaults %s", dom);
    } else {
        cf_fmt(w->at, sizeof w->at, "defaults %s %s", dom, e->path);
    }
    if (!prefs_name(w, e)) {
        return CF_FALSE;
    }
    nm       = (e->name != NULL) ? w->name : NULL;
    n        = (e->name != NULL) ? e->namelen : 0;
    loc.file = w->at;
    loc.line = 0;
    loc.col  = 0;

    switch (e->kind) {
    case CF_PREF_BEGIN_DICT:
        ok = sink->begin_table(sink->ctx, nm, n);
        break;
    case CF_PREF_BEGIN_ARRAY:
        ok = sink->begin_array(sink->ctx, nm, n);
        break;
    case CF_PREF_TEXT:
        cf_value_text(&val, e->text, e->textlen);
        textual = CF_TRUE;
        ok      = sink->scalar(sink->ctx, nm, n, &val, textual, &loc);
        break;
    case CF_PREF_INT:
        cf_value_int(&val, e->i);
        ok = sink->scalar(sink->ctx, nm, n, &val, textual, &loc);
        break;
    case CF_PREF_BOOL:
        cf_value_bool(&val, e->b);
        ok = sink->scalar(sink->ctx, nm, n, &val, textual, &loc);
        break;
    case CF_PREF_REAL:
#if CF_WITH_REAL
        cf_value_real(&val, e->r);
        ok = sink->scalar(sink->ctx, nm, n, &val, textual, &loc);
        break;
#else
        return cf_error_set(&w->st->err, CF_E_CONVERT, NULL, w->at,
                            "%s: this build has no floating point support", w->at);
#endif
    default:
        return cf_error_set(&w->st->err, CF_E_CONVERT, NULL, w->at,
                            "%s: only strings, numbers, booleans, arrays and "
                            "dictionaries are read; this value is %s",
                            w->at, (e->text != NULL) ? e->text : "another type");
    }
    if (!ok && w->st->err.code == CF_OK) {
        return cf_error_set(&w->st->err, CF_E_KEY, NULL, w->at,
                            "%s: a key this build cannot hold", w->at);
    }
    return ok;
}

static cf_bool prefs_load(const cf_source* s, cf_keyspace* ks) {
    prefs_state* st = (prefs_state*)s->ext;
    prefs_walk*  w  = (prefs_walk*)cf_arena_calloc(cf_ks_arena(ks), sizeof *w);
    int          rc;

    st->failed = CF_FALSE;
    cf_error_clear(&st->err);
    st->err.size = sizeof(cf_error);
    if (w == NULL || !cf_flatten_init(&w->fl, ks, s->rank)) {
        st->failed = CF_TRUE;
        return cf_error_set(&st->err, CF_E_MEMORY, NULL, st->domain,
                            "no room to read '%s'", st->domain);
    }
    w->s  = s;
    w->st = st;

    rc = cf_platform_preferences_walk(st->domain, cf_ks_arena(ks), prefs_event, w,
                                      &st->err);
    if (rc == 1) {
        /* An empty domain is not an error when the program said so; on
           any platform without user defaults the walk has already failed. */
        if ((st->flags & CF_OPTIONAL) != 0) {
            return CF_TRUE;
        }
        cf_error_set(&st->err, CF_E_SOURCE, NULL, st->domain,
                     "there are no user defaults for '%s'", st->domain);
        rc = -1;
    } else if (rc == 0 && (w->fl.failed || w->fl.overflow)) {
        cf_error_set(&st->err, CF_E_KEY, NULL, st->domain,
                     "'%s' holds a key this build cannot hold", st->domain);
        rc = -1;
    }
    st->failed = (rc != 0) ? CF_TRUE : CF_FALSE;
    return st->failed ? CF_FALSE : CF_TRUE;
}

static void prefs_destroy(cf_source* s) {
    (void)s;
}

static const cf_source_vtable cf_prefs_vt = {
    sizeof(cf_source_vtable), "preferences", prefs_load, prefs_destroy, NULL, NULL};

size_t cfi_source_preferences_state_size(void) {
    return sizeof(prefs_state);
}

cf_bool cfi_source_preferences(cf_source* s, const char* name, int rank,
                               const char* domain, unsigned flags, void* state,
                               size_t state_size) {
    prefs_state* st = (prefs_state*)state;

    if (s == NULL || domain == NULL || st == NULL || state_size < sizeof(prefs_state)) {
        return CF_FALSE;
    }
    memset(st, 0, sizeof *st);
    st->domain   = domain;
    st->flags    = flags;
    st->err.size = sizeof(cf_error);

    s->vt   = &cf_prefs_vt;
    s->name = (name != NULL) ? name : domain;
    s->rank = rank;
    s->ext  = st;
    s->ref  = NULL;
    return CF_TRUE;
}

const cf_error* cfi_source_preferences_error(const cf_source* s) {
    const prefs_state* st;

    if (s == NULL || s->vt != &cf_prefs_vt || s->ext == NULL) {
        return NULL;
    }
    st = (const prefs_state*)s->ext;
    return st->failed ? &st->err : NULL;
}
