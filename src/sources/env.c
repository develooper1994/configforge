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

static cf_bool cf_env_load(const cf_source* s, cf_keyspace* ks) {
    cf_listsink out;
    cf_keymap   map;
    cf_error    err = CF_ERROR_INIT;

    if (!cf_listsink_init(&out, ks, s->rank, (const cf_defset*)s->ref, CF_FALSE)) {
        return CF_FALSE;
    }
    cf_keymap_init(&map, CF_KEYMAP_ENV, s->u.env.prefix);
    if (cf_defset_count((const cf_defset*)s->ref) != 0) {
        /* Separate passes make escaped names win independently of
           environment enumeration order, without recording aliases as
           additional unknown keys. */
        out.natural_pass = CF_TRUE;
        if (!cf_lex_env(s->u.env.env, &map, cf_listsink_of(&out), &err)) {
            return CF_FALSE;
        }
        out.natural_pass = CF_FALSE;
    }
    return cf_lex_env(s->u.env.env, &map, cf_listsink_of(&out), &err) &&
           cf_listsink_finish(&out);
}

const char* cf_env_value(char* const* environment, const char* name, size_t* n) {
    size_t       namelen;
    char* const* e;

    if (environment == NULL || name == NULL || name[0] == '\0') {
        return NULL;
    }
    namelen = strlen(name);
    for (e = environment; *e != NULL; ++e) {
        if (strncmp(*e, name, namelen) == 0 && (*e)[namelen] == '=') {
            if (n != NULL) {
                *n = strlen(*e + namelen + 1);
            }
            return *e + namelen + 1;
        }
    }
    return NULL;
}

static void cf_env_destroy(cf_source* s) {
    (void)s;
}

static const cf_source_vtable cf_env_vt = {sizeof(cf_source_vtable), "env", cf_env_load,
                                           cf_env_destroy,           NULL,  NULL};

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
