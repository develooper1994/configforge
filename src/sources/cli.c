/*
 * cli.c - the command line as a source. C89.
 *
 * A wrapper over cf_lex_cli. Keys are written with dots: a command
 * line can carry one, so there is no reason to invent the ambiguous
 * escape the environment needs.
 */

#include "configforge/cf_entry.h"
#include "configforge/cf_source.h"

static cf_bool cf_cli_load(const cf_source* s, cf_keyspace* ks) {
    cf_keysink out;
    cf_keymap  map;
    cf_error   err = CF_ERROR_INIT;

    if (!cf_keysink_init(&out, ks, s->rank)) {
        return CF_FALSE;
    }
    cf_keymap_init(&map, CF_KEYMAP_DIRECT, NULL);
    return cf_lex_cli(s->u.cli.argc, s->u.cli.argv, &map, cf_keysink_of(&out), &err);
}

static void cf_cli_destroy(cf_source* s) {
    (void)s;
}

static const cf_source_vtable cf_cli_vt = {"cli", cf_cli_load, cf_cli_destroy};

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
