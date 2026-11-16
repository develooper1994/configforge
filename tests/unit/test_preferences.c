/*
 * test_preferences.c - macOS's user defaults as a platform capability
 * and a source. On macOS the test imports its own domains with
 * defaults(1) from a plist file it writes, reads them, and deletes them.
 * Where the platform cannot read them the walk must fail by name and
 * call nothing back: a source that read nothing would look exactly like
 * an empty domain.
 */

#include <configforge/configforge.h>
#include <configforge/cf_platform.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cf_test.h"

#define DOMAIN "org.configforge.test"
#define PLIST "cf_prefs.plist"

/* defaults(1), quietly: true when it succeeded. A failure here is the
   test's, not the library's. */
static int prefs(const char* args) {
#if defined(__APPLE__)
    char cmd[512];
    sprintf(cmd, "defaults %s >/dev/null 2>&1", args);
    return system(cmd) == 0;
#else
    (void)args; /* only macOS has user defaults to set up */
    return 0;
#endif
}

/* Whatever `domain` held, deleted; absent is fine. */
static void forget(const char* domain) {
    char args[256];
    sprintf(args, "delete %s", domain);
    (void)prefs(args);
}

/* `domain` holding exactly the plist dictionary `body` then `more`. */
static void write_domain(const char* domain, const char* body, const char* more) {
    static const char head[] =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
        "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
        "<plist version=\"1.0\"><dict>\n";
    static const char tail[] = "</dict></plist>\n";
    char              args[256];
    FILE*             f;

    forget(domain);
    f = fopen(PLIST, "wb");
    if (f == NULL) {
        printf("  setup: %s could not be written\n", PLIST);
        return;
    }
    fputs(head, f);
    fputs(body, f);
    fputs(more, f);
    fputs(tail, f);
    fclose(f);
    sprintf(args, "import %s %s", domain, PLIST);
    if (!prefs(args)) {
        printf("  setup: defaults %s failed\n", args);
    }
}

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
    cf_builder_int(b, "server.workers", 1, 1, 64, 0);
    return cf_builder_create(b, err);
}

static void test_on_macos(void) {
    static const char server[] =
        "<key>Server</key><dict><key>Port</key><integer>9443</integer>"
        "<key>Host</key><string>web1</string>"
        "<key>Workers</key><string>12</string></dict>\n";
    static const char rest[] =
        "<key>Hosts</key><array><string>a</string><string>b</string></array>\n"
        "<key>Ratio</key><real>0.5</real>\n"
        "<key>Enabled</key><true/>\n"
        "<key>Limit</key><integer>5000000000</integer>\n";
    static const char when[] = "<key>When</key><date>2026-01-01T00:00:00Z</date>\n";
    static const char bad[]  = "<key>Bad Name</key><string>x</string>\n";
    static char       ex[2048];
    cf_error          err = CF_ERROR_INIT;
    cf_config*        c;

    write_domain(DOMAIN, server, rest);
    c = load(DOMAIN, 0, &err);
    check(c != NULL, "a domain is a source");
    if (c != NULL) {
        check(cf_get_int(c, "server.port", 0) == 9443,
              "a dictionary is a section, an integer an int");
        check(cf_get_int(c, "server.workers", 0) == 12,
              "a string is text the contract types");
        has(cf_get_text(c, "server.host", ""), "web1", "and text where none is declared");
        check(cf_get_int(c, "hosts.count", 0) == 2, "an array is a list");
        has(cf_get_text(c, "hosts.1", ""), "b", "in order");
        check(cf_get_bool(c, "enabled", CF_FALSE) == CF_TRUE, "a boolean is a bool");
        check(cf_get_int(c, "limit", 0) == (cf_i64)5 * 1000000000,
              "an integer keeps all 64 bits");
#if CF_WITH_REAL
        check(cf_get_real(c, "ratio", 0.0) == 0.5, "a real is a real");
#endif
        cf_explain(c, "server.port", ex, sizeof ex);
        has(ex, "defaults " DOMAIN " Server/Port", "explain() names the value's place");
        cf_config_free(c);
    } else {
        printf("  %s\n", err.message);
    }

    write_domain(DOMAIN ".date", when, "");
    c = load(DOMAIN ".date", 0, &err);
    check(c == NULL, "a date is refused");
    has(err.message, "is a date", "by name");
    has(err.message, "When", "naming the value");
    cf_config_free(c);

    write_domain(DOMAIN ".name", bad, "");
    c = load(DOMAIN ".name", 0, &err);
    check(c == NULL && err.code == CF_E_KEY, "a name no key can carry is refused");
    has(err.message, "Bad Name", "naming it");
    cf_config_free(c);

    forget(DOMAIN ".absent");
    c = load(DOMAIN ".absent", 0, &err);
    check(c == NULL, "a domain with no values is an error");
    has(err.message, "no user defaults", "and says so");
    cf_config_free(c);
    c = load(DOMAIN ".absent", CF_OPTIONAL, &err);
    check(c != NULL, "unless it is optional");
    cf_config_free(c);

    forget(DOMAIN);
    forget(DOMAIN ".date");
    forget(DOMAIN ".name");
    (void)remove(PLIST);
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
    if (cf_platform_has_preferences()) {
        test_on_macos();
    } else {
        test_elsewhere_platform();
        test_elsewhere_source();
    }
    return cf_test_done();
}
