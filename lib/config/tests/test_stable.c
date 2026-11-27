/*
 * test_stable.c - a file read twice and compared (Y-5, D-189).
 */

#include <configforge/configforge.h>
#include <configforge/cf_platform.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cf_test.h"

static unsigned calls;
static unsigned grow_every; /* 1: the file moves at every call; else the first only */

static void rewrite(void* user) {
    char text[64];

    (void)user;
    calls++;
    if (grow_every != 0 || calls == 1) {
        /* a different length each time: the stamp's size moves */
        unsigned n = 4 + calls;
        memset(text, 'x', n);
        text[n] = '\0';
        check(spill("cf_st.json", text), "rewrote the fixture");
    }
}

static cf_bool read_it(unsigned attempts, cf_bool use_between, cf_error* err,
                       const char* path, char* out, size_t cap) {
    cf_arena* ar = cf_arena_new(NULL, 0);
    cf_blob   b;
    cf_bool   ok;

    calls = 0;
    ok = cf_file_read_stable(&b, ar, path, attempts, use_between ? rewrite : NULL, NULL,
                             err);
    if (ok && b.n < cap) {
        memcpy(out, b.p, b.n);
        out[b.n] = '\0';
    } else {
        out[0] = '\0';
    }
    cf_arena_free(ar);
    return ok;
}

int main(void) {
    char     got[128];
    cf_error err = CF_ERROR_INIT;

    /* (a) a stable file */
    check(spill("cf_st.json", "{\"a\": 1}"), "wrote the fixture");
    check(read_it(0, CF_FALSE, &err, "cf_st.json", got, sizeof got),
          "a stable file reads");
    check(strcmp(got, "{\"a\": 1}") == 0, "with its bytes");

    /* A platform with no file stamps (none) reads once: the rest needs them. */
    {
        cf_stamp st;
        if (!cf_platform_stamp(".", &st)) {
            puts("skipped: no file stamps");
            return cf_test_done();
        }
    }

    /* (b) one change: read again, and the new bytes are the ones returned */
    grow_every = 0;
    check(spill("cf_st.json", "{\"a\": 1}"), "wrote the fixture");
    check(read_it(0, CF_TRUE, &err, "cf_st.json", got, sizeof got),
          "a file that moved once reads");
    check(strcmp(got, "xxxxx") == 0, "the bytes of the second read");
    check(calls == 2, "in two reads");

    /* (c) it moves at every call */
    grow_every = 1;
    check(spill("cf_st.json", "{\"a\": 1}"), "wrote the fixture");
    cf_error_clear(&err);
    check(!read_it(0, CF_TRUE, &err, "cf_st.json", got, sizeof got),
          "a file that never settles fails");
    check(err.code == CF_E_SOURCE, "as a source error");
    check(strstr(err.message, "changed while it was read") != NULL, "that says so");
    check(calls == 3, "after three reads by default");
    cf_error_clear(&err);
    check(!read_it(2, CF_TRUE, &err, "cf_st.json", got, sizeof got),
          "two attempts fail too");
    check(calls == 2, "after two reads");

    /* (d) a missing file is not retried */
    remove("cf_st_missing.json");
    cf_error_clear(&err);
    check(!read_it(0, CF_TRUE, &err, "cf_st_missing.json", got, sizeof got),
          "a missing file fails");
    check(strstr(err.message, "cannot open") != NULL, "as cf_blob_file does");
    check(calls <= 1, "with no retry");

    /* (e) a builder reads a stable file as before */
    {
        cf_builder* b;
        cf_config*  cfg;

        check(spill("cf_st.json", "{\"port\": 7}"), "wrote the fixture");
        b = cf_build(NULL);
        cf_builder_file(b, "f", CF_RANK_USER_FILE, "cf_st.json", NULL, 0);
        cf_builder_int(b, "port", 1, 1, 65535, 0);
        cfg = cf_builder_create(b, &err);
        check(cfg != NULL && cf_get_int(cfg, "port", 0) == 7,
              "a builder reads a stable file");
        if (cfg != NULL) {
            cf_config_free(cfg);
        }
    }
    return cf_test_done();
}
