/*
 * test_file.c - configuration files as sources.
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

static char  a0[]    = "prog";
static char* argv0[] = {a0, NULL};

static cf_bool spill(const char* path, const char* text) {
    FILE* f = fopen(path, "wb");
    if (f == NULL) {
        return CF_FALSE;
    }
    fputs(text, f);
    fclose(f);
    return CF_TRUE;
}

/* ---------------------------------------------------------------- */

static void test_ini_file(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b;
    cf_config*  cfg;
    static char buf[2048];

    check(spill("cf_t1.ini", "# written by the test\n"
                             "[server]\n"
                             "port = 3000\n"
                             "host = 10.0.0.1\n"),
          "wrote the fixture");

    b = cf_build(NULL);
    cf_builder_file(b, "user-file", CF_RANK_USER_FILE, "cf_t1.ini", NULL, 0);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_builder_text(b, "server.host", "0.0.0.0", 0);

    cfg = cf_builder_create(b, &err);
    check(cfg != NULL, "a file source builds");
    if (cfg == NULL) {
        printf("  %s\n", err.message);
        return;
    }

    check(cf_get_int(cfg, "server.port", 0) == 3000, "the file wins");
    check(strcmp(cf_get_text(cfg, "server.host", ""), "10.0.0.1") == 0,
          "text from the file");

    /* The location is what a reader has to open and which line to
       change, which is the whole reason a file source exists. */
    cf_explain(cfg, "server.port", buf, sizeof buf);
    has(buf, "cf_t1.ini:3:1", "the file, line and column are reported");
    has(buf, "8080", "the default is still in the chain");
    has(buf, "shadowed", "and is marked as having lost");

    cf_config_free(cfg);
    remove("cf_t1.ini");
}

static void test_json_file_and_precedence(void) {
    static char  a1[]   = "--server.port=7000";
    static char* argv[] = {a0, a1, NULL};
    cf_error     err    = CF_ERROR_INIT;
    cf_builder*  b;
    cf_config*   cfg;

    check(spill("cf_t2.json",
                "{ \"server\": { \"port\": 3000, \"tags\": [\"a\",\"b\"] } }"),
          "wrote the json fixture");

    b = cf_build(NULL);
    cf_builder_cli(b, "cli", CF_RANK_CLI, 2, argv);
    cf_builder_file(b, "file", CF_RANK_USER_FILE, "cf_t2.json", NULL, 0);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);

    cfg = cf_builder_create(b, &err);
    check(cfg != NULL, "json file builds");
    if (cfg == NULL) {
        printf("  %s\n", err.message);
        return;
    }

    check(cf_get_int(cfg, "server.port", 0) == 7000, "the command line beats the file");
    /* Arrays survive the trip as index keys plus a length. */
    check(strcmp(cf_get_text(cfg, "server.tags.0", ""), "a") == 0, "element 0");
    check(cf_get_int(cfg, "server.tags.count", 0) == 2, "array length");

    cf_config_free(cfg);
    remove("cf_t2.json");
}

static void test_missing_file(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b;
    cf_config*  cfg;
    static char buf[1024];

    /* Required and absent: an error that names the file. */
    b = cf_build(NULL);
    cf_builder_file(b, "file", CF_RANK_USER_FILE, "cf_absent.ini", NULL, 0);
    cfg = cf_builder_create(b, &err);
    check(cfg == NULL, "a missing required file fails");
    check(err.code == CF_E_SOURCE, "reported as a source error");
    has(err.message, "cf_absent.ini", "names the file");

    /* Optional and absent: not an error, and not silence either. */
    b = cf_build(NULL);
    cf_builder_file(b, "file", CF_RANK_USER_FILE, "cf_absent.ini", NULL,
                    CF_FILE_OPTIONAL);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cfg = cf_builder_create(b, &err);
    check(cfg != NULL, "a missing optional file is accepted");
    if (cfg == NULL) {
        return;
    }
    check(cf_get_int(cfg, "server.port", 0) == 8080, "defaults still apply");

    cf_inspect(cfg, buf, sizeof buf);
    has(buf, "source 'file' supplied nothing",
        "an absent optional file is visible, not silent");
    cf_config_free(cfg);
}

static void test_uncompiled_format(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  cfg;

    check(spill("cf_t3.toml", "port = 1\n"), "wrote a toml fixture");

    cf_builder_file(b, "file", CF_RANK_USER_FILE, "cf_t3.toml", NULL, 0);
    cfg = cf_builder_create(b, &err);

    /* A build option must never make a file invisible. The error has
       to name the format and the option, or the reader goes looking
       for a typo that is not there. */
    check(cfg == NULL, "a format this build lacks is an error, not a skip");
    check(err.code == CF_E_CAPABILITY, "reported as a capability error");
    has(err.message, "toml", "names the format");
    has(err.message, "CF_WITH_TOML", "names the option to turn on");
    remove("cf_t3.toml");
}

static void test_unknown_extension(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  cfg;

    check(spill("cf_t4.zzz", "port = 1\n"), "wrote an odd fixture");
    cf_builder_file(b, "file", CF_RANK_USER_FILE, "cf_t4.zzz", NULL, 0);
    cfg = cf_builder_create(b, &err);
    check(cfg == NULL, "an unrecognised extension is an error");
    has(err.message, "set one explicitly", "and says what to do about it");

    /* Naming the format explicitly is the way out. */
    b = cf_build(NULL);
    cf_builder_file(b, "file", CF_RANK_USER_FILE, "cf_t4.zzz", "ini", 0);
    cf_builder_int(b, "port", 0, 0, 0, 0);
    cfg = cf_builder_create(b, &err);
    check(cfg != NULL, "an explicit format overrides the extension");
    if (cfg != NULL) {
        check(cf_get_int(cfg, "port", 0) == 1, "and it reads");
        cf_config_free(cfg);
    }
    remove("cf_t4.zzz");
}

static void test_malformed_file(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  cfg;

    check(spill("cf_t5.json", "{ \"a\": 1, }"), "wrote a broken fixture");
    cf_builder_file(b, "file", CF_RANK_USER_FILE, "cf_t5.json", NULL, 0);
    cfg = cf_builder_create(b, &err);

    check(cfg == NULL, "a malformed file fails construction");
    /* The specific parse error must survive, not be replaced by a
       generic "could not be read". */
    has(err.message, "cf_t5.json:1:", "with the exact position");
    has(err.message, "trailing comma", "and what was wrong");
    remove("cf_t5.json");
}

static void test_file_over_defaults_over_nothing(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b;
    cf_config*  cfg;
    cf_u64      d1, d2;

    check(spill("cf_t6a.ini", "[server]\nport = 3000\n"), "fixture a");
    check(spill("cf_t6b.json", "{\"server\":{\"port\":3000}}"), "fixture b");

    b = cf_build(NULL);
    cf_builder_file(b, "f", CF_RANK_USER_FILE, "cf_t6a.ini", NULL, 0);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cfg = cf_builder_create(b, &err);
    check(cfg != NULL, "ini builds");
    d1 = cf_digest(cfg);
    cf_config_free(cfg);

    b = cf_build(NULL);
    cf_builder_file(b, "f", CF_RANK_USER_FILE, "cf_t6b.json", NULL, 0);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cfg = cf_builder_create(b, &err);
    check(cfg != NULL, "json builds");
    d2 = cf_digest(cfg);
    cf_config_free(cfg);

    /* The same configuration written in two formats is the same
       configuration. The digest is over the resolved values, so the
       format it arrived in cannot show through. */
    check(d1 == d2, "format does not affect the resolved configuration");

    remove("cf_t6a.ini");
    remove("cf_t6b.json");
}

int main(void) {
    (void)argv0;
    test_ini_file();
    test_json_file_and_precedence();
    test_missing_file();
    test_uncompiled_format();
    test_unknown_extension();
    test_malformed_file();
    test_file_over_defaults_over_nothing();

    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
