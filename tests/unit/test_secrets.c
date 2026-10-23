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
static void lacks(const char* hay, const char* needle, const char* w) {
    ++g_run;
    if (hay != NULL && strstr(hay, needle) != NULL) {
        ++g_fail;
        printf("FAIL: %s  found \"%s\" in \"%s\"\n", w, needle, hay);
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
    has(err.message, "not a valid int", "but says what was wrong with it");
    cf_config_free(c);

    b = cf_build(NULL);
    cf_builder_memory(b, "memory", CF_RANK_USER_FILE, bad_range, 1);
    cf_builder_int(b, "device.pin", 0, 0, 9999, CF_SENSITIVE);
    c = cf_builder_create(b, &err);
    check(c == NULL, "an out-of-range sensitive value is refused");
    lacks(err.message, "987654", "and the error does not quote it");
    has(err.message, "outside 0..9999", "but names the range");
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
        has(buf, "outside 0..9999", "with its reason");
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

int main(void) {
    test_explain_and_inspect();
    test_refusals();
    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
