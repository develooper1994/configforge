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

#if !CF_HAVE_C11
#error "built as C11, and configforge.h does not see it"
#endif

static int  g_fail = 0, g_run = 0;
static void check(int c, const char* w) {
    ++g_run;
    if (!c) {
        ++g_fail;
        printf("FAIL: %s\n", w);
    }
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
    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
