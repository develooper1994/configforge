/*
 * test_registry.c - a Registry key as a source: names lowered and
 * joined, each type read as it says, the refusals named, and every
 * value explained by its full path. On Windows the test writes its own
 * keys under HKCU\Software\ConfigForgeTest with reg.exe and removes them;
 * anywhere else it checks that the source refuses by name.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <stdlib.h>
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

#define ROOT "HKCU\\Software\\ConfigForgeTest"

/* reg.exe, quietly; a failure here is the test's, not the library's. */
static void reg(const char* args) {
#ifdef _WIN32
    char cmd[512];
    sprintf(cmd, "reg %s >NUL 2>&1", args);
    if (system(cmd) != 0) {
        printf("  setup: reg %s failed\n", args);
    }
#else
    (void)args; /* only Windows has a Registry to set up */
#endif
}

static cf_config* load(const char* root, unsigned flags, cf_error* err) {
    cf_builder* b = cf_build(NULL);
    cf_builder_registry(b, "registry", CF_RANK_SYS_FILE, root, flags);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_builder_int(b, "server.workers", 1, 1, 64, 0);
    return cf_builder_create(b, err);
}

static void test_values(void) {
    static char ex[2048];
    cf_error    err = CF_ERROR_INIT;
    cf_config*  c;

    reg("add " ROOT "\\Good\\Server /v Port /t REG_DWORD /d 9443 /f");
    reg("add " ROOT "\\Good\\Server /v Workers /t REG_SZ /d 12 /f");
    reg("add " ROOT "\\Good\\Server /v Host /t REG_SZ /d web1 /f");
    reg("add " ROOT "\\Good /v Hosts /t REG_MULTI_SZ /d a\\0b /f");
    /* ^% is cmd.exe's escape: reg.exe must see %TEMP%, not its value. */
    reg("add " ROOT "\\Good /v Cache /t REG_EXPAND_SZ /d ^%TEMP^%\\cache /f");
    reg("add " ROOT "\\Good /v Limit /t REG_QWORD /d 5000000000 /f");

    c = load(ROOT "\\Good", 0, &err);
    check(c != NULL, "a Registry key is a source");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(cf_get_int(c, "server.port", 0) == 9443,
          "a subkey is a section, a DWORD an int");
    check(cf_get_int(c, "server.workers", 0) == 12,
          "a REG_SZ is text the contract types");
    has(cf_get_text(c, "server.host", ""), "web1", "and text where none is declared");
    check(cf_get_int(c, "hosts.count", 0) == 2, "a REG_MULTI_SZ is a list");
    has(cf_get_text(c, "hosts.1", ""), "b", "in order");
    has(cf_get_text(c, "cache", ""), "%TEMP%", "a REG_EXPAND_SZ is not expanded");
    check(cf_get_int(c, "limit", 0) == (cf_i64)5 * 1000000000,
          "a QWORD keeps all 64 bits");

    cf_explain(c, "server.port", ex, sizeof ex);
    has(ex, ROOT "\\Good\\Server\\Port", "explain() names the value's full path");
    cf_config_free(c);
}

static void test_refusals(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c;

    reg("add " ROOT "\\Binary /v Blob /t REG_BINARY /d 0a0b /f");
    c = load(ROOT "\\Binary", 0, &err);
    check(c == NULL, "a type with no meaning here is refused");
    has(err.message, "another Registry type", "by name");
    has(err.message, "Binary\\Blob", "naming the value");
    cf_config_free(c);

    reg("add " ROOT "\\Unnamed /ve /d something /f");
    c = load(ROOT "\\Unnamed", 0, &err);
    check(c == NULL, "a set unnamed value is refused");
    has(err.message, "unnamed value", "and says why");
    cf_config_free(c);

    reg("add " ROOT "\\BadName /v \"Bad Name\" /t REG_SZ /d x /f");
    c = load(ROOT "\\BadName", 0, &err);
    check(c == NULL && err.code == CF_E_KEY, "a name no key can carry is refused");
    has(err.message, "BadName\\Bad Name", "naming the value");
    cf_config_free(c);

    c = load(ROOT "\\Missing", 0, &err);
    check(c == NULL, "a missing key is an error");
    has(err.message, "no Registry key", "and says so");
    cf_config_free(c);
    c = load(ROOT "\\Missing", CF_FILE_OPTIONAL, &err);
    check(c != NULL, "unless it is optional");
    cf_config_free(c);

    c = load("HKXX\\Software", 0, &err);
    check(c == NULL, "a root with no hive is refused");
    has(err.message, "does not start with a hive", "naming the hives");
    cf_config_free(c);
}

static void test_elsewhere(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = load(ROOT, CF_FILE_OPTIONAL, &err);

    check(c == NULL, "without a Registry the source fails, even optional");
    has(err.message, "exists only on Windows", "and says why");
    cf_config_free(c);
}

int main(void) {
    if (cf_platform_has_registry()) {
        /* Left over from a run that stopped part way; absent is fine. */
        reg("delete " ROOT " /f");
        test_values();
        test_refusals();
        reg("delete " ROOT " /f");
    } else {
        test_elsewhere();
    }
    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
