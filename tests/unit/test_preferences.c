/*
 * test_preferences.c - macOS's user defaults as a platform capability.
 * Where the platform cannot read them the walk must fail by name and
 * call nothing back: a source that read nothing would look exactly like
 * an empty domain.
 */

#include <configforge/configforge.h>
#include <configforge/cf_platform.h>

#include <stdio.h>
#include <string.h>

#include "cf_test.h"

#define DOMAIN "org.configforge.test"

static int g_calls = 0;

static cf_bool count(void* ctx, const cf_platform_pref_event* e) {
    (void)ctx;
    (void)e;
    ++g_calls;
    return CF_TRUE;
}

static void test_elsewhere_platform(void) {
    static char buf[4096];
    cf_error    err   = CF_ERROR_INIT;
    cf_arena*   arena = cf_arena_fixed(buf, sizeof buf);
    int         r;

    r = cf_platform_preferences_walk(DOMAIN, arena, count, NULL, &err);
    check(r == -1, "a platform without user defaults returns -1");
    check(err.code == CF_E_CAPABILITY, "with the capability error");
    has(err.message, DOMAIN, "naming the domain");
#if CF_WITH_PREFERENCES
    has(err.message, "exist only on macOS", "and saying where they exist");
#else
    has(err.message, "compiled without CF_WITH_PREFERENCES", "and naming the option");
#endif
    check(g_calls == 0, "and the callback is never called");
}

static cf_config* load(const char* domain, unsigned flags, cf_error* err) {
    cf_builder* b = cf_build(NULL);
    cf_builder_preferences(b, "defaults", CF_RANK_SYS_FILE, domain, flags);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    return cf_builder_create(b, err);
}

static void test_elsewhere_source(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c;

    c = load(DOMAIN, 0, &err);
    check(c == NULL, "a source for user defaults fails where there are none");
    has(err.message, DOMAIN, "naming the domain");
#if CF_WITH_PREFERENCES
    has(err.message, "exist only on macOS", "and saying where they exist");
#else
    has(err.message, "compiled without CF_WITH_PREFERENCES", "and naming the option");
#endif
    if (c != NULL) {
        cf_config_free(c);
    }

    cf_error_clear(&err);
    err.size = sizeof(cf_error);
    c        = load(DOMAIN, CF_OPTIONAL, &err);
    check(c == NULL, "CF_OPTIONAL does not make that acceptable");
    has(err.message, DOMAIN, "it still names the domain");
    if (c != NULL) {
        cf_config_free(c);
    }
}

int main(void) {
    if (!cf_platform_has_preferences()) {
        test_elsewhere_platform();
        test_elsewhere_source();
    }
    return cf_test_done();
}
