/*
 * test_interpolate.c - references, ${key} (D-10): precedence first,
 * references after, the result typed by the contract, and every
 * refusal naming the key - a cycle, a key with no value, a secret a
 * plain key would show.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cf_test.h"

/* A configuration from `n` pairs in a file-like source and, above it,
   `m` more as if typed on a command line. */
static cf_config* build(const cf_pair* file, size_t n, const cf_pair* cli, size_t m,
                        cf_error* err) {
    cf_builder* b = cf_build(NULL);

    cf_builder_memory(b, "file", CF_RANK_USER_FILE, file, n);
    if (m > 0) {
        cf_builder_memory(b, "cli", CF_RANK_CLI, cli, m);
    }
    cf_builder_text(b, "server.host", "localhost", 0);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_builder_text(b, "server.url", NULL, 0);
    cf_builder_text(b, "db.password", NULL, CF_SENSITIVE);
    cf_builder_text(b, "db.dsn", NULL, CF_SENSITIVE);
    cf_builder_text(b, "db.leak", NULL, 0);
    cf_builder_int(b, "admin.port", 1, 1, 65535, 0);
    return cf_builder_create(b, err);
}

static void test_basic(void) {
    static const cf_pair file[] = {
        {"server.host", "web1"},
        {"server.port", "9443"},
        {"server.url", "https://${server.host}:${server.port}/"},
        {"admin.port", "${server.port}"}};
    static char ex[2048];
    cf_error    err = CF_ERROR_INIT;
    cf_config*  c   = build(file, 4, NULL, 0, &err);

    check(c != NULL, "references build");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    has(cf_get_text(c, "server.url", ""), "https://web1:9443/",
        "each ${key} is its value");
    check(cf_get_int(c, "admin.port", 0) == 9443,
          "and the result is typed by the contract");
    cf_explain(c, "server.url", ex, sizeof ex);
    has(ex, "from \"https://${server.host}:${server.port}/\"",
        "explain() shows the template");
    cf_config_free(c);
}

/* Precedence chooses each key first; a reference reads the winner. */
static void test_precedence_first(void) {
    static const cf_pair file[] = {{"server.host", "web1"},
                                   {"server.url", "http://${server.host}"}};
    static const cf_pair cli[]  = {{"server.host", "web2"}};
    cf_error             err    = CF_ERROR_INIT;
    cf_config*           c      = build(file, 2, cli, 1, &err);

    check(c != NULL && strcmp(cf_get_text(c, "server.url", ""), "http://web2") == 0,
          "a reference reads what a program reading that key would");
    cf_config_free(c);
}

static void test_chain_and_escape(void) {
    static const cf_pair file[] = {
        {"a.one", "${a.two}"}, {"a.two", "${a.three}-$${literal}"}, {"a.three", "x"}};
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = build(file, 3, NULL, 0, &err);

    check(c != NULL, "a reference to a reference builds");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    has(cf_get_text(c, "a.one", ""), "x-${literal}", "each finished before it is read");
    check(strcmp(cf_get_text(c, "a.two", ""), "x-${literal}") == 0,
          "and $${ stays a literal");
    cf_config_free(c);
}

static void test_refusals(void) {
    static const cf_pair cycle[] = {{"a.one", "${a.two}"}, {"a.two", "<${a.one}>"}};
    static const cf_pair none[]  = {{"server.url", "http://${server.hots}"}};
    static const cf_pair leak[]  = {{"db.password", "hunter2"},
                                    {"db.leak", "pw=${db.password}"}};
    static const cf_pair dsn[]   = {{"db.password", "hunter2"},
                                    {"db.dsn", "postgres://u:${db.password}@db"}};
    static const cf_pair bad[]   = {{"server.url", "${HOME}/x"}};
    static char          ex[2048];
    cf_error             err = CF_ERROR_INIT;
    cf_config*           c;

    c = build(cycle, 2, NULL, 0, &err);
    check(c == NULL, "a cycle is refused");
    has(err.message, "lead back to it", "and called one");
    cf_config_free(c);

    c = build(none, 1, NULL, 0, &err);
    check(c == NULL, "a reference to a key with no value is refused");
    has(err.message, "${server.hots} names a key with no value", "naming it");
    cf_config_free(c);

    c = build(leak, 2, NULL, 0, &err);
    check(c == NULL, "a plain key may not show a secret");
    has(err.message, "would show the secret", "and says why");
    cf_config_free(c);

    c = build(dsn, 2, NULL, 0, &err);
    check(c != NULL, "a sensitive key may refer to one");
    if (c != NULL) {
        cf_explain(c, "db.dsn", ex, sizeof ex);
        check(strstr(ex, "hunter2") == NULL && strstr(ex, "${db.password}") == NULL,
              "and neither its value nor its template is explained");
    }
    cf_config_free(c);

    c = build(bad, 1, NULL, 0, &err);
    check(c == NULL, "a reference naming no key is refused");
    has(err.message, "$${", "saying how to write a literal");
    cf_config_free(c);
}

/* A result the contract refuses falls back like any refused value. */
static void test_fallback(void) {
    static const cf_pair file[] = {{"server.host", "web1"},
                                   {"workers", "${server.host}"}};
    static char          ex[2048];
    cf_error             err = CF_ERROR_INIT;
    cf_builder*          b   = cf_build(NULL);
    cf_config*           c;
    cf_def               d;

    cf_builder_memory(b, "file", CF_RANK_USER_FILE, file, 2);
    cf_builder_text(b, "server.host", NULL, 0);
    cf_def_init(&d, CF_TYPE_INT, "workers");
    d.has_default = CF_TRUE;
    cf_value_int(&d.default_value, 4);
    d.on_invalid = CF_INVALID_FALLBACK;
    cf_builder_define(b, &d);
    c = cf_builder_create(b, &err);
    check(c != NULL && cf_get_int(c, "workers", 0) == 4,
          "an expanded value of the wrong type hands over to the default");
    if (c != NULL) {
        cf_explain(c, "workers", ex, sizeof ex);
        has(ex, "\"web1\" is not a valid int", "with the reason in the chain");
    }
    cf_config_free(c);
}

/* Tier 0: no contract, so the result is inferred, as any text is. */
static void test_tier0(void) {
    static const cf_pair file[] = {{"base", "40"}, {"twice", "${base}"}};
    cf_error             err    = CF_ERROR_INIT;
    cf_builder*          b      = cf_build(NULL);
    cf_config*           c;

    cf_builder_memory(b, "file", CF_RANK_USER_FILE, file, 2);
    c = cf_builder_create(b, &err);
    check(c != NULL && cf_type_of(c, "twice") == CF_TYPE_INT &&
              cf_get_int(c, "twice", 0) == 40,
          "an undeclared result is inferred");
    cf_config_free(c);
}

/* The report records the template, and a replay is the same
   configuration without reading it again. */
static void test_report(void) {
    static const cf_pair file[] = {{"server.host", "web1"},
                                   {"server.url", "http://${server.host}"}};
    static char          rep[16384];
    cf_error             err = CF_ERROR_INIT;
    cf_config*           c   = build(file, 2, NULL, 0, &err);
    cf_config*           back;
    cf_builder*          b;
    cf_report_info       info = CF_REPORT_INFO_INIT;
    FILE*                f;

    if (c == NULL) {
        check(0, "the report test builds");
        return;
    }
    if (!cf_has_feature("report") || !cf_has_feature("json")) {
        cf_config_free(c); /* a build without the report or its reader */
        return;
    }
    info.mode = CF_REPORT_FULL;
    cf_report(c, &info, rep, sizeof rep);
    has(rep, "\"written\": \"http://${server.host}\"",
        "the report records the template");
    f = fopen("cf_interp_rep.json", "wb");
    if (f != NULL) {
        fputs(rep, f);
        fclose(f);
    }
    b = cf_build(NULL);
    cf_builder_replay(b, "replay", CF_RANK_USER_FILE, "cf_interp_rep.json", 0);
    back = cf_builder_create(b, &err);
    check(back != NULL &&
              strcmp(cf_get_text(back, "server.url", ""), "http://web1") == 0,
          "a replay keeps the result and does not read it again");
    cf_config_free(back);
    cf_config_free(c);
    remove("cf_interp_rep.json");
}

/* A refusal of a sensitive value quotes nothing from it: a reason is
   stored on the candidate, shown by explain(), written into a blind
   report and raised as the error, and a malformed reference inside a
   password is still the password. */
static void test_secret_reasons(void) {
    static const cf_pair open_ref[] = {{"db.password", "S3CR3T${MARK"}};
    static const cf_pair no_key[]   = {{"db.password", "S3CR3T${mark.key}"}};
    static char          buf[16384];
    cf_error             err = CF_ERROR_INIT;
    cf_builder*          b;
    cf_config*           c;
    cf_def               d;

    c = build(open_ref, 1, NULL, 0, &err);
    check(c == NULL, "a sensitive value with a '${' that names no key is refused");
    lacks(err.message, "MARK", "and the error does not quote the value");
    lacks(err.message, "S3CR3T", "any of it");
    has(err.message, "db.password", "but names the key");
    cf_config_free(c);

    c = build(no_key, 1, NULL, 0, &err);
    check(c == NULL, "a sensitive value naming a key with no value is refused");
    lacks(err.message, "mark.key", "and the error does not quote the name it wrote");
    has(err.message, "names a key with no value", "but says what is wrong");
    cf_config_free(c);

    /* Under fallback the same reason stays on the chain, where explain()
       and the report show it. */
    b = cf_build(NULL);
    cf_builder_memory(b, "file", CF_RANK_USER_FILE, open_ref, 1);
    cf_def_init(&d, CF_TYPE_TEXT, "db.password");
    d.flags       = CF_SENSITIVE;
    d.on_invalid  = CF_INVALID_FALLBACK;
    d.has_default = CF_TRUE;
    cf_value_text(&d.default_value, "fallback", 8);
    cf_builder_define(b, &d);
    c = cf_builder_create(b, &err);
    check(c != NULL, "a lenient sensitive definition falls back");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    cf_explain(c, "db.password", buf, sizeof buf);
    has(buf, "names no key", "explain() gives the reason");
    lacks(buf, "MARK", "without quoting the value");
    if (cf_has_feature("report")) {
        cf_report_info info = CF_REPORT_INFO_INIT;
        info.mode           = CF_REPORT_BLIND;
        cf_report(c, &info, buf, sizeof buf);
        has(buf, "names no key", "a blind report gives it too");
        lacks(buf, "MARK", "and does not quote the value either");
        lacks(buf, "S3CR3T", "any of it");
    }
    cf_config_free(c);
}

/* Each key's text, and room for the names. */
static char interp_names[64][8];
static char interp_values[64][16];

/* k0 is eight bytes and each k<n> is two of k<n-1>, so k<n> is 8 << n
   bytes from a line of a dozen. After them, `copies` keys c<n> that
   each name the last of them. */
static cf_config* fanout(size_t levels, size_t copies, const cf_allocator* a,
                         cf_error* err) {
    static cf_pair pairs[64];
    cf_builder*    b = cf_build(a);
    size_t         i;

    for (i = 0; i < levels + copies && i < 64; ++i) {
        if (i >= levels) {
            sprintf(interp_names[i], "c%u", (unsigned)(i - levels));
            sprintf(interp_values[i], "${k%u}", (unsigned)(levels - 1));
        } else if (i == 0) {
            sprintf(interp_names[i], "k%u", (unsigned)i);
            strcpy(interp_values[i], "aaaaaaaa");
        } else {
            sprintf(interp_names[i], "k%u", (unsigned)i);
            sprintf(interp_values[i], "${k%u}${k%u}", (unsigned)(i - 1),
                    (unsigned)(i - 1));
        }
        pairs[i].key   = interp_names[i];
        pairs[i].value = interp_values[i];
    }
    cf_builder_memory(b, "file", CF_RANK_USER_FILE, pairs, i);
    return cf_builder_create(b, err);
}

static cf_config* doubling(size_t levels, const cf_allocator* a, cf_error* err) {
    return fanout(levels, 0, a, err);
}

/* The largest single request, so a test can say nothing was asked for
   past the limit, and every request added up, so one can say the same
   of all of them together. */
static size_t interp_largest;
static size_t interp_total;

static void* interp_alloc(void* ctx, size_t n) {
    (void)ctx;
    if (n > interp_largest) {
        interp_largest = n;
    }
    interp_total += n;
    return malloc(n);
}

static void interp_release(void* ctx, void* p) {
    (void)ctx;
    free(p);
}

/* A few lines can double their way to any size; the expansion stops at
   CF_EXPAND_MAX, by name, before it asks for the memory. */
static void test_expansion_limit(void) {
    cf_allocator a;
    cf_error     err = CF_ERROR_INIT;
    cf_config*   c;

    a.alloc   = interp_alloc;
    a.release = interp_release;
    a.ctx     = NULL;

    /* k17 is 8 << 17 bytes, exactly the limit. */
    c = doubling(18, &a, &err);
    check(c != NULL, "an expansion of exactly the limit builds");
    if (c == NULL) {
        printf("  %s\n", err.message);
    } else {
        const char* t = cf_get_text(c, "k17", "");
        check(strlen(t) == (size_t)CF_EXPAND_MAX && t[0] == 'a' &&
                  t[CF_EXPAND_MAX - 1] == 'a',
              "and is the whole of it");
    }
    cf_config_free(c);

    interp_largest = 0;
    c              = doubling(19, &a, &err);
    check(c == NULL, "one past the limit is refused");
    has(err.message, "k18", "naming the key");
    has(err.message, "more than", "and saying it is too large");
    check(interp_largest < (size_t)CF_EXPAND_MAX + CF_EXPAND_MAX / 2,
          "without asking for the memory it would take");
    cf_config_free(c);
}

/* The limit on one key is not a limit on the file: a value just under
   it, named by enough short lines, is any size at all with no key over
   it. All the expansions together stop at CF_EXPAND_TOTAL_MAX. */
static void test_expansion_total(void) {
    cf_allocator a;
    cf_error     err = CF_ERROR_INIT;
    cf_config*   c;

    a.alloc   = interp_alloc;
    a.release = interp_release;
    a.ctx     = NULL;

    /* k1 to k17 add up to two megabytes; thirteen copies of k17 make
       fifteen, under the sixteen allowed. */
    interp_total = 0;
    c            = fanout(18, 13, &a, &err);
    check(c != NULL, "references that add up to less than the total build");
    if (c == NULL) {
        printf("  %s\n", err.message);
    } else {
        check(strlen(cf_get_text(c, "c12", "")) == (size_t)CF_EXPAND_MAX,
              "and each is the whole of its value");
    }
    cf_config_free(c);

    /* Forty copies would be forty-two megabytes. */
    interp_total = 0;
    c            = fanout(18, 40, &a, &err);
    check(c == NULL, "references that add up to more than the total are refused");
    if (c == NULL) {
        char want[64];
        sprintf(want, "more than %lu bytes", (unsigned long)CF_EXPAND_TOTAL_MAX);
        has(err.message, want, "saying the total");
        has(err.message, "in all", "and that it is the total, not one key");
    }
    check(interp_total < (size_t)CF_EXPAND_TOTAL_MAX + 4 * (size_t)CF_EXPAND_MAX,
          "without asking for the memory the rest would take");
    if (interp_total >= (size_t)CF_EXPAND_TOTAL_MAX + 4 * (size_t)CF_EXPAND_MAX) {
        printf("  %lu bytes asked for\n", (unsigned long)interp_total);
    }
    cf_config_free(c);
}

/* A referenced key whose own expansion is refused, under fallback, with
   nothing below it to fall to, has no value: the key that named it is
   refused for that, as for any key with no value. */
static void test_reference_to_nothing(void) {
    static const char* const allowed[] = {"a", "b", NULL};
    static const cf_pair     file[]    = {
        {"a", "x${name}"}, {"name", "${other}"}, {"other", "zzz"}};
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  c;
    cf_def      d;

    cf_builder_memory(b, "file", CF_RANK_USER_FILE, file, 3);
    cf_def_init(&d, CF_TYPE_TEXT, "name");
    d.allowed    = allowed;
    d.on_invalid = CF_INVALID_FALLBACK;
    cf_builder_define(b, &d);
    c = cf_builder_create(b, &err);
    check(c == NULL, "a reference to a key whose value fell to nothing is refused");
    has(err.message, "${name} names a key with no value", "naming it");
    cf_config_free(c);
}

int main(void) {
    test_basic();
    test_precedence_first();
    test_chain_and_escape();
    test_refusals();
    test_fallback();
    test_tier0();
    test_report();
    test_secret_reasons();
    test_expansion_limit();
    test_expansion_total();
    test_reference_to_nothing();
    return cf_test_done();
}
