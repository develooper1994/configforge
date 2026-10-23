/*
 * test_toml.c - the TOML subset (D-23): what it reads, what it refuses,
 * and that what it writes reads back as the same configuration.
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

#if CF_WITH_TOML

static cf_bool spill(const char* path, const char* text) {
    FILE* f = fopen(path, "wb");
    if (f == NULL) {
        return CF_FALSE;
    }
    fputs(text, f);
    fclose(f);
    return CF_TRUE;
}

/* A configuration from one TOML file, with no definitions: TOML is
   typed, so the types come from the file. */
static cf_config* load(const char* text, cf_error* err) {
    cf_builder* b = cf_build(NULL);
    if (!spill("cf_toml.toml", text)) {
        return NULL;
    }
    cf_builder_file(b, "file", CF_RANK_USER_FILE, "cf_toml.toml", NULL, 0);
    return cf_builder_create(b, err);
}

static const char g_doc[] = "# a comment, and blank lines\n"
                            "\n"
                            "title = \"ConfigForge\"\n"
                            "\n"
                            "[server]\n"
                            "host = '0.0.0.0'          # a literal string\n"
                            "port = 8_080\n"
                            "tls = { enabled = true, cert = \"c\\u00e9rt.pem\" }\n"
                            "\n"
                            "[server.limits]\n"
                            "masks = [0x1F, 0o17, 0b101]\n"
                            "ports = [\n"
                            "    80,\n"
                            "    443,  # trailing comma below\n"
                            "]\n"
                            "db.pool.size = 4\n"
                            "\n"
                            "[[upstream]]\n"
                            "host = \"a\"\n"
                            "\n"
                            "[[upstream]]\n"
                            "host = \"b\"\n"
                            "weight = 2\n"
                            "\n"
                            "[upstream.tls]\n"
                            "enabled = false\n";

static void test_reads(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_config*  c   = load(g_doc, &err);
    static char ex[4096];

    check(c != NULL, "the subset parses");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(strcmp(cf_get_text(c, "title", ""), "ConfigForge") == 0,
          "a top-level string");
    check(strcmp(cf_get_text(c, "server.host", ""), "0.0.0.0") == 0,
          "a literal string");
    check(cf_get_int(c, "server.port", 0) == 8080, "an integer with an underscore");
    check(cf_get_bool(c, "server.tls.enabled", CF_FALSE), "an inline table's boolean");
    check(strcmp(cf_get_text(c, "server.tls.cert", ""), "c\xc3\xa9rt.pem") == 0,
          "a \\u escape, as UTF-8");
    check(cf_array_count(c, "server.limits.masks") == 3, "an array of three");
    check(cf_get_int(c, "server.limits.masks.0", 0) == 31, "hexadecimal");
    check(cf_get_int(c, "server.limits.masks.1", 0) == 15, "octal");
    check(cf_get_int(c, "server.limits.masks.2", 0) == 5, "binary");
    check(cf_array_count(c, "server.limits.ports") == 2,
          "an array over several lines, with a trailing comma");
    check(cf_get_int(c, "server.limits.db.pool.size", 0) == 4, "a dotted key");
    check(cf_array_count(c, "upstream") == 2, "an array of tables");
    check(strcmp(cf_get_text(c, "upstream.1.host", ""), "b") == 0,
          "whose second element has its own keys");
    check(cf_get_int(c, "upstream.1.weight", 0) == 2, "all of them");
    check(cf_has(c, "upstream.1.tls.enabled") &&
              !cf_get_bool(c, "upstream.1.tls.enabled", 1),
          "and a sub-table nested in the element, as TOML means it");
    check(!cf_has(c, "upstream.0.tls.enabled"), "and not in the element before");

    /* The point of the library: TOML values explain themselves with a
       line and column like any other. */
    cf_explain(c, "server.port", ex, sizeof ex);
    has(ex, "cf_toml.toml:7:", "explain() names the line the value is on");
    cf_config_free(c);
}

#if CF_WITH_REAL
static void test_floats(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = load("ratio = 0.75\nbig = 1e3\nsmall = -2.5E-3\n", &err);

    check(c != NULL, "floats parse");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(cf_get_real(c, "ratio", 0.0) == 0.75, "a fraction");
    check(cf_get_real(c, "big", 0.0) == 1000.0, "an exponent");
    check(cf_get_real(c, "small", 0.0) == -0.0025, "a negative exponent");
    cf_config_free(c);
}
#endif

/* Each refusal names the construct and what to write instead, with the
   place, so the reader is not left guessing. */
static void refused(const char* text, const char* needle, const char* what) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = load(text, &err);
    check(c == NULL, what);
    has(err.message, needle, what);
    has(err.message, "cf_toml.toml:", "and the place");
    cf_config_free(c);
}

static void test_refuses(void) {
    refused("when = 1979-05-27\n", "dates and times", "a date is refused");
    refused("text = \"\"\"\nlong\n\"\"\"\n", "multi-line strings",
            "a multi-line string");
    refused("x = inf\n", "inf and nan", "inf");
    refused("x = \"open\n", "not closed", "an unclosed string");
    refused("x = 1 2\n", "end of the line", "trailing text after a value");
    refused("[[a]]\nk = 1\n[b]\n[[a]]\nk = 2\n", "reopened",
            "an array of tables reopened after another table");
    refused("x = \"\\q\"\n", "unknown escape", "an unknown escape");
    refused("x = 0x\n", "expected a value", "a prefix with no digits");
}

/* A key set twice is not an error here: the later wins, and Doctor's
   CFD001 is where the earlier one is reported. Stated in toml.c. */
static void test_later_wins(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = load("port = 1\nport = 2\n", &err);

    check(c != NULL, "a key set twice builds");
    if (c != NULL) {
        check(cf_get_int(c, "port", 0) == 2, "and the later line wins");
        cf_config_free(c);
    }
}

/* What TOML writes, TOML reads, and it is the same configuration. */
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
    n = cf_emit(a, "toml", CF_EMIT_ALL, 0, out, sizeof out, &err);
    check(n > 0 && n < sizeof out, "the configuration is written as TOML");
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
        /* Say what differs, not only that something does. */
        static char ia[8192], ib[8192];
        cf_inspect(a, ia, sizeof ia);
        cf_inspect(b, ib, sizeof ib);
        printf("--- written:\n%s\n--- original:\n%s\n--- read back:\n%s\n", out, ia,
               ib);
    }
    cf_config_free(a);
    cf_config_free(b);
}

#endif /* CF_WITH_TOML */

int main(void) {
#if CF_WITH_TOML
    test_reads();
#if CF_WITH_REAL
    test_floats();
#endif
    test_refuses();
    test_later_wins();
    test_round_trip();
    remove("cf_toml.toml");
#else
    printf("skipped: this build has no TOML (CF_WITH_TOML=OFF)\n");
#endif
    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
