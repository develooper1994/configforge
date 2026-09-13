/*
 * test_define.c - the typed contract.
 */

#include "configforge/cf_define.h"
#include "configforge/cf_source.h"

#include <stdio.h>
#include <string.h>

static int  g_fail = 0, g_run = 0;
static void check(int c, const char* w) {
    ++g_run;
    if (!c) {
        ++g_fail;
        printf("FAIL: %s\n", w);
    }
}
static void has(const char* hay, const char* needle, const char* w) {
    ++g_run;
    if (hay == NULL || strstr(hay, needle) == NULL) {
        ++g_fail;
        printf("FAIL: %s  missing \"%s\" in \"%s\"\n", w, needle, hay ? hay : "(null)");
    }
}

/* ---------------------------------------------------------------- */

static void test_self_consistency(cf_arena* ar) {
    cf_defset                ds;
    cf_error                 err = CF_ERROR_INIT;
    cf_def                   d;
    static const char* const empty[] = {NULL};

    cf_defset_init(&ds, ar, 8);

    /* A definition that contradicts itself is a programming error, and
       the moment to refuse it is where it was written. */
    cf_def_init(&d, CF_TYPE_INT, "a.b");
    d.has_range = CF_TRUE;
    d.imin      = 100;
    d.imax      = 1;
    check(!cf_define(&ds, &d, &err) && err.code == CF_E_DEFINITION,
          "inverted range refused");
    has(err.message, "inverted", "inverted range explained");

    cf_def_init(&d, CF_TYPE_INT, "a.c");
    d.has_range   = CF_TRUE;
    d.imin        = 1;
    d.imax        = 10;
    d.has_default = CF_TRUE;
    cf_value_int(&d.default_value, 99);
    check(!cf_define(&ds, &d, &err), "default outside its own range refused");
    has(err.message, "outside its own range", "explained");

    cf_def_init(&d, CF_TYPE_INT, "a.d");
    d.flags       = CF_REQUIRED;
    d.has_default = CF_TRUE;
    cf_value_int(&d.default_value, 1);
    check(!cf_define(&ds, &d, &err), "required and defaulted refused");

    cf_def_init(&d, CF_TYPE_TEXT, "a.e");
    d.allowed = empty;
    check(!cf_define(&ds, &d, &err), "empty permitted list refused");

    cf_def_init(&d, CF_TYPE_BOOL, "a.f");
    d.has_range = CF_TRUE;
    check(!cf_define(&ds, &d, &err), "range on a bool refused");

    cf_def_init(&d, CF_TYPE_INT, "Server.Port");
    check(!cf_define(&ds, &d, &err) && err.code == CF_E_DEFINITION,
          "uppercase key refused");

    check(cf_def_int(&ds, "a.g", 5, 1, 10, 0, &err), "a sound definition passes");
    check(!cf_def_int(&ds, "a.g", 5, 1, 10, 0, &err), "duplicate refused");
    has(err.message, "twice", "duplicate explained");
}

static void test_lookup_and_nearest(cf_arena* ar) {
    cf_defset     ds;
    cf_error      err = CF_ERROR_INIT;
    const cf_def* d;
    size_t        dist = 0;

    cf_defset_init(&ds, ar, 4);
    check(cf_def_int(&ds, "server.port", 8080, 1, 65535, 0, &err), "def port");
    check(cf_def_text(&ds, "server.host", "0.0.0.0", 0, &err), "def host");
    check(cf_def_bool(&ds, "tls.enabled", CF_FALSE, 0, &err), "def tls");
    cf_defset_seal(&ds);

    d = cf_defset_find(&ds, "server.port", 11);
    check(d != NULL && d->type == CF_TYPE_INT, "find by binary search");
    check(cf_defset_find(&ds, "nope", 4) == NULL, "absent is NULL");

    /* Typo detection is available only because the definition set is a
       contract rather than documentation. */
    d = cf_defset_nearest(&ds, "server.pot", 10, &dist);
    check(d != NULL && strcmp(d->key, "server.port") == 0, "one deletion found");
    check(dist == 1, "distance reported");

    d = cf_defset_nearest(&ds, "server.prot", 11, &dist);
    check(d != NULL && strcmp(d->key, "server.port") == 0, "transposition found");

    /* Something four edits away is noise, not a suggestion. */
    check(cf_defset_nearest(&ds, "completely.other", 16, &dist) == NULL,
          "distant key suggests nothing");
}

/* ---------------------------------------------------------------- */

/* Defaults are a source like any other, so the tests install it the
   same way the facade does. */
static void seed_defaults(cf_keyspace* ks, const cf_defset* ds) {
    cf_source d;
    cf_source_defaults(&d, "default", CF_RANK_DEFAULT, ds);
    cf_source_load(&d, ks);
}

static void load(cf_keyspace* ks, cf_arena* ar, char* const* env, int argc,
                 char* const* argv) {
    cf_source e, c;
    cf_ks_init(ks, ar, 16);
    if (argv != NULL) {
        cf_source_cli(&c, "cli", CF_RANK_CLI, argc, argv);
        cf_source_load(&c, ks);
    }
    if (env != NULL) {
        cf_source_env(&e, "env", CF_RANK_ENV, "MYAPP_", env);
        cf_source_load(&e, ks);
    }
}

static void test_definition_decides_the_type(cf_arena* ar) {
    static char     e0[]  = "MYAPP_SERVER__PORT=8080";
    static char*    env[] = {e0, NULL};
    cf_defset       ds;
    cf_keyspace     ks;
    cf_error        err = CF_ERROR_INIT;
    cf_apply_report rep;
    const cf_value* v;

    cf_defset_init(&ds, ar, 4);
    /* Declared text, and the value looks like a number. Inference
       would have made it an int; the declaration must win. */
    cf_def_text(&ds, "server.port", NULL, 0, &err);
    cf_defset_seal(&ds);

    load(&ks, ar, env, 0, NULL);
    seed_defaults(&ks, &ds);
    cf_ks_resolve(&ks);
    check(cf_defs_apply(&ks, &ds, NULL, CF_UNKNOWN_IGNORE, &rep, &err),
          "apply succeeds");

    v = cf_ks_get(&ks, "server.port", 11);
    check(v != NULL && v->type == CF_TYPE_TEXT, "declaration beats inference");
    check(rep.converted == 1, "one conversion counted");
}

static void test_range_rejection_is_strict(cf_arena* ar) {
    static char  e0[]  = "MYAPP_SERVER__PORT=99999";
    static char* env[] = {e0, NULL};
    cf_defset    ds;
    cf_keyspace  ks;
    cf_error     err = CF_ERROR_INIT;

    cf_defset_init(&ds, ar, 4);
    cf_def_int(&ds, "server.port", 8080, 1, 65535, 0, &err);
    cf_defset_seal(&ds);

    load(&ks, ar, env, 0, NULL);
    seed_defaults(&ks, &ds);
    cf_ks_resolve(&ks);

    /* Strict is the default on purpose: a malformed production value
       must not quietly degrade to a development default. */
    check(!cf_defs_apply(&ks, &ds, NULL, CF_UNKNOWN_IGNORE, NULL, &err),
          "out-of-range value fails construction");
    check(err.code == CF_E_VALIDATE, "reported as a validation error");
    has(err.message, "99999", "names the offending value");
    has(err.message, "65535", "names the bound");
    has(err.source, "MYAPP_SERVER__PORT", "names where to fix it");
}

static void test_fallback_is_opt_in(cf_arena* ar) {
    static char     e0[]  = "MYAPP_SERVER__PORT=99999";
    static char*    env[] = {e0, NULL};
    cf_defset       ds;
    cf_keyspace     ks;
    cf_error        err = CF_ERROR_INIT;
    cf_def          d;
    const cf_value* v;
    const cf_entry* ent;

    cf_defset_init(&ds, ar, 4);
    cf_def_init(&d, CF_TYPE_INT, "server.port");
    d.has_range   = CF_TRUE;
    d.imin        = 1;
    d.imax        = 65535;
    d.has_default = CF_TRUE;
    cf_value_int(&d.default_value, 8080);
    d.on_invalid = CF_INVALID_FALLBACK;
    check(cf_define(&ds, &d, &err), "fallback definition accepted");
    cf_defset_seal(&ds);

    load(&ks, ar, env, 0, NULL);
    seed_defaults(&ks, &ds);
    cf_ks_resolve(&ks);
    check(cf_defs_apply(&ks, &ds, NULL, CF_UNKNOWN_IGNORE, NULL, &err),
          "fallback survives construction");

    v = cf_ks_get(&ks, "server.port", 11);
    check(v != NULL && v->u.i == 8080, "falls back to the default");

    /* The refusal is never erased: explain() has to be able to say the
       environment did supply a value and why it lost. */
    ent = cf_ks_find(&ks, "server.port", 11);
    check(ent->chain->outcome == CF_OUT_REJECTED, "refusal retained");
    has(ent->chain->reason, "outside", "refusal carries its reason");
    check(ent->chain->at != NULL, "refusal keeps its location");
}

static void test_required_and_unknown(cf_arena* ar) {
    cf_defset    ds;
    cf_keyspace  ks;
    cf_error     err   = CF_ERROR_INIT;
    static char  e0[]  = "MYAPP_SERVER__POT=1";
    static char* env[] = {e0, NULL};

    cf_defset_init(&ds, ar, 4);
    cf_def_text(&ds, "db.password", NULL, CF_REQUIRED | CF_SENSITIVE, &err);
    cf_def_int(&ds, "server.port", 8080, 0, 0, 0, &err);
    cf_defset_seal(&ds);

    load(&ks, ar, env, 0, NULL);
    seed_defaults(&ks, &ds);
    cf_ks_resolve(&ks);

    check(!cf_defs_apply(&ks, &ds, NULL, CF_UNKNOWN_IGNORE, NULL, &err),
          "missing required parameter fails");
    check(err.code == CF_E_REQUIRED, "reported as required");
    has(err.message, "db.password", "names the parameter");

    /* The same misspelling, with unknown keys refused. */
    {
        cf_defset   ds2;
        cf_keyspace ks2;
        cf_error    e2 = CF_ERROR_INIT;

        cf_defset_init(&ds2, ar, 4);
        cf_def_int(&ds2, "server.port", 8080, 0, 0, 0, &e2);
        cf_defset_seal(&ds2);

        load(&ks2, ar, env, 0, NULL);
        seed_defaults(&ks2, &ds2);
        cf_ks_resolve(&ks2);

        check(cf_defs_apply(&ks2, &ds2, NULL, CF_UNKNOWN_IGNORE, NULL, &e2),
              "unknown keys ignored by default");
        check(!cf_defs_apply(&ks2, &ds2, NULL, CF_UNKNOWN_REJECT, NULL, &e2),
              "unknown keys refused when asked");
        has(e2.message, "did you mean", "a typo is reported as a suggestion");
        has(e2.message, "server.port", "the suggestion is the right key");
    }
}

static void test_typed_source_mismatch(cf_arena* ar) {
    cf_defset   ds;
    cf_keyspace ks;
    cf_error    err = CF_ERROR_INIT;
    cf_value    v;

    cf_defset_init(&ds, ar, 4);
    cf_def_int(&ds, "a.b", 1, 0, 0, 0, &err);
    cf_defset_seal(&ds);

    cf_ks_init(&ks, ar, 4);
    /* A typed source said text where int was declared. Coercing would
       undo the only reason to have a typed source. */
    cf_value_text(&v, "7", 1);
    cf_ks_add(&ks, "a.b", 3, CF_RANK_ENV, &v, "doc.json:3:5");
    cf_ks_resolve(&ks);

    check(!cf_defs_apply(&ks, &ds, NULL, CF_UNKNOWN_IGNORE, NULL, &err),
          "typed mismatch is not coerced");
    has(err.message, "text where int", "mismatch explained");
}

static void test_allowed_values(cf_arena* ar) {
    static const char* const levels[] = {"debug", "info", "warn", NULL};
    static char              e0[]     = "MYAPP_LOG__LEVEL=verbose";
    static char*             env[]    = {e0, NULL};
    cf_defset                ds;
    cf_keyspace              ks;
    cf_error                 err = CF_ERROR_INIT;
    cf_def                   d;

    cf_defset_init(&ds, ar, 4);
    cf_def_init(&d, CF_TYPE_TEXT, "log.level");
    d.allowed     = levels;
    d.has_default = CF_TRUE;
    cf_value_text(&d.default_value, "info", 4);
    check(cf_define(&ds, &d, &err), "enum definition accepted");
    cf_defset_seal(&ds);

    load(&ks, ar, env, 0, NULL);
    seed_defaults(&ks, &ds);
    cf_ks_resolve(&ks);
    check(!cf_defs_apply(&ks, &ds, NULL, CF_UNKNOWN_IGNORE, NULL, &err),
          "value outside the permitted set refused");
    /* Naming what was permitted matters more than naming what was
       given: the reader already knows what they wrote. */
    has(err.message, "debug", "lists what is permitted");
    has(err.message, "warn", "lists all of it");
}

static void test_defaults_are_ordinary_candidates(cf_arena* ar) {
    static char     a0[]   = "myapp";
    static char     a1[]   = "--server.port=7000";
    static char*    argv[] = {a0, a1, NULL};
    cf_defset       ds;
    cf_keyspace     ks;
    cf_error        err = CF_ERROR_INIT;
    cf_apply_report rep;
    const cf_entry* e;
    const cf_value* v;

    cf_defset_init(&ds, ar, 4);
    cf_def_int(&ds, "server.port", 8080, 1, 65535, 0, &err);
    cf_def_text(&ds, "server.host", "0.0.0.0", 0, &err);
    cf_defset_seal(&ds);

    load(&ks, ar, NULL, 2, argv);
    seed_defaults(&ks, &ds);
    cf_ks_resolve(&ks);
    check(cf_defs_apply(&ks, &ds, NULL, CF_UNKNOWN_IGNORE, &rep, &err), "apply ok");

    v = cf_ks_get(&ks, "server.port", 11);
    check(v != NULL && v->u.i == 7000, "command line beats the default");

    /* A default is a value from the lowest-precedence source, not a
       separate mechanism, which is why it shows up in the chain. */
    e = cf_ks_find(&ks, "server.port", 11);
    check(e->chain != NULL && e->chain->next != NULL, "default is in the chain");
    check(e->chain->next->outcome == CF_OUT_SHADOWED, "and is marked shadowed");
    check(e->chain->next->value.u.i == 8080, "with its own value intact");
    check(rep.defaults_used == 1, "one default actually used");
}

int main(void) {
    cf_arena* ar = cf_arena_new(NULL, 0);

    test_self_consistency(ar);
    test_lookup_and_nearest(ar);
    test_definition_decides_the_type(ar);
    test_range_rejection_is_strict(ar);
    test_fallback_is_opt_in(ar);
    test_required_and_unknown(ar);
    test_typed_source_mismatch(ar);
    test_allowed_values(ar);
    test_defaults_are_ordinary_candidates(ar);

    cf_arena_free(ar);
    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
