/*
 * migration.c - renaming a key without breaking anybody's file.
 *
 * Version 1 of a program read db.host. Version 2 calls it
 * database.host, because there is now database.port and database.name
 * beside it. Every file written for version 1 still says db.host, on
 * machines nobody will log in to this month.
 *
 * The old key stays in the contract, CF_DEPRECATED, with replaced_by
 * naming the new one. A value written under the old name then
 * configures the new key - below the new spelling in the same file, and
 * by ordinary precedence across sources - and explain() says where it
 * was written and under which name, which is the line a person needs
 * to update the file. Doctor's CFD009 names the same place in CI.
 *
 *     ./migration
 *
 * The suite checks its output.
 */

#include <configforge/configforge.h>

#include <stdio.h>

#define FILE_NAME "app.ini"

static cf_config* load(cf_error* err) {
    cf_builder* b = cf_build(NULL);
    cf_def      old;

    cf_builder_file(b, "app.ini", CF_RANK_USER_FILE, FILE_NAME, NULL, 0);
    cf_builder_text(b, "database.host", "localhost", 0);
    cf_builder_int(b, "database.port", 5432, 1, 65535, 0);
    cf_def_init(&old, CF_TYPE_TEXT, "db.host");
    old.flags       = CF_DEPRECATED;
    old.replaced_by = "database.host";
    cf_builder_define(b, &old);
    cf_builder_unknown(b, CF_UNKNOWN_REJECT);
    return cf_builder_create(b, err);
}

static void run(const char* title, const char* text) {
    static char why[2048];
    cf_error    err = CF_ERROR_INIT;
    cf_config*  cfg;
    FILE*       f = fopen(FILE_NAME, "wb");

    if (f != NULL) {
        fputs(text, f);
        fclose(f);
    }
    cfg = load(&err);
    printf("%s\n", title);
    if (cfg == NULL) {
        printf("  refused: %s\n\n", err.message);
        return;
    }
    printf("  connecting to %s:%d\n", cf_get_text(cfg, "database.host", "?"),
           (int)cf_get_int(cfg, "database.port", 0));
    cf_explain(cfg, "database.host", why, sizeof why);
    fputs(why, stdout);
    fputs("\n", stdout);
    cf_config_free(cfg);
}

int main(void) {
    run("1. a file written for version 1", "[db]\nhost = db1.internal\n");
    run("2. a file half updated: the new spelling wins",
        "[database]\nhost = db2.internal\n[db]\nhost = db1.internal\n");
    remove(FILE_NAME);
    return 0;
}
