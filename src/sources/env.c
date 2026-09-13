/*
 * env.c - the environment as a source. C89.
 *
 * A wrapper over cf_lex_env, which is where the work is. The lexer
 * needs no arena, no key space and no source object, so it can be
 * tested against an array; this file only decides where the entries
 * go.
 */

#include "configforge/cf_entry.h"
#include "configforge/cf_source.h"
#include "configforge/cf_define.h"
#include "configforge/cf_platform.h"

#include <string.h>

/* The escaped spelling is what enumeration needs; nobody wants to type
   it. Once a definition set exists the natural spelling can be
   accepted too, because the key list makes it unambiguous - which is
   the gap D-06 recorded and deferred.
   Run after the enumeration pass, so if both spellings are set the
   escaped one wins on rank order and the natural one shows up shadowed
   rather than silently disappearing. Setting the same parameter twice
   two ways is worth seeing. */
static cf_bool env_natural_pass(const cf_source* s, cf_keysink* out) {
    const cf_defset* ds = (const cf_defset*)s->ref;
    cf_keymap        map;
    size_t           i;

    if (ds == NULL || s->u.env.env == NULL) {
        return CF_TRUE;
    }
    cf_keymap_init(&map, CF_KEYMAP_ENV, s->u.env.prefix);

    for (i = 0; i < cf_defset_count(ds); ++i) {
        const cf_def* d = cf_defset_at(ds, i);
        char          natural[CF_KEY_MAX];
        char          escaped[CF_KEY_MAX];
        size_t        klen, nlen, elen;
        char* const*  e;

        if (d == NULL || d->key == NULL) {
            continue;
        }
        klen = strlen(d->key);
        nlen =
            cf_keymap_from_key(&map, d->key, klen, CF_FALSE, natural, sizeof natural);
        elen = cf_keymap_from_key(&map, d->key, klen, CF_TRUE, escaped, sizeof escaped);
        /* When the key has no dot the two spellings are identical and
           the enumeration pass already saw it. */
        if (nlen == 0 || nlen == elen) {
            continue;
        }
        for (e = s->u.env.env; *e != NULL; ++e) {
            const char* line = *e;
            if (strncmp(line, natural, nlen) != 0 || line[nlen] != '=') {
                continue;
            }
            if (!out->sink.text(out->sink.ctx, d->key, klen, line + nlen + 1,
                                strlen(line + nlen + 1), natural)) {
                return CF_FALSE;
            }
            break;
        }
    }
    return CF_TRUE;
}

static cf_bool cf_env_load(const cf_source* s, cf_keyspace* ks) {
    cf_keysink out;
    cf_keymap  map;
    cf_error   err = CF_ERROR_INIT;

    if (!cf_keysink_init(&out, ks, s->rank)) {
        return CF_FALSE;
    }
    cf_keymap_init(&map, CF_KEYMAP_ENV, s->u.env.prefix);
    if (!env_natural_pass(s, &out)) {
        return CF_FALSE;
    }
    return cf_lex_env(s->u.env.env, &map, cf_keysink_of(&out), &err);
}

static void cf_env_destroy(cf_source* s) {
    (void)s;
}

static const cf_source_vtable cf_env_vt = {"env", cf_env_load, cf_env_destroy};

cf_bool cf_source_env(cf_source* s, const char* name, int rank, const char* prefix,
                      char* const* environment) {
    if (s == NULL) {
        return CF_FALSE;
    }
    s->vt           = &cf_env_vt;
    s->name         = (name != NULL) ? name : "env";
    s->rank         = rank;
    s->ext          = NULL;
    s->ref          = NULL;
    s->u.env.prefix = prefix;
    s->u.env.env    = environment;
    return CF_TRUE;
}

void cf_source_env_defs(cf_source* s, const cf_defset* ds) {
    if (s != NULL) {
        s->ref = ds;
    }
}

/* Kept for callers that want the derivation without a source. */
size_t cf_env_key(const char* prefix, const char* var, size_t varlen, char* out,
                  size_t cap) {
    cf_keymap map;
    cf_keymap_init(&map, CF_KEYMAP_ENV, prefix);
    return cf_keymap_to_key(&map, var, varlen, out, cap);
}
