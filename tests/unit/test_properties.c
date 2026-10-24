/*
 * test_properties.c - Java .properties: the separators, comments,
 * continuations and escapes java.util.Properties reads, the keys it
 * refuses, and the build that left it out.
 */

#include <configforge/configforge.h>

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

static cf_bool spill(const char* path, const char* text) {
    FILE* f = fopen(path, "wb");
    if (f == NULL) {
        return CF_FALSE;
    }
    fputs(text, f);
    fclose(f);
    return CF_TRUE;
}

#define FILE_NAME "cf_props.properties"

static cf_config* load(const char* text, cf_error* err) {
    cf_builder* b = cf_build(NULL);
    if (!spill(FILE_NAME, text)) {
        cf_builder_abort(b);
        return NULL;
    }
    cf_builder_file(b, "file", CF_RANK_USER_FILE, FILE_NAME, NULL, 0);
    return cf_builder_create(b, err);
}

#if CF_WITH_PROPERTIES

static const char g_doc[] = "# a comment\n"
                            "! and another\n"
                            "\n"
                            "server.port = 8080\n"
                            "server.host:0.0.0.0\n"
                            "greeting hello world\n"
                            "path=C:\\\\data\\\\app\n"
                            "tab=a\\tb\n"
                            "long = first, \\\n"
                            "       second\n"
                            "emoji=\\ud83d\\ude00\n"
                            "empty=\n"
                            "   indented.key = yes\r\n"
                            "server.port = 9090\n";

static void test_reads(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_config*  c   = load(g_doc, &err);
    static char ex[1024];

    check(c != NULL, "a .properties file builds");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(cf_get_int(c, "server.port", 0) == 9090,
          "'=' separates, and a later line wins within the file");
    check(strcmp(cf_get_text(c, "server.host", ""), "0.0.0.0") == 0, "so does ':'");
    check(strcmp(cf_get_text(c, "greeting", ""), "hello world") == 0,
          "and whitespace; the value runs to the end of the line");
    check(strcmp(cf_get_text(c, "path", ""), "C:\\data\\app") == 0,
          "\\\\ is a backslash");
    check(strcmp(cf_get_text(c, "tab", ""), "a\tb") == 0, "\\t is a tab");
    check(strcmp(cf_get_text(c, "long", ""), "first, second") == 0,
          "a continuation joins the lines without the next line's indentation");
    check(strcmp(cf_get_text(c, "emoji", ""), "\xf0\x9f\x98\x80") == 0,
          "a surrogate pair in \\u escapes is one character");
    check(cf_has(c, "empty") && strcmp(cf_get_text(c, "empty", "x"), "") == 0,
          "an empty value");
    check(strcmp(cf_get_text(c, "indented.key", ""), "yes") == 0,
          "leading whitespace and a CRLF line end");

    cf_explain(c, "server.host", ex, sizeof ex);
    has(ex, FILE_NAME ":5:1", "explain names the line");
    cf_config_free(c);
}

static void refused(const char* text, const char* why, const char* what) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = load(text, &err);
    check(c == NULL, what);
    if (c == NULL) {
        has(err.message, why, what);
    }
    cf_config_free(c);
}

static void test_refuses(void) {
    refused("server.Port=1\n", "is not a valid key",
            "a key the library cannot hold - Spring's camelCase - is refused, not "
            "guessed at");
    refused("caf\\u00e9.name=x\n", "is not a valid key", "so is one it cannot spell");
    refused("colon\\:key=1\n", "'colon:key' is not a valid key",
            "an escaped separator is part of the key, and named when it cannot be one");
    refused("a=\\u12\n", "four hexadecimal digits", "a short \\u escape");
    refused("a=\\ud83d\n", "half a surrogate pair", "half a surrogate pair");
    refused("=value\n", "empty key", "a line with no key");
}

static void test_typed_by_contract(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  c;

    check(spill(FILE_NAME, "port=8080\nname=007\n"), "wrote it");
    cf_builder_file(b, "file", CF_RANK_USER_FILE, FILE_NAME, NULL, 0);
    cf_builder_int(b, "port", 1, 1, 65535, 0);
    cf_builder_text(b, "name", "", 0);
    c = cf_builder_create(b, &err);
    check(c != NULL && cf_get_int(c, "port", 0) == 8080, "a defined int");
    check(c != NULL && strcmp(cf_get_text(c, "name", ""), "007") == 0,
          "and a defined text keeps its leading zeros");
    cf_config_free(c);
}

static void test_empty(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = load("# nothing\n", &err);
    check(c != NULL && cf_count(c) == 0,
          "a file of comments is an empty configuration");
    cf_config_free(c);
}

#else

static void test_disabled(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = load("a=1\n", &err);
    check(c == NULL, "a .properties file is refused without CF_WITH_PROPERTIES");
    has(err.message, "CF_WITH_PROPERTIES", "and the error names the option");
    cf_config_free(c);
}

#endif /* CF_WITH_PROPERTIES */

int main(void) {
#if CF_WITH_PROPERTIES
    test_reads();
    test_refuses();
    test_typed_by_contract();
    test_empty();
#else
    test_disabled();
#endif
    remove(FILE_NAME);
    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
