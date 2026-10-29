/*
 * flags.c - feature flags, rolled out by host and killed in an incident.
 *
 * A service ships every feature off (features.ini). The deployment drops
 * a file onto its canary hosts that turns two of them on
 * (conf.d/50-canary.ini). Then search_v2 misbehaves at three in the
 * morning, and whoever is on call kills it with one environment
 * variable, SHOP_FEATURES__SEARCH_V2=false, without a deploy.
 *
 * The program does not list its features: "features" is an open prefix,
 * and it asks which are configured. The morning after, explain() says
 * which layer turned search_v2 off, and what it beat - the question a
 * postmortem asks first.
 *
 * The files are in examples/c/flags/, shared with the C++ and Rust
 * versions; the suite runs this there.
 *
 *     cd examples/c/flags && ../../../build/examples/c/flags
 */

#include <configforge/configforge.h>

#include <stdio.h>

int main(void) {
    static char  kill[] = "SHOP_FEATURES__SEARCH_V2=false";
    static char* env[]  = {kill, NULL};
    static char  why[2048];
    cf_error     err = CF_ERROR_INIT;
    cf_builder*  b   = cf_build(NULL);
    cf_config*   cfg;
    cf_iter      it;

    cf_builder_file(b, "features.ini", CF_RANK_SYS_FILE, "features.ini", NULL, 0);
    cf_builder_dir(b, "conf.d", CF_RANK_USER_FILE, "conf.d", NULL, CF_OPTIONAL);
    cf_builder_env(b, "env", CF_RANK_ENV, "SHOP_", env);
    cf_builder_open(b, "features"); /* the flags are discovered, not declared */
    cf_builder_unknown(b, CF_UNKNOWN_REJECT);
    cfg = cf_builder_create(b, &err);
    if (cfg == NULL) {
        fprintf(stderr, "flags: %s\n", err.message);
        return 1;
    }

    cf_iter_children(&it, cfg, "features");
    while (cf_iter_next(&it)) {
        char key[CF_KEY_MAX];
        cf_fmt(key, sizeof key, "features.%S", it.name, it.namelen);
        printf("%-14.*s %s\n", (int)it.namelen, it.name,
               cf_get_bool(cfg, key, CF_FALSE) ? "on" : "off");
    }
    printf("\n");
    cf_explain(cfg, "features.search_v2", why, sizeof why);
    fputs(why, stdout);
    cf_config_free(cfg);
    return 0;
}
