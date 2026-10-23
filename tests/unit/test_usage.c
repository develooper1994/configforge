/*
 * test_usage.c - how to use ConfigForge, tier by tier, as a test.
 *
 * Read this file top to bottom to learn the library. Each section is
 * the smallest real program for one usage tier (Design.md, section 2.1),
 * and each is compiled and run by the suite - so, unlike prose, it
 * cannot describe an interface that no longer exists.
 *
 *   Tier 0  read values           cf_app, cf_get_*
 *   Tier 1  declare a contract    cf_builder_int/text/bool, cf_explain
 *   Tier 2  choose the sources    files, environment, command line
 *   Tier 3  policy                which sources may set a key
 *   Tier 4  change and watch      cf_derive, cf_emit, cf_changed
 *
 * A tier never needs the one after it. The comments say what a reader
 * of that tier needs to know, and nothing from a later one.
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

static cf_bool spill(const char* path, const char* text) {
    FILE* f = fopen(path, "wb");
    if (f == NULL) {
        return CF_FALSE;
    }
    fputs(text, f);
    fclose(f);
    return CF_TRUE;
}

static char a0[] = "myapp";

/* ================================================================== *
 * Tier 0 - read values
 *
 * One call builds the configuration from the usual places: defaults,
 * myapp's files, MYAPP_* environment variables and the command line.
 * Everything that can go wrong goes wrong in that call, once; reading
 * afterwards cannot fail, and the fallback is the default.
 * ================================================================== */
static void tier0(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* cfg;

    static char* argv[] = {a0, NULL}; /* main's argc and argv, in a program */

    cfg = cf_app("myapp", 1, argv, &err);
    if (cfg == NULL) {
        printf("  %s\n", err.message); /* what went wrong, and where */
        check(0, "tier 0: builds");
        return;
    }
    check(cf_get_int(cfg, "server.port", 8080) == 8080,
          "tier 0: an unset value reads as the fallback given");
    check(strcmp(cf_get_text(cfg, "server.host", "0.0.0.0"), "0.0.0.0") == 0,
          "tier 0: text too");
    cf_config_free(cfg);
}

/* ================================================================== *
 * Tier 1 - declare a contract
 *
 * Declaring a key gives it a type, a default and bounds. A value from
 * anywhere that breaks them is refused when the configuration is built,
 * with the value, the bound and where it was set. And explain() says
 * why any value is what it is - the reason this library exists.
 * ================================================================== */
static void tier1(void) {
    static char  bad[]   = "--server.port=99999";
    static char  good[]  = "--server.port=7000";
    static char* argv1[] = {a0, bad, NULL};
    static char* argv2[] = {a0, good, NULL};
    static char  why[2048];
    cf_error     err = CF_ERROR_INIT;
    cf_builder*  b;
    cf_config*   cfg;

    b = cf_build(NULL);
    cf_builder_cli(b, "cli", CF_RANK_CLI, 2, argv1);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cfg = cf_builder_create(b, &err);
    check(cfg == NULL, "tier 1: a value outside its bounds is refused");
    check(strstr(err.message, "99999 is outside 1..65535") != NULL,
          "tier 1: and the message names the value and the bound");
    check(strcmp(err.source, "argv[1]") == 0, "tier 1: and where it was set");

    b = cf_build(NULL);
    cf_builder_cli(b, "cli", CF_RANK_CLI, 2, argv2);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cfg = cf_builder_create(b, &err);
    if (cfg == NULL) {
        check(0, "tier 1: builds");
        return;
    }
    check(cf_get_int(cfg, "server.port", 0) == 7000, "tier 1: the value is typed");

    cf_explain(cfg, "server.port", why, sizeof why);
    /* server.port = 7000  (int)
         cli         7000      selected   argv[1]
         default     8080      shadowed                                */
    check(strstr(why, "selected") != NULL && strstr(why, "shadowed") != NULL,
          "tier 1: explain() shows the winner and what it beat");
    cf_config_free(cfg);
}

/* ================================================================== *
 * Tier 2 - choose the sources
 *
 * Each source has a rank, and the lower rank wins: the command line
 * over the environment over a file over the default. Within one file,
 * the later line wins.
 * ================================================================== */
static void tier2(void) {
    static char  e0[]   = "MYAPP_LOG__LEVEL=debug";
    static char* env[]  = {e0, NULL};
    static char  a1[]   = "--server.port=9000";
    static char* argv[] = {a0, a1, NULL};
    cf_error     err    = CF_ERROR_INIT;
    cf_builder*  b      = cf_build(NULL);
    cf_config*   cfg;

    check(spill("cf_usage.ini", "[server]\nport = 3000\nhost = 10.0.0.1\n"
                                "[log]\nlevel = info\n"),
          "tier 2: wrote a file");
    cf_builder_file(b, "file", CF_RANK_USER_FILE, "cf_usage.ini", NULL, 0);
    cf_builder_env(b, "env", CF_RANK_ENV, "MYAPP_", env);
    cf_builder_cli(b, "cli", CF_RANK_CLI, 2, argv);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_builder_text(b, "server.host", "0.0.0.0", 0);
    cf_builder_text(b, "log.level", "warning", 0);
    cfg = cf_builder_create(b, &err);
    if (cfg == NULL) {
        printf("  %s\n", err.message);
        check(0, "tier 2: builds");
        remove("cf_usage.ini");
        return;
    }
    check(cf_get_int(cfg, "server.port", 0) == 9000, "tier 2: the command line wins");
    check(strcmp(cf_get_text(cfg, "log.level", ""), "debug") == 0,
          "tier 2: the environment beats the file");
    check(strcmp(cf_get_text(cfg, "server.host", ""), "10.0.0.1") == 0,
          "tier 2: the file beats the default");
    cf_config_free(cfg);
    remove("cf_usage.ini");
}

/* ================================================================== *
 * Tier 3 - policy
 *
 * A definition can name the sources allowed to set its key, in the
 * order they win. A secret that may only come from the environment
 * ignores the command line - and the refused candidate stays on record,
 * so explain() and Doctor can say it was tried.
 * ================================================================== */
static void tier3(void) {
    static const char* const only_env[] = {"env", NULL};
    static char              e0[]       = "MYAPP_DB__PASSWORD=from-the-vault";
    static char*             env[]      = {e0, NULL};
    static char              a1[]       = "--db.password=typed-in-a-terminal";
    static char*             argv[]     = {a0, a1, NULL};
    static char              why[2048];
    cf_error                 err = CF_ERROR_INIT;
    cf_builder*              b   = cf_build(NULL);
    cf_config*               cfg;
    cf_def                   d;

    cf_builder_env(b, "env", CF_RANK_ENV, "MYAPP_", env);
    cf_builder_cli(b, "cli", CF_RANK_CLI, 2, argv);
    cf_def_init(&d, CF_TYPE_TEXT, "db.password");
    d.flags  = CF_REQUIRED | CF_SENSITIVE;
    d.policy = only_env;
    cf_builder_define(b, &d);
    cfg = cf_builder_create(b, &err);
    if (cfg == NULL) {
        printf("  %s\n", err.message);
        check(0, "tier 3: builds");
        return;
    }
    check(strcmp(cf_get_text(cfg, "db.password", ""), "from-the-vault") == 0,
          "tier 3: only the environment may set the secret");
    cf_explain(cfg, "db.password", why, sizeof why);
    check(strstr(why, "rejected") != NULL,
          "tier 3: the command line's try is on record");
    cf_config_free(cfg);
}

/* ================================================================== *
 * Tier 4 - change and watch
 *
 * A configuration never changes once built, so any thread can read it
 * without a lock. Changing a value derives a new one, under the same
 * contract; cf_emit writes what somebody configured, and nothing they
 * did not; cf_changed asks whether a file moved on.
 * ================================================================== */
static void tier4(void) {
    static const cf_pair change[] = {{"server.port", "9443"}};
    static char          saved[1024];
    cf_error             err = CF_ERROR_INIT;
    cf_builder*          b   = cf_build(NULL);
    cf_config*           cfg;
    cf_config*           next;

    check(spill("cf_usage4.ini", "[server]\nport = 3000\n"), "tier 4: wrote a file");
    cf_builder_file(b, "file", CF_RANK_USER_FILE, "cf_usage4.ini", NULL, 0);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_builder_bool(b, "tls.enabled", CF_FALSE, 0);
    cfg = cf_builder_create(b, &err);
    if (cfg == NULL) {
        printf("  %s\n", err.message);
        check(0, "tier 4: builds");
        remove("cf_usage4.ini");
        return;
    }

    next = cf_derive(cfg, "settings", change, 1, &err);
    check(next != NULL && cf_get_int(next, "server.port", 0) == 9443,
          "tier 4: deriving sets a value");
    check(cf_get_int(cfg, "server.port", 0) == 3000,
          "tier 4: and leaves the original alone");

    if (next != NULL) {
        cf_emit(next, "ini", CF_EMIT_CONFIGURED, 0, saved, sizeof saved, &err);
        check(strstr(saved, "9443") != NULL, "tier 4: emit writes what was configured");
        check(strstr(saved, "tls") == NULL,
              "tier 4: and not the defaults nobody touched");
        cf_config_free(next);
    }

    check(!cf_changed(cfg), "tier 4: nothing has moved on yet");
    check(spill("cf_usage4.ini", "[server]\nport = 3001\n# edited\n"),
          "tier 4: edited");
    check(cf_changed(cfg), "tier 4: and now it has; rebuild and swap");

    cf_config_free(cfg);
    remove("cf_usage4.ini");
}

int main(void) {
    tier0();
    tier1();
    tier2();
    tier3();
    tier4();
    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
