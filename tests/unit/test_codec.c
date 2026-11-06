/*
 * test_codec.c - a codec written outside the library (cf_kv_codec.h),
 * handed to one file, and registered on a builder (D-68) so that its
 * extension chooses it, its name finds it and cf_emit writes it.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <string.h>

#include "cf_kv_codec.h"
#include "cf_test.h"

/* ------------------------------------------------------------------ */

static void test_reads_through_it(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  c;
    static char ex[2048];

    check(spill("cf_codec.kv", "# written for a codec the library does not have\n"
                               "server.port: 9443\n"
                               "server.host: example.org\n"),
          "wrote a file in a foreign format");
    cf_builder_file_codec(b, "file", CF_RANK_USER_FILE, "cf_codec.kv", &kv_codec, 0);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_builder_text(b, "server.host", "0.0.0.0", 0);
    c = cf_builder_create(b, &err);
    check(c != NULL, "a codec handed to the builder is used");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(cf_get_int(c, "server.port", 0) == 9443,
          "its text is typed by the definition, like any textual format");
    check(strcmp(cf_get_text(c, "server.host", ""), "example.org") == 0, "all of it");
    cf_explain(c, "server.port", ex, sizeof ex);
    has(ex, "cf_codec.kv:2:", "and the locations it reports reach explain()");
    cf_config_free(c);
}

static void test_the_contract_still_applies(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  c;

    check(spill("cf_codec.kv", "server.port: 99999\n"), "wrote an out-of-range value");
    cf_builder_file_codec(b, "file", CF_RANK_USER_FILE, "cf_codec.kv", &kv_codec, 0);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    c = cf_builder_create(b, &err);
    check(c == NULL, "a value from a foreign codec is validated like any other");
    has(err.source, "cf_codec.kv:1", "and the error names the place");
    cf_config_free(c);
}

static void test_without_it(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  c;

    /* The same file by extension: the library does not know ".kv", and
       says so rather than reading it as something else. */
    cf_builder_file(b, "file", CF_RANK_USER_FILE, "cf_codec.kv", NULL, 0);
    c = cf_builder_create(b, &err);
    check(c == NULL, "without the codec the format is unknown");
    has(err.message, "cannot tell the format", "and the error says so");
    cf_config_free(c);

    b = cf_build(NULL);
    cf_builder_file_codec(b, "file", CF_RANK_USER_FILE, "cf_codec.kv", NULL, 0);
    c = cf_builder_create(b, &err);
    check(c == NULL, "a missing codec is refused at construction");
    has(err.message, "no codec", "and named");
    cf_config_free(c);
}

/* Registered on the builder: the extension chooses it, a named format
   finds it, cf_config_codec and cf_emit know it, and a derived
   configuration keeps it. */
static void test_registered(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  c;
    cf_config*  d;
    static char out[1024];
    size_t      n;

    check(spill("cf_codec.kv", "server.port: 9443\n") &&
              spill("cf_codec.conf", "server.host: example.org\n"),
          "wrote two files in the foreign format");
    check(cf_builder_codec(b, &kv_codec), "a codec is registered on the builder");
    cf_builder_file(b, "file", CF_RANK_USER_FILE, "cf_codec.kv", NULL, 0);
    cf_builder_file(b, "named", CF_RANK_SYS_FILE, "cf_codec.conf", "kv", 0);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_builder_text(b, "server.host", "0.0.0.0", 0);
    c = cf_builder_create(b, &err);
    check(c != NULL, "its extension chooses it, and its name finds it");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(cf_get_int(c, "server.port", 0) == 9443, "the .kv file is read through it");
    check_text(cf_get_text(c, "server.host", ""), "example.org",
               "and the .conf file it was named for, ahead of INI's extension");
    check(cf_config_codec(c, "kv") == &kv_codec, "the configuration knows it by name");
    check(cf_config_codec(c, "nonsense") == NULL, "and no codec it was not given");
#if CF_WITH_JSON
    check(cf_config_codec(c, "json") == cf_codec_by_name("json"),
          "and the built-in ones still");
#endif
#if CF_WITH_EMIT
    n = cf_emit(c, "kv", CF_EMIT_ALL, 0, out, sizeof out, &err);
    check(n > 0, "cf_emit writes through it");
    has(out, "server.port: 9443\n", "line by line");
    has(out, "server.host: example.org\n", "every key");
#else
    (void)n;
    (void)out;
#endif
    d = cf_derive(c, "override", NULL, 0, &err);
    check(d != NULL && cf_config_codec(d, "kv") == &kv_codec,
          "a derived configuration keeps the codecs");
    cf_config_free(d);
    cf_config_free(c);
    remove("cf_codec.conf");
}

/* The conflicts a registry for the whole process could not show. */
static void test_registered_refusals(void) {
    static const cf_codec nameless = {sizeof(cf_codec), "",   ".x", CF_CODEC_READ,
                                      kv_decode,        NULL, 0};
    static const cf_codec mute     = {sizeof(cf_codec), "mute", ".m", 0, NULL, NULL, 0};
    cf_error              err      = CF_ERROR_INIT;
    cf_builder*           b        = cf_build(NULL);
    cf_config*            c;

    check(cf_builder_codec(b, &kv_codec), "the first codec called kv");
    check(!cf_builder_codec(b, &kv_codec), "a second one by that name is refused");
    c = cf_builder_create(b, &err);
    check(c == NULL, "and the builder does not build");
    has(err.message, "already has a codec called 'kv'", "and says why");
    cf_config_free(c);

    b = cf_build(NULL);
    check(!cf_builder_codec(b, &nameless), "a codec without a name is refused");
    cf_builder_abort(b);
    b = cf_build(NULL);
    check(!cf_builder_codec(b, &mute), "and one that can neither read nor write");
    cf_builder_abort(b);
    b = cf_build(NULL);
    check(!cf_builder_codec(b, NULL), "and none at all");
    cf_builder_abort(b);
}

#if CF_WITH_DIR
/* A directory's files are chosen by the same codecs. */
static void test_registered_dir(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  c;

    check(spill("cf_codec.d/10-base.kv", "server.port: 9000\n"),
          "a .kv file in a drop-in directory");
    cf_builder_codec(b, &kv_codec);
    cf_builder_dir(b, "conf.d", CF_RANK_USER_FILE, "cf_codec.d", NULL, 0);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    c = cf_builder_create(b, &err);
    check(c != NULL && cf_get_int(c, "server.port", 0) == 9000,
          "is read through the builder's codec");
    if (c == NULL) {
        printf("  %s\n", err.message);
    }
    cf_config_free(c);
    remove("cf_codec.d/10-base.kv");
}
#endif

int main(void) {
    test_reads_through_it();
    test_the_contract_still_applies();
    test_without_it();
    test_registered();
    test_registered_refusals();
#if CF_WITH_DIR
    test_registered_dir();
#endif
    remove("cf_codec.kv");
    return cf_test_done();
}
