/*
 * test_c11.c - what configforge.h adds for a program compiled as C11
 * (D-53): cf_get, typed by where the value goes, refusing an integer the
 * target cannot hold rather than truncating it. Built with
 * C_STANDARD 11 where the compiler has it.
 */

#include <configforge/configforge.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "cf_test.h"

#if !CF_HAVE_C11
#error "built as C11, and configforge.h does not see it"
#endif

/* D-74: the typed calls, cf_builder_key with the type inferred, and
   CF_DEFINE make the same contract, byte for byte. */
static void schema_of(cf_builder* b, char* out, size_t cap) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = cf_builder_create(b, &err);
    check(c != NULL, "a contract builds");
    out[0] = '\0';
    if (c != NULL) {
        cf_schema(c, "t", out, cap);
        cf_config_free(c);
    }
}

static void test_one_define(void) {
    static char typed[4096], keyed[4096], generic[4096];
    cf_builder* b   = cf_build(NULL);
    cf_error    err = CF_ERROR_INIT;

    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_builder_text(b, "server.host", "0.0.0.0", 0);
    cf_builder_bool(b, "tls.enabled", CF_FALSE, 0);
    schema_of(b, typed, sizeof typed);

    b = cf_build(NULL);
    cf_builder_key(b, "server.port", CF_TYPE_NULL, "8080", "1", "65535", 0);
    cf_builder_key(b, "server.host", CF_TYPE_NULL, "0.0.0.0", NULL, NULL, 0);
    cf_builder_key(b, "tls.enabled", CF_TYPE_BOOL, "false", NULL, NULL, 0);
    schema_of(b, keyed, sizeof keyed);

    b = cf_build(NULL);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0); /* a range: typed */
    CF_DEFINE(b, "server.host", "0.0.0.0");
    CF_DEFINE(b, "tls.enabled", (bool)false); /* C11's false is the int 0 */
    schema_of(b, generic, sizeof generic);

    /* Empty in a build without the schema writer, and equal there too. */
    check(strcmp(keyed, typed) == 0, "cf_builder_key: the same");
    check(strcmp(generic, typed) == 0, "CF_DEFINE: the same");

    b = cf_build(NULL);
    cf_builder_key(b, "server.port", CF_TYPE_INT, "eighty", NULL, NULL, 0);
    check(cf_builder_create(b, &err) == NULL && err.code == CF_E_DEFINITION,
          "a default that is not its type is refused");
    b = cf_build(NULL);
    cf_builder_key(b, "server.port", CF_TYPE_NULL, NULL, NULL, NULL, 0);
    check(cf_builder_create(b, &err) == NULL && err.code == CF_E_DEFINITION,
          "and so is a key with neither");
    b = cf_build(NULL);
    cf_builder_key(b, "tls.enabled", CF_TYPE_NULL, "true", "0", "1", 0);
    check(cf_builder_create(b, &err) == NULL && err.code == CF_E_DEFINITION,
          "a range on a bool is refused");
    b = cf_build(NULL);
    cf_builder_key(b, "server.port", CF_TYPE_INT, "8080", "1", NULL, 0);
    check(cf_builder_create(b, &err) == NULL && err.code == CF_E_DEFINITION,
          "and so is half a range");

    /* CF_DEFINE of an int, which has no range to give. */
    b = cf_build(NULL);
    cf_builder_int(b, "server.workers", 4, 0, 0, 0);
    schema_of(b, typed, sizeof typed);
    b = cf_build(NULL);
    CF_DEFINE(b, "server.workers", 4);
    schema_of(b, generic, sizeof generic);
    check(strcmp(generic, typed) == 0, "CF_DEFINE of an int: the same");
}

int main(void) {
    static const cf_pair pairs[] = {{"server.port", "8080"},
                                    {"big", "5000000000"},
                                    {"tls", "true"},
                                    {"host", "web1"},
                                    {"ratio", "0.5"}};
    cf_error             err     = CF_ERROR_INIT;
    cf_builder*          b       = cf_build(NULL);
    cf_config*           c;
    int                  port = 0, small = 7;
    long long            wide = 0;
    bool                 tls  = false;
    const char*          host = NULL;

    test_one_define();
    cf_builder_memory(b, "file", CF_RANK_USER_FILE, pairs, 5);
#if CF_WITH_REAL
    cf_builder_real(b, "ratio", 0.0, 0.0, 1.0, 0); /* a real needs its definition */
#endif
    c = cf_builder_create(b, &err);
    check(c != NULL, "created");
    check(cf_get(c, "server.port", &port) == CF_OK && port == 8080, "an int");
    check(cf_get(c, "big", &small) == CF_E_CONVERT && small == 7,
          "too big for an int: refused, and the int left alone");
    check(cf_get(c, "big", &wide) == CF_OK && wide == 5000000000LL,
          "but a long long holds it");
    check(cf_get(c, "tls", &tls) == CF_OK && tls, "a bool");
    check(cf_get(c, "host", &host) == CF_OK && strcmp(host, "web1") == 0, "text");
    check(cf_get(c, "host", &port) == CF_E_CONVERT, "text is not an int");
    check(cf_get(c, "nope", &port) == CF_E_KEY, "absent is absent");
#if CF_WITH_REAL
    {
        double r = 0;
        check(cf_get(c, "ratio", &r) == CF_OK && r == 0.5, "a double");
    }
#endif
    cf_config_free(c);
    return cf_test_done();
}
