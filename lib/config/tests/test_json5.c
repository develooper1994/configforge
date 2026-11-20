/*
 * test_json5.c - JSON5 (json5.org 1.0.0) and JSONC, a mode of the JSON
 * reader: what each addition reads as, what stays refused, that JSON
 * itself is no looser for it, and that places stay exact.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <string.h>

#include "cf_test.h"

#if CF_WITH_JSON

/* A configuration from one file, with no definitions: JSON5 is typed. */
static cf_config* load_as(const char* path, const char* text, cf_error* err) {
    cf_builder* b = cf_build(NULL);
    if (!spill(path, text)) {
        cf_builder_abort(b);
        return NULL;
    }
    cf_builder_file(b, "file", CF_RANK_USER_FILE, path, NULL, 0);
    return cf_builder_create(b, err);
}

static cf_config* load(const char* text, cf_error* err) {
    return load_as("cf_json5.json5", text, err);
}

static const char g_doc[] =
    "// JSON5: everything JSON has, and what people write by hand\n"
    "{\n"
    "  server: {\n"
    "    host: 'localhost',          // single quotes\n"
    "    port: 0x1F90,               /* hexadecimal */\n"
    "    tags: ['a', \"b\",],          // a trailing comma\n"
    "  },\n"
    "  'quoted': \"it's\",\n"
    "  under_1: +1,\n"
    "  counts: [+1, -2],\n"
    "  zero: 0,\n"
    "  line: 'one \\\n"
    "two',\n"
    "  esc: '\\x41\\u00e9\\'\\q',\n"
    "  \\u0061b: true,\n"
    "  nothing: null,\n"
    "}\n";

static void test_reads(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_config*  c   = load(g_doc, &err);
    static char ex[4096];

    check(c != NULL, "the document reads");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check_text(cf_get_text(c, "server.host", ""), "localhost",
               "a single-quoted string");
    check(cf_get_int(c, "server.port", 0) == 8080, "a hexadecimal integer");
    check(cf_array_count(c, "server.tags") == 2, "an array with a trailing comma");
    check_text(cf_get_text(c, "quoted", ""), "it's", "a quoted name and a string");
    check(cf_get_int(c, "under_1", 0) == 1, "an identifier name and a plus sign");
    check(cf_get_int(c, "counts.1", 0) == -2 && cf_get_int(c, "zero", 1) == 0,
          "signed and zero integers");
    check_text(cf_get_text(c, "line", ""), "one two",
               "a line continuation holds nothing");
    check_text(cf_get_text(c, "esc", ""), "A\xc3\xa9'q",
               "\\x, \\u, \\' and a character escaping itself");
    check(cf_get_bool(c, "ab", CF_FALSE), "an escape in an identifier name");
    check(cf_has(c, "nothing"), "null");

    /* Lines are counted through comments and continuations alike. */
    cf_explain(c, "server.port", ex, sizeof ex);
    has(ex, "cf_json5.json5:5:", "explain() names the value's line");
    cf_explain(c, "esc", ex, sizeof ex);
    has(ex, "cf_json5.json5:14:", "and a line after a continuation");
    cf_config_free(c);
}

/* JSONC is JSON5's comments and trailing commas: .jsonc chooses it. */
static void test_jsonc(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = load_as("cf_json5.jsonc",
                             "{\n  // tsconfig.json, say\n  \"compiler\": {\n"
                             "    \"strict\": true, /* why */\n  },\n}\n",
                             &err);

    check(c != NULL, "a .jsonc file reads");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(cf_get_bool(c, "compiler.strict", CF_FALSE), "with its comments and commas");
    cf_config_free(c);
    remove("cf_json5.jsonc");
}

/* A .json file is JSON: none of this is allowed there. */
static void test_json_stays_json(void) {
    static const char* const loose[] = {
        "{\"a\": 1, // no\n}", "{\"a\": 1,}",       "{a: 1}",
        "{\"a\": 'x'}",        "{\"a\": 0x10}",     "{\"a\": .5}",
        "{\"a\": +1}",         "{\"a\": Infinity}", "{\"a\": \"\\x41\"}"};
    size_t i;

    for (i = 0; i < sizeof loose / sizeof loose[0]; ++i) {
        cf_error   err = CF_ERROR_INIT;
        cf_config* c   = load_as("cf_json5.json", loose[i], &err);
        check(c == NULL, loose[i]);
        cf_config_free(c);
    }
    remove("cf_json5.json");
}

/* What JSON5 still refuses, each by its reason. */
static void test_refuses(void) {
    static const char* const bad[][2] = {
        {"{a: 01}", "leading zero"},
        {"{a: '\\1'}", "may not be escaped"},
        {"{a: '\\01'}", "followed by a digit"},
        {"{a: 1} /* open", "not closed"},
        {"{a: 'x\ny'}", "raw newline"},
        {"{1a: 1}", "expected a member name"},
        {"{a: 1,,}", "expected a member name"},
        {"{a: 0x}", "no hexadecimal digit"},
        {"{a: '\\x4'}", "two hexadecimal digits"},
        {"[1]", "must be an object"},
        {"{a: .}", "malformed number"},
        {"{a: 1e}", "exponent"},
        {"{\\u0031: 1}", "a name may hold"},
    };
    size_t i;

    for (i = 0; i < sizeof bad / sizeof bad[0]; ++i) {
        cf_error   err = CF_ERROR_INIT;
        cf_config* c   = load(bad[i][0], &err);
        check(c == NULL, bad[i][0]);
        if (c == NULL) {
            has(err.message, bad[i][1], bad[i][0]);
        }
        cf_config_free(c);
    }
}

#if CF_WITH_REAL
static void test_points(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = load("{ratio: .5, whole: 5., e: 5.e1, neg: -.25}", &err);

    check(c != NULL, "numbers with a point at either end read");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(cf_get_real(c, "ratio", 0.0) == 0.5, "a leading point");
    check(cf_get_real(c, "whole", 0.0) == 5.0, "a trailing point");
    check(cf_get_real(c, "e", 0.0) == 50.0, "a trailing point and an exponent");
    check(cf_get_real(c, "neg", 0.0) == -0.25, "a sign and a leading point");
    cf_config_free(c);
}
#endif

static void test_numbers(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = load("{neg: -0xFF, low: -0x8000000000000000, up: 0XaB}", &err);

    check(c != NULL, "hexadecimal integers read");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(cf_get_int(c, "neg", 0) == -255, "with a sign");
    check(cf_get_int(c, "low", 0) == CF_I64_MIN, "down to the smallest integer");
    check(cf_get_int(c, "up", 0) == 171, "in either case");
    cf_config_free(c);

    /* Past the integers: real, as a decimal one is - or, in a build
       without floating point, refused by name. */
    c = load("{big: 0x10000000000000000}", &err);
#if CF_WITH_REAL
    check(c != NULL && cf_get_real(c, "big", 0.0) == 18446744073709551616.0,
          "a hexadecimal integer past 64 bits is real");
#else
    check(c == NULL, "a hexadecimal integer past 64 bits needs floating point");
    if (c == NULL) {
        has(err.message, "no floating point", "and the refusal says so");
    }
#endif
    cf_config_free(c);
}

/* Infinity and NaN are their text, as TOML's inf and nan are: the
   library holds no value that is not finite. */
static void test_nonfinite(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = load("{a: Infinity, b: -Infinity, c: NaN}", &err);

    check(c != NULL, "Infinity and NaN read");
    if (c != NULL) {
        check_text(cf_get_text(c, "b", ""), "-Infinity", "as their text");
        check_text(cf_get_text(c, "c", ""), "NaN", "both");
    }
    cf_config_free(c);
}

#if CF_WITH_REAL
static void test_nonfinite_against_real(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_config*  c;
    cf_builder* b;
    cf_def      d;

    if (!spill("cf_json5.json5", "{ratio: Infinity}")) {
        return;
    }
    b = cf_build(NULL);
    cf_def_init(&d, CF_TYPE_REAL, "ratio");
    check(cf_builder_define(b, &d), "ratio is defined as real");
    cf_builder_file(b, "file", CF_RANK_USER_FILE, "cf_json5.json5", NULL, 0);
    c = cf_builder_create(b, &err);
    check(c == NULL || cf_get_real(c, "ratio", 0.5) == 0.5,
          "Infinity does not become a real");
    cf_config_free(c);
}
#endif

/* JSON5's white space past ASCII, and a lone carriage return ending a
   line, for the places explain() gives. */
static void test_space(void) {
    cf_error    err = CF_ERROR_INIT;
    static char ex[1024];
    cf_config*  c = load("\xef\xbb\xbf{\xc2\xa0\xe2\x80\x83"
                         "a: 1,\r"
                         "\xe2\x80\xa8"
                         "b: 2\xe3\x80\x80}",
                         &err);

    check(c != NULL, "the byte order mark and Unicode spaces are white space");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(cf_get_int(c, "b", 0) == 2, "and the values between them read");
    cf_explain(c, "b", ex, sizeof ex);
    has(ex,
        "cf_json5.json5:3:", "a carriage return and a line separator each end a line");
    cf_config_free(c);
}

#if CF_WITH_EMIT
/* Written as JSON, which reads back through JSON5 as the same thing. */
static void test_round_trip(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_config*  a   = load(g_doc, &err);
    cf_config*  b;
    static char out[8192];
    size_t      n;

    if (a == NULL) {
        check(0, "round trip: the original builds");
        return;
    }
    n = cf_emit(a, "json5", CF_EMIT_ALL, 0, out, sizeof out, &err);
    check(n > 0 && n < sizeof out, "the configuration is written through json5");
    if (n == 0) {
        printf("  %s\n", err.message);
        cf_config_free(a);
        return;
    }
    b = load(out, &err);
    check(b != NULL && cf_digest(a) == cf_digest(b), "and reads back the same");
    cf_config_free(b);
    b = load_as("cf_json5.json", out, &err);
    check(b != NULL, "what it writes is JSON");
    cf_config_free(a);
    cf_config_free(b);
    remove("cf_json5.json");
}
#endif

#endif /* CF_WITH_JSON */

int main(void) {
#if CF_WITH_JSON
    test_reads();
    test_jsonc();
    test_json_stays_json();
    test_refuses();
    test_numbers();
#if CF_WITH_REAL
    test_points();
#endif
    test_nonfinite();
#if CF_WITH_REAL
    test_nonfinite_against_real();
#endif
    test_space();
#if CF_WITH_EMIT
    test_round_trip();
#endif
    remove("cf_json5.json5");
#else
    printf("skipped: this build has no JSON (CF_WITH_JSON=OFF)\n");
#endif
    return cf_test_done();
}
