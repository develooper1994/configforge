/*
 * test_budget.c - what one document may make of the key space is bounded in
 * the flattener, for every format at once (D-82): at most
 * CF_DECODE_FACTOR times its size and CF_DECODE_FLOOR more, or it is
 * refused by a limit that names itself.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <string.h>

#include "cf_test.h"

#include "configforge/cf_flatten.h"

static void test_formula(void) {
    check(cf_decode_budget(0) == CF_DECODE_FLOOR,
          "an empty document may make the floor");
    check(cf_decode_budget(1000) == (size_t)1000 * CF_DECODE_FACTOR + CF_DECODE_FLOOR,
          "a document may make the factor times itself, and the floor");
    check(cf_decode_budget((size_t)-1) == (size_t)-1,
          "the bound saturates, it does not wrap");
}

/* `text` through `name`'s codec into a fresh key space, with `budget` bytes
   when it is not 0 and its own bound when it is. */
static cf_bool decode(const char* name, const char* text, size_t budget, cf_error* err,
                      cf_bool* over) {
    const cf_codec* codec = cf_codec_by_name(name);
    cf_arena*       ar    = cf_arena_new(NULL, 0);
    cf_keyspace     ks;
    cf_flatten      fl;
    cf_blob         blob;
    cf_bool         ok;

    check(codec != NULL && ar != NULL, "a codec and an arena");
    check(cf_ks_init(&ks, ar, 32) && cf_flatten_init(&fl, &ks, 0), "a flattener");
    cf_blob_mem(&blob, text, strlen(text), "doc");
    if (budget != 0) {
        cf_flatten_budget(&fl, budget);
    }
    ok    = cf_flatten_decode(&fl, codec, &blob, "doc", err);
    *over = fl.over_budget;
    cf_arena_free(ar);
    return ok;
}

static void one(const char* name, const char* text) {
    char     msg[96];
    cf_error err  = CF_ERROR_INIT;
    cf_error ok   = CF_ERROR_INIT;
    cf_bool  over = CF_FALSE;

    if (cf_codec_by_name(name) == NULL) {
        return;
    }
    sprintf(msg, "%s: a small budget refuses the document", name);
    check(!decode(name, text, 256, &err, &over), msg);
    sprintf(msg, "%s: with the source's code", name);
    check(err.code == CF_E_SOURCE, msg);
    sprintf(msg, "%s: and the limit by name", name);
    has(err.message, "a document may make", msg);
    sprintf(msg, "%s: the flattener says it was the budget", name);
    check(over, msg);

    over = CF_FALSE;
    sprintf(msg, "%s: under its own bound the document is read", name);
    check(decode(name, text, 0, &ok, &over), msg);
    sprintf(msg, "%s: and the budget did not refuse it", name);
    check(!over, msg);
}

static void test_every_format(void) {
    one("json", "{\"a\":{\"k0\":1,\"k1\":2,\"k2\":3,\"k3\":4,\"k4\":5,\"k5\":6}}");
    one("json5", "{a:{k0:1,k1:2,k2:3,k3:4,k4:5,k5:6}}");
    one("toml", "[a]\nk0=1\nk1=2\nk2=3\nk3=4\nk4=5\nk5=6\n");
    one("ini", "[a]\nk0=1\nk1=2\nk2=3\nk3=4\nk4=5\nk5=6\n");
    one("properties", "a.k0=1\na.k1=2\na.k2=3\na.k3=4\na.k4=5\na.k5=6\n");
    one("yaml", "a:\n  k0: 1\n  k1: 2\n  k2: 3\n  k3: 4\n  k4: 5\n  k5: 6\n");
    one("xml", "<a><k0>1</k0><k1>2</k1><k2>3</k2><k3>4</k3><k4>5</k4><k5>6</k5></a>");
}

#if CF_WITH_YAML
/* A block of 2000 keys, and thirty aliases of it: 60,000 keys from 19 KB,
   which YAML's own alias limits allow and the budget does not. */
static void test_aliases(void) {
    static char buf[64 * 1024];
    size_t      n = 0;
    int         i;
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  c;

    n += (size_t)sprintf(buf + n, "base: &b\n");
    for (i = 0; i < 2000; ++i) {
        n += (size_t)sprintf(buf + n, "  k%d: 1\n", i);
    }
    for (i = 0; i < 30; ++i) {
        n += (size_t)sprintf(buf + n, "c%d: *b\n", i);
    }
    check(cf_builder_document(b, "doc", CF_RANK_USER_FILE, buf, n, "yaml", 0),
          "the document is added");
    c = cf_builder_create(b, &err);
    check(c == NULL, "aliases that make a multiple of the document are refused");
    has(err.message, "a document may make", "by the budget's name");
    cf_config_free(c);
}
#endif

int main(void) {
    test_formula();
    test_every_format();
#if CF_WITH_YAML
    test_aliases();
#endif
    return cf_test_done();
}
