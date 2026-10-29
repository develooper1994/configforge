/*
 * test_config.c - the facade, and the M0 contract.
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

/* ---------------------------------------------------------------- */

static void test_tier0(void) {
    static char  a1[]   = "--server.port=7000";
    static char  a2[]   = "--tls.enabled";
    static char* argv[] = {a0, a1, a2, NULL};
    static char  e0[]   = "MYAPP_SERVER__HOST=10.0.0.1";
    static char* env[]  = {e0, NULL};

    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  cfg;

    cf_builder_cli(b, "cli", CF_RANK_CLI, 3, argv);
    cf_builder_env(b, "env", CF_RANK_ENV, "MYAPP_", env);

    cfg = cf_builder_create(b, &err);
    check(cfg != NULL, "tier 0 builds with no definitions at all");
    if (cfg == NULL) {
        printf("  %s\n", err.message);
        return;
    }

    /* Without a contract, Tier 0 inference decides, and it is timid. */
    check(cf_get_int(cfg, "server.port", 0) == 7000, "int inferred");
    check(cf_get_bool(cfg, "tls.enabled", CF_FALSE), "bare flag is true");
    check(strcmp(cf_get_text(cfg, "server.host", ""), "10.0.0.1") == 0,
          "dotted address stays text");

    /* A read of the wrong type returns the fallback rather than
       coercing, and cf_try_ says which of the two happened. */
    check(cf_get_text(cfg, "server.port", "fallback") != NULL &&
              strcmp(cf_get_text(cfg, "server.port", "fallback"), "fallback") == 0,
          "wrong-type read returns the fallback");
    check(cf_try_text(cfg, "server.port", NULL, NULL) == CF_E_CONVERT,
          "wrong type is distinguishable");
    check(cf_try_int(cfg, "no.such.key", NULL) == CF_E_KEY,
          "absent is distinguishable from wrong type");
    check(cf_type_of(cfg, "server.port") == CF_TYPE_INT, "type reported");
    check(!cf_has(cfg, "no.such.key"), "absence reported");

    cf_config_free(cfg);
}

static void test_one_error_site(void) {
    static char  a1[]   = "--server.port=99999";
    static char* argv[] = {a0, a1, NULL};
    cf_error     err    = CF_ERROR_INIT;
    cf_builder*  b      = cf_build(NULL);
    cf_config*   cfg;

    cf_builder_cli(b, "cli", CF_RANK_CLI, 2, argv);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);

    cfg = cf_builder_create(b, &err);
    /* Everything that can go wrong goes wrong here, once. There is no
       second place to check. */
    check(cfg == NULL, "a malformed value fails construction");
    check(err.code == CF_E_VALIDATE, "reported as validation");
    has(err.message, "99999", "names the value");
    has(err.message, "65535", "names the bound");
    has(err.source, "argv[1]", "names where to fix it");
}

static void test_deferred_builder_error(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  cfg;

    /* A definition block reads as a block. The first failure is held
       and reported once, by create. */
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_builder_int(b, "a.b", 5, 100, 1, 0); /* inverted range */
    cf_builder_text(b, "server.host", "0.0.0.0", 0);

    cfg = cf_builder_create(b, &err);
    check(cfg == NULL, "a bad definition fails construction");
    check(err.code == CF_E_DEFINITION, "reported as a definition error");
    has(err.key, "a.b", "names the offending definition");
    /* The first failure is the useful one; later ones are noise. */
    has(err.message, "inverted", "reports the first failure, not the last");
}

static void test_defaults_are_a_source(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_app_builder("myapp", 1, argv0);
    cf_config*  cfg;
    static char buf[2048];

    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cfg = cf_builder_create(b, &err);
    check(cfg != NULL, "builds");
    if (cfg == NULL) {
        return;
    }

    check(cf_get_int(cfg, "server.port", 0) == 8080, "default applies");

    cf_explain(cfg, "server.port", buf, sizeof buf);
    /* A default is a value from the lowest-precedence source, not a
       separate mechanism, so it has a name to print. */
    has(buf, "default", "the default source is named");
    check(strstr(buf, "?") == NULL, "no unnamed source in the output");
    has(buf, "selected", "and it is the winner here");

    cf_config_free(cfg);
}

static void test_prefix_derivation(void) {
    static char  e0[]  = "MY_APP_SERVER__PORT=4242";
    static char* env[] = {e0, NULL};
    cf_error     err   = CF_ERROR_INIT;
    cf_builder*  b     = cf_build(NULL);
    cf_config*   cfg;

    /* "my-app" becomes MY_APP_. The folding is the library's own
       ASCII one: toupper is locale-dependent, and a Turkish locale
       would produce a prefix no variable matches. */
    cf_builder_env(b, "env", CF_RANK_ENV, "MY_APP_", env);
    cfg = cf_builder_create(b, &err);
    check(cfg != NULL && cf_get_int(cfg, "server.port", 0) == 4242,
          "prefix with a separator works");
    cf_config_free(cfg);
}

static void test_digest_and_report(void) {
    static char            a1[]   = "--server.port=7000";
    static char*           argv[] = {a0, a1, NULL};
    cf_error               err    = CF_ERROR_INIT;
    cf_builder*            b1     = cf_build(NULL);
    cf_builder*            b2     = cf_build(NULL);
    cf_config *            c1, *c2;
    const cf_apply_report* rep;

    cf_builder_cli(b1, "cli", CF_RANK_CLI, 2, argv);
    cf_builder_int(b1, "server.port", 8080, 1, 65535, 0);
    c1 = cf_builder_create(b1, &err);

    /* Same resolved configuration, sources registered in the opposite
       order: the digest is over the result, not over how it got there. */
    cf_builder_int(b2, "server.port", 8080, 1, 65535, 0);
    cf_builder_cli(b2, "cli", CF_RANK_CLI, 2, argv);
    c2 = cf_builder_create(b2, &err);

    check(c1 != NULL && c2 != NULL, "both build");
    if (c1 == NULL || c2 == NULL) {
        return;
    }
    check(cf_digest(c1) == cf_digest(c2), "digest independent of order");
    check(cf_count(c1) == 1, "one key");

    rep = cf_apply_stats(c1);
    check(rep != NULL && rep->converted == 1, "one conversion counted");
    check(rep->unknown == 0, "nothing unknown");

    cf_config_free(c1);
    cf_config_free(c2);
}

static void test_abort_and_null_safety(void) {
    cf_builder* b   = cf_build(NULL);
    cf_error    err = CF_ERROR_INIT;

    cf_builder_int(b, "a.b", 1, 0, 0, 0);
    cf_builder_abort(b); /* must not leak; ASan checks this */

    /* Every accessor must survive a NULL configuration: a caller who
       ignored the create result should get a fallback, not a crash. */
    check(cf_get_int(NULL, "a.b", 42) == 42, "NULL config returns fallback");
    check(cf_get_text(NULL, "a.b", "x") != NULL, "NULL config text");
    check(cf_explain(NULL, "a.b", NULL, 0) == 0, "NULL config explains nothing");
    check(cf_count(NULL) == 0, "NULL config has no keys");
    check(cf_digest(NULL) == 0, "NULL config has no digest");
    cf_config_free(NULL);

    check(cf_builder_create(NULL, &err) == NULL, "NULL builder fails");
    check(err.code == CF_E_MEMORY, "and says why");
}

static void test_sizing_without_a_buffer(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_app_builder("myapp", 1, argv0);
    cf_config*  cfg;
    static char buf[4096];
    size_t      n, m;

    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cfg = cf_builder_create(b, &err);
    if (cfg == NULL) {
        check(0, "builds");
        return;
    }

    n = cf_inspect(cfg, NULL, 0);
    m = cf_inspect(cfg, buf, sizeof buf);
    check(n == m && n > 0, "a caller can size the buffer by measuring first");

    {
        char   small[8];
        size_t t = cf_inspect(cfg, small, sizeof small);
        check(t == n, "a truncated render still reports the full length");
        check(small[sizeof small - 1] == '\0', "and terminates");
    }
    cf_config_free(cfg);
}

/* D-06 left this open: without a key list the environment can only be
   enumerated through the escaped spelling, because MYAPP_A_B_C could be
   four different keys. A contract closes the ambiguity, so Tier 1 takes
   the spelling people actually write. */
static void test_natural_env_spelling(void) {
    static char  e0[]  = "MYAPP_SERVER_PORT=4242";
    static char* env[] = {e0, NULL};
    cf_error     err   = CF_ERROR_INIT;
    cf_builder*  b;
    cf_config*   cfg;
    static char  buf[2048];

    /* Tier 0: no contract, so only the escaped spelling is enumerable
       and the natural one reads as a different key. */
    b = cf_build(NULL);
    cf_builder_env(b, "env", CF_RANK_ENV, "MYAPP_", env);
    cfg = cf_builder_create(b, &err);
    check(cfg != NULL, "tier 0 builds");
    if (cfg != NULL) {
        check(!cf_has(cfg, "server.port"),
              "without a contract the natural spelling is another key");
        check(cf_get_int(cfg, "server_port", 0) == 4242, "and it is read literally");
        cf_config_free(cfg);
    }

    /* Tier 1: the key list makes it unambiguous. */
    b = cf_build(NULL);
    cf_builder_env(b, "env", CF_RANK_ENV, "MYAPP_", env);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cfg = cf_builder_create(b, &err);
    check(cfg != NULL, "tier 1 builds");
    if (cfg == NULL) {
        printf("  %s\n", err.message);
        return;
    }

    check(cf_get_int(cfg, "server.port", 0) == 4242,
          "with a contract the natural spelling is accepted");

    /* And it says where it came from, in the spelling that was used. */
    cf_explain(cfg, "server.port", buf, sizeof buf);
    has(buf, "MYAPP_SERVER_PORT", "the variable name is reported as written");
    cf_config_free(cfg);
}

/* Setting the same parameter two ways is worth seeing rather than
   resolving silently. */
static void test_both_spellings(void) {
    static char  e0[]  = "MYAPP_SERVER__PORT=1111";
    static char  e1[]  = "MYAPP_SERVER_PORT=2222";
    static char* env[] = {e0, e1, NULL};
    cf_error     err   = CF_ERROR_INIT;
    cf_builder*  b     = cf_build(NULL);
    cf_config*   cfg;
    static char  buf[2048];

    cf_builder_env(b, "env", CF_RANK_ENV, "MYAPP_", env);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cfg = cf_builder_create(b, &err);
    if (cfg == NULL) {
        check(0, "builds");
        return;
    }

    check(cf_get_int(cfg, "server.port", 0) == 1111, "the unambiguous spelling wins");
    cf_explain(cfg, "server.port", buf, sizeof buf);
    has(buf, "MYAPP_SERVER__PORT", "the winner is named");
    has(buf, "MYAPP_SERVER_PORT", "and so is the other one");
    has(buf, "shadowed", "which did not silently disappear");
    cf_config_free(cfg);
}

/* Per-parameter policy. The specification writes it as
   "database.password: Environment > Local", and the definition carries
   exactly that list. */
static void test_policy(void) {
    static const char* const only_env[] = {"env", NULL};
    static char              c1[]       = "--db.password=from-cli";
    static char*             cargv[]    = {a0, c1, NULL};
    static char              e1[]       = "MYAPP_DB__PASSWORD=from-env";
    static char*             cenv[]     = {e1, NULL};
    cf_error                 err        = CF_ERROR_INIT;
    cf_builder*              b;
    cf_config*               cfg;
    cf_def                   d;
    static char              buf[2048];

    b = cf_build(NULL);
    cf_builder_cli(b, "cli", CF_RANK_CLI, 2, cargv);
    cf_builder_env(b, "env", CF_RANK_ENV, "MYAPP_", cenv);
    cf_def_init(&d, CF_TYPE_TEXT, "db.password");
    d.policy = only_env;
    cf_builder_define(b, &d);
    cfg = cf_builder_create(b, &err);

    check(cfg != NULL, "a policy builds");
    if (cfg == NULL) {
        printf("  %s\n", err.message);
        return;
    }

    /* The command line normally wins and here it must not: the whole
       point of a per-parameter policy is that one key departs from the
       global order. */
    check(strcmp(cf_get_text(cfg, "db.password", ""), "from-env") == 0,
          "the policy beats the global precedence");

    /* A violation is visible, not silent. Refusing to start would let
       any user stop the program by passing a locked option, and for a
       secret the damage is done the moment it reaches the command line
       - so the useful response is to say so loudly. */
    cf_explain(cfg, "db.password", buf, sizeof buf);
    has(buf, "rejected", "the forbidden source is marked");
    has(buf, "may only be set from env", "with the policy in the reason");
    has(buf, "from-cli", "and the value it offered is still shown");
    cf_config_free(cfg);
}

/* A policy is an order, not just a set: naming two sources says which
   of the two wins. */
/* A policy naming only sources this build does not have means the key
   can never be set, which is a typo rather than an intention. */
/* A policy says who may set a key; nobody sets a default. With no
   permitted source holding a value, the key is its default - here and in
   a configuration derived from this one - and not left without one. */
static void test_policy_keeps_default(void) {
    static const char* const only_env[] = {"env", NULL};
    static char              c1[]       = "--update.channel=nightly";
    static char*             cargv[]    = {a0, c1, NULL};
    static char*             cenv[]     = {NULL};
    static char              buf[2048];
    cf_error                 err = CF_ERROR_INIT;
    cf_builder*              b   = cf_build(NULL);
    cf_config*               cfg;
    cf_config*               d;
    cf_pair                  p;
    cf_def                   def;

    cf_builder_cli(b, "cli", CF_RANK_CLI, 2, cargv);
    cf_builder_env(b, "env", CF_RANK_ENV, "MYAPP_", cenv);
    cf_def_init(&def, CF_TYPE_TEXT, "update.channel");
    def.policy      = only_env;
    def.has_default = CF_TRUE;
    cf_value_text(&def.default_value, "stable", 6);
    cf_builder_define(b, &def);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cfg = cf_builder_create(b, &err);
    check(cfg != NULL, "a policy with a default builds");
    if (cfg == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(strcmp(cf_get_text(cfg, "update.channel", "?"), "stable") == 0,
          "the default is the answer when no permitted source has one");
    cf_explain(cfg, "update.channel", buf, sizeof buf);
    has(buf, "may only be set from env", "the command line is still refused");
    has(buf, "\"stable\"              selected", "and the default selected");

    p.key   = "server.port";
    p.value = "9000";
    d       = cf_derive(cfg, "admin", &p, 1, &err);
    check(d != NULL && strcmp(cf_get_text(d, "update.channel", "?"), "stable") == 0,
          "and in a derived configuration too");
    cf_config_free(d);
    cf_config_free(cfg);
}

static void test_policy_typo(void) {
    static const char* const nowhere[] = {"envrionment", NULL};
    cf_error                 err       = CF_ERROR_INIT;
    cf_builder*              b         = cf_build(NULL);
    cf_def                   d;

    cf_builder_env(b, "env", CF_RANK_ENV, "MYAPP_", NULL);
    cf_def_init(&d, CF_TYPE_TEXT, "db.password");
    d.policy = nowhere;
    cf_builder_define(b, &d);

    check(cf_builder_create(b, &err) == NULL, "an unreachable policy fails");
    check(err.code == CF_E_POLICY, "reported as a policy error");
    has(err.message, "nothing could ever set it", "and says what is wrong");
    has(err.message, "envrionment", "naming the source that does not exist");
}

static void test_policy_reorders(void) {
    static const char* const file_first[] = {"file", "cli", NULL};
    static char              c1[]         = "--tls.min=1.2";
    static char*             cargv[]      = {a0, c1, NULL};
    static cf_pair           pairs[]      = {{"tls.min", "1.3"}};
    cf_error                 err          = CF_ERROR_INIT;
    cf_builder*              b            = cf_build(NULL);
    cf_config*               cfg;
    cf_def                   d;
    static char              buf[2048];

    cf_builder_cli(b, "cli", CF_RANK_CLI, 2, cargv);
    cf_builder_memory(b, "file", CF_RANK_SYS_FILE, pairs, 1);
    cf_def_init(&d, CF_TYPE_TEXT, "tls.min");
    d.policy = file_first;
    cf_builder_define(b, &d);
    cfg = cf_builder_create(b, &err);
    if (cfg == NULL) {
        check(0, "builds");
        printf("  %s\n", err.message);
        return;
    }
    /* An operator-controlled setting surviving a command line that
       tries to override it, which is the case this exists for. */
    check(strcmp(cf_get_text(cfg, "tls.min", ""), "1.3") == 0,
          "the lower-ranked source wins where the policy says so");
    cf_explain(cfg, "tls.min", buf, sizeof buf);
    has(buf, "policy puts file first", "and the loser is told why");
    cf_config_free(cfg);
}

int main(void) {
    test_tier0();
    test_policy();
    test_policy_reorders();
    test_policy_typo();
    test_policy_keeps_default();
    test_natural_env_spelling();
    test_both_spellings();
    test_one_error_site();
    test_deferred_builder_error();
    test_defaults_are_a_source();
    test_prefix_derivation();
    test_digest_and_report();
    test_abort_and_null_safety();
    test_sizing_without_a_buffer();

    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
