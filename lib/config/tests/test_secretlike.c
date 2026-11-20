/*
 * test_secretlike.c - an undefined key named like a secret is kept as one
 * (D-84).
 *
 * The contract has db.password (CF_SENSITIVE) and db.host. The source
 * writes db.pasword, which nobody defined and which is one edit from the
 * secret, and db.hots, one edit from a plain key. The first is shown by no
 * text the library writes; the second is shown as any undefined key is.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cf_test.h"

static char sl_buf[65536];

static cf_builder* sl_builder(const char* secret_value, const char* prefix) {
    cf_builder*    b = cf_build(NULL);
    static cf_pair pairs[3];

    pairs[0].key   = "db.pasword";
    pairs[0].value = secret_value;
    pairs[1].key   = "db.hots";
    pairs[1].value = "plain-host-value";
    pairs[2].key   = NULL;
    pairs[2].value = NULL;
    cf_builder_memory(b, "mem", CF_RANK_USER_FILE, pairs, 2);
    cf_builder_text(b, "db.password", NULL, CF_SENSITIVE);
    cf_builder_text(b, "db.host", NULL, 0);
    cf_builder_unknown(b, CF_UNKNOWN_IGNORE);
    if (prefix != NULL) {
        cf_builder_open(b, prefix);
    }
    return b;
}

static cf_config* sl_make(const char* value) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = cf_builder_create(sl_builder(value, NULL), &err);
    check(c != NULL, "the configuration builds");
    return c;
}

static void sl_report(void) {
    cf_config*     c    = sl_make("MARK-LIKE-4417");
    cf_report_info info = CF_REPORT_INFO_INIT;

    if (c == NULL) {
        return;
    }
    if (cf_has_feature("report")) {
        info.mode        = CF_REPORT_BLIND;
        info.application = "secretlike";
        info.profile     = "test";
        cf_report(c, &info, sl_buf, sizeof sl_buf);
        lacks(sl_buf, "MARK-LIKE", "blind report hides the like-secret's value");
        has(sl_buf, "plain-host-value", "blind report shows a plain typo's value");
        has(sl_buf, "\"like\": \"db.password\"",
            "blind report names the secret it is like");
    }

    cf_explain(c, "db.pasword", sl_buf, sizeof sl_buf);
    lacks(sl_buf, "MARK-LIKE", "explain hides the like-secret's value");
    cf_explain(c, "db.hots", sl_buf, sizeof sl_buf);
    has(sl_buf, "plain-host-value", "explain shows a plain typo's value");
    cf_config_free(c);
}

static void sl_fingerprint(void) {
    cf_config* a = sl_make("MARK-LIKE-4417");
    cf_config* b = sl_make("MARK-LIKE-4418");
    cf_config* c = sl_make("MARK-LIKE-4417");

    if (a != NULL && b != NULL && c != NULL) {
        check(cf_fingerprint(a) != cf_fingerprint(b),
              "the fingerprint changes with the like-secret's value");
        check(cf_fingerprint(a) == cf_fingerprint(c), "the fingerprint is stable");
    }
    cf_config_free(a);
    cf_config_free(b);
    cf_config_free(c);
}

static void sl_diff(void) {
    cf_config* a = sl_make("MARK-LIKE-4417");
    cf_config* b = sl_make("MARK-LIKE-4418");

    if (a != NULL && b != NULL) {
        cf_diff_text(a, b, sl_buf, sizeof sl_buf);
        has(sl_buf, "db.pasword", "the diff names the key");
        lacks(sl_buf, "MARK-LIKE", "the diff hides the like-secret's value");
    }
    cf_config_free(a);
    cf_config_free(b);
}

static void sl_reference(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = cf_builder_create(sl_builder("${MARK", NULL), &err);

    check(c == NULL, "a malformed reference in a like-secret is refused");
    lacks(err.message, "MARK", "the refusal does not quote the like-secret");
    cf_config_free(c);
}

static void sl_open(void) {
    cf_error       err = CF_ERROR_INIT;
    cf_builder*    b   = cf_build(NULL);
    cf_config*     c;
    static cf_pair pairs[2];

    pairs[0].key   = "plugins.db.pasword";
    pairs[0].value = "open-prefix-value";
    pairs[1].key   = NULL;
    pairs[1].value = NULL;
    cf_builder_memory(b, "mem", CF_RANK_USER_FILE, pairs, 1);
    cf_builder_text(b, "db.password", NULL, CF_SENSITIVE);
    cf_builder_unknown(b, CF_UNKNOWN_IGNORE);
    cf_builder_open(b, "plugins");
    c = cf_builder_create(b, &err);
    check(c != NULL, "the open-prefix configuration builds");
    if (c != NULL) {
        cf_explain(c, "plugins.db.pasword", sl_buf, sizeof sl_buf);
        has(sl_buf, "open-prefix-value", "a key under an open prefix is shown");
        cf_config_free(c);
    }
}

int main(void) {
    sl_report();
    sl_fingerprint();
    sl_diff();
    sl_reference();
    sl_open();
    return cf_test_done();
}
