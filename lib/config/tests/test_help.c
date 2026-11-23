/*
 * test_help.c - cf_usage: the --help text from the contract.
 *
 * One contract with a definition of each kind, the whole text compared
 * with what it should be, and then the edges: measuring, a buffer too
 * short, no configuration, no prefix, no program name, no definitions.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <string.h>

#include "cf_test.h"

/* C90 only promises string literals of 509 bytes, so the text is in
   lines, joined once. */
static const char* const golden_lines[] = {
    "Usage: myapp [options]\n",
    "\n",
    "Options:\n",
    "  --db.password <text>\n",
    "        Database password.\n",
    "        Set it in the environment.\n",
    "        env MYAPP_DB_PASSWORD; sensitive\n",
    "  --log.level <debug|info|warn>\n",
    "        Log level.\n",
    "        default \"info\"; env MYAPP_LOG_LEVEL\n",
    "  --old.port <int>\n",
    "        deprecated, use --server.port; env MYAPP_OLD_PORT\n",
    "  -p, --server.port <int>\n",
    "        Port to listen on.\n",
    "        default 8080; range 1..65535; env MYAPP_SERVER_PORT\n",
    "  --tags <text>...\n",
    "        Labels.\n",
    "        repeatable; env MYAPP_TAGS\n",
    "  --tls.enabled\n",
    "        default false; env MYAPP_TLS_ENABLED\n",
    NULL};

static char golden[1024];

static void join_golden(void) {
    size_t i;

    golden[0] = '\0';
    for (i = 0; golden_lines[i] != NULL; ++i) {
        strcat(golden, golden_lines[i]);
    }
}

static cf_config* usage_contract(void) {
    static const char* const levels[]  = {"debug", "info", "warn", NULL};
    static const char* const aliases[] = {"p", NULL};
    cf_error                 err       = CF_ERROR_INIT;
    cf_builder*              b         = cf_build(NULL);
    cf_def                   d;
    cf_config*               c;

    cf_def_init(&d, CF_TYPE_INT, "server.port");
    d.description = "Port to listen on.";
    d.aliases     = aliases;
    d.has_default = CF_TRUE;
    cf_value_int(&d.default_value, 8080);
    d.has_range = CF_TRUE;
    d.imin      = 1;
    d.imax      = 65535;
    cf_builder_define(b, &d);

    cf_def_init(&d, CF_TYPE_TEXT, "log.level");
    d.description = "Log level.";
    d.allowed     = levels;
    d.has_default = CF_TRUE;
    cf_value_text(&d.default_value, "info", 4);
    cf_builder_define(b, &d);

    cf_builder_bool(b, "tls.enabled", CF_FALSE, 0);

    cf_def_init(&d, CF_TYPE_TEXT, "db.password");
    d.description = "Database password.\nSet it in the environment.";
    d.flags       = CF_SENSITIVE;
    d.has_default = CF_TRUE;
    cf_value_text(&d.default_value, "x", 1);
    cf_builder_define(b, &d);

    cf_def_init(&d, CF_TYPE_INT, "old.port");
    d.flags       = CF_DEPRECATED;
    d.replaced_by = "server.port";
    cf_builder_define(b, &d);

    cf_def_init(&d, CF_TYPE_TEXT, "tags");
    d.description    = "Labels.";
    d.list_mode      = CF_LIST_APPEND;
    d.list_separator = ',';
    cf_builder_define(b, &d);

    c = cf_builder_create(b, &err);
    check(c != NULL, "the contract builds");
    if (c == NULL) {
        printf("  %s\n", err.message);
    }
    return c;
}

static void test_text(void) {
    static char buf[4096];
    cf_config*  c = usage_contract();
    size_t      n, m;
    char        small[20];

    if (c == NULL) {
        return;
    }
    n = cf_usage(c, "myapp", "MYAPP_", buf, sizeof buf);
    check(strcmp(buf, golden) == 0, "the whole text is as written");
    if (strcmp(buf, golden) != 0) {
        printf("--- got\n%s--- want\n%s", buf, golden);
    }
    check(n == strlen(golden), "the length returned is the text's");
    check(cf_usage(c, "myapp", "MYAPP_", NULL, 0) == n, "NULL buffer measures");
    check(strstr(buf, "default \"x\"") == NULL, "a sensitive default is not shown");

    memset(small, 'z', sizeof small);
    m = cf_usage(c, "myapp", "MYAPP_", small, sizeof small);
    check(m == n, "a short buffer still returns the full length");
    check(small[19] == '\0' && strncmp(small, golden, 19) == 0,
          "a short buffer holds the prefix that fits, terminated");

    n = cf_usage(c, "myapp", NULL, buf, sizeof buf);
    check(strstr(buf, "env ") == NULL && strstr(buf, "MYAPP") == NULL,
          "no prefix, no environment names");
    check(strstr(buf, "default 8080; range 1..65535\n") != NULL,
          "the details end where the environment name was");

    n = cf_usage(c, NULL, "MYAPP_", buf, sizeof buf);
    check(strncmp(buf, "Usage: program [options]\n", 25) == 0, "no name, 'program'");
    (void)n;
    cf_config_free(c);
}

static void test_edges(void) {
    static char buf[64];
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  c   = cf_builder_create(b, &err);

    buf[0] = 'z';
    check(cf_usage(NULL, "myapp", NULL, buf, sizeof buf) == 0 && buf[0] == '\0',
          "no configuration: nothing, and the buffer is empty");
    check(c != NULL, "an empty contract builds");
    if (c != NULL) {
        check(cf_usage(c, "myapp", "MYAPP_", buf, sizeof buf) == 23 &&
                  strcmp(buf, "Usage: myapp [options]\n") == 0,
              "no definitions: the Usage line alone");
        cf_config_free(c);
    }
}

int main(void) {
    join_golden();
    test_text();
    test_edges();
    return cf_test_done();
}
