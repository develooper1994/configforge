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
#else
/* A build without floating point refuses a float by name rather than
   reading it as something else - and still compiles a TOML reader. */
static void test_floats(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = load("ratio = 0.75\n", &err);

    check(c == NULL, "a float is refused without CF_WITH_REAL");
    has(err.message, "no floating point", "and the reason is given");
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
    refused("when = 1979-02-29\n", "out of range", "a February 29th of no leap year");
    refused("when = 1979-13-01\n", "out of range", "a thirteenth month");
    refused("when = 07:32\n", "with seconds", "a time without seconds");
    refused("when = 1979-05-27T25:00:00\n", "time is malformed", "a 25th hour");
    refused("when = 1979-05-27T07:32:00+24:00\n", "out of range",
            "an offset of 24 hours");
    refused("\"\"\"k\"\"\" = 1\n", "a key may not be a multi-line string",
            "a multi-line key");
    refused("x = \"\"\"a\"\"\"\"\"\"\n", "more than two quotes", "six quotes to close");
    refused("x = \"\"\"a\\ b\"\"\"\n", "unknown escape",
            "a backslash and space not ending a line");
    refused("x = \"open\n", "not closed", "an unclosed string");
    refused("x = 1 2\n", "end of the line", "trailing text after a value");
    refused("[[a]]\nk = 1\n[b]\n[[a]]\nk = 2\n", "reopened",
            "an array of tables reopened after another table");
    refused("x = \"\\q\"\n", "unknown escape", "an unknown escape");
    refused("x = 0x\n", "malformed integer", "a prefix with no digits");
    /* The number grammar of TOML 1.0, which toml-test found loose. */
    refused("x = 01\n", "leading zero", "a leading zero");
    refused("x = 1.\n", "after the point", "a point with no digit after it");
    refused("x = 1e\n", "exponent", "an exponent with no digit");
    refused("x = 1__0\n", "malformed", "two underscores");
    refused("x = 1_\n", "malformed", "a trailing underscore");
    refused("x = +0x1\n", "no sign", "a sign on a hexadecimal integer");
    refused("x = 1x\n", "malformed", "a letter after the digits");
    /* What TOML 1.0 says a document is made of. */
    refused("x = 1 # a\x01"
            "b\n",
            "control character", "a control in a comment");
    refused("x = 1 # \x7f\n", "control character", "DEL in a comment");
    refused("x = \"a\x7f\"\n", "control character", "DEL in a string");
    refused("x = 1 # \xc3\n", "not valid UTF-8", "a cut sequence in a comment");
    refused("x = \"\xed\xa0\x80\"\n", "not valid UTF-8", "a surrogate in a string");
    refused("x = \"\xc0\xaf\"\n", "not valid UTF-8", "an overlong form");
    refused("x = 1\ry = 2\n", "end of the line", "a carriage return alone");
}

/* TOML's values that have no type of their own here: dates and times,
   and inf and nan, are their text, for a definition to type; multi-line
   strings are strings, and the lines after them keep their numbers. */
static void test_values(void) {
    cf_error    err = CF_ERROR_INIT;
    static char ex[1024];
    cf_config*  c = load("odt = 1979-05-27T07:32:00.999-07:00\n"
                         "ldt = 1979-05-27 07:32:00\n"
                         "ld = 2000-02-29\n"
                         "lt = 00:32:00.5\n"
                         "big = -inf\n"
                         "odd = nan\n"
                         "poem = \"\"\"\nRoses\r\nare \"red\"\"\"\"\n"
                         "trim = \"\"\"a \\\n     b\"\"\"\n"
                         "raw = '''\nC:\\x\\'''\n"
                         "after = 1\n",
                         &err);

    check(c != NULL, "dates, times, inf, nan and multi-line strings read");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(strcmp(cf_get_text(c, "odt", ""), "1979-05-27T07:32:00.999-07:00") == 0,
          "an offset date-time, as written");
    check(strcmp(cf_get_text(c, "ldt", ""), "1979-05-27 07:32:00") == 0,
          "a local date-time with a space");
    check(strcmp(cf_get_text(c, "ld", ""), "2000-02-29") == 0, "a leap day");
    check(strcmp(cf_get_text(c, "lt", ""), "00:32:00.5") == 0, "a local time");
    check(strcmp(cf_get_text(c, "big", ""), "-inf") == 0, "-inf, as its text");
    check(strcmp(cf_get_text(c, "odd", ""), "nan") == 0, "nan, as its text");
    check(strcmp(cf_get_text(c, "poem", ""), "Roses\nare \"red\"") == 0,
          "the first line break dropped, CRLF as LF, quotes before the end kept");
    check(strcmp(cf_get_text(c, "trim", ""), "a b") == 0,
          "a backslash ending a line takes the space after it");
    check(strcmp(cf_get_text(c, "raw", ""), "C:\\x\\") == 0,
          "a literal multi-line string");
    cf_explain(c, "after", ex, sizeof ex);
    has(ex, "cf_toml.toml:14:", "and the line after them is counted right");
    cf_config_free(c);
}

#if CF_WITH_REAL
/* inf read as its text meets a definition that says real: the library
   keeps reals finite, so the definition refuses it, by the key's name. */
static void test_inf_against_real(void) {
    cf_builder* b;
    cf_config*  c;
    cf_def      d;
    cf_error    err = CF_ERROR_INIT;

    if (!spill("cf_toml.toml", "ratio = inf\n")) {
        return;
    }
    b = cf_build(NULL);
    cf_def_init(&d, CF_TYPE_REAL, "ratio");
    check(cf_builder_define(b, &d), "ratio is defined as real");
    cf_builder_file(b, "file", CF_RANK_USER_FILE, "cf_toml.toml", NULL, 0);
    c = cf_builder_create(b, &err);
    check(c == NULL || cf_get_real(c, "ratio", 0.5) == 0.5,
          "inf does not become a real");
    if (c == NULL) {
        has(err.message, "ratio", "and the refusal names the key");
    }
    cf_config_free(c);
}
#endif

/* CRLF line endings, and UTF-8 that is well formed, read as they are. */
static void test_line_endings(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = load(
        "# caf\xc3\xa9\r\n[s]\r\nname = \"\xe2\x82\xac\" # \xf0\x9f\x98\x80\r\n", &err);

    check(c != NULL, "CRLF and UTF-8 read");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(strcmp(cf_get_text(c, "s.name", ""), "\xe2\x82\xac") == 0,
          "and the value is the bytes written");
    cf_config_free(c);
}

/* TOML defines a key, and a table, once (toml.io/en/v1.0.0): each way of
   defining one twice that toml-test found accepted, and the ways that
   look alike and are allowed. */
static void test_defined_once(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c;

    refused("port = 1\nport = 2\n", "'port' is already defined", "a key set twice");
    refused("a = 1\n\"a\" = 2\n", "already defined", "the same key, quoted");
    refused("[a]\nb = 1\n[a]\nc = 2\n", "defined twice", "a table with two headers");
    refused("[a]\nb = 1\n[a.b]\n", "is a value", "a header over a value");
    refused("a = false\na.b = true\n", "is a value", "a dotted key through a value");
    refused("a = {b = 1}\na.c = 2\n", "inline table", "a key added to an inline table");
    refused("a = {}\n[a.b]\n", "inline table", "a header added to an inline table");
    refused("a = {b = 1, b = 2}\n", "already defined",
            "a key twice in an inline table");
    refused("a = [{b = 1, b = 2}]\n", "already defined",
            "a key twice in a table in an array");
    refused("[fruit]\napple.color = 1\n[fruit.apple]\n", "dotted keys",
            "a header over what dotted keys defined");
    refused("[a.b.c]\nz = 9\n[a]\nb.c.t = 1\n", "a [header] defines",
            "a dotted key adding to what a header defined");
    refused("fruit = []\n[[fruit]]\n", "not as an array of tables",
            "[[ ]] on a value array");
    refused("[[t]]\n[t]\n", "array of tables", "a header over an array of tables");
    refused("[t]\n[[t]]\n", "not as an array of tables", "[[ ]] over a table");
    refused("a = [{b = 1}]\n[a.c]\n", "is a value", "a header into a value array");
    refused("t = {a = 1,}\n", "no comma after its last key",
            "a trailing comma in an inline table");

    /* Allowed: a table named on the way to a header and defined after it;
       sub-tables of what dotted keys defined; dotted keys sharing a table;
       and a header under the last element of an array of tables. */
    c = load("[a.b]\nx = 1\n[a]\ny = 2\n"
             "[f]\napple.color = 1\napple.size = 2\n[f.apple.texture]\nsmooth = true\n"
             "[[u]]\n[u.tls]\non = true\n[[u]]\n[u.tls]\non = false\n"
             "[[al]]\n[[al.s]]\nn = 1\n[[al]]\n[[al.s]]\nn = 2\n",
             &err);
    check(c != NULL, "what TOML allows still builds");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(cf_get_int(c, "a.b.x", 0) == 1 && cf_get_int(c, "a.y", 0) == 2,
          "a table defined after its sub-table");
    check(cf_get_bool(c, "f.apple.texture.smooth", CF_FALSE),
          "a sub-table of a table dotted keys defined");
    check(cf_get_bool(c, "u.0.tls.on", CF_FALSE) && !cf_get_bool(c, "u.1.tls.on", 1),
          "the same header under each element of an array of tables");
    check(cf_get_int(c, "al.1.s.0.n", 0) == 2,
          "an array of tables in each element of another is its own");
    cf_config_free(c);
}

/* Many keys, one table: the definitions are a hash, not a list searched
   from the start, so the record grows and rehashes without losing one. */
static void test_many_keys(void) {
    static char doc[64 * 1024];
    cf_error    err = CF_ERROR_INIT;
    cf_config*  c;
    size_t      n = 0;
    int         i;

    for (i = 0; i < 2000; ++i) {
        n += (size_t)sprintf(doc + n, "k%d = %d\n", i, i);
    }
    n += (size_t)sprintf(doc + n, "k1999 = 0\n");
    (void)n;
    refused(doc, "'k1999' is already defined", "a key twice among two thousand");
    doc[strlen(doc) - strlen("k1999 = 0\n")] = '\0';
    c                                        = load(doc, &err);
    check(c != NULL && cf_get_int(c, "k1234", 0) == 1234,
          "two thousand distinct keys build");
    cf_config_free(c);
}

#if CF_WITH_EMIT
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
#endif

#endif /* CF_WITH_TOML */

int main(void) {
#if CF_WITH_TOML
    test_reads();
    test_floats();
    test_refuses();
    test_values();
#if CF_WITH_REAL
    test_inf_against_real();
#endif
    test_line_endings();
    test_defined_once();
    test_many_keys();
#if CF_WITH_EMIT
    test_round_trip();
#endif
    remove("cf_toml.toml");
#else
    printf("skipped: this build has no TOML (CF_WITH_TOML=OFF)\n");
#endif
    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
