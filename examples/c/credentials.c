/*
 * credentials.c - a service started by systemd, and where its values came from.
 *
 * systemd hands a service its credentials as files in the directory
 * $CREDENTIALS_DIRECTORY names. cf_builder_credentials reads that directory
 * as a directory of secrets, and every value it supplies says so:
 * "systemd credential <id>", never "a file in a directory".
 *
 *     CREDENTIALS_DIRECTORY=./creds ./credentials
 *
 * The build makes the directory and sets the variable for the suite, which
 * checks every line of the output. The password is never printed, only how
 * long it is.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <string.h>

int main(void) {
    static char text[4096];
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_def      d;
    cf_config*  c;

    cf_builder_credentials(b, NULL, CF_RANK_ENV, "MYAPP_", CF_OPTIONAL);

    cf_def_init(&d, CF_TYPE_TEXT, "db.password");
    d.flags = CF_SENSITIVE;
    cf_builder_define(b, &d);
    cf_builder_text(b, "db.user", "app", 0);

    c = cf_builder_create(b, &err);
    if (c == NULL) {
        fprintf(stderr, "credentials: %s\n", err.message);
        return 1;
    }

    printf("db.user = %s\n", cf_get_text(c, "db.user", "?"));
    printf("a password of %u characters\n",
           (unsigned)strlen(cf_get_text(c, "db.password", "")));
    cf_explain(c, "db.user", text, sizeof text);
    fputs(text, stdout);

    cf_config_free(c);
    return 0;
}
