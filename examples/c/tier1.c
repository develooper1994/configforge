/*
 * tier1.c - the same program with a contract.
 *
 * The contract is what buys typo detection, range checking, required
 * parameters and a type that beats inference. Note what it does not
 * cost: there is still exactly one place where anything can fail.
 *
 *   ./tier1 --server.port=99999      range refused, names the bound
 *   ./tier1 --server.pot=1           typo, suggests the right key
 *   ./tier1 --log.level=verbose      not permitted, lists what is
 *   ./tier1 --cf.explain=server.port
 *
 * Note how the diagnostic switch is declared: a program that reads its
 * command line through this library should not also parse it itself,
 * because then the library sees a flag it was never told about. A
 * switch is just another parameter.
 */

#include <configforge/configforge.h>

#include <stdio.h>

int main(int argc, char** argv) {
    static const char* const levels[] = {"debug", "info", "warn", NULL};

    cf_error    err = CF_ERROR_INIT;
    cf_builder* b;
    cf_config*  cfg;
    cf_def      lvl;
    static char buf[8192];
    const char* which;

    b = cf_app_builder("myapp", argc, argv);

    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_builder_text(b, "server.host", "0.0.0.0", 0);
    cf_builder_bool(b, "tls.enabled", CF_FALSE, 0);
    cf_builder_text(b, "cf.explain", NULL, 0);

    cf_def_init(&lvl, CF_TYPE_TEXT, "log.level");
    lvl.allowed     = levels;
    lvl.has_default = CF_TRUE;
    cf_value_text(&lvl.default_value, "info", 4);
    cf_builder_define(b, &lvl);

    /* An unknown key is a typo far more often than it is someone
       else's setting, so this program asks for it to be refused. */
    cf_builder_unknown(b, CF_UNKNOWN_REJECT);

    cfg = cf_builder_create(b, &err);
    if (cfg == NULL) {
        fprintf(stderr, "configuration error (%s): %s\n",
                cf_code_name((cf_code)err.code), err.message);
        if (err.source[0] != '\0') {
            fprintf(stderr, "  set at: %s\n", err.source);
        }
        return 1;
    }

    which = cf_get_text(cfg, "cf.explain", NULL);
    if (which != NULL) {
        cf_explain(cfg, which, buf, sizeof buf);
        fputs(buf, stdout);
        cf_config_free(cfg);
        return 0;
    }

    cf_inspect(cfg, buf, sizeof buf);
    fputs(buf, stdout);

    cf_config_free(cfg);
    return 0;
}
