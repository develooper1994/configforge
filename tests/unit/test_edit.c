/*
 * test_edit.c - cf_edit: create, update and delete as one change, held
 * to the contract and written in one transaction or not at all; and
 * cf_live_commit, which reloads after. The refusals run in every build;
 * the writes need a store, so CF_WITH_SQLITE.
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

#define DB "cf_edit.db"

/* What every build can check: an edit's own refusals, and the
   capability and permission cf_write asks at commit. */
static void test_refusals(void) {
    static const cf_pair mem[] = {{"a.b", "1"}};
    cf_error             err   = CF_ERROR_INIT;
    cf_builder*          b     = cf_build(NULL);
    cf_config*           c;
    cf_edit*             e;

    cf_builder_memory(b, "mem", CF_RANK_USER_FILE, mem, 1);
    c = cf_builder_create(b, &err);
    check(c != NULL, "created");

    e = cf_edit_begin(c, "mem", &err);
    check(e != NULL && cf_edit_set_int(e, "a.b", 2) && cf_edit_count(e) == 1,
          "an edit");
    check(cf_edit_set_int(e, "a.b", 3) && cf_edit_count(e) == 1,
          "a key set twice is one change");
    check(!cf_edit_commit(e, "note", &err) && err.code == CF_E_CAPABILITY,
          "a source that cannot be written refuses the commit");

    e = cf_edit_begin(c, "mem", &err);
    check(!cf_edit_set_text(e, "Not A Key", "x"), "a key that is not one is refused");
    check(cf_edit_set_int(e, "a.b", 9), "later sets still work");
    check(!cf_edit_commit(e, "note", &err) && err.code == CF_E_KEY &&
              strstr(err.message, "not a valid key") != NULL,
          "and commit reports the first refusal, writing nothing");

    e = cf_edit_begin(c, "nowhere", &err);
    check(!cf_edit_commit(e, NULL, &err) && strstr(err.message, "nowhere") != NULL,
          "a source of no such name is refused at commit");

    e = cf_edit_begin(c, "mem", &err);
    cf_config_free(c); /* the edit holds its own reference */
    check(cf_edit_set_bool(e, "a.b", CF_TRUE),
          "the edit outlives the caller's reference");
    cf_edit_abort(e);
    check(cf_edit_begin(NULL, "x", &err) == NULL, "no configuration, no edit");
}

#if CF_WITH_SQLITE

static cf_builder* recipe(void* user) {
    static const cf_pair file[] = {{"log.level", "info"}};
    cf_builder*          b      = cf_build(NULL);
    (void)user;
    cf_builder_memory(b, "file", CF_RANK_SYS_FILE, file, 1);
    cf_builder_store(b, "store", CF_RANK_USER_FILE, DB, CF_OPTIONAL);
    cf_builder_writable(b, "store");
    cf_builder_int(b, "pool.size", 4, 1, 64, 0);
    cf_builder_text(b, "log.level", "warn", 0);
    cf_builder_bool(b, "feature.beta", CF_FALSE, 0);
    return b;
}

static cf_config* build(cf_error* err) {
    return cf_builder_create(recipe(NULL), err);
}

static void test_crud(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c;
    cf_edit*   e;

    remove(DB);
    c = build(&err);
    check(c != NULL && cf_get_int(c, "pool.size", 0) == 4, "defaults before any edit");

    /* Create and update. */
    e = cf_edit_begin(c, "store", &err);
    cf_edit_set_int(e, "pool.size", 32);
    cf_edit_set_text(e, "log.level", "debug");
    cf_edit_set_bool(e, "feature.beta", CF_TRUE);
    check(cf_edit_commit(e, "admin, 2026-10-30: load test", &err), "committed");
    cf_config_free(c);
    c = build(&err);
    check(c != NULL && cf_get_int(c, "pool.size", 0) == 32 &&
              strcmp(cf_get_text(c, "log.level", ""), "debug") == 0 &&
              cf_get_bool(c, "feature.beta", CF_FALSE),
          "read back after a rebuild");

    /* The contract holds for the whole change: one bad value, nothing
       written. */
    e = cf_edit_begin(c, "store", &err);
    cf_edit_set_text(e, "log.level", "trace");
    cf_edit_set_int(e, "pool.size", 999);
    check(!cf_edit_commit(e, "admin", &err) && strcmp(err.key, "pool.size") == 0,
          "a value outside its range refuses the whole commit");
    cf_config_free(c);
    c = build(&err);
    check(c != NULL && strcmp(cf_get_text(c, "log.level", ""), "debug") == 0,
          "and nothing of it was written");

    /* Delete: the next source down supplies the key again. */
    e = cf_edit_begin(c, "store", &err);
    cf_edit_unset(e, "log.level");
    cf_edit_unset(e, "pool.size");
    check(cf_edit_commit(e, "admin: back to the file", &err), "deleted");
    cf_config_free(c);
    c = build(&err);
    check(c != NULL && strcmp(cf_get_text(c, "log.level", ""), "info") == 0 &&
              cf_get_int(c, "pool.size", 0) == 4,
          "the file and the default supply them again");
    cf_config_free(c);
}

static void test_live(void) {
    cf_error       err = CF_ERROR_INIT;
    cf_live*       l;
    cf_config*     c;
    cf_live_result r;

    remove(DB);
    l = cf_live_open(CF_LIVE_MANUAL, recipe, NULL, NULL, NULL, &err);
    check(l != NULL, "live: opens");
    if (l == NULL) {
        return;
    }
    c = cf_live_get(l);
    {
        cf_edit* e = cf_edit_begin(c, "store", &err);
        cf_edit_set_int(e, "pool.size", 16);
        r = cf_live_commit(l, e, "admin", &err);
    }
    cf_config_free(c);
    c = cf_live_get(l);
    check(r == CF_LIVE_SWAPPED && cf_get_int(c, "pool.size", 0) == 16,
          "cf_live_commit writes and reloads: readers see it");
    cf_config_free(c);
    cf_live_close(l);
    remove(DB);
}

#endif

int main(void) {
    test_refusals();
#if CF_WITH_SQLITE
    test_crud();
    test_live();
#endif
    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
