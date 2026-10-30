/*
 * test_null.c - every entry point of the facade, handed nothing.
 *
 * A configuration library is called from code that has already gone
 * wrong somewhere: a builder that could not be made, a lookup with a
 * key that was never set, a getter asked for the wrong type. Each of
 * those has one acceptable answer - a refusal or the caller's own
 * fallback - and the unacceptable one is a crash. Every call here gets
 * a NULL where it can take one, or a value of the wrong kind, and the
 * test is that it answers.
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

/* A NULL builder refuses every call and creates nothing. */
static void test_null_builder(void) {
    cf_error err = CF_ERROR_INIT;
    cf_def   d;

    cf_def_init(&d, CF_TYPE_TEXT, "a");
    check(!cf_builder_memory(NULL, "m", 0, NULL, 0), "memory");
    check(!cf_builder_env(NULL, "e", 0, "P_", NULL), "env");
    check(!cf_builder_cli(NULL, "c", 0, 0, NULL), "cli");
    check(!cf_builder_file(NULL, "f", 0, "x.ini", NULL, 0), "file");
    check(!cf_builder_file_codec(NULL, "f", 0, "x.ini", NULL, 0), "file_codec");
    check(!cf_builder_dotenv(NULL, "d", 0, ".env", "P_", 0), "dotenv");
    check(!cf_builder_dir(NULL, "d", 0, "conf.d", NULL, 0), "dir");
    check(!cf_builder_secrets(NULL, "s", 0, "secrets", NULL, 0), "secrets");
    check(!cf_builder_registry(NULL, "r", 0, "HKCU\\Software\\X", 0), "registry");
    check(!cf_builder_store(NULL, "s", 0, "x.db", 0), "store");
    check(!cf_builder_replay(NULL, "r", 0, "x.json", 0), "replay");
    check(!cf_builder_writable(NULL, "s"), "writable");
    check(!cf_builder_source(NULL, NULL), "source");
    check(!cf_builder_define(NULL, &d), "define");
    check(!cf_builder_open(NULL, "plugins"), "open");
    check(!cf_builder_bool(NULL, "a", CF_FALSE, 0), "bool");
    check(!cf_builder_int(NULL, "a", 0, 0, 0, 0), "int");
    check(!cf_builder_text(NULL, "a", NULL, 0), "text");
#if CF_WITH_REAL
    check(!cf_builder_real(NULL, "a", 0.0, 0.0, 0.0, 0), "real");
#endif
    cf_builder_unknown(NULL, CF_UNKNOWN_REJECT);
    cf_builder_abort(NULL);
    check(cf_builder_create(NULL, &err) == NULL && err.message[0] != '\0',
          "create refuses, and says why");
    check(cf_builder_create(NULL, NULL) == NULL, "even with nowhere to say it");
}

/* A builder that is fine, handed NULL in its other arguments. */
static void test_null_arguments(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  c;

    check(!cf_builder_writable(b, NULL), "a grant to no name");
    check(!cf_builder_source(b, NULL), "no source");
    check(!cf_builder_define(b, NULL), "no definition");
    check(!cf_builder_open(b, NULL), "no prefix");
    c = cf_builder_create(b, &err);
    cf_config_free(c);
}

/* Reads from no configuration return the fallback; reads of a key of
   another type, too. */
static void test_reads(void) {
    static const cf_pair pairs[] = {{"n", "1"}, {"t", "x"}, {"b", "true"}};
    cf_error             err     = CF_ERROR_INIT;
    cf_builder*          b       = cf_build(NULL);
    cf_config*           c;
    cf_bool              bv = CF_FALSE;
    cf_i64               iv = 0;
    const char*          tv = NULL;
    size_t               n  = 0;
    char                 buf[64];

    check(cf_get_bool(NULL, "b", CF_TRUE) == CF_TRUE, "bool from nothing");
    check(cf_get_int(NULL, "n", 7) == 7, "int from nothing");
    check(strcmp(cf_get_text(NULL, "t", "f"), "f") == 0, "text from nothing");
#if CF_WITH_REAL
    check(cf_get_real(NULL, "r", 1.5) == 1.5, "real from nothing");
#endif
    check(cf_get_text_n(NULL, "t", &n, "f") != NULL, "text_n from nothing");
    check(cf_try_int(NULL, "n", &iv) != CF_OK, "try from nothing");
    check(cf_type_of(NULL, "n") == CF_TYPE_NULL, "type of nothing");
    check(!cf_has(NULL, "n"), "has in nothing");
    check(cf_explain(NULL, "n", buf, sizeof buf) == 0, "explain nothing");
    check(cf_inspect(NULL, buf, sizeof buf) == 0, "inspect nothing");
    check(cf_count(NULL) == 0, "count nothing");
    check(cf_digest(NULL) == 0 || cf_digest(NULL) != 0, "digest of nothing answers");
    check(!cf_changed(NULL), "nothing has not changed");
    check(cf_watch_count(NULL) == 0 && cf_watch_path(NULL, 0) == NULL,
          "watches nothing");
    check(cf_watcher_open(NULL, NULL, &err) == NULL, "a watcher over nothing");
    check(cf_derive(NULL, "s", NULL, 0, &err) == NULL, "derive from nothing");
    check(cf_derive_in(NULL, NULL, "s", NULL, 0, &err) == NULL,
          "derive_in from nothing");
    check(!cf_write(NULL, "s", NULL, 0, NULL, &err), "write to nothing");
    cf_config_ref(NULL);
    cf_config_free(NULL);

    cf_builder_memory(b, "m", 0, pairs, 3);
    c = cf_builder_create(b, &err);
    check(c != NULL, "a small configuration");
    if (c == NULL) {
        return;
    }
    check(cf_get_bool(c, "t", CF_TRUE) == CF_TRUE,
          "bool of a text key is the fallback");
    check(cf_get_int(c, "t", 9) == 9, "int of a text key is the fallback");
    check(strcmp(cf_get_text(c, "n", "f"), "f") == 0,
          "text of an int key is the fallback");
#if CF_WITH_REAL
    check(cf_get_real(c, "t", 2.5) == 2.5, "real of a text key is the fallback");
#endif
    check(cf_get_int(c, NULL, 4) == 4, "a NULL key is the fallback");
    check(!cf_has(c, NULL), "has no NULL key");
    check(cf_type_of(c, NULL) == CF_TYPE_NULL, "a NULL key has no type");
    check(cf_try_bool(c, "t", &bv) != CF_OK, "try bool of text refuses");
    check(cf_try_int(c, "b", &iv) != CF_OK, "try int of a bool refuses");
    check(cf_try_text(c, "n", &tv, &n) != CF_OK, "try text of an int refuses");
    check(cf_try_int(c, "missing", &iv) != CF_OK, "try of a missing key refuses");
    check(cf_try_int(c, "n", NULL) == CF_OK,
          "try with nowhere to put it still answers");
    check(cf_watch_path(c, 99) == NULL, "a watch index out of range");
    check(cf_explain(c, NULL, buf, sizeof buf) == 0, "explain of a NULL key");
    check(!cf_write(c, NULL, NULL, 0, NULL, &err), "write to no target");
    check(!cf_write(c, "m", NULL, 3, NULL, &err), "write of no items");
    check(cf_derive_in(c, NULL, "s", NULL, 0, &err) == NULL, "derive into no arena");
    cf_config_free(c);
}

int main(void) {
    test_null_builder();
    test_null_arguments();
    test_reads();
    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
