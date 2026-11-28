/*
 * test_units.c - a unit on an int definition (C-2a, D-113, D-222).
 *
 * "1h30m" and "512MiB" are read as the int in the unit's base, wherever the
 * text comes from, within the range given in that base; explain() shows
 * the number and the text as written. The grammar is test_numeric's.
 */

#include <configforge/cf_numeric.h>
#include <configforge/configforge.h>

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "cf_test.h"

/* The contract of the tests: a timeout and a cache limit with units, and
   one int without. `flags` goes on both unit keys. */
static void define_all(cf_builder* b, unsigned flags, cf_on_invalid on_invalid) {
    cf_def d;

    cf_def_init(&d, CF_TYPE_INT, "timeout");
    d.unit        = cf_unit_duration_get();
    d.flags       = flags;
    d.on_invalid  = on_invalid;
    d.has_range   = CF_TRUE;
    d.imin        = 0;
    d.imax        = 86400000;
    d.has_default = CF_TRUE;
    cf_value_int(&d.default_value, 30000);
    check(cf_builder_define(b, &d), "timeout is defined");

    cf_def_init(&d, CF_TYPE_INT, "cache.limit");
    d.unit        = cf_unit_size_get();
    d.flags       = flags;
    d.on_invalid  = on_invalid;
    d.has_range   = CF_TRUE;
    d.imin        = 1;
    d.imax        = 1073741824;
    d.has_default = CF_TRUE;
    cf_value_int(&d.default_value, 1048576);
    check(cf_builder_define(b, &d), "cache.limit is defined");

    check(cf_builder_int(b, "retries", 3, 0, 100, 0), "retries is defined");
    check(cf_builder_int(b, "base", 60, 0, 1000, 0), "base is defined");
}

static cf_config* from_pairs(const cf_pair* pairs, size_t n, unsigned flags,
                             cf_on_invalid on_invalid, cf_error* err) {
    cf_builder* b = cf_build(NULL);

    cf_builder_memory(b, "file", CF_RANK_USER_FILE, pairs, n);
    define_all(b, flags, on_invalid);
    return cf_builder_create(b, err);
}

static cf_config* from_json(const char* doc, cf_error* err) {
    cf_builder* b = cf_build(NULL);

    check(cf_builder_document(b, "doc.json", CF_RANK_USER_FILE, doc, strlen(doc),
                              "json", 0),
          "the document is taken");
    define_all(b, 0, CF_INVALID_STRICT);
    return cf_builder_create(b, err);
}

/* The text a source wrote, the int the program reads, and the history. */
static void test_textual(void) {
    static const cf_pair pairs[] = {
        {"timeout", "1h30m"}, {"cache.limit", "512MiB"}, {"retries", "5"}};
    static char ex[2048];
    cf_error    err = CF_ERROR_INIT;
    cf_config*  c   = from_pairs(pairs, 3, 0, CF_INVALID_STRICT, &err);

    check(c != NULL, "a unit text builds");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(cf_get_int(c, "timeout", 0) == 5400000, "1h30m is 5400000 ms");
    check(cf_get_int(c, "cache.limit", 0) == 536870912, "512MiB is 536870912 B");
    check(cf_get_int(c, "retries", 0) == 5, "an int without a unit is read as before");
    check(cf_type_of(c, "timeout") == CF_TYPE_INT, "a unit key stays an int");

    cf_explain(c, "timeout", ex, sizeof ex);
    has(ex, "timeout = 5400000", "explain shows the number");
    has(ex, "from \"1h30m\"", "explain shows the text as written");
    cf_explain(c, "cache.limit", ex, sizeof ex);
    has(ex, "from \"512MiB\"", "explain shows the size as written");
    cf_explain(c, "retries", ex, sizeof ex);
    lacks(ex, "from \"", "an int with no unit has no written text");
    cf_config_free(c);
}

/* A bare number is the base unit; with no text at all, the default. */
static void test_bare_and_default(void) {
    static const cf_pair pairs[] = {{"timeout", "250"}};
    cf_error             err     = CF_ERROR_INIT;
    cf_config*           c       = from_pairs(pairs, 1, 0, CF_INVALID_STRICT, &err);

    check(c != NULL, "a bare number builds");
    if (c != NULL) {
        check(cf_get_int(c, "timeout", 0) == 250, "250 is 250 ms");
        check(cf_get_int(c, "cache.limit", 0) == 1048576, "the default, as written");
    }
    cf_config_free(c);
}

/* A quoted string in a typed document is read as the unit; a number is
   what it was. */
static void test_typed(void) {
    static char ex[2048];
    cf_error    err = CF_ERROR_INIT;
    cf_config*  c =
        from_json("{\"timeout\": \"90s\", \"cache\": {\"limit\": 2048}}", &err);

    check(c != NULL, "a typed string and a typed int build");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(cf_get_int(c, "timeout", 0) == 90000, "\"90s\" is 90000 ms");
    check(cf_get_int(c, "cache.limit", 0) == 2048, "a typed int is itself");
    cf_explain(c, "timeout", ex, sizeof ex);
    has(ex, "from \"90s\"", "the typed text is kept as written");
    cf_explain(c, "cache.limit", ex, sizeof ex);
    lacks(ex, "from \"", "a typed int has no written text");
    cf_config_free(c);

    c = from_json("{\"retries\": \"30s\"}", &err);
    check(c == NULL, "a unit is no coercion for a key that has none");
    has(err.message, "source supplied text where int was declared",
        "the typed text of a plain int is refused as before");
}

/* The environment reads it too. */
static void test_env(void) {
    static char  e0[]  = "APP_TIMEOUT=2m";
    static char  e1[]  = "APP_CACHE__LIMIT=1GiB";
    static char* env[] = {e0, e1, NULL};
    cf_error     err   = CF_ERROR_INIT;
    cf_builder*  b     = cf_build(NULL);
    cf_config*   c;

    cf_builder_env(b, "env", CF_RANK_ENV, "APP_", env);
    define_all(b, 0, CF_INVALID_STRICT);
    c = cf_builder_create(b, &err);
    check(c != NULL, "the environment builds");
    if (c != NULL) {
        check(cf_get_int(c, "timeout", 0) == 120000, "APP_TIMEOUT=2m is 120000 ms");
        check(cf_get_int(c, "cache.limit", 0) == 1073741824, "1GiB is 2^30 B");
    }
    cf_config_free(c);
}

/* The range is in the base unit, and a refusal says why. */
static void test_refusals(void) {
    static const cf_pair over[]  = {{"timeout", "25h"}};
    static const cf_pair soon[]  = {{"timeout", "soon"}};
    static const cf_pair year[]  = {{"timeout", "1y"}};
    static const cf_pair plain[] = {{"retries", "30s"}};
    cf_error             err     = CF_ERROR_INIT;
    cf_config*           c;

    c = from_pairs(over, 1, 0, CF_INVALID_STRICT, &err);
    check(c == NULL, "25h is outside a 24-hour range");
    has(err.message, "90000000 is outside 0..86400000", "the range is in ms");
    cf_config_free(c);

    c = from_pairs(soon, 1, 0, CF_INVALID_STRICT, &err);
    check(c == NULL, "soon is no duration");
    has(err.message, "\"soon\" is not a valid duration", "the unit is named");
    cf_config_free(c);

    c = from_pairs(year, 1, 0, CF_INVALID_STRICT, &err);
    check(c == NULL, "a year is ambiguous");
    has(err.message, "\"1y\" is not a valid duration: is ambiguous: use days",
        "the unit's own reason is added");
    cf_config_free(c);

    c = from_pairs(plain, 1, 0, CF_INVALID_STRICT, &err);
    check(c == NULL, "a plain int takes no unit");
    has(err.message, "\"30s\" is not a valid int", "as it always said");
    cf_config_free(c);
}

/* A refused unit text falls back like any refused value, and explain
   keeps the refusal. */
static void test_fallback(void) {
    static const cf_pair soon[] = {{"timeout", "soon"}};
    static char          ex[2048];
    cf_error             err = CF_ERROR_INIT;
    cf_config*           c   = from_pairs(soon, 1, 0, CF_INVALID_FALLBACK, &err);

    check(c != NULL, "a refused text falls back");
    if (c != NULL) {
        check(cf_get_int(c, "timeout", 0) == 30000, "to the default");
        cf_explain(c, "timeout", ex, sizeof ex);
        has(ex, "\"soon\" is not a valid duration", "the refusal stays visible");
    }
    cf_config_free(c);
}

/* A secret's refusal names neither the text nor the unit's reason. */
static void test_sensitive(void) {
    static const cf_pair secret[] = {{"timeout", "s3cr3t-1y"}, {"cache.limit", "1y"}};
    static char          ex[2048];
    cf_error             err = CF_ERROR_INIT;
    cf_config*           c;

    c = from_pairs(secret, 2, CF_SENSITIVE, CF_INVALID_STRICT, &err);
    check(c == NULL, "a sensitive key refuses an invalid unit text");
    lacks(err.message, "s3cr3t", "the text is not in the error");
    lacks(err.message, "ambiguous", "the unit's reason is not in the error");
    lacks(err.message, "duration", "nor is the unit");
    has(err.message, cf_refusal_text(CF_REFUSAL_TYPE), "the kind's fixed wording");
    cf_config_free(c);

    c = from_pairs(secret, 2, CF_SENSITIVE, CF_INVALID_FALLBACK, &err);
    check(c != NULL, "and falls back when it is allowed to");
    if (c != NULL) {
        cf_explain(c, "timeout", ex, sizeof ex);
        lacks(ex, "s3cr3t", "explain does not show the text");
        lacks(ex, "ambiguous", "or the unit's reason");
        has(ex, cf_refusal_text(CF_REFUSAL_TYPE), "only the fixed wording");
    }
    cf_config_free(c);
}

/* A reference is expanded first, then read as the unit; the template is
   what was written. */
static void test_reference(void) {
    static const cf_pair pairs[] = {{"base", "90"}, {"timeout", "${base}s"}};
    static char          ex[2048];
    cf_error             err = CF_ERROR_INIT;
    cf_config*           c   = from_pairs(pairs, 2, 0, CF_INVALID_STRICT, &err);

    check(c != NULL, "a reference in a unit text builds");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(cf_get_int(c, "timeout", 0) == 90000, "${base}s is 90s");
    cf_explain(c, "timeout", ex, sizeof ex);
    has(ex, "from \"${base}s\"", "the template is the text as written");
    cf_config_free(c);
}

/* Each element of a list of ints converts. */
static void test_list(void) {
    static char  e0[]  = "APP_DELAYS=1s,2s,1m";
    static char* env[] = {e0, NULL};
    cf_error     err   = CF_ERROR_INIT;
    cf_builder*  b     = cf_build(NULL);
    cf_def       d;
    cf_config*   c;

    cf_builder_env(b, "env", CF_RANK_ENV, "APP_", env);
    cf_def_init(&d, CF_TYPE_INT, "delays");
    d.unit           = cf_unit_duration_get();
    d.list_mode      = CF_LIST_REPLACE;
    d.list_separator = ',';
    check(cf_builder_define(b, &d), "a list of durations is defined");
    c = cf_builder_create(b, &err);
    check(c != NULL, "a list of durations builds");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(cf_get_int(c, "delays.0", 0) == 1000, "the first element is 1s");
    check(cf_get_int(c, "delays.1", 0) == 2000, "the second is 2s");
    check(cf_get_int(c, "delays.2", 0) == 60000, "the third is 1m");
    cf_config_free(c);
}

/* A definition that contradicts itself is refused where it is written. */
static void test_definition(void) {
    cf_arena* ar = cf_arena_new(NULL, 0);
    cf_defset ds;
    cf_error  err = CF_ERROR_INIT;
    cf_def    d;
    cf_unit   noparse;

    cf_defset_init(&ds, ar, 8);

    cf_def_init(&d, CF_TYPE_TEXT, "name");
    d.unit = cf_unit_duration_get();
    check(!cf_define(&ds, &d, &err) && err.code == CF_E_DEFINITION,
          "a unit on a text key is refused");
    has(err.message, "a unit needs an int", "and says why");

    cf_def_init(&d, CF_TYPE_BOOL, "flag");
    d.unit = cf_unit_size_get();
    check(!cf_define(&ds, &d, &err), "a unit on a bool is refused");

    noparse       = *cf_unit_duration_get();
    noparse.parse = NULL;
    cf_def_init(&d, CF_TYPE_INT, "nop");
    d.unit = &noparse;
    check(!cf_define(&ds, &d, &err) && err.code == CF_E_DEFINITION,
          "a unit with no parser is refused");
    has(err.message, "a unit needs a parser", "and says why");

    cf_def_init(&d, CF_TYPE_INT, "ok");
    d.unit = cf_unit_duration_get();
    check(cf_define(&ds, &d, &err), "a unit on an int is accepted");
    check(cf_defset_find(&ds, "ok", 2) != NULL &&
              cf_defset_find(&ds, "ok", 2)->unit == cf_unit_duration_get(),
          "the set keeps the unit");
    cf_arena_free(ar);
}

/* A caller built against 1.8 has a smaller cf_def: its unit is NULL
   whatever the bytes past its size hold (D-51). */
static void test_old_layout(void) {
    cf_arena* ar = cf_arena_new(NULL, 0);
    cf_defset ds;
    cf_error  err = CF_ERROR_INIT;
    cf_def    d;

    cf_defset_init(&ds, ar, 8);
    memset(&d, 0, sizeof d);
    cf_def_init_sized(&d, offsetof(cf_def, unit), CF_TYPE_INT, "old");
    d.unit = cf_unit_duration_get();
    check(cf_define(&ds, &d, &err), "a 1.8 descriptor is accepted");
    check(cf_defset_find(&ds, "old", 3) != NULL &&
              cf_defset_find(&ds, "old", 3)->unit == NULL,
          "and has no unit");

    memset(&d, 0, sizeof d);
    cf_def_init(&d, CF_TYPE_INT, "new");
    check(d.size == sizeof d && d.unit == NULL, "cf_def_init leaves the unit NULL");
    cf_arena_free(ar);
}

/* A unit of the program's own works through cf_def.unit like the built-ins. */
static cf_bool tick_parse(const cf_unit* u, const char* s, size_t n, cf_i64* out,
                          const char** reason) {
    static const cf_unit_term terms[] = {{"tk", 1}, {"ktk", 1000}};
    cf_unit_table             t;

    t.terms      = terms;
    t.count      = 2;
    t.per_base   = 1;
    t.sum        = CF_TRUE;
    t.allow_bare = u->options.allow_bare;
    t.not_whole  = "is not a whole number of ticks";
    t.refused    = NULL;
    return cf_unit_table_parse(&t, s, n, out, reason);
}

static size_t tick_format(const cf_unit* u, cf_i64 v, char* buf, size_t cap) {
    (void)u;
    return (size_t)cf_fmt(buf, cap, "%Itk", v);
}

static void test_own_unit(void) {
    static const cf_pair pairs[] = {{"clock", "3ktk5tk"}};
    static char          ex[2048];
    cf_unit              tick;
    cf_error             err = CF_ERROR_INIT;
    cf_builder*          b   = cf_build(NULL);
    cf_def               d;
    cf_config*           c;

    memset(&tick, 0, sizeof tick);
    cf_unit_options_init(&tick.options);
    tick.size   = sizeof tick;
    tick.name   = "tick";
    tick.parse  = tick_parse;
    tick.format = tick_format;

    cf_builder_memory(b, "file", CF_RANK_USER_FILE, pairs, 1);
    cf_def_init(&d, CF_TYPE_INT, "clock");
    d.unit = &tick;
    check(cf_builder_define(b, &d), "a program's own unit is defined");
    c = cf_builder_create(b, &err);
    check(c != NULL, "its text builds");
    if (c != NULL) {
        check(cf_get_int(c, "clock", 0) == 3005, "3ktk5tk is 3005 ticks");
        cf_explain(c, "clock", ex, sizeof ex);
        has(ex, "from \"3ktk5tk\"", "and is shown as written");
    }
    cf_config_free(c);

    b = cf_build(NULL);
    {
        static const cf_pair bad[] = {{"clock", "3s"}};
        cf_builder_memory(b, "file", CF_RANK_USER_FILE, bad, 1);
    }
    cf_builder_define(b, &d);
    c = cf_builder_create(b, &err);
    check(c == NULL, "its refusal builds nothing");
    has(err.message, "\"3s\" is not a valid tick", "and names the unit");
    cf_config_free(c);
}

int main(void) {
    test_textual();
    test_bare_and_default();
    test_typed();
    test_env();
    test_refusals();
    test_fallback();
    test_sensitive();
    test_reference();
    test_list();
    test_definition();
    test_old_layout();
    test_own_unit();
    return cf_test_done();
}
