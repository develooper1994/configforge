/*
 * cli.c - the command line as a source. C89.
 *
 * A wrapper over cf_lex_cli. Keys are written with dots: a command
 * line can carry one, so there is no reason to invent the ambiguous
 * escape the environment needs.
 */

#include "configforge/cf_entry.h"
#include "configforge/cf_source.h"
#include "configforge/cf_define.h"

#include <string.h>

/* The spec is built from the definition set when there is one. Without
   it the lexer gets NULL and falls back to the two forms that need no
   knowledge - the same shape as the environment's two spellings. */
static cf_cli_kind cli_kind_of(const void* ctx, const char* key, size_t klen) {
    const cf_defset* ds = (const cf_defset*)ctx;
    const cf_def*    d  = cf_defset_find(ds, key, klen);

    if (d == NULL) {
        return CF_CLI_UNKNOWN;
    }
    return (d->type == CF_TYPE_BOOL) ? CF_CLI_FLAG : CF_CLI_VALUE;
}

static size_t cli_alias_of(const void* ctx, const char* raw, size_t rawlen, char* out,
                           size_t cap) {
    const cf_defset* ds = (const cf_defset*)ctx;
    size_t           i, j;

    for (i = 0; i < cf_defset_count(ds); ++i) {
        const cf_def* d = cf_defset_at(ds, i);
        if (d == NULL || d->aliases == NULL) {
            continue;
        }
        for (j = 0; d->aliases[j] != NULL; ++j) {
            size_t n = strlen(d->aliases[j]);
            if (n != rawlen || memcmp(d->aliases[j], raw, n) != 0) {
                continue;
            }
            n = strlen(d->key);
            if (n + 1 > cap) {
                return 0;
            }
            memcpy(out, d->key, n + 1);
            return n;
        }
    }
    return 0;
}

static cf_bool cf_cli_load(const cf_source* s, cf_keyspace* ks) {
    const cf_defset*   ds = (const cf_defset*)s->ref;
    cf_keysink         out;
    cf_keymap          map;
    cf_cli_spec        built;
    const cf_cli_spec* spec = NULL;
    cf_error           err  = CF_ERROR_INIT;

    if (ds != NULL) {
        built.classify = cli_kind_of;
        built.alias    = cli_alias_of;
        built.ctx      = ds;
        spec           = &built;
    }

    if (!cf_keysink_init(&out, ks, s->rank)) {
        return CF_FALSE;
    }
    cf_keymap_init(&map, CF_KEYMAP_DIRECT, NULL);
    return cf_lex_cli(s->u.cli.argc, s->u.cli.argv, &map, spec, cf_keysink_of(&out),
                      &err);
}

static void cf_cli_destroy(cf_source* s) {
    (void)s;
}

static const cf_source_vtable cf_cli_vt = {sizeof(cf_source_vtable), "cli", cf_cli_load,
                                           cf_cli_destroy,           NULL,  NULL};

cf_bool cf_source_cli(cf_source* s, const char* name, int rank, int argc,
                      char* const* argv) {
    if (s == NULL) {
        return CF_FALSE;
    }
    s->vt         = &cf_cli_vt;
    s->name       = (name != NULL) ? name : "cli";
    s->rank       = rank;
    s->ext        = NULL;
    s->ref        = NULL;
    s->u.cli.argc = argc;
    s->u.cli.argv = argv;
    return CF_TRUE;
}

void cf_source_cli_defs(cf_source* s, const cf_defset* ds) {
    if (s != NULL) {
        s->ref = ds;
    }
}
