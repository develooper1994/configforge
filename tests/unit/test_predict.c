/*
 * test_predict.c - sizes guessed, then checked (D-76): the key space is
 * sized from a document's lines or from the generation before, and
 * whatever the guess - too small, too large, none - the keys read are
 * exactly the keys in the file. The guess only chooses a first capacity;
 * the growth that was always there is the fallback, and these are the
 * documents that make the guess wrong.
 */

#include <configforge/configforge.h>
#include <configforge/cf_source.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cf_test.h"

/* A document of `n` keys, section s, key k<i> = "v<i>" padded by `pad`
   bytes: INI one to a line, or JSON all on one line (nothing to count),
   with `comments` comment lines before the first key (INI). */
static char* document(int json, size_t n, size_t pad, size_t comments) {
    size_t cap = 64 + comments * 80 + n * (pad + 48);
    char*  t   = (char*)malloc(cap);
    char*  p   = (char*)malloc(pad + 1);
    size_t at  = 0;
    size_t i;

    if (t == NULL || p == NULL) {
        free(t);
        free(p);
        return NULL;
    }
    memset(p, 'p', pad);
    p[pad] = '\0';
    at += (size_t)sprintf(t + at, "%s", json ? "{\"s\":{" : "[s]\n");
    for (i = 0; i < comments; ++i) {
        at += (size_t)sprintf(t + at, "%s",
                              "; a comment, nothing in it to read as a key\n");
    }
    for (i = 0; i < n; ++i) {
        if (json) {
            at += (size_t)sprintf(t + at, "\"k%lu\":\"v%lu%s\"%s", (unsigned long)i,
                                  (unsigned long)i, p, (i + 1 < n) ? "," : "");
        } else {
            at += (size_t)sprintf(t + at, "k%lu = v%lu%s\n", (unsigned long)i,
                                  (unsigned long)i, p);
        }
    }
    if (json) {
        at += (size_t)sprintf(t + at, "}}");
    }
    t[at] = '\0';
    free(p);
    return t;
}

static int put(const char* path, int json, size_t n, size_t pad, size_t comments) {
    char* t  = document(json, n, pad, comments);
    int   ok = (t != NULL) && spill(path, t);
    free(t);
    return ok;
}

/* The file read back: every key there, the first and the last right. */
static void read_back(const char* what, const char* path, const char* format,
                      size_t n) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  c;
    char        key[32];
    char        want[32];

    cf_builder_file(b, "f", CF_RANK_USER_FILE, path, format, 0);
    c = cf_builder_create(b, &err);
    check(c != NULL, what);
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(cf_count(c) == n, what);
    if (n > 0) {
        sprintf(key, "s.k%lu", (unsigned long)(n - 1));
        sprintf(want, "v%lu", (unsigned long)(n - 1));
        check(strncmp(cf_get_text(c, key, ""), want, strlen(want)) == 0, what);
        check(strncmp(cf_get_text(c, "s.k0", ""), "v0", 2) == 0, what);
    }
    cf_config_free(c);
}

static void test_guess_right_small_large_none(void) {
    /* no keys at all, one, a few, many: the guess is 0, 1, ... 20000 */
    check(put("cf_predict.ini", 0, 0, 0, 0), "wrote an empty section");
    read_back("a document with no key", "cf_predict.ini", "ini", 0);
    check(put("cf_predict.ini", 0, 1, 0, 0), "wrote one key");
    read_back("a document of one key", "cf_predict.ini", "ini", 1);
    check(put("cf_predict.ini", 0, 5, 0, 0), "wrote five keys");
    read_back("a document of five keys", "cf_predict.ini", "ini", 5);
    check(put("cf_predict.ini", 0, 20000, 0, 0), "wrote 20000 keys");
    read_back("a document of 20000 keys", "cf_predict.ini", "ini", 20000);
    remove("cf_predict.ini");
}

static void test_guess_too_small(void) {
    /* all on one line: nothing to count, the whole of it by doubling */
    check(put("cf_predict.json", 1, 5000, 0, 0), "wrote minified JSON");
    read_back("5000 keys on one line", "cf_predict.json", "json", 5000);
    remove("cf_predict.json");
}

static void test_guess_too_large(void) {
    /* long values: few keys in many bytes; and comments: many lines, no key */
    check(put("cf_predict.ini", 0, 40, 4000, 0), "wrote long values");
    read_back("long values", "cf_predict.ini", "ini", 40);
    check(put("cf_predict.ini", 0, 3, 0, 20000), "wrote a commented file");
    read_back("a file of comments", "cf_predict.ini", "ini", 3);
    remove("cf_predict.ini");
}

static char g_small[96 * 1024];
static char g_big[4 * 1024 * 1024];

static void test_fixed_arena(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b;
    cf_config*  c;

    /* a fixed arena has a budget the caller chose, and no guess is spent
       against it: what fits, fits, and what does not fails as before */
    check(put("cf_predict.ini", 0, 1500, 0, 0), "wrote 1500 keys");
    b = cf_build_in(cf_arena_fixed(g_big, sizeof g_big));
    cf_builder_file(b, "f", CF_RANK_USER_FILE, "cf_predict.ini", "ini", 0);
    c = cf_builder_create(b, &err);
    check(c != NULL && cf_count(c) == 1500, "1500 keys in a fixed arena");
    if (c != NULL) {
        cf_config_free(c);
    }

    b = cf_build_in(cf_arena_fixed(g_small, sizeof g_small));
    cf_builder_file(b, "f", CF_RANK_USER_FILE, "cf_predict.ini", "ini", 0);
    c = cf_builder_create(b, &err);
    check(c == NULL, "1500 keys do not fit 96 KB: refused, as before");
    if (c != NULL) {
        cf_config_free(c);
    }

    check(put("cf_predict.ini", 0, 100, 0, 0), "wrote 100 keys");
    b = cf_build_in(cf_arena_fixed(g_small, sizeof g_small));
    cf_builder_file(b, "f", CF_RANK_USER_FILE, "cf_predict.ini", "ini", 0);
    c = cf_builder_create(b, &err);
    check(c != NULL && cf_count(c) == 100, "100 keys fit 96 KB, and are all there");
    if (c != NULL) {
        cf_config_free(c);
    }
    remove("cf_predict.ini");
}

/* The capacity a file's guess gives a fresh key space: the source loaded
   into one directly, so nothing else has added to it. 0 when it failed. */
static size_t reserved_for(const char* path, const char* format, size_t* keys) {
    cf_arena*   ar = cf_arena_new(NULL, 0);
    cf_keyspace ks;
    cf_source   s;
    void*       st;
    size_t      cap = 0;

    *keys = 0;
    if (ar == NULL) {
        return 0;
    }
    st = cf_arena_calloc(ar, cf_source_file_state_size());
    if (st != NULL &&
        cf_source_file(&s, "f", 0, path, format, 0, st, cf_source_file_state_size()) &&
        cf_ks_init(&ks, ar, 16) && cf_source_load(&s, &ks)) {
        cap   = ks.ccand;
        *keys = ks.ncand;
    }
    cf_arena_free(ar);
    return cap;
}

static void test_guess_bounded_by_the_bytes(void) {
    const size_t lines = 200000;
    size_t       n     = 0;
    char*        t     = (char*)malloc(lines * 2 + 64);
    size_t       keys;
    size_t       cap;
    size_t       i;

    /* one key whose value is a string of 200,000 one-character lines:
       every line starts like a key, but a record cannot come from fewer
       than four bytes (k=v and a line break), so the reservation is held
       to a record per four bytes of input, not a record per line */
    if (t == NULL) {
        check(0, "allocated the long string");
        return;
    }
    /* copied by its known size, so that what is written is seen to fit */
    memcpy(t, "s = \"\"\"\n", 8);
    n = 8;
    for (i = 0; i < lines; ++i) {
        t[n++] = 'a';
        t[n++] = '\n';
    }
    memcpy(t + n, "\"\"\"\n", 5);
    n += 4;
    check(spill("cf_predict.toml", t), "wrote one string of many lines");
    free(t);
    cap = reserved_for("cf_predict.toml", "toml", &keys);
    check(keys == 1, "the string is one key");
    check(cap > 0 && cap <= n / 4,
          "the reservation is at most a record per four bytes of input");
    remove("cf_predict.toml");

    /* an ordinary file of 1,000 keys still gets its capacity up front:
       exactly 1000, where doubling from 16 would have stopped at 1024 */
    check(put("cf_predict.ini", 0, 1000, 0, 0), "wrote 1000 keys");
    cap = reserved_for("cf_predict.ini", "ini", &keys);
    check(keys == 1000, "1000 keys read");
    check(cap == 1000, "and 1000 records reserved before the first");
    remove("cf_predict.ini");
}

/* ---------------------------------------------------------------- */

static const char* g_path = "cf_predict_live.ini";

static cf_builder* build(void* user) {
    cf_builder* b = cf_build(NULL);
    (void)user;
    cf_builder_file(b, "f", CF_RANK_USER_FILE, g_path, "ini", 0);
    return b;
}

static void reload_to(cf_live* l, const char* what, size_t n) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c;

    check(put(g_path, 0, n, 0, 0), what);
    check(cf_live_reload(l, &err) != CF_LIVE_FAILED, what);
    c = cf_live_get(l);
    check(c != NULL && cf_count(c) == n, what);
    if (c != NULL && n > 0) {
        char key[32];
        sprintf(key, "s.k%lu", (unsigned long)(n - 1));
        check(strncmp(cf_get_text(c, key, ""), "v", 1) == 0, what);
    }
    cf_config_free(c);
}

static void test_reload_after_a_different_size(void) {
    cf_error err = CF_ERROR_INIT;
    cf_live* l;

    /* the generation before is the guess for the next: it is wrong when a
       file shrinks to nothing, grows past it, or comes back */
    check(put(g_path, 0, 300, 0, 0), "wrote the first generation");
    l = cf_live_open(CF_LIVE_MANUAL, build, NULL, NULL, NULL, &err);
    check(l != NULL, "opened on 300 keys");
    if (l == NULL) {
        return;
    }
    reload_to(l, "a generation of 3 after 300 (guess too large)", 3);
    reload_to(l, "a generation of 4000 after 3 (guess too small)", 4000);
    reload_to(l, "an empty generation after 4000", 0);
    reload_to(l, "a generation of 300 after none (no guess)", 300);
    reload_to(l, "the same size again", 300);
    cf_live_close(l);
    remove(g_path);
}

int main(void) {
    test_guess_right_small_large_none();
    test_guess_too_small();
    test_guess_too_large();
    test_fixed_arena();
    test_guess_bounded_by_the_bytes();
    test_reload_after_a_different_size();
    return cf_test_done();
}
