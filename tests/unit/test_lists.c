/*
 * test_lists.c - declared scalar lists and their flat-source forms.
 */

#include "configforge/cf_define.h"
#include "configforge/cf_report.h"
#include "configforge/cf_source.h"
#include "configforge/configforge.h"

#include <stdio.h>
#include <limits.h>
#include <string.h>

static int g_run = 0, g_fail = 0;

static void check(int condition, const char* message) {
    ++g_run;
    if (!condition) {
        ++g_fail;
        printf("FAIL: %s\n", message);
    }
}

static void text_is(const cf_keyspace* ks, const char* key, const char* text) {
    const cf_value* v = cf_ks_get(ks, key, strlen(key));
    check(v != NULL && v->type == CF_TYPE_TEXT && v->u.t.n == strlen(text) &&
              memcmp(v->u.t.p, text, v->u.t.n) == 0,
          key);
}

static void int_is(const cf_keyspace* ks, const char* key, cf_i64 value) {
    const cf_value* v = cf_ks_get(ks, key, strlen(key));
    check(v != NULL && v->type == CF_TYPE_INT && v->u.i == value, key);
}

static void defaults(cf_keyspace* ks, const cf_defset* ds) {
    cf_source s;
    check(cf_source_defaults(&s, "default", CF_RANK_DEFAULT, ds), "default source");
    check(cf_source_load(&s, ks), "load list defaults");
}

static void test_contract(cf_arena* ar) {
    cf_defset     ds;
    cf_def        d;
    cf_value      values[2];
    cf_error      err = CF_ERROR_INIT;
    const cf_def* item;
    char          first[] = "red";
    char          name[32];
    size_t        i;
    check(cf_defset_init(&ds, ar, 8), "definition set");
    cf_value_text(&values[0], first, 3);
    cf_value_text(&values[1], "blue", 4);
    cf_def_init(&d, CF_TYPE_TEXT, "tags");
    d.list_mode          = CF_LIST_REPLACE;
    d.list_separator     = ',';
    d.has_default        = CF_TRUE;
    d.list_defaults      = values;
    d.list_default_count = 2;
    d.flags              = CF_SENSITIVE;
    check(cf_define(&ds, &d, &err), "define a defaulted text list");
    first[0] = 'X';
    cf_value_null(&values[1]);
    item = cf_defset_find(&ds, "tags.123", 8);
    check(item != NULL && item->type == CF_TYPE_TEXT &&
              item->list_mode == CF_LIST_NONE && !item->has_default &&
              (item->flags & CF_SENSITIVE) != 0,
          "immutable item projection");
    for (i = 0; i < 12; ++i) {
        cf_fmt(name, sizeof name, "value%zu", i);
        check(cf_def_int(&ds, name, 1, 0, 10, 0, &err), "grow the authored set");
    }
    cf_defset_seal(&ds);
    check(item == cf_defset_find(&ds, "tags.456", 8),
          "projection survives growth and sorting without a mutable cache");
    check(cf_defset_count(&ds) == 13, "enumeration contains authored definitions only");
    d = *cf_defset_find(&ds, "tags", 4);
    check(d.list_default_count == 2 && d.list_defaults[1].type == CF_TYPE_TEXT &&
              memcmp(d.list_defaults[0].u.t.p, "red", 3) == 0,
          "default elements and text are copied");
    item = cf_defset_find(&ds, "tags.count", 10);
    check(item != NULL && item->type == CF_TYPE_INT && item->has_default &&
              item->default_value.u.i == 2 && item->imin == 0,
          "count has its own integer contract and default");
    check(cf_defset_find(&ds, "tags.01", 7) == NULL, "noncanonical index refused");
    check(cf_defset_find(&ds, "tags.9223372036854775808", 24) == NULL,
          "overflowing index refused");
    check(cf_defset_find(&ds, "tags.1.name", 11) == NULL, "nested member refused");
    check(cf_defset_list(&ds, "tags.1.name", 11) != NULL,
          "invalid member still belongs to the declared list");
    check(cf_defset_list(&ds, "tagset.0", 8) == NULL, "list prefix is segment bounded");
}

static void test_descriptor_refusals(cf_arena* ar) {
    cf_defset ds;
    cf_def    d;
    cf_value  v;
    cf_error  err = CF_ERROR_INIT;
    check(cf_defset_init(&ds, ar, 8), "refusal definition set");
    cf_def_init(&d, CF_TYPE_INT, "numbers");
    d.list_separator = ',';
    check(!cf_define(&ds, &d, &err), "separator without list mode refused");
    d.list_mode = (cf_list_mode)99;
    check(!cf_define(&ds, &d, &err), "unknown list mode refused");
    d.list_mode          = CF_LIST_REPLACE;
    d.has_default        = CF_TRUE;
    d.list_default_count = 1;
    check(!cf_define(&ds, &d, &err), "missing default array refused");
    cf_value_text(&v, "1", 1);
    d.list_defaults = &v;
    check(!cf_define(&ds, &d, &err), "typed list default is not coerced");
    cf_value_int(&v, 20);
    d.has_range = CF_TRUE;
    d.imin      = 0;
    d.imax      = 10;
    check(!cf_define(&ds, &d, &err), "default item outside its range refused");
    d.list_default_count = (size_t)-1;
    check(!cf_define(&ds, &d, &err),
          "overflow refused before reading the default array");
    d.list_default_count = 0;
    d.list_defaults      = NULL;
    cf_value_int(&d.default_value, 2);
    check(!cf_define(&ds, &d, &err),
          "a scalar default cannot silently become an empty list");
    cf_value_null(&d.default_value);
    check(cf_define(&ds, &d, &err), "explicitly empty default accepted");
    cf_def_init(&d, CF_TYPE_INT, "numbers.0");
    check(!cf_define(&ds, &d, &err), "list owns its indexed definitions");
    cf_def_init(&d, CF_TYPE_INT, "other.count");
    check(cf_define(&ds, &d, &err), "independent scalar count");
    cf_def_init(&d, CF_TYPE_TEXT, "other");
    d.list_mode = CF_LIST_REPLACE;
    check(!cf_define(&ds, &d, &err), "list cannot take over an existing member");

    cf_def_init(&d, CF_TYPE_INT, "old");
    d.has_default = CF_TRUE;
    cf_value_int(&d.default_value, 7);
    memset((char*)&d + offsetof(cf_def, list_mode), 0xa5,
           sizeof d - offsetof(cf_def, list_mode));
    d.size = offsetof(cf_def, list_mode) + sizeof(cf_list_mode);
    check(cf_define(&ds, &d, &err),
          "old tail padding is not a supplied list extension");
    check(cf_defset_find(&ds, "old", 3)->list_mode == CF_LIST_NONE,
          "incomplete extension normalized to scalar");
}

static void test_flat_inputs(cf_arena* ar) {
    static char              e0[]  = "APP_TAGS=first,, last,";
    static char*             env[] = {e0, NULL};
    static char              a0[] = "app", a1[] = "--tags=x,y", a2[] = "-t", a3[] = "";
    static char              a4[] = "--tags", a5[] = "z";
    static char              a6[] = "--switches=false", a7[] = "--switches=true";
    static char*             argv[]    = {a0, a1, a2, a3, a4, a5, a6, a7};
    static const char* const aliases[] = {"t", NULL};
    cf_defset                ds;
    cf_def                   d;
    cf_keyspace              ks;
    cf_source                s;
    cf_value                 values[2];
    const cf_value*          v;
    const cf_entry*          e;
    const cf_candidate*      c;
    cf_error                 err        = CF_ERROR_INIT;
    size_t                   cli_counts = 0;

    check(cf_defset_init(&ds, ar, 8), "flat input definitions");
    cf_def_init(&d, CF_TYPE_TEXT, "tags");
    d.list_mode      = CF_LIST_REPLACE;
    d.list_separator = ',';
    d.aliases        = aliases;
    cf_value_text(&values[0], "default-a", 9);
    cf_value_text(&values[1], "default-b", 9);
    d.has_default        = CF_TRUE;
    d.list_defaults      = values;
    d.list_default_count = 2;
    check(cf_define(&ds, &d, &err), "declare repeated text option");
    cf_def_init(&d, CF_TYPE_BOOL, "switches");
    d.list_mode = CF_LIST_REPLACE;
    check(cf_define(&ds, &d, &err), "declare boolean list");
    cf_defset_seal(&ds);
    check(cf_ks_init(&ks, ar, 16), "flat input keyspace");
    defaults(&ks, &ds);
    check(cf_source_env(&s, "env", CF_RANK_ENV, "APP_", env), "environment source");
    cf_source_env_defs(&s, &ds);
    check(cf_source_load(&s, &ks), "split environment list");
    check(cf_source_cli(&s, "cli", CF_RANK_CLI, 8, argv), "command-line source");
    cf_source_cli_defs(&s, &ds);
    check(cf_source_load(&s, &ks), "accumulate repeated options and aliases");
    check(cf_ks_resolve(&ks), "resolve flat input");
    check(cf_defs_apply(&ks, &ds, NULL, CF_UNKNOWN_REJECT, NULL, &err), err.message);
    int_is(&ks, "tags.count", 3);
    text_is(&ks, "tags.0", "x,y");
    text_is(&ks, "tags.1", "");
    text_is(&ks, "tags.2", "z");
    check(cf_ks_get(&ks, "tags.3", 6) == NULL, "winning count truncates lower list");
    int_is(&ks, "switches.count", 2);
    v = cf_ks_get(&ks, "switches.0", 10);
    check(v != NULL && v->type == CF_TYPE_BOOL && !v->u.b,
          "boolean list takes explicit values instead of acting as a flag");
    v = cf_ks_get(&ks, "switches.1", 10);
    check(v != NULL && v->type == CF_TYPE_BOOL && v->u.b, "second boolean element");
    e = cf_ks_find(&ks, "tags.count", 10);
    for (c = e->chain; c != NULL; c = c->next) {
        if (c->source == CF_RANK_CLI) {
            ++cli_counts;
        }
    }
    check(cli_counts == 1, "repetition emits exactly one count candidate");
#if CF_WITH_REPORT
    {
        static char out[12000];
        check(cf_report_key_write(&ks, &ds, NULL, "tags.0", 6, CF_REPORT_FULL, out,
                                  sizeof out) > 0,
              "render item report");
        check(strstr(out, "\"default\": \"default-a\"") != NULL,
              "item report carries its own default, not a whole-list default");
        check(strstr(out, "\"list\": { \"key\": \"tags\"") != NULL,
              "item report identifies the list contract");
    }
#endif
}

static void test_shapes(cf_arena* ar) {
    cf_defset   ds;
    cf_def      d;
    cf_keyspace ks;
    cf_value    v;
    cf_error    err = CF_ERROR_INIT;
    check(cf_defset_init(&ds, ar, 8), "shape definitions");
    cf_def_init(&d, CF_TYPE_TEXT, "tags");
    d.list_mode = CF_LIST_REPLACE;
    check(cf_define(&ds, &d, &err), "shape list");
    cf_defset_seal(&ds);
    check(cf_ks_init(&ks, ar, 16), "shape keyspace");
    cf_value_text(&v, "one", 3);
    check(cf_ks_add(&ks, "tags.0", 6, 10, &v, "memory"), "one indexed member");
    check(cf_ks_resolve(&ks), "resolve missing count");
    check(!cf_defs_apply(&ks, &ds, NULL, CF_UNKNOWN_IGNORE, NULL, &err) &&
              err.code == CF_E_VALIDATE,
          "list members require a count");
    ds.partial = CF_TRUE;
    check(cf_defs_apply(&ks, &ds, NULL, CF_UNKNOWN_IGNORE, NULL, &err),
          "partial write validation does not require the rest of the list");
    ds.partial = CF_FALSE;
    check(cf_ks_init(&ks, ar, 16), "sparse keyspace");
    cf_value_int(&v, CF_I64_MAX);
    check(cf_ks_add(&ks, "tags.count", 10, 10, &v, "memory"), "huge sparse count");
    check(cf_ks_resolve(&ks), "resolve sparse list");
    check(!cf_defs_apply(&ks, &ds, NULL, CF_UNKNOWN_IGNORE, NULL, &err),
          "sparse list is refused without a loop over its count");
    check(cf_ks_init(&ks, ar, 16), "malformed member keyspace");
    cf_value_text(&v, "one", 3);
    check(cf_ks_add(&ks, "tags.01", 7, 10, &v, "memory"), "noncanonical member");
    check(cf_ks_resolve(&ks), "resolve noncanonical member");
    check(!cf_defs_apply(&ks, &ds, NULL, CF_UNKNOWN_IGNORE, NULL, &err) &&
              err.code == CF_E_KEY,
          "unknown-ignore cannot bypass a list contract");
}

static void test_environment_spellings(cf_arena* ar) {
    static char  natural[] = "APP_SERVER_TAGS=natural,a";
    static char  escaped[] = "APP_SERVER__TAGS=escaped";
    static char* env[]     = {escaped, natural, NULL};
    cf_defset    ds;
    cf_def       d;
    cf_keyspace  ks;
    cf_source    s;
    cf_error     err = CF_ERROR_INIT;
    check(cf_defset_init(&ds, ar, 8), "natural list definitions");
    cf_def_init(&d, CF_TYPE_TEXT, "server.tags");
    d.list_mode      = CF_LIST_REPLACE;
    d.list_separator = ',';
    check(cf_define(&ds, &d, &err), "dotted list");
    cf_defset_seal(&ds);
    check(cf_ks_init(&ks, ar, 16), "natural list keyspace");
    check(cf_source_env(&s, "env", CF_RANK_ENV, "APP_", env), "natural environment");
    cf_source_env_defs(&s, &ds);
    check(cf_source_load(&s, &ks), "load both spellings");
    check(cf_ks_resolve(&ks), "resolve both spellings");
    check(cf_defs_apply(&ks, &ds, NULL, CF_UNKNOWN_REJECT, NULL, &err),
          "natural spelling does not also create an unknown underscore key");
    int_is(&ks, "server.tags.count", 1);
    text_is(&ks, "server.tags.0", "escaped");
    check(cf_ks_get(&ks, "server.tags.1", 13) == NULL,
          "escaped spelling wins regardless of environment enumeration order");
    check(cf_ks_get(&ks, "server_tags", 11) == NULL,
          "natural alias is not an extra key");

    check(cf_defset_init(&ds, ar, 8), "ambiguous natural definitions");
    check(cf_define(&ds, &d, &err), "ambiguous dotted list");
    cf_def_init(&d, CF_TYPE_TEXT, "server_tags");
    check(cf_define(&ds, &d, &err), "literal underscore definition");
    cf_defset_seal(&ds);
    check(cf_ks_init(&ks, ar, 16), "ambiguous keyspace");
    check(cf_source_env(&s, "env", CF_RANK_ENV, "APP_", env + 1),
          "only the natural-looking variable");
    cf_source_env_defs(&s, &ds);
    check(cf_source_load(&s, &ks), "load ambiguous spelling");
    check(cf_ks_resolve(&ks), "resolve ambiguous spelling");
    check(cf_defs_apply(&ks, &ds, NULL, CF_UNKNOWN_REJECT, NULL, &err),
          "canonical underscore definition remains usable");
    text_is(&ks, "server_tags", "natural,a");
    check(cf_ks_get(&ks, "server.tags.count", 17) == NULL,
          "ambiguous natural alias never configures two definitions");
}

static void add_text(cf_keyspace* ks, const char* key, int rank, const char* value) {
    check(cf_ks_add_text(ks, key, strlen(key), rank, value, strlen(value), "input"),
          "add list input");
}

static void add_count(cf_keyspace* ks, int rank, cf_i64 count, cf_bool resolved) {
    cf_value v;
    cf_value_int(&v, count);
    check(resolved ? cf_ks_add_resolved(ks, "tags.count", 10, rank, &v, "snapshot")
                   : cf_ks_add(ks, "tags.count", 10, rank, &v, "input"),
          "add source-local count");
}

static const char* list_source_name(const void* ctx, int rank) {
    (void)ctx;
    if (rank == CF_RANK_ENV) {
        return "env";
    }
    if (rank == CF_RANK_CLI) {
        return "cli";
    }
    return "other";
}

static void test_append(cf_arena* ar) {
    cf_defset       ds;
    cf_def          d;
    cf_value        v[2];
    cf_keyspace     ks, derived;
    cf_source       snapshot;
    const cf_entry* e;
    cf_error        err = CF_ERROR_INIT;
    cf_u64          digest;
    check(cf_defset_init(&ds, ar, 8), "append definitions");
    cf_def_init(&d, CF_TYPE_TEXT, "tags");
    d.list_mode   = CF_LIST_APPEND;
    d.has_default = CF_TRUE;
    cf_value_text(&v[0], "D0", 2);
    cf_value_text(&v[1], "D1", 2);
    d.list_defaults      = v;
    d.list_default_count = 2;
    check(cf_define(&ds, &d, &err), "append list with defaults");
    cf_defset_seal(&ds);
    check(cf_ks_init(&ks, ar, 16), "append keyspace");
    defaults(&ks, &ds);
    add_text(&ks, "tags.0", CF_RANK_ENV, "E0");
    add_text(&ks, "tags.1", CF_RANK_ENV, "E1");
    add_text(&ks, "tags.6", CF_RANK_ENV, "outside");
    add_count(&ks, CF_RANK_ENV, 7, CF_FALSE);
    add_count(&ks, CF_RANK_ENV, 2, CF_FALSE);
    add_text(&ks, "tags.0", CF_RANK_CLI, "old");
    add_text(&ks, "tags.0", CF_RANK_CLI, "C0");
    add_text(&ks, "tags.1", CF_RANK_CLI, "C1");
    add_count(&ks, CF_RANK_CLI, 2, CF_FALSE);
    add_text(&ks, "tags.1", 1, "PATCH");
    add_text(&ks, "picked", 1, "${tags.2}");
    check(cf_ks_resolve(&ks), "resolve append inputs");
    check(cf_defs_apply(&ks, &ds, NULL, CF_UNKNOWN_IGNORE, NULL, &err), err.message);
    int_is(&ks, "tags.count", 6);
    text_is(&ks, "tags.0", "D0");
    text_is(&ks, "tags.1", "PATCH");
    text_is(&ks, "tags.2", "E0");
    text_is(&ks, "tags.3", "E1");
    text_is(&ks, "tags.4", "C0");
    text_is(&ks, "tags.5", "C1");
    text_is(&ks, "picked", "E0");
    e = cf_ks_find(&ks, "tags.6", 6);
    check(e != NULL && e->selected == NULL && e->chain->outcome == CF_OUT_TRUNCATED,
          "source-local excess is retained but cannot fill another source's list");
    e = cf_ks_find(&ks, "tags.2", 6);
    check(e != NULL && e->selected != NULL &&
              strcmp(cf_ks_original_key(&ks, e->selected), "tags.0") == 0,
          "source-local key is retained after reindexing");
    e = cf_ks_find(&ks, "tags.count", 10);
    check(e != NULL && (e->selected->flags & CF_CAND_LIST_RESULT) != 0,
          "aggregate count is distinguished from a source assignment");
    digest = cf_ks_digest(&ks);
    check(cf_defs_apply(&ks, &ds, NULL, CF_UNKNOWN_IGNORE, NULL, &err),
          "reapplying the contract is allowed");
    check(cf_ks_digest(&ks) == digest, "reapplying does not append twice");
    check(cf_ks_init(&derived, ar, 16), "derived list keyspace");
    check(cf_source_snapshot(&snapshot, "base", &ks), "snapshot source");
    check(cf_source_load(&snapshot, &derived), "replay original list contributions");
    check(cf_ks_resolve(&derived), "resolve derived list");
    check(cf_defs_apply(&derived, &ds, NULL, CF_UNKNOWN_IGNORE, NULL, &err),
          err.message);
    check(cf_ks_digest(&derived) == digest, "derivation preserves list layout, final "
                                            "patches and same-source last-write order");
    check(cf_ks_candidate_count(&derived) == cf_ks_candidate_count(&ks),
          "derivation neither loses history nor duplicates aggregate counts");
}

static void test_append_policy_and_snapshots(cf_arena* ar) {
    static const char* const policy[] = {"env", "cli", NULL};
    cf_defset                ds;
    cf_def                   d;
    cf_value                 v;
    cf_keyspace              ks;
    cf_rank_names            names;
    cf_error                 err = CF_ERROR_INIT;
    names.name_of                = list_source_name;
    names.ctx                    = NULL;
    check(cf_defset_init(&ds, ar, 8), "append policy definitions");
    cf_def_init(&d, CF_TYPE_TEXT, "tags");
    d.list_mode   = CF_LIST_APPEND;
    d.has_default = CF_TRUE;
    cf_value_text(&v, "D", 1);
    d.list_defaults      = &v;
    d.list_default_count = 1;
    d.policy             = policy;
    check(cf_define(&ds, &d, &err), "append policy");
    cf_defset_seal(&ds);
    check(cf_ks_init(&ks, ar, 16), "append policy keyspace");
    defaults(&ks, &ds);
    add_text(&ks, "tags.0", CF_RANK_ENV, "E");
    add_count(&ks, CF_RANK_ENV, 1, CF_FALSE);
    add_text(&ks, "tags.0", CF_RANK_CLI, "C");
    add_count(&ks, CF_RANK_CLI, 1, CF_FALSE);
    add_text(&ks, "tags.0", 1, "forbidden");
    add_count(&ks, 1, 1, CF_FALSE);
    check(cf_ks_resolve(&ks), "resolve policy lists");
    check(cf_defs_apply(&ks, &ds, &names, CF_UNKNOWN_REJECT, NULL, &err), err.message);
    int_is(&ks, "tags.count", 3);
    text_is(&ks, "tags.0", "D");
    text_is(&ks, "tags.1", "C");
    text_is(&ks, "tags.2", "E");

    check(cf_ks_init(&ks, ar, 16), "resolved append base");
    defaults(&ks, &ds);
    cf_value_text(&v, "snapshot-D", 10);
    check(cf_ks_add_resolved(&ks, "tags.0", 6, CF_RANK_CLI, &v, "snapshot"),
          "resolved image or replay element");
    add_count(&ks, CF_RANK_CLI, 1, CF_TRUE);
    add_text(&ks, "tags.0", CF_RANK_ENV, "E");
    add_count(&ks, CF_RANK_ENV, 1, CF_FALSE);
    check(cf_ks_resolve(&ks), "resolve snapshot base with a higher policy source");
    check(cf_defs_apply(&ks, &ds, &names, CF_UNKNOWN_REJECT, NULL, &err), err.message);
    int_is(&ks, "tags.count", 2);
    text_is(&ks, "tags.0", "snapshot-D");
    text_is(&ks, "tags.1", "E");
}

static void test_list_renames(void) {
    static char  a0[] = "app", a1[] = "--old.tags=one", a2[] = "--new.tags=two";
    static char* argv[] = {a0, a1, a2};
    cf_builder*  b;
    cf_config*   c;
    cf_config*   derived;
    cf_def       d;
    cf_error     err = CF_ERROR_INIT;
    int          explicit_new;
    for (explicit_new = 0; explicit_new < 2; ++explicit_new) {
        b = cf_build(NULL);
        check(b != NULL, "rename builder");
        if (b == NULL) {
            return;
        }
        cf_def_init(&d, CF_TYPE_TEXT, "old.tags");
        d.list_mode   = CF_LIST_REPLACE;
        d.flags       = CF_DEPRECATED | CF_SENSITIVE;
        d.replaced_by = "new.tags";
        check(cf_builder_define(b, &d), "old list definition");
        cf_def_init(&d, CF_TYPE_TEXT, "new.tags");
        d.list_mode = CF_LIST_REPLACE;
        d.flags     = CF_SENSITIVE;
        check(cf_builder_define(b, &d), "new list definition");
        check(cf_builder_cli(b, "cli", CF_RANK_CLI, 2 + explicit_new, argv),
              "renamed list source");
        c = cf_builder_create(b, &err);
        check(c != NULL, err.message);
        if (c == NULL) {
            continue;
        }
        check(cf_get_int(c, "new.tags.count", -1) == 1, "renamed list count");
        check(strcmp(cf_get_text(c, "new.tags.0", ""), explicit_new ? "two" : "one") ==
                  0,
              "current spelling wins over a renamed list member");
        derived = cf_derive(c, "runtime", NULL, 0, &err);
        check(derived != NULL, err.message);
        cf_config_free(c);
        if (derived != NULL) {
            check(strcmp(cf_get_text(derived, "new.tags.0", ""),
                         explicit_new ? "two" : "one") == 0,
                  "renamed list survives derivation after the base is freed");
            cf_config_free(derived);
        }
    }
    b = cf_build(NULL);
    check(b != NULL, "sensitivity boundary builder");
    if (b == NULL) {
        return;
    }
    cf_def_init(&d, CF_TYPE_TEXT, "old.tags");
    d.list_mode   = CF_LIST_REPLACE;
    d.flags       = CF_DEPRECATED | CF_SENSITIVE;
    d.replaced_by = "new.tags";
    check(cf_builder_define(b, &d), "private old list");
    cf_def_init(&d, CF_TYPE_TEXT, "new.tags");
    d.list_mode = CF_LIST_REPLACE;
    check(cf_builder_define(b, &d), "public replacement definition");
    c = cf_builder_create(b, &err);
    check(c == NULL && err.code == CF_E_DEFINITION,
          "renaming cannot silently declassify a sensitive list");
    cf_config_free(c);
}

#if CF_WITH_INI
/* INI carries no types, so the definition decides: a declared list with
   a separator splits an INI value as it splits the environment's, and a
   key that is not one keeps its text. */
static void test_ini_value_lists(void) {
    FILE*       f = fopen("cf_lists_fixture.ini", "wb");
    cf_builder* b;
    cf_config*  c;
    cf_def      d;
    cf_error    err = CF_ERROR_INIT;

    check(f != NULL, "write the INI fixture");
    if (f == NULL) {
        return;
    }
    fputs("ports = 80,443,8443,22\nnames = a,b\nempty =\n", f);
    fclose(f);
    b = cf_build(NULL);
    cf_def_init(&d, CF_TYPE_INT, "ports");
    d.list_mode      = CF_LIST_REPLACE;
    d.list_separator = ',';
    check(cf_builder_define(b, &d), "a list of ports, separated by commas");
    cf_def_init(&d, CF_TYPE_TEXT, "empty");
    d.list_mode      = CF_LIST_REPLACE;
    d.list_separator = ',';
    check(cf_builder_define(b, &d), "and one left empty");
    cf_builder_text(b, "names", NULL, 0);
    check(cf_builder_file(b, "ini", CF_RANK_USER_FILE, "cf_lists_fixture.ini", NULL, 0),
          "the INI file");
    c = cf_builder_create(b, &err);
    check(c != NULL, "builds");
    if (c != NULL) {
        check(
            cf_get_int(c, "ports.count", 0) == 4 && cf_get_int(c, "ports.0", 0) == 80 &&
                cf_get_int(c, "ports.2", 0) == 8443 &&
                cf_get_int(c, "ports.3", 0) == 22,
            "the INI value is split into the list's elements, typed by the definition");
        check(strcmp(cf_get_text(c, "names", ""), "a,b") == 0,
              "a key that is not a list keeps its text, commas and all");
        check(cf_get_int(c, "empty.count", -1) == 0, "an empty value is an empty list");
        cf_config_free(c);
    } else {
        printf("  %s\n", err.message);
    }
    remove("cf_lists_fixture.ini");
}
#endif

#if CF_WITH_JSON && (CF_WITH_DOTENV || CF_WITH_REPORT)
static cf_bool list_spill(const char* path, const char* text) {
    FILE*   f = fopen(path, "wb");
    cf_bool ok;
    if (f == NULL) {
        return CF_FALSE;
    }
    ok = fputs(text, f) >= 0;
    if (fclose(f) != 0) {
        ok = CF_FALSE;
    }
    return ok;
}
#endif

#if CF_WITH_JSON && CF_WITH_DOTENV
static void test_file_facade(void) {
    static char  a0[] = "app", a1[] = "--service.tags=cli";
    static char* argv[] = {a0, a1};
    cf_builder*  b;
    cf_config*   c;
    cf_config*   derived;
    cf_def       d;
    cf_value     v;
    cf_error     err = CF_ERROR_INIT;
    cf_bool      json =
        list_spill("cf_lists_fixture.json", "{\"service\":{\"tags\":[\"json\"]}}\n");
    cf_bool dotenv = list_spill("cf_lists_fixture.env", "APP_SERVICE_TAGS=dotenv\n");
    check(json && dotenv, "write list source fixtures");
    if (!json || !dotenv) {
        if (json) {
            remove("cf_lists_fixture.json");
        }
        if (dotenv) {
            remove("cf_lists_fixture.env");
        }
        return;
    }
    b = cf_build(NULL);
    check(b != NULL, "list file builder");
    if (b != NULL) {
        cf_def_init(&d, CF_TYPE_TEXT, "service.tags");
        d.list_mode      = CF_LIST_APPEND;
        d.list_separator = ',';
        d.has_default    = CF_TRUE;
        cf_value_text(&v, "default", 7);
        d.list_defaults      = &v;
        d.list_default_count = 1;
        check(cf_builder_define(b, &d), "file list definition");
        check(cf_builder_file(b, "file", CF_RANK_SYS_FILE, "cf_lists_fixture.json",
                              NULL, 0),
              "JSON list source");
        check(cf_builder_dotenv(b, "dotenv", CF_RANK_USER_FILE, "cf_lists_fixture.env",
                                "APP_", 0),
              "dotenv list source");
        check(cf_builder_cli(b, "cli", CF_RANK_CLI, 2, argv), "CLI list source");
        c = cf_builder_create(b, &err);
        check(c != NULL, err.message);
        if (c != NULL) {
            check(cf_get_int(c, "service.tags.count", -1) == 4,
                  "facade wires definitions to all flat list sources");
            check(strcmp(cf_get_text(c, "service.tags.0", ""), "default") == 0 &&
                      strcmp(cf_get_text(c, "service.tags.1", ""), "json") == 0 &&
                      strcmp(cf_get_text(c, "service.tags.2", ""), "dotenv") == 0 &&
                      strcmp(cf_get_text(c, "service.tags.3", ""), "cli") == 0,
                  "typed JSON, dotenv, defaults and CLI form one ordered list");
            derived = cf_derive(c, "runtime", NULL, 0, &err);
            check(derived != NULL, err.message);
            cf_config_free(c);
            if (derived != NULL) {
                check(cf_get_int(derived, "service.tags.count", -1) == 4 &&
                          strcmp(cf_get_text(derived, "service.tags.1", ""), "json") ==
                              0,
                      "facade derivation does not seed or append defaults twice");
                cf_config_free(derived);
            }
        }
    }
    check(remove("cf_lists_fixture.json") == 0, "remove JSON fixture");
    check(remove("cf_lists_fixture.env") == 0, "remove dotenv fixture");
}
#endif

static void list_contract_on(cf_builder* b) {
    cf_def   d;
    cf_value v;
    cf_def_init(&d, CF_TYPE_TEXT, "tags");
    d.list_mode      = CF_LIST_APPEND;
    d.list_separator = ',';
    d.has_default    = CF_TRUE;
    cf_value_text(&v, "default", 7);
    d.list_defaults      = &v;
    d.list_default_count = 1;
    cf_builder_define(b, &d);
    cf_def_init(&d, CF_TYPE_BOOL, "switches");
    d.list_mode = CF_LIST_APPEND;
    cf_builder_define(b, &d);
}

static void list_inputs_on(cf_builder* b) {
    static char  e0[]  = "APP_TAGS=a,,b";
    static char* env[] = {e0, NULL};
    static char  a0[] = "app", a1[] = "--tags=c0", a2[] = "--tags=c1";
    static char  a3[] = "--switches=false", a4[] = "--switches=true";
    static char* argv[] = {a0, a1, a2, a3, a4};
    list_contract_on(b);
    cf_builder_env(b, "env", CF_RANK_ENV, "APP_", env);
    cf_builder_cli(b, "cli", CF_RANK_CLI, 5, argv);
}

static void test_list_capacity(void) {
    static char buffer[65536];
    cf_builder* b = cf_build(NULL);
    cf_config*  c;
    cf_error    err = CF_ERROR_INIT;
    cf_u64      digest;
    size_t      capacity, passed = 0, refused = 0;
    list_inputs_on(b);
    c = cf_builder_create(b, &err);
    check(c != NULL, err.message);
    if (c == NULL) {
        return;
    }
    digest = cf_digest(c);
    check(cf_count(c) == 10 && cf_get_int(c, "tags.count", -1) == 6 &&
              strcmp(cf_get_text(c, "tags.2", "absent"), "") == 0 &&
              strcmp(cf_get_text(c, "tags.5", ""), "c1") == 0 &&
              cf_get_int(c, "switches.count", -1) == 2,
          "two independent list plans retain every element and empty field");
    cf_config_free(c);
    for (capacity = 256; capacity <= sizeof buffer; capacity += 256) {
        cf_arena* ar = cf_arena_fixed(buffer, capacity);
        if (ar == NULL) {
            continue;
        }
        b = cf_build_in(ar);
        if (b == NULL) {
            cf_arena_free(ar);
            continue;
        }
        list_inputs_on(b);
        cf_error_clear(&err);
        c = cf_builder_create(b, &err);
        if (c == NULL) {
            ++refused;
            check(err.code != CF_OK && err.message[0] != '\0',
                  "list allocation failure is explicit");
        } else {
            ++passed;
            check(cf_digest(c) == digest && cf_count(c) == 10,
                  "a smaller arena never drops list input or provenance layout");
            cf_config_free(c);
        }
        cf_arena_free(ar);
    }
    check(passed > 0 && refused > 0,
          "list capacity sweep crosses the allocation boundary");
}

static void test_empty_list_inputs(void) {
    static char  e0[]  = "APP_TAGS=";
    static char  e1[]  = "APP_WHOLE=a,b";
    static char* env[] = {e0, e1, NULL};
    cf_builder*  b     = cf_build(NULL);
    cf_config*   c;
    cf_error     err = CF_ERROR_INIT;
    cf_def       d;
    cf_def_init(&d, CF_TYPE_TEXT, "tags");
    d.list_mode      = CF_LIST_REPLACE;
    d.list_separator = ',';
    cf_builder_define(b, &d);
    cf_def_init(&d, CF_TYPE_TEXT, "whole");
    d.list_mode = CF_LIST_REPLACE;
    cf_builder_define(b, &d);
    cf_builder_env(b, "env", CF_RANK_ENV, "APP_", env);
    c = cf_builder_create(b, &err);
    check(c != NULL, err.message);
    if (c != NULL) {
        check(cf_get_int(c, "tags.count", -1) == 0 &&
                  strcmp(cf_get_text(c, "tags.0", "absent"), "absent") == 0,
              "empty environment input is an empty list, not an empty element");
        check(cf_get_int(c, "whole.count", -1) == 1 &&
                  strcmp(cf_get_text(c, "whole.0", ""), "a,b") == 0,
              "a zero separator preserves the entire input as one element");
        cf_config_free(c);
    }
}

#if CF_WITH_IMAGE || (CF_WITH_REPORT && CF_WITH_JSON)
static void test_list_snapshots(cf_arena* ar) {
    cf_builder* b = cf_build(NULL);
    cf_config*  c;
    cf_config*  back;
    cf_config*  derived;
    cf_error    err = CF_ERROR_INIT;
    cf_u64      digest;
    list_inputs_on(b);
    c = cf_builder_create(b, &err);
    check(c != NULL, err.message);
    if (c == NULL) {
        return;
    }
    digest = cf_digest(c);
#if CF_WITH_IMAGE
    {
        static union {
            cf_i64 align;
            char   bytes[8192];
        } image;
        cf_source source;
        size_t    state_size = cf_source_image_state_size();
        void*     state      = cf_arena_alloc(ar, state_size);
        size_t n = cf_image_write(cf_keys(c), NULL, 0, image.bytes, sizeof image.bytes);
        cf_bool loaded = n > 0 && n <= sizeof image.bytes && state != NULL &&
                         cf_source_image_mem(&source, "image", CF_RANK_ENV, image.bytes,
                                             n, state, state_size);
        check(loaded, "write and open a real multi-list image");
        if (loaded) {
            b = cf_build(NULL);
            list_contract_on(b);
            cf_builder_source(b, &source);
            back = cf_builder_create(b, &err);
            check(back != NULL && cf_digest(back) == digest,
                  "image loading does not append either list's defaults again");
            if (back != NULL) {
                derived = cf_derive(back, "view", NULL, 0, &err);
                cf_config_free(back);
                check(derived != NULL && cf_digest(derived) == digest,
                      "image-derived values survive releasing their base");
                cf_config_free(derived);
            }
        }
    }
#else
    (void)ar;
#endif
#if CF_WITH_REPORT && CF_WITH_JSON
    {
        static char    report[32768];
        cf_report_info info = CF_REPORT_INFO_INIT;
        size_t         n;
        info.mode = CF_REPORT_FULL;
        n         = cf_report(c, &info, report, sizeof report);
        check(n > 0 && n < sizeof report, "full multi-list report fits");
        if (n > 0 && n < sizeof report && list_spill("cf_lists_replay.json", report)) {
            b = cf_build(NULL);
            list_contract_on(b);
            cf_builder_replay(b, "replay", CF_RANK_ENV, "cf_lists_replay.json", 0);
            back = cf_builder_create(b, &err);
            check(back != NULL && cf_digest(back) == digest,
                  "actual Report Replay uses the final lists as resolved bases");
            if (back != NULL) {
                derived = cf_derive(back, "view", NULL, 0, &err);
                cf_config_free(back);
                check(derived != NULL && cf_digest(derived) == digest,
                      "replayed lists survive repeated derivation without growing");
                cf_config_free(derived);
            }
            check(remove("cf_lists_replay.json") == 0, "remove list replay fixture");
        } else {
            check(0, "write full list replay fixture");
        }
    }
#endif
    cf_config_free(c);
}
#endif

struct list_live_input {
    const cf_pair* pairs;
    size_t         count;
    cf_list_mode   mode;
    cf_bool        policy;
};

static cf_builder* list_live_build(void* user) {
    static const char* const policy[] = {"mem", NULL};
    struct list_live_input*  input    = (struct list_live_input*)user;
    cf_builder*              b        = cf_build(NULL);
    cf_def                   d;
    cf_value                 v;
    cf_def_init(&d, CF_TYPE_TEXT, "tags");
    d.list_mode   = input->mode;
    d.flags       = CF_RESTART;
    d.policy      = input->policy ? policy : NULL;
    d.has_default = CF_TRUE;
    cf_value_text(&v, "base", 4);
    d.list_defaults      = &v;
    d.list_default_count = 1;
    cf_builder_define(b, &d);
    cf_builder_text(b, "level", "info", 0);
    cf_builder_memory(b, "mem", input->policy ? CF_RANK_USER_FILE : INT_MIN,
                      input->pairs, input->count);
    return b;
}

static void test_restart_lists(void) {
    static const cf_pair before[] = {
        {"tags.count", "1"}, {"tags.0", "$${literal}"}, {"level", "info"}};
    static const cf_pair after[]   = {{"tags.count", "2"},
                                      {"tags.0", "changed"},
                                      {"tags.1", "extra"},
                                      {"level", "debug"}};
    static const cf_pair empty[]   = {{"tags.count", "0"}, {"level", "quiet"}};
    static const cf_pair element[] = {
        {"tags.count", "1"}, {"tags.0", "changed"}, {"level", "debug"}};
    struct list_live_input input;
    cf_live*               live;
    cf_config*             c;
    cf_error               err = CF_ERROR_INIT;
    char                   keys[256];
    int                    append, scenario;
    for (scenario = 0; scenario < 4; ++scenario) {
        append       = scenario % 2;
        input.policy = scenario < 2;
        input.pairs  = before;
        input.count  = 3;
        input.mode   = append ? CF_LIST_APPEND : CF_LIST_REPLACE;
        live = cf_live_open(CF_LIVE_MANUAL, list_live_build, NULL, &input, NULL, &err);
        check(live != NULL, err.message);
        if (live == NULL) {
            continue;
        }
        input.pairs = after;
        input.count = 4;
        check(cf_live_reload(live, &err) == CF_LIVE_SWAPPED,
              "reload accepts unrelated changes while keeping a restart list");
        c = cf_live_get(live);
        check(cf_get_int(c, "tags.count", -1) == 1 + append,
              "restart list length stays at its running value");
        check(strcmp(cf_get_text(c, append ? "tags.1" : "tags.0", ""), "${literal}") ==
                  0,
              "restart list retains literal text without re-expanding it");
        check(strcmp(cf_get_text(c, "level", ""), "debug") == 0,
              "non-restart setting still reloads");
        check(cf_live_restarts(live, keys, sizeof keys) > 0 &&
                  strstr(keys, "tags.count\n") != NULL,
              "restart list is reported as waiting");
        {
            cf_config* derived = cf_derive(c, "view", NULL, 0, &err);
            check(derived != NULL &&
                      cf_get_int(derived, "tags.count", -1) == 1 + append &&
                      strcmp(cf_get_text(derived, append ? "tags.1" : "tags.0", ""),
                             "${literal}") == 0,
                  "a derived view preserves the complete held list and its policy");
            cf_config_free(derived);
        }
        cf_config_free(c);
        input.pairs = empty;
        input.count = 2;
        check(cf_live_reload(live, &err) == CF_LIVE_SWAPPED,
              "an empty requested list does not erase the running list");
        c = cf_live_get(live);
        check(cf_get_int(c, "tags.count", -1) == 1 + append &&
                  strcmp(cf_get_text(c, "level", ""), "quiet") == 0,
              "held length survives repeated reloads without being appended again");
        cf_config_free(c);
        input.pairs = element;
        input.count = 3;
        check(cf_live_reload(live, &err) == CF_LIVE_SWAPPED,
              "an element-only change keeps the entire list");
        c = cf_live_get(live);
        check(strcmp(cf_get_text(c, append ? "tags.1" : "tags.0", ""), "${literal}") ==
                  0,
              "unchanged count does not hide changed content");
        cf_config_free(c);
        input.pairs = before;
        check(cf_live_reload(live, &err) == CF_LIVE_SWAPPED &&
                  cf_live_restarts(live, keys, sizeof keys) == 0,
              "restoring the requested list clears the restart notice");
        cf_live_close(live);
    }
    input.mode   = CF_LIST_REPLACE;
    input.policy = CF_TRUE;
    input.pairs  = empty;
    input.count  = 2;
    live = cf_live_open(CF_LIVE_MANUAL, list_live_build, NULL, &input, NULL, &err);
    check(live != NULL, err.message);
    if (live != NULL) {
        input.pairs = after;
        input.count = 4;
        check(cf_live_reload(live, &err) == CF_LIVE_SWAPPED,
              "an initially empty restart list remains present and empty");
        c = cf_live_get(live);
        check(cf_get_int(c, "tags.count", -1) == 0 &&
                  strcmp(cf_get_text(c, "tags.0", "absent"), "absent") == 0,
              "no lower-source element leaks through a held empty list");
        cf_config_free(c);
        cf_live_close(live);
    }
}

struct list_store_row {
    char key[CF_KEY_MAX];
    char value[64];
};
static struct list_store_row g_list_rows[16];
static size_t                g_list_rows_count, g_list_writes;

static void list_store_put(const char* key, const char* value) {
    size_t i;
    for (i = 0; i < g_list_rows_count && strcmp(g_list_rows[i].key, key) != 0; ++i) {
    }
    if (i == g_list_rows_count) {
        ++g_list_rows_count;
    }
    cf_fmt(g_list_rows[i].key, sizeof g_list_rows[i].key, "%s", key);
    cf_fmt(g_list_rows[i].value, sizeof g_list_rows[i].value, "%s", value);
}

static cf_bool list_store_load(const cf_source* s, cf_keyspace* ks) {
    size_t i;
    for (i = 0; i < g_list_rows_count; ++i) {
        if (!cf_ks_add_text(ks, g_list_rows[i].key, strlen(g_list_rows[i].key), s->rank,
                            g_list_rows[i].value, strlen(g_list_rows[i].value),
                            "test store")) {
            return CF_FALSE;
        }
    }
    return CF_TRUE;
}

static cf_bool list_store_write(const cf_source* s, const cf_write_item* items,
                                size_t count, const char* meta, cf_error* err) {
    size_t i;
    char   text[64];
    (void)s;
    (void)meta;
    ++g_list_writes;
    if (g_list_rows_count + count > 16) {
        return cf_error_set(err, CF_E_MEMORY, NULL, NULL, "test store is full");
    }
    for (i = 0; i < count; ++i) {
        if (items[i].op == CF_WRITE_SET) {
            cf_value_to_text(&items[i].value, text, sizeof text);
            list_store_put(items[i].key, text);
        } else {
            size_t j;
            for (j = 0; j < g_list_rows_count; ++j) {
                if (strcmp(g_list_rows[j].key, items[i].key) == 0) {
                    g_list_rows[j] = g_list_rows[--g_list_rows_count];
                    break;
                }
            }
        }
    }
    return CF_TRUE;
}

static const cf_source_vtable list_store_vt = {
    sizeof(cf_source_vtable), "test-store", list_store_load, NULL, NULL,
    list_store_write};

static cf_config* list_write_build(cf_bool rename, cf_error* err) {
    static const cf_pair file[] = {{"tags.count", "1"}, {"tags.0", "file"}};
    cf_builder*          b      = cf_build(NULL);
    cf_source            s;
    cf_def               d;
    cf_value             v;
    memset(&s, 0, sizeof s);
    s.vt   = &list_store_vt;
    s.name = "store";
    s.rank = CF_RANK_ENV;
    cf_builder_source(b, &s);
    cf_builder_writable(b, "store");
    cf_builder_memory(b, "file", CF_RANK_SYS_FILE, file, 2);
    if (rename) {
        cf_def_init(&d, CF_TYPE_TEXT, "old");
        d.list_mode   = CF_LIST_APPEND;
        d.flags       = CF_DEPRECATED;
        d.replaced_by = "tags";
        cf_builder_define(b, &d);
    }
    cf_def_init(&d, CF_TYPE_TEXT, "tags");
    d.list_mode   = CF_LIST_APPEND;
    d.has_default = CF_TRUE;
    cf_value_text(&v, "default", 7);
    d.list_defaults      = &v;
    d.list_default_count = 1;
    cf_builder_define(b, &d);
    cf_builder_text(b, "level", "info", 0);
    return cf_builder_create(b, err);
}

static void test_list_writes(void) {
    cf_config*    c;
    cf_config*    reloaded;
    cf_write_item items[2];
    cf_error      err = CF_ERROR_INIT;
    int           rename;
    for (rename = 0; rename < 2; ++rename) {
        g_list_rows_count = g_list_writes = 0;
        list_store_put(rename ? "old.count" : "tags.count", "1");
        list_store_put(rename ? "old.0" : "tags.0", "stored");
        c = list_write_build(rename, &err);
        check(c != NULL, err.message);
        if (c == NULL) {
            continue;
        }
        items[0].key = "tags.2";
        items[0].op  = CF_WRITE_SET;
        cf_value_text(&items[0].value, "updated", 7);
        check(cf_write(c, "store", items, 1, "test", &err), err.message);
        check(g_list_writes == 1, "existing appended element is written once");
        reloaded = list_write_build(rename, &err);
        check(reloaded != NULL && cf_get_int(reloaded, "tags.count", -1) == 3 &&
                  strcmp(cf_get_text(reloaded, "tags.2", ""), "updated") == 0,
              "a final list index writes the target's local index and survives reload");
        cf_config_free(reloaded);
        check(strcmp(cf_get_text(c, "tags.2", ""), "stored") == 0,
              "a write does not mutate its snapshot");
        g_list_writes = 0;
        items[0].key  = "tags.0";
        check(!cf_write(c, "store", items, 1, "test", &err) &&
                  err.code == CF_E_CAPABILITY && g_list_writes == 0,
              "a contributing source cannot patch another source's element");
        items[0].key = "tags.2";
        items[0].op  = CF_WRITE_DELETE;
        check(!cf_write(c, "store", items, 1, "test", &err) &&
                  err.code == CF_E_CAPABILITY && g_list_writes == 0,
              "deleting an appended element cannot silently create a hole");
        items[0].key = "level";
        items[0].op  = CF_WRITE_SET;
        cf_value_text(&items[0].value, "debug", 5);
        items[1].key = "tags.count";
        items[1].op  = CF_WRITE_SET;
        cf_value_int(&items[1].value, 1);
        check(!cf_write(c, "store", items, 2, "test", &err) &&
                  err.code == CF_E_CAPABILITY && g_list_writes == 0,
              "ambiguous count changes refuse the entire batch before its callback");
        cf_config_free(c);
    }
    g_list_rows_count = g_list_writes = 0;
    list_store_put("tags.1", "patch");
    c = list_write_build(CF_FALSE, &err);
    check(c != NULL, err.message);
    if (c != NULL) {
        items[0].key = "tags.1";
        items[0].op  = CF_WRITE_SET;
        cf_value_text(&items[0].value, "updated", 7);
        check(cf_write(c, "store", items, 1, "test", &err),
              "a source without a count writes final-index patches");
        reloaded = list_write_build(CF_FALSE, &err);
        check(reloaded != NULL && cf_get_int(reloaded, "tags.count", -1) == 2 &&
                  strcmp(cf_get_text(reloaded, "tags.1", ""), "updated") == 0,
              "a final-index patch is not shifted or appended");
        cf_config_free(reloaded);
        items[0].op = CF_WRITE_DELETE;
        check(cf_write(c, "store", items, 1, "test", &err), "a patch can be removed");
        reloaded = list_write_build(CF_FALSE, &err);
        check(reloaded != NULL &&
                  strcmp(cf_get_text(reloaded, "tags.1", ""), "file") == 0,
              "removing a patch reveals its original element");
        cf_config_free(reloaded);
        cf_config_free(c);
    }
}

#if CF_WITH_REPORT
static void test_schema_and_privacy(cf_arena* ar) {
    cf_defset   ds;
    cf_def      d;
    cf_keyspace ks;
    cf_value    v;
    cf_error    err = CF_ERROR_INIT;
    static char out[12000];
    check(cf_defset_init(&ds, ar, 8), "schema definitions");
    cf_def_init(&d, CF_TYPE_TEXT, "private");
    d.flags       = CF_SENSITIVE;
    d.list_mode   = CF_LIST_REPLACE;
    d.has_default = CF_TRUE;
    cf_value_text(&v, "list-default-canary", 19);
    d.list_defaults      = &v;
    d.list_default_count = 1;
    check(cf_define(&ds, &d, &err), "secret list definition");
    cf_def_init(&d, CF_TYPE_INT, "ports");
    d.list_mode   = CF_LIST_REPLACE;
    d.has_default = CF_TRUE;
    cf_value_int(&v, 80);
    d.list_defaults      = &v;
    d.list_default_count = 1;
    d.has_range          = CF_TRUE;
    d.imin               = 1;
    d.imax               = 65535;
    check(cf_define(&ds, &d, &err), "integer list definition");
    cf_defset_seal(&ds);
    check(cf_schema_write(&ds, "lists", CF_TRUE, out, sizeof out, &err) > 0,
          "render array schema");
    check(strstr(out, "\"ports\": { \"type\": \"array\"") != NULL &&
              strstr(out, "\"default\": [80]") != NULL &&
              strstr(out, "\"items\": { \"type\": \"integer\", \"minimum\": 1") != NULL,
          "schema puts type and range on elements and default on array");
    check(strstr(out, "list-default-canary") == NULL, "schema omits secret default");
    check(cf_ks_init(&ks, ar, 16), "privacy keyspace");
    defaults(&ks, &ds);
    check(cf_ks_resolve(&ks), "resolve list defaults");
    check(cf_defs_apply(&ks, &ds, NULL, CF_UNKNOWN_REJECT, NULL, &err),
          "apply list defaults");
    check(cf_report_write(&ks, &ds, NULL, NULL, out, sizeof out) > 0,
          "render blind list report");
    check(strstr(out, "list-default-canary") == NULL, "blind list report is private");
    check(cf_report_key_write(&ks, &ds, NULL, "private", 7, CF_REPORT_BLIND, out,
                              sizeof out) > 0,
          "render blind list root");
    check(strstr(out, "list-default-canary") == NULL, "blind root metadata is private");
    check(cf_report_key_write(&ks, &ds, NULL, "private", 7, CF_REPORT_FULL, out,
                              sizeof out) > 0,
          "render full list root");
    check(strstr(out, "\"default\": [\"list-default-canary\"]") != NULL,
          "explicit full mode includes the typed list default");
}
#endif

int main(void) {
    cf_arena* ar = cf_arena_new(NULL, 0);
    check(ar != NULL, "arena");
    if (ar != NULL) {
        test_contract(ar);
        test_descriptor_refusals(ar);
        test_flat_inputs(ar);
        test_shapes(ar);
        test_environment_spellings(ar);
        test_append(ar);
        test_append_policy_and_snapshots(ar);
        test_list_renames();
        test_restart_lists();
        test_list_writes();
        test_empty_list_inputs();
        test_list_capacity();
#if CF_WITH_IMAGE || (CF_WITH_REPORT && CF_WITH_JSON)
        test_list_snapshots(ar);
#endif
#if CF_WITH_JSON && CF_WITH_DOTENV
        test_file_facade();
#endif
#if CF_WITH_INI
        test_ini_value_lists();
#endif
#if CF_WITH_REPORT
        test_schema_and_privacy(ar);
#endif
        cf_arena_free(ar);
    }
    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
