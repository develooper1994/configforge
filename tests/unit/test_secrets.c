/*
 * test_secrets.c - a sensitive value does not appear in a diagnostic.
 *
 * Design.md, the rules for sources and policy: secrets should not
 * automatically appear in diagnostics. explain() is the question this
 * library exists to answer, and the place a secret is most often pasted
 * - into a ticket, a chat, a log somebody else reads - so the chain is
 * shown in full and the value is not: which source won, which lost and
 * where each was set, then "<sensitive, N bytes>" where the value would
 * be. The same goes for the reason a value was refused, which explain()
 * shows and a strict definition raises as an error.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <string.h>

#include "cf_test.h"

static void test_explain_and_inspect(void) {
    static const cf_pair env_like[] = {{"db.password", "older-secret-value"}};
    cf_error             err        = CF_ERROR_INIT;
    cf_builder*          b          = cf_build(NULL);
    cf_config*           c;
    static char          buf[4096];

    check(
        spill("cf_secret.ini", "[db]\nhost = db.internal\npassword = hunter2-horse\n"),
        "wrote a file with a secret in it");
    cf_builder_memory(b, "vault", CF_RANK_DEFAULT, env_like, 1);
    cf_builder_file(b, "file", CF_RANK_USER_FILE, "cf_secret.ini", NULL, 0);
    cf_builder_text(b, "db.host", "localhost", 0);
    cf_builder_text(b, "db.password", NULL, CF_SENSITIVE);
    c = cf_builder_create(b, &err);
    check(c != NULL, "builds");
    if (c == NULL) {
        printf("  %s\n", err.message);
        remove("cf_secret.ini");
        return;
    }
    check(strcmp(cf_get_text(c, "db.password", ""), "hunter2-horse") == 0,
          "the program still reads the secret");

    cf_explain(c, "db.password", buf, sizeof buf);
    lacks(buf, "hunter2", "explain() does not show the winning secret");
    lacks(buf, "older-secret", "or the one it shadowed");
    has(buf, "<sensitive, 13 bytes>", "it shows that there is one, and its length");
    has(buf, "cf_secret.ini:3", "and the whole chain: where it was set");
    has(buf, "vault", "and the source it overrode");

    cf_explain(c, "db.host", buf, sizeof buf);
    has(buf, "db.internal", "a value that is not sensitive is shown as it is");

    cf_inspect(c, buf, sizeof buf);
    lacks(buf, "hunter2", "inspect() does not show it either");
    has(buf, "db.internal", "while showing everything else");

    cf_config_free(c);
    remove("cf_secret.ini");
}

static void test_refusals(void) {
    static const cf_pair bad_type[]  = {{"device.pin", "12a4"}};
    static const cf_pair bad_range[] = {{"device.pin", "987654"}};
    cf_error             err         = CF_ERROR_INIT;
    cf_builder*          b;
    cf_config*           c;
    static char          buf[4096];

    /* Strict: the reason becomes the error. */
    b = cf_build(NULL);
    cf_builder_memory(b, "memory", CF_RANK_USER_FILE, bad_type, 1);
    cf_builder_int(b, "device.pin", 0, 0, 9999, CF_SENSITIVE);
    c = cf_builder_create(b, &err);
    check(c == NULL, "a malformed sensitive value is refused");
    lacks(err.message, "12a4", "and the error does not quote it");
    has(err.message, "wrong type", "but says what was wrong with it");
    cf_config_free(c);

    b = cf_build(NULL);
    cf_builder_memory(b, "memory", CF_RANK_USER_FILE, bad_range, 1);
    cf_builder_int(b, "device.pin", 0, 0, 9999, CF_SENSITIVE);
    c = cf_builder_create(b, &err);
    check(c == NULL, "an out-of-range sensitive value is refused");
    lacks(err.message, "987654", "and the error does not quote it");
    has(err.message, "out of range", "but says it was out of range");
    lacks(err.message, "9999", "without the bounds, which a secret keeps to itself");
    cf_config_free(c);

    /* The same refusal, kept on the chain instead of raised, is what
       explain() shows. */
    b = cf_build(NULL);
    cf_builder_memory(b, "memory", CF_RANK_USER_FILE, bad_range, 1);
    {
        cf_def d;
        cf_def_init(&d, CF_TYPE_INT, "device.pin");
        d.flags       = CF_SENSITIVE;
        d.on_invalid  = CF_INVALID_FALLBACK;
        d.has_default = CF_TRUE;
        cf_value_int(&d.default_value, 1234);
        d.has_range = CF_TRUE;
        d.imin      = 0;
        d.imax      = 9999;
        cf_builder_define(b, &d);
    }
    c = cf_builder_create(b, &err);
    check(c != NULL, "a lenient definition falls back");
    if (c != NULL) {
        cf_explain(c, "device.pin", buf, sizeof buf);
        lacks(buf, "987654", "and explain() shows the refusal without the value");
        has(buf, "out of range", "with its kind");
        cf_config_free(c);
    }

    /* The control: without CF_SENSITIVE the value is quoted, which is
       what makes the reason useful everywhere else. */
    b = cf_build(NULL);
    cf_builder_memory(b, "memory", CF_RANK_USER_FILE, bad_range, 1);
    cf_builder_int(b, "device.pin", 0, 0, 9999, 0);
    c = cf_builder_create(b, &err);
    has(err.message, "987654", "an ordinary value is quoted in its refusal");
    cf_config_free(c);
}

/* The kind of a sensitive key's refusal, each of the seven: a marker for
   a value, refused that way, and the reason says the kind and nothing of
   the marker - not in the error a strict definition raises, not in
   explain(), not in a blind report, not in inspect(). The values are the
   marker, and for the range the digits it is, since a number cannot spell
   one. The last kind, a reference to a key with no value, fails the whole
   construction whatever the definition says, so it is only an error. */
#define KINDS 7

static const char* const kind_values[KINDS] = {
    "MARKER-s3cr3t", "987654321",        "MARKER-s3cr3t",         "MARKER-s3cr3t",
    "MARKER-s3cr3t", "MARKER-s3cr3t-${", "MARKER-s3cr3t-${nokey}"};

static const char* const kind_words[KINDS] = {"wrong type",
                                              "out of range",
                                              "not one of the permitted values",
                                              "too short",
                                              "too long",
                                              "a malformed reference",
                                              "a reference names a key with no value"};

static const char* const kind_names[KINDS] = {
    "type", "range", "allowed", "too-short", "too-long", "reference", "no-value"};

static cf_config* kind_build(int which, cf_bool lenient, cf_error* err) {
    static const char* const permitted[] = {"alpha", "beta", NULL};
    cf_pair                  pair;
    cf_builder*              b = cf_build(NULL);
    cf_def                   d;

    pair.key   = "k.v";
    pair.value = kind_values[which];
    cf_builder_memory(b, "memory", CF_RANK_USER_FILE, &pair, 1);
    cf_def_init(&d, (which <= 1) ? CF_TYPE_INT : CF_TYPE_TEXT, "k.v");
    d.flags      = CF_SENSITIVE;
    d.on_invalid = lenient ? CF_INVALID_FALLBACK : CF_INVALID_STRICT;
    if (which == 1) {
        d.has_range = CF_TRUE;
        d.imin      = 0;
        d.imax      = 9;
    } else if (which == 2) {
        d.allowed = permitted;
    } else if (which == 3) {
        d.min_len = 64;
    } else if (which == 4) {
        d.max_len = 4;
    }
    cf_builder_define(b, &d);
    return cf_builder_create(b, err);
}

/* None of what a marker is made of, or of the reference it names. */
static void no_marker(const char* text, const char* what) {
    lacks(text, "MARKER", what);
    lacks(text, "s3cr3t", what);
    lacks(text, "987654321", what);
    lacks(text, "nokey", what);
}

static void test_kinds(void) {
    static char buf[16384];
    int         which;

    for (which = 0; which < KINDS; ++which) {
        cf_error   err = CF_ERROR_INIT;
        cf_config* c   = kind_build(which, CF_FALSE, &err);

        check(c == NULL, "a sensitive value refused for its kind is refused");
        has(err.message, kind_words[which], "the error names the kind");
        no_marker(err.message, "and nothing of the value");
        cf_config_free(c);
        if (which == KINDS - 1) {
            break; /* no definition makes this one a candidate's refusal */
        }

        c = kind_build(which, CF_TRUE, &err);
        check(c != NULL, "and under fallback it builds");
        if (c == NULL) {
            continue;
        }
        cf_explain(c, "k.v", buf, sizeof buf);
        has(buf, kind_words[which], "explain() names the kind");
        no_marker(buf, "and nothing of the value");
        cf_inspect(c, buf, sizeof buf);
        no_marker(buf, "inspect() shows nothing of it");
        if (cf_has_feature("report")) {
            cf_report_info info = CF_REPORT_INFO_INIT;
            char           field[64];
            info.mode = CF_REPORT_BLIND;
            cf_report(c, &info, buf, sizeof buf);
            has(buf, kind_words[which], "a blind report names the kind");
            sprintf(field, "\"reason_kind\": \"%s\"", kind_names[which]);
            has(buf, field, "and writes its short name for a reader");
            no_marker(buf, "and nothing of the value");
        }
        cf_config_free(c);
    }
}

/* A secret written under a key's old name. The old name is CF_DEPRECATED
   with replaced_by naming the sensitive one, and is not itself marked
   sensitive - nobody thinks to - so it takes that from what replaced it.
   Its entry stays in the key space beside the new one, and everything
   that reads the old entry has to see the secret as a secret. */
static cf_config* secrets_renamed(const char* value, const char* leak, cf_error* err) {
    cf_pair     pairs[2];
    cf_builder* b = cf_build(NULL);
    cf_def      d;

    pairs[0].key   = "db.pass";
    pairs[0].value = value;
    pairs[1].key   = "db.leak";
    pairs[1].value = leak;
    cf_builder_memory(b, "memory", CF_RANK_USER_FILE, pairs, (leak != NULL) ? 2 : 1);
    cf_builder_text(b, "db.password", NULL, CF_SENSITIVE);
    cf_builder_text(b, "db.leak", NULL, 0);
    cf_def_init(&d, CF_TYPE_TEXT, "db.pass");
    d.flags       = CF_DEPRECATED;
    d.replaced_by = "db.password";
    cf_builder_define(b, &d);
    return cf_builder_create(b, err);
}

static void test_old_name(void) {
    static char buf[16384];
    cf_error    err    = CF_ERROR_INIT;
    cf_config*  before = secrets_renamed("M4RK3R-first", NULL, &err);
    cf_config*  after  = secrets_renamed("M4RK3R-second", NULL, &err);
    cf_config*  c;

    check(before != NULL && after != NULL, "a secret under the old name builds");
    if (before == NULL || after == NULL) {
        printf("  %s\n", err.message);
        cf_config_free(before);
        cf_config_free(after);
        return;
    }
    check(strcmp(cf_get_text(before, "db.password", ""), "M4RK3R-first") == 0,
          "and configures the new name");

    cf_explain(before, "db.pass", buf, sizeof buf);
    lacks(buf, "M4RK3R", "explain() of the old name does not show it");
    cf_explain(before, "db.password", buf, sizeof buf);
    lacks(buf, "M4RK3R", "nor of the new one");
    if (cf_has_feature("report")) {
        cf_report_info info = CF_REPORT_INFO_INIT;
        info.mode           = CF_REPORT_BLIND;
        cf_report(before, &info, buf, sizeof buf);
        lacks(buf, "M4RK3R", "a blind report does not show it");
    }
    cf_diff_text(before, after, buf, sizeof buf);
    lacks(buf, "M4RK3R", "nor does a diff");
    cf_diff_json(before, after, buf, sizeof buf);
    lacks(buf, "M4RK3R", "in either form");
    cf_config_free(before);
    cf_config_free(after);

    /* A plain key may not show it through the old name either. */
    c = secrets_renamed("M4RK3R-first", "pw=${db.pass}", &err);
    check(c == NULL, "a plain key referring to the old name is refused");
    has(err.message, "would show the secret", "as it is for the new name");
    lacks(err.message, "M4RK3R", "and the error does not show it");
    cf_config_free(c);
}

int main(void) {
    test_explain_and_inspect();
    test_refusals();
    test_kinds();
    test_old_name();
    return cf_test_done();
}
