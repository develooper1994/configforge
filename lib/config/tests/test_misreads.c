/*
 * test_misreads.c - a read the contract does not have is recorded
 * (cf_builder_track_misreads, D-121, C-8a). C89.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <string.h>

#include "cf_test.h"

static cf_config* make(cf_bool track, cf_bool contract) {
    static const cf_pair pairs[] = {
        {"server.port", "9000"}, {"stray", "1"}, {"plugins.a", "x"}};
    static const cf_pair bare[] = {{"port", "9000"}};
    cf_error             err    = CF_ERROR_INIT;
    cf_builder*          b      = cf_build(NULL);
    cf_config*           c;

    if (contract) {
        cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
        cf_builder_text(b, "server.host", "h", 0);
        cf_builder_text(b, "opt.note", NULL, 0);
        cf_builder_open(b, "plugins");
        cf_builder_memory(b, "mem", 10, pairs, 3);
        cf_builder_unknown(b, CF_UNKNOWN_IGNORE);
    } else {
        cf_builder_memory(b, "mem", 10, bare, 1);
    }
    if (track) {
        cf_builder_track_misreads(b);
    }
    c = cf_builder_create(b, &err);
    check(c != NULL, "the configuration builds");
    return c;
}

static cf_bool is(const cf_config* c, size_t i, const char* key, cf_misread_kind kind,
                  cf_type wanted, cf_type found) {
    const char*     k = NULL;
    cf_misread_kind kd;
    cf_type         w, f;

    if (!cf_misread_at(c, i, &k, &kd, &w, &f) || k == NULL) {
        return CF_FALSE;
    }
    return (strcmp(k, key) == 0 && kd == kind && w == wanted && f == found) ? CF_TRUE
                                                                            : CF_FALSE;
}

static void test_off(void) {
    cf_config*  c = make(CF_FALSE, CF_TRUE);
    static char buf[16384];

    if (c == NULL) {
        return;
    }
    check(!cf_tracks_misreads(c), "tracking misreads is off by default");
    (void)cf_get_int(c, "server.prot", 1);
    (void)cf_get_text(c, "server.port", "");
    check(cf_misreads(c) == 0 && !cf_misread_at(c, 0, NULL, NULL, NULL, NULL),
          "an untracked configuration records nothing");
#if CF_WITH_REPORT
    (void)cf_report(c, NULL, buf, sizeof buf);
    check(strstr(buf, "\"misreads\"") == NULL, "and its report has no \"misreads\"");
#else
    (void)buf;
#endif
    check(cf_misreads(NULL) == 0 && !cf_tracks_misreads(NULL), "NULL is nothing");
    cf_config_free(c);
}

static void test_undefined(void) {
    cf_config*  c = make(CF_TRUE, CF_TRUE);
    char        key[32];
    const char* got = NULL;
    int         i;

    if (c == NULL) {
        return;
    }
    check(cf_tracks_misreads(c), "tracking is on when asked for");
    strcpy(key, "server.prot");
    for (i = 0; i < 3; ++i) {
        (void)cf_get_int(c, key, 1);
    }
    check(cf_misreads(c) == 1, "three equal misreads are one");
    check(is(c, 0, "server.prot", CF_MISREAD_UNDEFINED, CF_TYPE_INT, CF_TYPE_NULL),
          "an undefined key: kind, wanted and found");
    (void)cf_misread_at(c, 0, &got, NULL, NULL, NULL);
    memset(key, 'z', sizeof key - 1);
    key[sizeof key - 1] = '\0';
    check(got != NULL && strcmp(got, "server.prot") == 0, "the key is copied");
    check(!cf_misread_at(c, 1, NULL, NULL, NULL, NULL), "nothing past the end");

    (void)cf_get_text(c, "stray", "");
    check(cf_misreads(c) == 2 &&
              is(c, 1, "stray", CF_MISREAD_UNDEFINED, CF_TYPE_TEXT, CF_TYPE_NULL),
          "a key a source supplied and the contract does not define counts");
    cf_config_free(c);
}

static void test_type(void) {
    cf_config* c = make(CF_TRUE, CF_TRUE);
    cf_i64     i = 0;

    if (c == NULL) {
        return;
    }
    (void)cf_get_text(c, "server.port", "");
    check(is(c, 0, "server.port", CF_MISREAD_TYPE, CF_TYPE_TEXT, CF_TYPE_INT),
          "a text read of an int");
    (void)cf_try_int(c, "server.host", &i);
    check(cf_misreads(c) == 2 &&
              is(c, 1, "server.host", CF_MISREAD_TYPE, CF_TYPE_INT, CF_TYPE_TEXT),
          "an int try of a text");
    (void)cf_get_int(c, "server.host", 0);
    (void)cf_get_text(c, "server.host", "");
    check(cf_misreads(c) == 2, "the same key and wanted type is one entry");
    (void)cf_get_bool(c, "server.host", CF_FALSE);
    check(cf_misreads(c) == 3, "another wanted type is another entry");
    cf_config_free(c);
}

static void test_not_recorded(void) {
    cf_config*  c = make(CF_TRUE, CF_TRUE);
    static char buf[4096];

    if (c == NULL) {
        return;
    }
    (void)cf_get_int(c, "server.port", 0);
    (void)cf_get_text(c, "opt.note", "x");
    (void)cf_get_text(c, "plugins.a", "");
    (void)cf_get_int(c, "plugins.zzz", 0);
    (void)cf_has(c, "nope");
    (void)cf_type_of(c, "nope");
    (void)cf_explain(c, "nope", buf, sizeof buf);
    (void)cf_get_int(NULL, "x", 0);
    (void)cf_get_int(c, NULL, 0);
    check(cf_misreads(c) == 0, "a correct read, a probe, an open prefix: nothing");
    cf_config_free(c);
}

static void test_defined_no_value(void) {
    cf_config* c = make(CF_TRUE, CF_TRUE);

    if (c == NULL) {
        return;
    }
    (void)cf_get_int(c, "opt.note", 0);
    check(is(c, 0, "opt.note", CF_MISREAD_TYPE, CF_TYPE_INT, CF_TYPE_TEXT),
          "a defined key with no value read as the wrong type: found from the "
          "definition");
    cf_config_free(c);
}

static void test_tier0(void) {
    cf_config* c = make(CF_TRUE, CF_FALSE);

    if (c == NULL) {
        return;
    }
    (void)cf_get_text(c, "port", "");
    check(cf_misreads(c) == 1 &&
              is(c, 0, "port", CF_MISREAD_TYPE, CF_TYPE_TEXT, CF_TYPE_INT),
          "without a contract the inferred type is judged");
    (void)cf_get_int(c, "nope", 1);
    check(cf_misreads(c) == 1, "and with no definitions nothing is undefined");
    cf_config_free(c);
}

static void test_full(void) {
    cf_config*  c = make(CF_TRUE, CF_TRUE);
    char        key[16];
    int         i;
    const char* got = NULL;

    if (c == NULL) {
        return;
    }
    for (i = 0; i < 17; ++i) {
        sprintf(key, "m%02d", i);
        (void)cf_get_int(c, key, 0);
    }
    check(cf_misreads(c) == 16 && !cf_misread_at(c, 16, NULL, NULL, NULL, NULL),
          "sixteen are kept, the seventeenth is lost");
    check(cf_misread_at(c, 0, &got, NULL, NULL, NULL) && strcmp(got, "m00") == 0 &&
              cf_misread_at(c, 15, &got, NULL, NULL, NULL) && strcmp(got, "m15") == 0,
          "in the order first recorded");
    cf_config_free(c);
}

static void test_long_key(void) {
    cf_config*  c = make(CF_TRUE, CF_TRUE);
    char        key[301];
    const char* got = NULL;

    if (c == NULL) {
        return;
    }
    memset(key, 'a', 300);
    key[300] = '\0';
    (void)cf_get_int(c, key, 0);
    check(cf_misreads(c) == 1 && cf_misread_at(c, 0, &got, NULL, NULL, NULL) &&
              got != NULL && strlen(got) == (size_t)(CF_KEY_MAX - 1),
          "a long key is cut at CF_KEY_MAX - 1 bytes");
    cf_config_free(c);
}

#if CF_WITH_REPORT
static void test_report(void) {
    cf_config*     c    = make(CF_TRUE, CF_TRUE);
    cf_report_info info = CF_REPORT_INFO_INIT;
    static char    buf[16384];
    static char    full[16384];
    const char*    p;
    size_t         n;

    if (c == NULL) {
        return;
    }
    (void)cf_report(c, NULL, buf, sizeof buf);
    check(strstr(buf, "\"misreads\": []") != NULL,
          "tracking with no misread: an empty array");

    (void)cf_get_int(c, "server.prot", 1);
    (void)cf_get_text(c, "server.port", "");
    n = cf_report(c, NULL, buf, sizeof buf);
    p = strstr(buf, "\"misreads\": [");
    check(p != NULL, "the report has a \"misreads\" array");
    if (p != NULL) {
        const char* a =
            strstr(p, "{ \"key\": \"server.prot\", \"kind\": \"undefined\", "
                      "\"wanted\": \"int\" }");
        const char* b = strstr(p, "{ \"key\": \"server.port\", \"kind\": \"type\", "
                                  "\"wanted\": \"text\", \"found\": \"int\" }");
        check(a != NULL && b != NULL && a < b, "both, in the order first recorded");
    }
    info.mode = CF_REPORT_FULL;
    (void)cf_report(c, &info, full, sizeof full);
    check(strstr(full, "\"misreads\": [") != NULL &&
              strstr(full, "\"key\": \"server.prot\", \"kind\": \"undefined\"") != NULL,
          "a full report says the same");
#if CF_WITH_JSON
    {
        cf_error    err = CF_ERROR_INIT;
        cf_builder* b2  = cf_build(NULL);
        cf_config*  r;

        cf_builder_document(b2, "r", 0, buf, n, "json", 0);
        r = cf_builder_create(b2, &err);
        check(r != NULL, "the report is valid JSON");
        cf_config_free(r);
    }
#else
    (void)n;
#endif
    cf_config_free(c);
}
#endif

static void test_derive(void) {
    cf_config* c = make(CF_TRUE, CF_TRUE);
    cf_config* d;
    cf_error   err = CF_ERROR_INIT;
    cf_pair    pair;

    if (c == NULL) {
        return;
    }
    (void)cf_get_int(c, "server.prot", 1);
    pair.key   = "server.port";
    pair.value = "9001";
    d          = cf_derive(c, "admin", &pair, 1, &err);
    check(d != NULL && cf_tracks_misreads(d) && cf_misreads(d) == 0,
          "a derived configuration tracks, from nothing");
    cf_config_free(d);
    cf_config_free(c);
}

int main(void) {
    test_off();
    test_undefined();
    test_type();
    test_not_recorded();
    test_defined_no_value();
    test_tier0();
    test_full();
    test_long_key();
#if CF_WITH_REPORT
    test_report();
#endif
    test_derive();
    return cf_test_done();
}
