/*
 * test_yaml.c - YAML through libyaml (D-39): what it reads, what it
 * refuses by name, and that what it writes reads back as the same
 * configuration.
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

#if CF_WITH_YAML

/* A configuration from one YAML file, with no definitions. */
static cf_config* load(const char* text, cf_error* err) {
    cf_builder* b = cf_build(NULL);
    if (!spill("cf_yaml.yaml", text)) {
        return NULL;
    }
    cf_builder_file(b, "file", CF_RANK_USER_FILE, "cf_yaml.yaml", NULL, 0);
    return cf_builder_create(b, err);
}

static const char g_doc[] = "# a comment\n"
                            "title: ConfigForge\n"
                            "server:\n"
                            "  host: \"0.0.0.0\"\n"
                            "  port: 8080\n"
                            "  tls: {enabled: true, cert: 'c\xc3\xa9rt.pem'}\n"
                            "  tags: [a, \"b\"]\n"
                            "country: no\n"
                            "version: \"3.10\"\n"
                            "nothing: ~\n"
                            "empty:\n"
                            "upstream:\n"
                            "  - host: a\n"
                            "  - host: b\n"
                            "    weight: 2\n"
                            "motd: |\n"
                            "  two\n"
                            "  lines\n";

static void test_reads(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_config*  c   = load(g_doc, &err);
    static char ex[4096];

    check(c != NULL, "a YAML document builds");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(strcmp(cf_get_text(c, "title", ""), "ConfigForge") == 0, "a plain scalar");
    check(cf_get_int(c, "server.port", 0) == 8080, "a number, by Tier 0 inference");
    check(strcmp(cf_get_text(c, "server.host", ""), "0.0.0.0") == 0, "a quoted string");
    check(cf_get_bool(c, "server.tls.enabled", CF_FALSE), "a flow mapping");
    check(strcmp(cf_get_text(c, "server.tls.cert", ""), "c\xc3\xa9rt.pem") == 0,
          "with UTF-8 in it");
    check(cf_array_count(c, "server.tags") == 2, "a flow sequence");
    check(strcmp(cf_get_text(c, "server.tags.1", ""), "b") == 0,
          "its elements by index");
    check(cf_array_count(c, "upstream") == 2, "a block sequence of mappings");
    check(cf_get_int(c, "upstream.1.weight", 0) == 2, "and the keys inside them");
    check(strcmp(cf_get_text(c, "motd", ""), "two\nlines\n") == 0, "a literal block");

    /* The YAML problem, which the library's one spelling of a boolean
       (D-24) answers for every version of YAML at once. */
    check(cf_type_of(c, "country") == CF_TYPE_TEXT, "no is the text \"no\", not false");
    check(strcmp(cf_get_text(c, "version", ""), "3.10") == 0,
          "and a quoted 3.10 keeps its zero");
    check(cf_type_of(c, "nothing") == CF_TYPE_NULL, "~ is null");
    check(cf_type_of(c, "empty") == CF_TYPE_NULL, "and so is nothing at all");

    cf_explain(c, "server.port", ex, sizeof ex);
    has(ex, "cf_yaml.yaml:5:9", "explain() names the line and column");
    cf_config_free(c);
}

/* With a contract, the definition types a plain scalar - and a quoted
   one says it is text, which a number definition refuses. */
static void test_types(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b;
    cf_config*  c;

    check(spill("cf_yaml.yaml", "port: 9000\nname: 42\n"), "wrote a file");
    b = cf_build(NULL);
    cf_builder_file(b, "file", CF_RANK_USER_FILE, "cf_yaml.yaml", NULL, 0);
    cf_builder_int(b, "port", 1, 1, 65535, 0);
    cf_builder_text(b, "name", "x", 0);
    c = cf_builder_create(b, &err);
    check(c != NULL && cf_get_int(c, "port", 0) == 9000,
          "a plain scalar becomes an int");
    check(c != NULL && strcmp(cf_get_text(c, "name", ""), "42") == 0,
          "or text, as the definition says");
    cf_config_free(c);

    check(spill("cf_yaml.yaml", "port: \"9000\"\n"), "wrote a quoted number");
    b = cf_build(NULL);
    cf_builder_file(b, "file", CF_RANK_USER_FILE, "cf_yaml.yaml", NULL, 0);
    cf_builder_int(b, "port", 1, 1, 65535, 0);
    c = cf_builder_create(b, &err);
    check(c == NULL, "a quoted number is text, and an int definition refuses it");
    cf_config_free(c);
}

static void refused(const char* text, const char* needle, const char* what) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = load(text, &err);
    check(c == NULL, what);
    has(err.message, needle, what);
    cf_config_free(c);
}

/* Anchors and aliases (D-63): the alias is the anchored node again, and
   each value it holds explains itself at the place it was written. */
static void test_anchors(void) {
    cf_error    err = CF_ERROR_INIT;
    static char ex[1024];
    cf_config*  c = load("base: &b\n"
                         "  host: a\n"
                         "  ports: [1, 2]\n"
                         "other: *b\n"
                         "name: &n plain\n"
                         "again: *n\n"
                         "list: [*n, *n]\n",
                         &err);

    check(c != NULL, "anchors and aliases read");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(strcmp(cf_get_text(c, "other.host", ""), "a") == 0, "an aliased mapping");
    check(cf_array_count(c, "other.ports") == 2 &&
              cf_get_int(c, "other.ports.1", 0) == 2,
          "with its sequence");
    check(strcmp(cf_get_text(c, "again", ""), "plain") == 0, "an aliased scalar");
    check(cf_array_count(c, "list") == 2, "aliases as elements");
    cf_explain(c, "other.host", ex, sizeof ex);
    has(ex,
        "cf_yaml.yaml:2:", "and an aliased value is explained where it was written");
    cf_config_free(c);
}

/* The core schema's tags type a value as YAML 1.2 says. */
static void test_tags(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = load("port: !!int \"80\"\n"
                          "zip: !!str 01234\n"
                          "on: !!bool true\n"
                          "none: !!null ~\n"
                          "t: !!map {a: 1}\n"
                          "plain: ! 12\n",
                          &err);

    check(c != NULL, "core tags read");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(cf_type_of(c, "port") == CF_TYPE_INT && cf_get_int(c, "port", 0) == 80,
          "!!int on a quoted scalar is an int");
    check(cf_type_of(c, "zip") == CF_TYPE_TEXT &&
              strcmp(cf_get_text(c, "zip", ""), "01234") == 0,
          "!!str keeps the zero");
    check(cf_get_bool(c, "on", CF_FALSE), "!!bool");
    check(cf_type_of(c, "none") == CF_TYPE_NULL, "!!null");
    check(cf_get_int(c, "t.a", 0) == 1, "!!map");
    check(cf_type_of(c, "plain") == CF_TYPE_TEXT, "the non-specific ! is text");
    cf_config_free(c);
}

static void test_refuses(void) {
    refused("other: *b\n", "not defined before it", "an alias of no anchor");
    refused("a: &a [1, *a]\n", "inside the node", "an alias inside its own anchor");
    refused("a: &a [x, x, x, x, x, x, x, x]\n"
            "b: &b [*a, *a, *a, *a, *a, *a, *a, *a]\n"
            "c: &c [*b, *b, *b, *b, *b, *b, *b, *b]\n"
            "d: &d [*c, *c, *c, *c, *c, *c, *c, *c]\n"
            "e: &e [*d, *d, *d, *d, *d, *d, *d, *d]\n"
            "f: &f [*e, *e, *e, *e, *e, *e, *e, *e]\n",
            "far more than the document", "a billion laughs, refused in linear time");
    refused("port: !!int eighty\n", "not a !!int", "a core tag on the wrong value");
    refused("port: !custom 80\n", "not one this library resolves",
            "a tag of the writing application's");
    refused("a: 1\n---\nb: 2\n", "a second document", "a second document is refused");
    refused("- a\n- b\n", "a mapping at the top", "a top-level sequence is refused");
    refused("just text\n", "a mapping at the top", "a top-level scalar is refused");
    refused("? [a, b]\n: 1\n", "a key has to be a scalar", "a complex key is refused");
    refused("a: [1, 2\n", "cf_yaml.yaml:", "a syntax error names the place");
    refused("Bad Key: 1\n", "not a valid key", "a key the library cannot hold");
}

static void test_empty(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = load("# nothing but a comment\n", &err);
    check(c != NULL && cf_count(c) == 0, "an empty file is an empty configuration");
    cf_config_free(c);
}

#if CF_WITH_EMIT
/* What YAML writes, YAML reads, and it is the same configuration. */
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
    n = cf_emit(a, "yaml", CF_EMIT_ALL, 0, out, sizeof out, &err);
    check(n > 0 && n < sizeof out, "the configuration is written as YAML");
    has(out, "country: \"no\"\n", "a string that looks like a word is quoted");
    has(out, "  port: 8080\n", "a number is not");
    cf_config_free(a);
    if (n == 0) {
        printf("  %s\n", err.message);
        return;
    }
    a = load(g_doc, &err);
    b = load(out, &err);
    check(b != NULL, "and what was written parses");
    if (b == NULL) {
        printf("  %s\n--- written:\n%s\n", err.message, out);
        cf_config_free(a);
        return;
    }
    check(a != NULL && cf_digest(a) == cf_digest(b), "as the same configuration");
    if (a != NULL && cf_digest(a) != cf_digest(b)) {
        static char ia[8192], ib[8192];
        cf_inspect(a, ia, sizeof ia);
        cf_inspect(b, ib, sizeof ib);
        printf("--- written:\n%s\n--- original:\n%s\n--- read back:\n%s\n", out, ia,
               ib);
    }
    cf_config_free(a);
    cf_config_free(b);
}
#endif

#else /* !CF_WITH_YAML */

/* Without it, a .yaml file is not read as nothing: the build says which
   option it needs. */
static void test_disabled(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  c;

    check(spill("cf_yaml.yaml", "port: 1\n"), "wrote a YAML file");
    cf_builder_file(b, "file", CF_RANK_USER_FILE, "cf_yaml.yaml", NULL, 0);
    c = cf_builder_create(b, &err);
    check(c == NULL, "a YAML file is refused without CF_WITH_YAML");
    has(err.message, "CF_WITH_YAML", "and the error names the option");
    cf_config_free(c);
}

#endif /* CF_WITH_YAML */

int main(void) {
#if CF_WITH_YAML
    test_reads();
    test_types();
    test_anchors();
    test_tags();
    test_refuses();
    test_empty();
#if CF_WITH_EMIT
    test_round_trip();
#endif
#else
    test_disabled();
#endif
    remove("cf_yaml.yaml");
    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
