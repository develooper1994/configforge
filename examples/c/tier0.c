/*
 * tier0.c - the smallest complete program.
 *
 * This is the program the specification opens with, and it is built
 * and run by the test suite so that the first page of the
 * documentation cannot rot.
 *
 *   ./tier0
 *   ./tier0 --server.port=7000
 *   MYAPP_SERVER__PORT=9090 ./tier0
 */

#include <configforge/configforge.h>

#include <stdio.h>

int main(int argc, char** argv) {
    cf_error    err = CF_ERROR_INIT;
    cf_config*  cfg;
    cf_i64      port;
    const char* host;
    cf_bool     tls;

    cfg = cf_app("myapp", argc, argv, &err);
    if (cfg == NULL) {
        fputs(err.message, stderr);
        return 1;
    }

    port = cf_get_int(cfg, "server.port", 8080);
    host = cf_get_text(cfg, "server.host", "0.0.0.0");
    tls  = cf_get_bool(cfg, "tls.enabled", CF_FALSE);

    printf("%s:%d tls=%s\n", host, (int)port, tls ? "on" : "off");

    cf_config_free(cfg);
    return 0;
}
