/*
 * schema.c - give the editor the program's contract.
 *
 * The definitions a program declares are already a schema; this writes
 * them as one. A settings file that names it,
 *
 *     { "$schema": "./myapp.schema.json", "server": { "port": 8080 } }
 *
 * gets completion, the descriptions as hover text, and a squiggle under
 * "prot" or 99999 in most editors with no plugin - the contract reaches
 * the person writing the file before the program ever runs.
 *
 * A program would offer this as a command (--print-schema); here it is
 * the whole program.
 *
 *     ./schema > myapp.schema.json
 */

#include <configforge/configforge.h>

#include <stdio.h>

int main(void) {
    static const char* const levels[] = {"debug", "info", "warn", "error", NULL};
    static char              out[8192];
    cf_error                 err = CF_ERROR_INIT;
    cf_builder*              b   = cf_build(NULL);
    cf_config*               cfg;
    cf_def                   d;
    size_t                   n;

    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_builder_text(b, "server.host", "0.0.0.0", 0);
    cf_builder_text(b, "db.password", NULL, CF_SENSITIVE);

    cf_def_init(&d, CF_TYPE_TEXT, "log.level");
    d.description = "How much to log";
    d.allowed     = levels;
    d.has_default = CF_TRUE;
    cf_value_text(&d.default_value, "info", 4);
    cf_builder_define(b, &d);

    /* Unknown keys refused, so the schema closes its objects and an
       editor flags a misspelt key exactly as the program would. */
    cf_builder_unknown(b, CF_UNKNOWN_REJECT);

    cfg = cf_builder_create(b, &err);
    if (cfg == NULL) {
        fprintf(stderr, "schema: %s\n", err.message);
        return 1;
    }
    n = cf_schema(cfg, "myapp", out, sizeof out);
    cf_config_free(cfg);
    if (n == 0 || n >= sizeof out) {
        fprintf(stderr, "schema: this build writes no schema (CF_WITH_REPORT)\n");
        return 1;
    }
    fputs(out, stdout);
    return 0;
}
