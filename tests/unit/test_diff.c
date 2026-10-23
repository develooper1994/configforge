/*
 * test_diff.c - what changed between two configurations, and where the
 * new value came from.
 *
 * The case it is for: a program watches its files, rebuilds, and wants
 * to log what the reload did. Every kind of change is here - a value
 * that moved, a key that appeared and one that went, and a value that
 * stayed the same but now comes from somewhere else - with a secret
 * among them that must not reach the log.
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

static cf_bool spill(const char* path, const char* text) {
    FILE* f = fopen(path, "wb");
    if (f == NULL) {
        return CF_FALSE;
    }
    fputs(text, f);
    fclose(f);
    return CF_TRUE;
}

/* The same program twice: the definitions do not change, the file
   does. */
static cf_config* load(const char* text) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  c;

    if (!spill("cf_diff.ini", text)) {
        return NULL;
    }
    cf_builder_file(b, "file", CF_RANK_USER_FILE, "cf_diff.ini", NULL, 0);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_builder_int(b, "cache.size", 64, 1, 4096, 0);
    cf_builder_text(b, "db.password", NULL, CF_SENSITIVE);
    cf_builder_text(b, "log.level", NULL, 0);
    cf_builder_text(b, "old.key", NULL, 0);
    c = cf_builder_create(b, &err);
    if (c == NULL) {
        printf("  %s\n", err.message);
    }
    return c;
}

typedef struct tally {
    int added, removed, value, origin, calls;
    int stop_after;
} tally;

static cf_bool count_kinds(void* ctx, const char* key, size_t klen, cf_change kind,
                           const cf_candidate* before, const cf_candidate* after) {
    tally* t = (tally*)ctx;
    (void)key;
    (void)klen;
    ++t->calls;
    switch (kind) {
    case CF_CHANGE_ADDED:
        t->added += (before == NULL && after != NULL) ? 1 : 100;
        break;
    case CF_CHANGE_REMOVED:
        t->removed += (before != NULL && after == NULL) ? 1 : 100;
        break;
    case CF_CHANGE_VALUE:
        t->value += (before != NULL && after != NULL) ? 1 : 100;
        break;
    case CF_CHANGE_ORIGIN:
        t->origin += (before != NULL && after != NULL) ? 1 : 100;
        break;
    }
    return (t->stop_after == 0 || t->calls < t->stop_after) ? CF_TRUE : CF_FALSE;
}

static void test_a_reload(void) {
    /* Declared rather than written into the call: clang-format 18 and 22
       lay out a string continued inside an argument list differently. */
    static const char before_ini[] = "[server]\nport = 8080\n"
                                     "[db]\npassword = first-secret\n"
                                     "[old]\nkey = gone-soon\n";
    static const char after_ini[]  = "[server]\nport = 9443\n"
                                     "[cache]\nsize = 64\n"
                                     "[db]\npassword = second-secret-longer\n"
                                     "[log]\nlevel = debug\n";
    cf_config*        before       = load(before_ini);
    cf_config*        after        = load(after_ini);
    static char       text[2048];
    tally             t;
    size_t            n;

    check(before != NULL && after != NULL, "both configurations build");
    if (before == NULL || after == NULL) {
        cf_config_free(before);
        cf_config_free(after);
        return;
    }

    memset(&t, 0, sizeof t);
    n = cf_diff(before, after, count_kinds, &t);
    check(n == 5, "five keys changed");
    check(t.value == 2, "two values moved: the port and the password");
    check(t.added == 1, "one key appeared");
    check(t.removed == 1, "one went");
    check(t.origin == 1,
          "and one kept its value but now comes from the file, not the default");

    cf_diff_text(before, after, text, sizeof text);
    has(text,
        "server.port: 8080 -> 9443 (file cf_diff.ini:2:", "a moved value, and where");
    has(text, "log.level: added, \"debug\" (file cf_diff.ini:", "an added key");
    has(text,
        "old.key: removed, was \"gone-soon\" (file cf_diff.ini:", "a removed one");
    has(text,
        "cache.size: 64, now from file cf_diff.ini:", "a value with a new origin");
    has(text, ", was from default", "and the one it had before");
    has(text, "db.password: changed, <sensitive> (file", "a secret, changed");
    check(strstr(text, "first-secret") == NULL, "without its old value");
    check(strstr(text, "second-secret") == NULL, "or its new one");

    /* Measure, then write: the same length. */
    check(cf_diff_text(before, after, NULL, 0) == strlen(text), "measuring agrees");

    /* Stopping early. */
    memset(&t, 0, sizeof t);
    t.stop_after = 2;
    cf_diff(before, after, count_kinds, &t);
    check(t.calls == 2, "the callback can stop the walk");

    check(cf_diff(after, after, NULL, NULL) == 0,
          "a configuration has no changes from itself");

    cf_config_free(before);
    cf_config_free(after);
}

int main(void) {
    test_a_reload();
    remove("cf_diff.ini");
    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
