/*
 * test_lifetime.c - what a builder borrows, and for how long.
 *
 * Every name, path, prefix and definition string here is a heap copy,
 * scribbled over and freed before the configuration is created. A
 * library that kept a pointer to one would read freed memory: the
 * sanitize preset's AddressSanitizer reports it as a use after free, and
 * elsewhere the scribbling shows in what explain() and the reads say.
 *
 * The pairs of a memory source, an environment and a command line are
 * the exception: the builder keeps the pointer and reads it when it
 * creates the configuration, so they are freed just after that, not
 * before, and what the configuration says must be its own copy.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cf_test.h"

/* A heap copy the test frees later. */
static char* heap(const char* s) {
    size_t n = strlen(s) + 1;
    char*  p = (char*)malloc(n);
    if (p != NULL) {
        memcpy(p, s, n);
    }
    return p;
}

/* Overwritten, then freed: nothing may read it again. */
static void gone(char* p) {
    if (p != NULL) {
        memset(p, 'X', strlen(p));
        free(p);
    }
}

int main(void) {
    static char why[1024];
    cf_pair     pairs[3];
    char*       keys[3];
    char*       values[3];
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  c;
    cf_def      d;
    char*       name     = heap("site-file");
    char*       key      = heap("log.level");
    char*       desc     = heap("how much to say");
    char*       allowed0 = heap("debug");
    char*       allowed1 = heap("info");
    char*       prefix   = heap("plugins");
    char*       cli_name = heap("launcher");
    char*       env_name = heap("container");
    char*       env_pfx  = heap("MYAPP_");
    char*       args[3];
    char*       vars[2];
    const char* allowed[3];
    size_t      i;

    /* What a memory source, an environment and a command line hand over
       is needed until create, and not after: it is freed once it
       returns. */
    args[0]   = heap("prog");
    args[1]   = heap("--plugins.b.y=sunrise");
    args[2]   = NULL;
    vars[0]   = heap("MYAPP_PLUGINS__C__Z=sunset");
    vars[1]   = NULL;
    keys[0]   = heap("server.port");
    values[0] = heap("9443");
    keys[1]   = heap("log.level");
    values[1] = heap("debug");
    keys[2]   = heap("plugins.a.x");
    values[2] = heap("1");
    for (i = 0; i < 3; ++i) {
        pairs[i].key   = keys[i];
        pairs[i].value = values[i];
    }
    allowed[0] = allowed0;
    allowed[1] = allowed1;
    allowed[2] = NULL;
    check(name && key && desc && allowed0 && allowed1 && prefix, "allocated");
    check(cli_name && env_name && env_pfx && args[0] && args[1] && vars[0],
          "allocated, the command line and the environment too");

    cf_builder_memory(b, name, CF_RANK_USER_FILE, pairs, 3);
    cf_builder_env(b, env_name, CF_RANK_ENV, env_pfx, vars);
    cf_builder_cli(b, cli_name, CF_RANK_CLI, 2, args);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_def_init(&d, CF_TYPE_TEXT, key);
    d.description = desc;
    d.allowed     = allowed;
    cf_builder_define(b, &d);
    cf_builder_open(b, prefix);
    cf_builder_unknown(b, CF_UNKNOWN_REJECT);

    gone(name);
    gone(key);
    gone(desc);
    gone(allowed0);
    gone(allowed1);
    gone(prefix);
    gone(cli_name);
    gone(env_name);
    gone(env_pfx);

    c = cf_builder_create(b, &err);
    for (i = 0; i < 3; ++i) {
        gone(keys[i]);
        gone(values[i]);
    }
    gone(args[0]);
    gone(args[1]);
    gone(vars[0]);
    check(c != NULL, "created after every string it was handed is gone");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return 1;
    }
    check(cf_get_int(c, "server.port", 0) == 9443,
          "a value from freed pairs is the copy");
    check(strcmp(cf_get_text(c, "log.level", ""), "debug") == 0,
          "the allowed spellings were copied: debug is still one");
    check(cf_has(c, "plugins.a.x"), "the open prefix was copied");
    (void)cf_explain(c, "log.level", why, sizeof why);
    check(strstr(why, "site-file") != NULL && strstr(why, "XXX") == NULL,
          "explain() names the source by a copy of its name");
    check(strcmp(cf_get_text(c, "plugins.b.y", ""), "sunrise") == 0,
          "a value from a freed command line is the copy");
    check(strcmp(cf_get_text(c, "plugins.c.z", ""), "sunset") == 0,
          "and one from a freed environment, found by a prefix that was copied");
    (void)cf_explain(c, "plugins.b.y", why, sizeof why);
    check(strstr(why, "launcher") != NULL && strstr(why, "argv[1]") != NULL &&
              strstr(why, "XXX") == NULL,
          "explain() names the command line and the argument it read");
    (void)cf_explain(c, "plugins.c.z", why, sizeof why);
    check(strstr(why, "container") != NULL &&
              strstr(why, "MYAPP_PLUGINS__C__Z") != NULL && strstr(why, "XXX") == NULL,
          "and the environment and the variable");
    cf_config_free(c);
    return cf_test_done();
}
