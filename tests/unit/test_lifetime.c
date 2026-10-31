/*
 * test_lifetime.c - no string is borrowed past the call that took it.
 *
 * Every name, path, prefix and definition string here is a heap copy,
 * scribbled over and freed before the configuration is created. A
 * library that kept a pointer to one would read freed memory: the
 * sanitize preset's AddressSanitizer reports it as a use after free, and
 * elsewhere the scribbling shows in what explain() and the reads say.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int  g_fail = 0, g_run = 0;
static void check(int c, const char* w) {
    ++g_run;
    if (!c) {
        ++g_fail;
        printf("FAIL: %s\n", w);
    }
}

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
    const char* allowed[3];
    size_t      i;

    /* The values a memory source hands over are needed until create,
       and not after: they are freed once it returns. */
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

    cf_builder_memory(b, name, CF_RANK_USER_FILE, pairs, 3);
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

    c = cf_builder_create(b, &err);
    for (i = 0; i < 3; ++i) {
        gone(keys[i]);
        gone(values[i]);
    }
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
    cf_config_free(c);
    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
