/*
 * test_store.c - the SQLite store (Design.md, section 39): written in
 * one transaction, read back typed, and explained with what its writer
 * said; and, without CF_WITH_SQLITE, refused by name.
 */

#ifndef _WIN32
#define _POSIX_C_SOURCE 200112L /* umask, chmod, stat */
#endif

#include <configforge/configforge.h>

#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <sys/stat.h>
#include <sys/types.h>
#endif

#include "cf_test.h"

#define DB "cf_store.db"

static cf_config* load(unsigned flags, cf_error* err) {
    cf_builder* b = cf_build(NULL);
    cf_builder_store(b, "store", CF_RANK_USER_FILE, DB, flags);
    return cf_builder_create(b, err);
}

#if CF_WITH_SQLITE

static void test_write_and_read(void) {
    cf_write_item items[4];
    cf_error      err = CF_ERROR_INIT;
    cf_config*    c;
    static char   ex[2048];

    remove(DB);
    items[0].key = "server.port";
    items[0].op  = CF_WRITE_SET;
    cf_value_int(&items[0].value, 9443);
    items[1].key = "tls.enabled";
    items[1].op  = CF_WRITE_SET;
    cf_value_bool(&items[1].value, CF_TRUE);
    items[2].key = "log.level";
    items[2].op  = CF_WRITE_SET;
    cf_value_text(&items[2].value, "debug", 5);
    items[3].key = "proxy";
    items[3].op  = CF_WRITE_SET;
    cf_value_null(&items[3].value);
    check(cf_store_set(DB, items, 4, "admin, 2026-10-24: incident 42", &err),
          "a store is written, and created");
    if (err.message[0] != '\0') {
        printf("  %s\n", err.message);
    }

    c = load(0, &err);
    check(c != NULL, "and read");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(cf_type_of(c, "server.port") == CF_TYPE_INT &&
              cf_get_int(c, "server.port", 0) == 9443,
          "an int stays an int: the type travels with the value");
    check(cf_get_bool(c, "tls.enabled", CF_FALSE), "a bool");
    check(strcmp(cf_get_text(c, "log.level", ""), "debug") == 0, "a text");
    check(cf_has(c, "proxy") && cf_type_of(c, "proxy") == CF_TYPE_NULL, "and a null");

    /* What the writer said is where the value was set. */
    cf_explain(c, "server.port", ex, sizeof ex);
    has(ex, "cf_store.db: admin, 2026-10-24: incident 42",
        "explain() says who changed it, when and why");
    cf_config_free(c);
}

static void test_replace(void) {
    cf_write_item one;
    cf_error      err = CF_ERROR_INIT;
    cf_config*    c;

    one.key = "server.port";
    one.op  = CF_WRITE_SET;
    cf_value_int(&one.value, 8000);
    check(cf_store_set(DB, &one, 1, NULL, &err), "a second write");
    c = load(0, &err);
    check(c != NULL && cf_get_int(c, "server.port", 0) == 8000, "replaces the value");
    check(c != NULL && cf_get_bool(c, "tls.enabled", CF_FALSE), "and leaves the rest");
    cf_config_free(c);
}

/* One bad item refuses the whole change. */
static void test_atomic(void) {
    cf_write_item two[2];
    cf_error      err = CF_ERROR_INIT;
    cf_config*    c;

    two[0].key = "cache.size";
    two[0].op  = CF_WRITE_SET;
    cf_value_int(&two[0].value, 64);
    two[1].key = "Bad Key";
    two[1].op  = CF_WRITE_SET;
    cf_value_int(&two[1].value, 1);
    check(!cf_store_set(DB, two, 2, NULL, &err),
          "a change with an invalid key is refused");
    has(err.message, "Bad Key", "and names it");
    c = load(0, &err);
    check(c != NULL && !cf_has(c, "cache.size"), "and nothing of it was written");
    cf_config_free(c);
}

/* The contract applies to a store as to any source. */
static void test_contract(void) {
    cf_write_item one;
    cf_error      err = CF_ERROR_INIT;
    cf_builder*   b;
    cf_config*    c;

    one.key = "server.port";
    one.op  = CF_WRITE_SET;
    cf_value_int(&one.value, 99999);
    check(cf_store_set(DB, &one, 1, "a typo", &err), "a store holds what it is given");
    b = cf_build(NULL);
    cf_builder_store(b, "store", CF_RANK_USER_FILE, DB, 0);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    c = cf_builder_create(b, &err);
    check(c == NULL, "and the contract refuses what it should not hold");
    has(err.message, "99999 is outside 1..65535", "with the usual message");
    cf_config_free(c);
}

/* A running program's change, kept: granted, checked, written, and
   explained after the restart with what the writer said. */
static void test_write_granted(void) {
    cf_write_item one;
    cf_error      err = CF_ERROR_INIT;
    cf_builder*   b;
    cf_config*    c;
    static char   ex[2048];
    int           pass;

    remove(DB);
    for (pass = 0; pass < 2; ++pass) {
        b = cf_build(NULL);
        cf_builder_store(b, "store", CF_RANK_USER_FILE, DB, CF_FILE_OPTIONAL);
        cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
        cf_builder_writable(b, "store");
        c = cf_builder_create(b, &err);
        check(c != NULL, "a granted store builds, missing or not");
        if (c == NULL) {
            printf("  %s\n", err.message);
            return;
        }
        if (pass == 0) {
            one.key = "server.port";
            one.op  = CF_WRITE_SET;
            cf_value_int(&one.value, 99999);
            check(!cf_write(c, "store", &one, 1, NULL, &err),
                  "a value the contract refuses is not written");
            check(fopen(DB, "rb") == NULL, "and the store is not even created");
            cf_value_int(&one.value, 9443);
            check(cf_write(c, "store", &one, 1, "ops, 2026-10-24: move off 8080", &err),
                  "one it accepts is");
        } else {
            check(cf_get_int(c, "server.port", 0) == 9443, "and is read back");
            cf_explain(c, "server.port", ex, sizeof ex);
            has(ex, "move off 8080", "explained with what the writer said");
        }
        cf_config_free(c);
    }
}

#ifndef _WIN32

/* The permission bits of `path`, or -1 when it cannot be stat-ed. */
static int mode_of(const char* path) {
    struct stat st;

    return (stat(path, &st) == 0) ? (int)(st.st_mode & 0777) : -1;
}

/* A new store is readable and writable by its owner alone, whatever the
   umask says. */
static void test_new_store_private(void) {
    cf_write_item one;
    cf_error      err = CF_ERROR_INIT;
    mode_t        old = umask(022);
    int           pass;

    one.key = "token";
    one.op  = CF_WRITE_SET;
    cf_value_text(&one.value, "s3cret", 6);
    for (pass = 0; pass < 2; ++pass) {
        remove(DB);
        umask(pass == 0 ? 022 : 000);
        check(cf_store_set(DB, &one, 1, NULL, &err), "a new store is written");
        if (pass == 0) {
            check(mode_of(DB) == 0600, "and is 0600 under umask 022");
        } else {
            check(mode_of(DB) == 0600, "and is 0600 under umask 000");
        }
    }
    umask(old);
}

/* An existing file keeps the mode it has, and what it holds. */
static void test_existing_keeps_mode(void) {
    cf_write_item one;
    cf_error      err = CF_ERROR_INIT;
    cf_config*    c;

    one.key = "token";
    one.op  = CF_WRITE_SET;
    cf_value_text(&one.value, "kept", 4);
    remove(DB);
    check(cf_store_set(DB, &one, 1, NULL, &err), "a store to start from");
    check(chmod(DB, 0644) == 0, "made 0644");
    one.key = "other";
    check(cf_store_set(DB, &one, 1, NULL, &err), "written again");
    check(mode_of(DB) == 0644, "an existing store keeps its mode");
    c = load(0, &err);
    check(c != NULL && strcmp(cf_get_text(c, "token", ""), "kept") == 0 &&
              strcmp(cf_get_text(c, "other", ""), "kept") == 0,
          "and its contents");
    cf_config_free(c);
}

static void test_no_directory(void) {
    cf_write_item one;
    cf_error      err = CF_ERROR_INIT;

    one.key = "token";
    one.op  = CF_WRITE_SET;
    cf_value_int(&one.value, 1);
    check(!cf_store_set("cf_no_such_dir/cf_store.db", &one, 1, NULL, &err),
          "a store in a directory that is not there is refused");
    has(err.message, "cf_no_such_dir/cf_store.db", "and the error names the path");
}

#endif /* !_WIN32 */

static void test_not_a_store(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c;
    FILE*      f = fopen(DB, "wb");

    if (f != NULL) {
        fputs("not a database", f);
        fclose(f);
    }
    c = load(0, &err);
    check(c == NULL, "a file that is not a store is refused");
    has(err.message, DB, "and named");
    cf_config_free(c);
}

static void test_missing(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c;

    remove(DB);
    c = load(0, &err);
    check(c == NULL, "a missing store is an error");
    cf_config_free(c);
    c = load(CF_FILE_OPTIONAL, &err);
    check(c != NULL, "unless it is optional");
    cf_config_free(c);
    check(fopen(DB, "rb") == NULL, "and reading never creates one");
}

#else

static void test_disabled(void) {
    cf_write_item one;
    cf_error      err = CF_ERROR_INIT;
    cf_config*    c   = load(CF_FILE_OPTIONAL, &err);

    check(c == NULL, "a store is refused without CF_WITH_SQLITE, even an optional one");
    has(err.message, "CF_WITH_SQLITE", "and the error names the option");
    cf_config_free(c);

    one.key = "a";
    one.op  = CF_WRITE_SET;
    cf_value_int(&one.value, 1);
    check(!cf_store_set(DB, &one, 1, NULL, &err), "and so is writing one");
    has(err.message, "CF_WITH_SQLITE", "by name");
    check(!cf_has_feature("sqlite"), "and the build does not claim it");
}

#endif /* CF_WITH_SQLITE */

int main(void) {
#if CF_WITH_SQLITE
    check(cf_has_feature("sqlite"), "the build says it has a store");
    test_write_and_read();
    test_replace();
    test_atomic();
    test_contract();
    test_write_granted();
#ifndef _WIN32
    test_new_store_private();
    test_existing_keeps_mode();
    test_no_directory();
#else
    printf("  skipped: file modes are checked on POSIX only\n");
#endif
    test_not_a_store();
    test_missing();
#else
    test_disabled();
#endif
    remove(DB);
    return cf_test_done();
}
