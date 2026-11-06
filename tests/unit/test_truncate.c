/*
 * test_truncate.c - where an array ends.
 *
 * An array's length says where it ends. What lies past it is not in the
 * configuration: it is not read, listed, written, or counted into the
 * digest, whichever source set it and whichever source set the length.
 * explain() and the report still show it, as truncated and with the
 * length that took it out, because "why is my element missing" is the
 * question somebody asks.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <string.h>

#include "cf_test.h"

/* Two sources, the second the higher: the elements come from one and the
   length from the other, which is how an array gets shortened. */
static cf_config* two(const cf_pair* low, size_t nlow, const cf_pair* high,
                      size_t nhigh, cf_error* err) {
    cf_builder* b = cf_build(NULL);

    cf_builder_memory(b, "low", CF_RANK_USER_FILE, low, nlow);
    cf_builder_memory(b, "high", CF_RANK_CLI, high, nhigh);
    return cf_builder_create(b, err);
}

static cf_config* one(const cf_pair* pairs, size_t n, cf_error* err) {
    cf_builder* b = cf_build(NULL);

    cf_builder_memory(b, "file", CF_RANK_USER_FILE, pairs, n);
    return cf_builder_create(b, err);
}

/* What a walk of a prefix yields, in order and space apart, so that a test
   says what the whole sequence is rather than that one name is in it. */
static const char* names(const cf_config* c, const char* prefix) {
    static char out[512];
    cf_iter     it;
    size_t      at = 0;

    out[0] = '\0';
    if (!cf_iter_keys(&it, c, prefix)) {
        return "(no walk)";
    }
    while (cf_iter_next(&it) && at + it.namelen + 2 < sizeof out) {
        if (at > 0) {
            out[at++] = ' ';
        }
        memcpy(out + at, it.name, it.namelen);
        at += it.namelen;
        out[at] = '\0';
    }
    return out;
}

static const char* explained(const cf_config* c, const char* key) {
    static char buf[4096];

    buf[0] = '\0';
    cf_explain(c, key, buf, sizeof buf);
    return buf;
}

/* ---------------------------------------------------------------- */

static void test_a_higher_source_shortens_an_array(void) {
    static const cf_pair low[] = {
        {"t.0", "a"}, {"t.1", "b"}, {"t.2", "c"}, {"t.count", "3"}};
    static const cf_pair high[]  = {{"t.count", "1"}};
    static const cf_pair never[] = {{"t.0", "a"}};
    cf_error             err     = CF_ERROR_INIT;
    cf_config*           c       = two(low, 4, high, 1, &err);
    cf_config*           clean;

    check(c != NULL, "builds");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }

    check(cf_array_count(c, "t") == 1, "the winning length is the length");
    check(cf_has(c, "t.0") && strcmp(cf_get_text(c, "t.0", ""), "a") == 0,
          "the element inside it is there");
    check(!cf_has(c, "t.1"), "the next is not, though a source set it");
    check(!cf_has(c, "t.2"), "nor the one after");
    check(strcmp(cf_get_text(c, "t.1", "none"), "none") == 0,
          "a read gets its fallback");
    check(cf_type_of(c, "t.1") == CF_TYPE_NULL, "and the type of nothing");
    check(strcmp(names(c, "t"), "0 count") == 0,
          "a walk of the array stops where it ends");

    /* Cut, but not silent: the reader's question is why. */
    {
        const char* why = explained(c, "t.1");

        has(why, "t.1 = <no value>", "explain says there is no value");
        has(why, "\"b\"", "and still shows what was set");
        has(why, "truncated: past the end: t.count is 1, from high",
            "and what took it out");
    }

    /* The digest is of what the configuration holds, so it is the one a
       configuration that never had the elements has. */
    clean = two(never, 1, high, 1, &err);
    check(clean != NULL && cf_digest(c) == cf_digest(clean),
          "the digest is that of a configuration that never had them");
    cf_config_free(clean);

#if CF_WITH_EMIT && CF_WITH_JSON
    {
        static char buf[1024];

        check(cf_emit(c, "json", CF_EMIT_ALL, 0, buf, sizeof buf, &err) > 0,
              "it is written out");
        has(buf, "\"a\"", "with the element inside the length");
        lacks(buf, "\"b\"", "and not the one past it");
        lacks(buf, "\"c\"", "nor the next");
    }
#endif
#if CF_WITH_REPORT
    {
        static char    buf[16384];
        cf_report_info info = CF_REPORT_INFO_INIT;

        info.mode = CF_REPORT_FULL;
        check(cf_report(c, &info, buf, sizeof buf) > 0, "the report is written");
        has(buf, "\"outcome\": \"truncated\"", "and says what was cut");
        has(buf, "past the end: t.count is 1, from high", "and by what");
    }
#endif
    cf_config_free(c);
}

/* A table that happens to have a member called count - a pool size, a
   retry count - is not an array, and a member of it whose name is a
   number is not an element. */
static void test_a_table_with_a_count_is_left_alone(void) {
    static const cf_pair pairs[] = {{"pool.count", "2"},
                                    {"pool.size", "10"},
                                    {"pool.5", "x"},
                                    {"retry.count", "3"}};
    cf_error             err     = CF_ERROR_INIT;
    cf_config*           c       = one(pairs, 4, &err);

    check(c != NULL, "builds");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(cf_has(c, "pool.5"), "a digit for a name does not make a member an element");
    check(cf_get_int(c, "pool.size", 0) == 10, "the rest of the table is there");
    check(cf_get_int(c, "pool.count", 0) == 2,
          "and the count, which is a setting here");
    check(cf_get_int(c, "retry.count", 0) == 3,
          "a count with nothing beside it is one too");
    cf_config_free(c);

    /* A member called count that has members of its own is not the
       length, so it does not make the group an array either. */
    {
        static const cf_pair nested[] = {
            {"cfg.0", "a"}, {"cfg.1", "b"}, {"cfg.count", "1"}, {"cfg.count.max", "5"}};

        c = one(nested, 4, &err);
        check(c != NULL && cf_has(c, "cfg.1") && cf_has(c, "cfg.count.max"),
              "a count that is a table is not a length");
        cf_config_free(c);
    }
}

/* A key nothing supplied a value for - here one its policy refused - is
   not in the document, so it does not make the group a table. */
static void test_a_member_with_no_value_makes_no_table(void) {
    static const char* const only_other[] = {"other", NULL};
    static const cf_pair     pairs[]      = {
        {"t.0", "a"}, {"t.1", "b"}, {"t.count", "1"}, {"t.token", "x"}};
    static const cf_pair other[] = {{"unrelated", "1"}};
    cf_error             err     = CF_ERROR_INIT;
    cf_builder*          b       = cf_build(NULL);
    cf_def               d;
    cf_config*           c;

    cf_builder_memory(b, "file", CF_RANK_USER_FILE, pairs, 4);
    cf_builder_memory(b, "other", CF_RANK_SYS_FILE, other, 1);
    cf_def_init(&d, CF_TYPE_TEXT, "t.token");
    d.policy = only_other;
    cf_builder_define(b, &d);
    c = cf_builder_create(b, &err);
    check(c != NULL, "builds");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(!cf_has(c, "t.token"), "the key the policy refused has no value");
    check(cf_has(c, "t.0") && !cf_has(c, "t.1"), "and what it is beside is an array");
    cf_config_free(c);
}

static void test_an_array_of_tables(void) {
    static const cf_pair low[]  = {{"servers.0.name", "a"}, {"servers.0.port", "1"},
                                   {"servers.1.name", "b"}, {"servers.1.port", "2"},
                                   {"servers.2.name", "c"}, {"servers.2.port", "3"},
                                   {"servers.count", "3"}};
    static const cf_pair high[] = {{"servers.count", "2"}};
    cf_error             err    = CF_ERROR_INIT;
    cf_config*           c      = two(low, 7, high, 1, &err);

    check(c != NULL, "builds");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(cf_array_count(c, "servers") == 2, "two servers");
    check(cf_get_int(c, "servers.1.port", 0) == 2, "the last one kept has all it had");
    check(!cf_has(c, "servers.2.name") && !cf_has(c, "servers.2.port"),
          "the one past the end loses every member");
    check(strcmp(names(c, "servers"), "0.name 0.port 1.name 1.port count") == 0,
          "and a walk sees two");
    cf_config_free(c);
}

static void test_nested_arrays(void) {
    cf_error err = CF_ERROR_INIT;

    /* The inner one is shortened. */
    {
        static const cf_pair low[] = {
            {"t.0.u.0", "p"}, {"t.0.u.1", "q"}, {"t.0.u.count", "2"}, {"t.count", "1"}};
        static const cf_pair high[] = {{"t.0.u.count", "1"}};
        cf_config*           c      = two(low, 4, high, 1, &err);

        check(c != NULL, "builds with the inner array shortened");
        if (c != NULL) {
            check(cf_has(c, "t.0.u.0"), "the inner element inside its length");
            check(!cf_has(c, "t.0.u.1"), "and not the one past it");
            check(cf_array_count(c, "t") == 1 && cf_array_count(c, "t.0.u") == 1,
                  "the outer array is as it was");
            cf_config_free(c);
        }
    }

    /* The outer one is, and takes an inner array, length and all, with the
       element it belonged to. */
    {
        static const cf_pair pairs[] = {{"t.0.u.0", "p"},
                                        {"t.0.u.count", "1"},
                                        {"t.1.u.0", "q"},
                                        {"t.1.u.count", "1"},
                                        {"t.count", "1"}};
        cf_config*           c       = one(pairs, 5, &err);

        check(c != NULL, "builds with the outer array shortened");
        if (c != NULL) {
            check(cf_has(c, "t.0.u.0"), "the element inside it keeps its own array");
            check(!cf_has(c, "t.1.u.0"), "the one past it loses its inner elements");
            check(!cf_has(c, "t.1.u.count"), "and the length of its inner array");
            check(cf_array_count(c, "t.1.u") == 0, "which leaves that array empty");
            cf_config_free(c);
        }
    }

    /* Both at once: what is cut is cut for the outermost length that
       reaches it, because it says why the element is not there, and the
       inner one is only a length of something that is no longer
       anywhere. */
    {
        static const cf_pair pairs[] = {
            {"t.1.u.0", "a"}, {"t.1.u.1", "b"}, {"t.1.u.count", "1"}, {"t.count", "1"}};
        cf_config* c = one(pairs, 4, &err);

        check(c != NULL, "builds with both shortened");
        if (c != NULL) {
            const char* why = explained(c, "t.1.u.1");

            check(!cf_has(c, "t.1.u.0") && !cf_has(c, "t.1.u.1"),
                  "nothing is left of it");
            has(why, "past the end: t.count is 1", "the reason is the outer length");
            lacks(why, "t.1.u.count is", "and not the inner one it was beyond as well");
            cf_config_free(c);
        }
    }
}

static void test_a_length_of_nothing(void) {
    static const cf_pair pairs[] = {{"t.0", "a"}, {"t.1", "b"}, {"t.count", "0"}};
    cf_error             err     = CF_ERROR_INIT;
    cf_config*           c       = one(pairs, 3, &err);

    check(c != NULL, "builds");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(cf_array_count(c, "t") == 0, "empty");
    check(!cf_has(c, "t.0") && !cf_has(c, "t.1"), "with no element in it");
    check(strcmp(names(c, "t"), "count") == 0, "a walk finds only the length");
    cf_config_free(c);
}

/* A length that is not a count fixes nothing, and a template is not a
   number until its references are in, which is after the cut. */
static void test_a_length_that_fixes_nothing(void) {
    static const cf_pair negative[] = {{"t.0", "a"},
                                       {"t.1", "b"},
                                       {"t.99999999999999999999999999", "c"},
                                       {"t.count", "-1"}};
    static const cf_pair text[]     = {{"t.0", "a"}, {"t.1", "b"}, {"t.count", "many"}};
    static const cf_pair template_[] = {
        {"n", "1"}, {"t.0", "a"}, {"t.1", "b"}, {"t.count", "${n}"}};
    cf_error   err = CF_ERROR_INIT;
    cf_config* c;

    c = one(negative, 4, &err);
    check(c != NULL && cf_has(c, "t.0") && cf_has(c, "t.1") &&
              cf_has(c, "t.99999999999999999999999999"),
          "a negative length cuts nothing, as large an index as there is");
    cf_config_free(c);

    c = one(text, 3, &err);
    check(c != NULL && cf_has(c, "t.0") && cf_has(c, "t.1"), "text is not a length");
    cf_config_free(c);

    c = one(template_, 4, &err);
    check(c != NULL && cf_has(c, "t.0") && cf_has(c, "t.1"),
          "nor is a length that is still a template");
    cf_config_free(c);
}

/* The default of a length is a length: the configuration is then what it
   says it is, and explain() says whose it was. */
static void test_a_default_length(void) {
    static const cf_pair elements[] = {{"t.0", "a"}, {"t.1", "b"}};
    static const cf_pair longer[]   = {{"t.0", "a"}, {"t.1", "b"}, {"t.count", "2"}};
    cf_error             err        = CF_ERROR_INIT;
    cf_builder*          b          = cf_build(NULL);
    cf_config*           c;

    cf_builder_memory(b, "file", CF_RANK_USER_FILE, elements, 2);
    cf_builder_int(b, "t.count", 1, 0, 100, 0);
    c = cf_builder_create(b, &err);
    check(c != NULL, "builds");
    if (c != NULL) {
        check(cf_array_count(c, "t") == 1 && cf_has(c, "t.0") && !cf_has(c, "t.1"),
              "a default length shortens the array");
        has(explained(c, "t.1"), "past the end: t.count is 1, from the default",
            "and says that is where it came from");
        cf_config_free(c);
    }

    b = cf_build(NULL);
    cf_builder_memory(b, "file", CF_RANK_USER_FILE, longer, 3);
    cf_builder_int(b, "t.count", 1, 0, 100, 0);
    c = cf_builder_create(b, &err);
    check(c != NULL && cf_has(c, "t.1"), "a length a source sets is not the default");
    cf_config_free(c);
}

/* An index is a number, wherever the digits are written and however many
   there are. */
static void test_indexes_are_numbers(void) {
    static const cf_pair padded[] = {
        {"t.count", "2"}, {"t.000", "a"}, {"t.01", "b"},
        {"t.002", "c"},   {"t.007", "d"}, {"t.99999999999999999999999999", "e"}};
    static const cf_pair edge[] = {{"t.count", "9223372036854775807"},
                                   {"t.9223372036854775806", "a"},
                                   {"t.9223372036854775807", "b"},
                                   {"t.10000000000000000000", "c"}};
    cf_error             err    = CF_ERROR_INIT;
    cf_config*           c;

    c = one(padded, 6, &err);
    check(c != NULL, "builds");
    if (c != NULL) {
        check(cf_has(c, "t.000"), "zero with zeros is zero");
        check(cf_has(c, "t.01"), "and one with a zero is one");
        check(!cf_has(c, "t.002"), "two with zeros is not inside a length of two");
        check(!cf_has(c, "t.007"), "nor is seven");
        check(!cf_has(c, "t.99999999999999999999999999"),
              "nor is a number no integer holds");
        cf_config_free(c);
    }

    c = one(edge, 4, &err);
    check(c != NULL, "builds at the largest length");
    if (c != NULL) {
        check(cf_has(c, "t.9223372036854775806"), "the last index inside it is there");
        check(!cf_has(c, "t.9223372036854775807"), "the length itself is not an index");
        check(!cf_has(c, "t.10000000000000000000"), "and a larger number is past it");
        cf_config_free(c);
    }
}

/* A configuration derived from another is worked out again, so a longer
   length brings elements back and a length of nothing takes them all. */
static void test_a_derived_configuration_recomputes(void) {
    static const cf_pair low[] = {
        {"t.0", "a"}, {"t.1", "b"}, {"t.2", "c"}, {"t.count", "3"}};
    static const cf_pair high[]  = {{"t.count", "1"}};
    static const cf_pair wider[] = {{"t.count", "3"}};
    static const cf_pair none[]  = {{"t.count", "0"}};
    cf_error             err     = CF_ERROR_INIT;
    cf_config*           base    = two(low, 4, high, 1, &err);
    cf_config*           d;

    check(base != NULL, "builds");
    if (base == NULL) {
        printf("  %s\n", err.message);
        return;
    }

    d = cf_derive(base, "runtime", wider, 1, &err);
    check(d != NULL, "derives a longer one");
    if (d != NULL) {
        check(cf_array_count(d, "t") == 3 &&
                  strcmp(cf_get_text(d, "t.2", ""), "c") == 0,
              "its elements are back");
        check(cf_digest(d) != cf_digest(base), "and it is another configuration");
        cf_config_free(d);
    }

    d = cf_derive(base, "runtime", none, 1, &err);
    check(d != NULL, "derives an empty one");
    if (d != NULL) {
        check(cf_array_count(d, "t") == 0 && !cf_has(d, "t.0"), "with nothing in it");
        cf_config_free(d);
    }

    check(cf_array_count(base, "t") == 1 && cf_has(base, "t.0") && !cf_has(base, "t.1"),
          "the one it came from is as it was");
    cf_config_free(base);
}

#if CF_WITH_REPORT && CF_WITH_JSON
/* A report replays to the configuration it was written from, and does
   not bring back what the length had cut. */
static void test_replay(void) {
    static const cf_pair low[] = {
        {"t.0", "a"}, {"t.1", "b"}, {"t.2", "c"}, {"t.count", "3"}};
    static const cf_pair high[] = {{"t.count", "1"}};
    static char          buf[16384];
    cf_error             err  = CF_ERROR_INIT;
    cf_config*           c    = two(low, 4, high, 1, &err);
    cf_report_info       info = CF_REPORT_INFO_INIT;
    cf_builder*          b;
    cf_config*           back;

    check(c != NULL, "builds");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    info.mode = CF_REPORT_FULL;
    check(cf_report(c, &info, buf, sizeof buf) > 0, "a full report");
    check(spill("cf_trunc_rep.json", buf), "written to disk");

    b = cf_build(NULL);
    cf_builder_replay(b, "replay", CF_RANK_ENV, "cf_trunc_rep.json", 0);
    back = cf_builder_create(b, &err);
    check(back != NULL, "replays");
    if (back == NULL) {
        printf("  %s\n", err.message);
    } else {
        check(cf_digest(back) == cf_digest(c), "to the same configuration");
        check(cf_array_count(back, "t") == 1 && cf_has(back, "t.0") &&
                  !cf_has(back, "t.1"),
              "with the array as long as it was");
        cf_config_free(back);
    }
    cf_config_free(c);
    remove("cf_trunc_rep.json");
}
#endif

/* A reference reads what a program asking for that key would get, and
   that is nothing for an element that was cut. */
static void test_a_reference_to_a_cut_element(void) {
    static const cf_pair kept[] = {
        {"t.0", "a"}, {"t.1", "b"}, {"t.count", "2"}, {"msg", "${t.0}!"}};
    static const cf_pair gone[] = {
        {"t.0", "a"}, {"t.1", "b"}, {"t.count", "2"}, {"msg", "${t.1}!"}};
    static const cf_pair high[] = {{"t.count", "1"}};
    cf_error             err    = CF_ERROR_INIT;
    cf_config*           c;

    c = two(kept, 4, high, 1, &err);
    check(c != NULL, "a reference to an element that is there builds");
    if (c != NULL) {
        check(strcmp(cf_get_text(c, "msg", ""), "a!") == 0, "and reads it");
        cf_config_free(c);
    }

    err.code = CF_OK;
    c        = two(gone, 4, high, 1, &err);
    check(c == NULL, "a reference to one that was cut does not");
    check(err.code == CF_E_VALIDATE, "it is a validation error");
    has(err.message, "names a key with no value", "that says what is wrong");
    has(err.message, "t.1", "and which key it is");
    cf_config_free(c);
}

/* A key the contract requires, past the end of the array it is in, is
   required and has no value. */
static void test_a_required_element_past_the_end(void) {
    static const cf_pair short_[] = {{"t.0", "a"}, {"t.1", "b"}, {"t.count", "1"}};
    static const cf_pair long_[]  = {{"t.0", "a"}, {"t.1", "b"}, {"t.count", "2"}};
    cf_error             err      = CF_ERROR_INIT;
    cf_builder*          b        = cf_build(NULL);
    cf_config*           c;

    cf_builder_memory(b, "file", CF_RANK_USER_FILE, short_, 3);
    cf_builder_text(b, "t.1", NULL, CF_REQUIRED);
    c = cf_builder_create(b, &err);
    check(c == NULL, "refused");
    check(err.code == CF_E_REQUIRED, "as a required key");
    has(err.message, "'t.1' is required and has no value", "with the key named");
    cf_config_free(c);

    b = cf_build(NULL);
    cf_builder_memory(b, "file", CF_RANK_USER_FILE, long_, 3);
    cf_builder_text(b, "t.1", NULL, CF_REQUIRED);
    c = cf_builder_create(b, &err);
    check(c != NULL && strcmp(cf_get_text(c, "t.1", ""), "b") == 0,
          "and fine when the array reaches it");
    cf_config_free(c);
}

/* A value refused for what it is says so still, and what could have been
   the answer instead is what the length cut. */
static void test_a_refused_value_keeps_its_reason(void) {
    static const cf_pair pairs[] = {{"t.0", "1"}, {"t.1", "99"}, {"t.count", "1"}};
    cf_error             err     = CF_ERROR_INIT;
    cf_builder*          b       = cf_build(NULL);
    cf_def               d;
    cf_config*           c;

    cf_builder_memory(b, "file", CF_RANK_USER_FILE, pairs, 3);
    cf_def_init(&d, CF_TYPE_INT, "t.1");
    d.has_range   = CF_TRUE;
    d.imin        = 0;
    d.imax        = 10;
    d.has_default = CF_TRUE;
    cf_value_int(&d.default_value, 5);
    d.on_invalid = CF_INVALID_FALLBACK;
    cf_builder_define(b, &d);
    c = cf_builder_create(b, &err);

    check(c != NULL, "builds");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(!cf_has(c, "t.1"),
          "neither the refused value nor its fallback is an element");
    {
        const char* why = explained(c, "t.1");

        has(why, "rejected: 99 is outside 0..10", "the refusal is still said");
        has(why, "truncated: past the end: t.count is 1", "and so is the cut");
    }
    cf_config_free(c);
}

/* The reason is stored and shown like any other, and does not quote what
   the contract keeps out of them. */
static void test_a_sensitive_length(void) {
    static const cf_pair pairs[] = {{"t.0", "a"}, {"t.1", "b"}, {"t.2", "c"},
                                    {"t.3", "d"}, {"t.4", "e"}, {"t.5", "f"},
                                    {"t.6", "g"}, {"t.7", "h"}, {"t.count", "7"}};
    cf_error             err     = CF_ERROR_INIT;
    cf_builder*          b       = cf_build(NULL);
    cf_def               d;
    cf_config*           c;

    cf_builder_memory(b, "file", CF_RANK_USER_FILE, pairs, 9);
    cf_def_init(&d, CF_TYPE_INT, "t.count");
    d.flags = CF_SENSITIVE;
    cf_builder_define(b, &d);
    c = cf_builder_create(b, &err);

    check(c != NULL, "builds");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(!cf_has(c, "t.7") && cf_has(c, "t.6"), "the length still ends the array");
    {
        const char* why = explained(c, "t.7");

        has(why, "past the end: t.count is <sensitive>, from file",
            "the reason names the length and where it came from");
        lacks(why, "is 7", "and not its value");
    }
    cf_config_free(c);
}

/* The cure for ghosts is the length, not replacement: within it each
   element is the winner of its own key, so two sources can make one array,
   and a source that means to replace a list says how long its list is. */
static void test_keys_within_the_length_resolve_one_at_a_time(void) {
    static const cf_pair low[]   = {{"t.0", "a"}, {"t.1", "b"}, {"t.count", "2"}};
    static const cf_pair patch[] = {{"t.1", "B"}};
    static const cf_pair whole[] = {{"t.0", "X"}, {"t.count", "1"}};
    cf_error             err     = CF_ERROR_INIT;
    cf_config*           c       = two(low, 3, patch, 1, &err);
    cf_config*           r;

    check(c != NULL, "builds with a patch");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(cf_array_count(c, "t") == 2, "the length is still the lower source's");
    check(strcmp(cf_get_text(c, "t.0", ""), "a") == 0 &&
              strcmp(cf_get_text(c, "t.1", ""), "B") == 0,
          "and each element is the winner of its own key");
    cf_config_free(c);

    r = two(low, 3, whole, 2, &err);
    check(r != NULL, "builds with a list of its own");
    if (r == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(cf_array_count(r, "t") == 1 && strcmp(cf_get_text(r, "t.0", ""), "X") == 0 &&
              !cf_has(r, "t.1"),
          "a source that gives the length with its list replaces the other's");
    cf_config_free(r);
}

/* An element is in the configuration only if the length that wins reaches
   it, so a source that adds one must say how long the array is. */
static void test_an_element_needs_the_length(void) {
    static const cf_pair low[]  = {{"t.0", "a"}, {"t.count", "1"}};
    static const cf_pair high[] = {{"t.1", "b"}};
    cf_error             err    = CF_ERROR_INIT;
    cf_config*           c      = two(low, 2, high, 1, &err);

    check(c != NULL, "builds");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(!cf_has(c, "t.1"), "a higher source's element past the length is cut too");
    has(explained(c, "t.1"), "truncated: past the end: t.count is 1, from low",
        "and the length is the lower source's");
    cf_config_free(c);
}

/* An element that two sources set is cut as a whole: neither of them is
   left shadowed behind a winner that is no longer there, because a reader
   of the chain would take the shadowed one for a value that is still in
   play. */
static void test_every_source_of_a_cut_element_is_cut(void) {
    static const cf_pair low[]  = {{"t.0", "a"}, {"t.1", "b"}, {"t.count", "2"}};
    static const cf_pair high[] = {{"t.1", "B"}, {"t.count", "1"}};
    cf_error             err    = CF_ERROR_INIT;
    cf_config*           c      = two(low, 3, high, 2, &err);
    const char*          why;
    const char*          first;

    check(c != NULL, "builds");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(!cf_has(c, "t.1"), "the element is cut");
    why   = explained(c, "t.1");
    first = strstr(why, "truncated: past the end");
    check(first != NULL && strstr(first + 1, "truncated: past the end") != NULL,
          "and both of its candidates say so");
    lacks(why, "shadowed", "none is left behind a winner that is gone");
    has(why, "\"B\"", "the higher one's value is still shown");
    has(why, "\"b\"", "and so is the lower one's");
    cf_config_free(c);
}

/* An array's keys are the ones under its own name and a dot. A neighbour
   whose name only begins the same way is another array, and is cut by its
   own length or by none. */
static void test_a_neighbour_that_starts_the_same_is_another_array(void) {
    static const cf_pair low[]  = {{"s.0", "a"},     {"s.1", "b"},  {"s.2", "c"},
                                   {"s.count", "3"}, {"sx.0", "p"}, {"sx.1", "q"},
                                   {"sx.count", "2"}};
    static const cf_pair high[] = {{"s.count", "1"}};
    cf_error             err    = CF_ERROR_INIT;
    cf_config*           c      = two(low, 7, high, 1, &err);

    check(c != NULL, "builds");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(cf_array_count(c, "s") == 1 && !cf_has(c, "s.1"),
          "the array is cut though another's name begins with its own");
    check(cf_array_count(c, "sx") == 2 && cf_has(c, "sx.0") && cf_has(c, "sx.1"),
          "and the other keeps what its own length allows");
    cf_config_free(c);
}

int main(void) {
    test_a_higher_source_shortens_an_array();
    test_a_table_with_a_count_is_left_alone();
    test_a_member_with_no_value_makes_no_table();
    test_an_array_of_tables();
    test_nested_arrays();
    test_a_length_of_nothing();
    test_a_length_that_fixes_nothing();
    test_a_default_length();
    test_indexes_are_numbers();
    test_a_derived_configuration_recomputes();
#if CF_WITH_REPORT && CF_WITH_JSON
    test_replay();
#endif
    test_a_reference_to_a_cut_element();
    test_a_required_element_past_the_end();
    test_a_refused_value_keeps_its_reason();
    test_a_sensitive_length();
    test_keys_within_the_length_resolve_one_at_a_time();
    test_an_element_needs_the_length();
    test_every_source_of_a_cut_element_is_cut();
    test_a_neighbour_that_starts_the_same_is_another_array();

    return cf_test_done();
}
