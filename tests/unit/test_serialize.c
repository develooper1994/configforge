/*
 * test_serialize.c - the serialization layer.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <string.h>

#include "cf_test.h"

/* A format the library knows and this build left out, for the checks of
   what that does - XML unless it is in, then YAML; none when every one
   is in, and then those checks have nothing to say. */
#if !CF_WITH_XML
#define ABSENT "xml"
#define ABSENT_OPTION "CF_WITH_XML"
#elif !CF_WITH_YAML
#define ABSENT "yaml"
#define ABSENT_OPTION "CF_WITH_YAML"
#endif

#if !CF_WITH_REAL
static void has_msg(const cf_error* e, const char* needle, const char* w) {
    ++g_run;
    if (strstr(e->message, needle) == NULL) {
        ++g_fail;
        printf("FAIL: %s  missing \"%s\" in \"%s\"\n", w, needle, e->message);
    }
}
#endif

static void eq(const char* got, const char* want, const char* w) {
    ++g_run;
    if (got == NULL || strcmp(got, want) != 0) {
        ++g_fail;
        printf("FAIL: %s\n  got  |%s|\n  want |%s|\n", w, got ? got : "(null)", want);
    }
}

/* ---------------------------------------------------------------- */

static void test_sink(void) {
    char    buf[8];
    cf_sink s;

    cf_sink_init(&s, buf, sizeof buf);
    cf_sink_puts(&s, "abc");
    cf_sink_fmt(&s, "=%d", 42);
    cf_sink_terminate(&s);
    eq(buf, "abc=42", "sink writes");
    check(!cf_sink_overflowed(&s), "no overflow");

    cf_sink_init(&s, buf, sizeof buf);
    cf_sink_puts(&s, "0123456789");
    cf_sink_terminate(&s);
    check(s.len == 10, "overflow still reports the full length");
    check(cf_sink_overflowed(&s), "overflow detected");
    check(buf[sizeof buf - 1] == '\0', "and the buffer terminates");

    /* Measure-only, so a caller can size a buffer by rendering into
       nothing first. */
    cf_sink_init(&s, NULL, 0);
    cf_sink_puts(&s, "0123456789");
    check(s.len == 10, "measuring without a buffer counts");
}

/* ---------------------------------------------------------------- */

static cf_bool decode(const char* fmt, const char* text, cf_keyspace* ks, cf_arena* ar,
                      int rank, cf_error* err) {
    const cf_codec* c = cf_codec_by_name(fmt);
    cf_flatten      fl;
    cf_blob         b;

    (void)ar;
    if (c == NULL) {
        return CF_FALSE;
    }
    cf_blob_mem(&b, text, strlen(text), "doc");
    cf_flatten_init(&fl, ks, rank);
    return c->decode(&b, cf_flatten_sink(&fl), err);
}

static void test_ini(cf_arena* ar) {
    static const char* const doc = "# a comment\n"
                                   "; another\n"
                                   "top = 1\n"
                                   "\n"
                                   "[server]\n"
                                   "port = 8080\n"
                                   "host=0.0.0.0\n"
                                   "greeting = \"hello world\"   \n"
                                   "\n"
                                   "[server.tls]\n"
                                   "enabled = true\n";

    cf_keyspace     ks;
    cf_error        err = CF_ERROR_INIT;
    const cf_value* v;
    const cf_entry* e;

    cf_ks_init(&ks, ar, 16);
    check(decode("ini", doc, &ks, ar, 10, &err), "ini decodes");
    cf_ks_resolve(&ks);
    cf_ks_infer_untyped(&ks);

    check(cf_ks_count(&ks) == 5, "five keys");

    v = cf_ks_get(&ks, "top", 3);
    check(v != NULL && v->type == CF_TYPE_INT && v->u.i == 1, "top-level key");

    /* A section is a key prefix, and a dotted section nests further. */
    v = cf_ks_get(&ks, "server.port", 11);
    check(v != NULL && v->u.i == 8080, "section becomes a prefix");
    v = cf_ks_get(&ks, "server.tls.enabled", 18);
    check(v != NULL && v->type == CF_TYPE_BOOL && v->u.b, "dotted section");

    /* Quotes wrap a value; they are stripped, not required. */
    v = cf_ks_get(&ks, "server.greeting", 15);
    check(v != NULL && v->type == CF_TYPE_TEXT, "quoted value is text");
    check(v != NULL && strcmp(v->u.t.p, "hello world") == 0,
          "quotes stripped, inner space kept");

    /* The location is what a reader has to open to change it. */
    e = cf_ks_find(&ks, "server.port", 11);
    check(e != NULL && e->chain != NULL && e->chain->at != NULL, "location recorded");
    if (e != NULL && e->chain != NULL) {
        eq(e->chain->at, "doc:6:1", "line and column are exact");
    }
}

static void ini_rejects(cf_arena* ar, const char* doc, const char* what) {
    cf_keyspace ks;
    cf_error    err = CF_ERROR_INIT;

    cf_ks_init(&ks, ar, 8);
    ++g_run;
    if (decode("ini", doc, &ks, ar, 10, &err)) {
        ++g_fail;
        printf("FAIL: ini accepted %s\n", what);
    } else if (err.message[0] == '\0') {
        ++g_fail;
        printf("FAIL: ini rejected %s without saying why\n", what);
    }
}

static void test_ini_errors(cf_arena* ar) {
    ini_rejects(ar, "[unclosed\n", "an unclosed section header");
    ini_rejects(ar, "[]\n", "an empty section header");
    ini_rejects(ar, "[a] junk\n", "trailing text after a header");
    ini_rejects(ar, "novalue\n", "a line with no '='");
    ini_rejects(ar, " = 1\n", "an empty key");
    ini_rejects(ar, "[a..b]\nx=1\n", "an empty section segment");
    ini_rejects(ar, "[Server]\nx=1\n", "an uppercase section");
    ini_rejects(ar, "Key = 1\n", "an uppercase key");
    ini_rejects(ar, "p[] = 1\nq = 2\np[] = 3\n",
                "a second run of name[] lines, which would overwrite the first");
    ini_rejects(ar, "p[01] = 1\n", "an index with a leading zero");
    ini_rejects(ar, "p[x] = 1\n", "an index that is not a number");
}

/* Arrays as PHP's parse_ini_file writes them (D-60): name[] appends,
   name[i] sets element i, and underneath both are the flat name.i and
   name.count every other format's array is. */
static void test_ini_arrays(cf_arena* ar) {
    static const char* const doc = "ports[] = 80\n"
                                   "ports[] = \"443\"\n"
                                   "ports[1] = 8443\n"
                                   "[server]\n"
                                   "names[] = a\n"
                                   "names[] = b c\n"
                                   "json = {\"a\": 1}\n"
                                   "list = {a, b}\n";
    cf_keyspace              ks;
    cf_error                 err = CF_ERROR_INIT;
    const cf_value*          v;

    cf_ks_init(&ks, ar, 16);
    check(decode("ini", doc, &ks, ar, 10, &err), "ini arrays decode");
    if (err.message[0] != '\0') {
        printf("  %s\n", err.message);
    }
    cf_ks_resolve(&ks);
    cf_ks_infer_untyped(&ks);

    v = cf_ks_get(&ks, "ports.count", 11);
    check(v != NULL && v->type == CF_TYPE_INT && v->u.i == 2,
          "name[] lines make a count");
    v = cf_ks_get(&ks, "ports.0", 7);
    check(v != NULL && v->u.i == 80, "the first element");
    v = cf_ks_get(&ks, "ports.1", 7);
    check(v != NULL && v->u.i == 8443, "and name[i], later, sets the element at i");
    v = cf_ks_get(&ks, "server.names.count", 18);
    check(v != NULL && v->u.i == 2, "an array inside a section");
    v = cf_ks_get(&ks, "server.names.1", 14);
    check(v != NULL && v->type == CF_TYPE_TEXT && strcmp(v->u.t.p, "b c") == 0,
          "an element with a blank in it");
    v = cf_ks_get(&ks, "server.json", 11);
    check(v != NULL && v->type == CF_TYPE_TEXT && strcmp(v->u.t.p, "{\"a\": 1}") == 0,
          "a value in braces is text, as it always was: INI has no brace lists");
    v = cf_ks_get(&ks, "server.list", 11);
    check(v != NULL && v->type == CF_TYPE_TEXT, "not even one that looks like a list");
}

/* ---------------------------------------------------------------- */

static void test_json(cf_arena* ar) {
    static const char* const doc = "{\n"
                                   "  \"server\": {\n"
                                   "    \"port\": 8080,\n"
                                   "    \"host\": \"0.0.0.0\",\n"
                                   "    \"tls\": { \"enabled\": true },\n"
                                   "    \"tags\": [\"a\", \"b\", \"c\"]\n"
                                   "  },\n"
                                   "  \"nothing\": null\n"
                                   "}\n";

    cf_keyspace     ks;
    cf_error        err = CF_ERROR_INIT;
    const cf_value* v;
    const cf_entry* e;

    cf_ks_init(&ks, ar, 16);
    check(decode("json", doc, &ks, ar, 10, &err), "json decodes");
    if (err.message[0] != '\0') {
        printf("  %s\n", err.message);
    }
    cf_ks_resolve(&ks);

    v = cf_ks_get(&ks, "server.port", 11);
    check(v != NULL && v->type == CF_TYPE_INT && v->u.i == 8080,
          "a whole number is an int");
#if CF_WITH_REAL
    {
        /* A fraction in its own document, because a build without
           floating point must refuse one rather than lose it, and that
           refusal is tested separately below. */
        cf_keyspace kr;
        cf_error    er = CF_ERROR_INIT;
        cf_ks_init(&kr, ar, 4);
        check(decode("json", "{\"ratio\":0.5}", &kr, ar, 10, &er),
              "a fraction decodes");
        cf_ks_resolve(&kr);
        v = cf_ks_get(&kr, "ratio", 5);
        check(v != NULL && v->type == CF_TYPE_REAL, "a fraction is a real");
    }
#else
    /* Losing a value silently would be worse than refusing the file. */
    {
        cf_keyspace kr;
        cf_error    er = CF_ERROR_INIT;
        cf_ks_init(&kr, ar, 4);
        check(!decode("json", "{\"ratio\":0.5}", &kr, ar, 10, &er),
              "a build without floating point refuses a fraction");
        has_msg(&er, "floating point", "and says why");
    }
#endif
    v = cf_ks_get(&ks, "server.tls.enabled", 18);
    check(v != NULL && v->type == CF_TYPE_BOOL, "nested object");
    v = cf_ks_get(&ks, "nothing", 7);
    check(v != NULL && v->type == CF_TYPE_NULL, "null is a value, not absence");

    /* Arrays become index keys plus a synthesised length. Without the
       length a shorter override would leave ghosts behind. */
    v = cf_ks_get(&ks, "server.tags.0", 13);
    check(v != NULL && strcmp(v->u.t.p, "a") == 0, "array element 0");
    v = cf_ks_get(&ks, "server.tags.2", 13);
    check(v != NULL && strcmp(v->u.t.p, "c") == 0, "array element 2");
    v = cf_ks_get(&ks, "server.tags.count", 17);
    check(v != NULL && v->type == CF_TYPE_INT && v->u.i == 3, "array length recorded");

    /* JSON carries types, so a definition that disagrees is a mismatch
       rather than something to coerce. */
    e = cf_ks_find(&ks, "server.port", 11);
    check(e != NULL && (e->chain->flags & CF_CAND_TEXTUAL) == 0,
          "json values arrive typed");
    check(e != NULL && e->chain->at != NULL, "location recorded");
}

static void test_json_strings(cf_arena* ar) {
    static const char* const doc =
        "{\"a\":\"tab\\there\",\"b\":\"q\\\"q\",\"c\":\"\\u00e7\","
        "\"d\":\"\\ud83d\\ude00\",\"e\":\"\\\\\"}";
    cf_keyspace     ks;
    cf_error        err = CF_ERROR_INIT;
    const cf_value* v;

    cf_ks_init(&ks, ar, 8);
    check(decode("json", doc, &ks, ar, 10, &err), "escapes decode");
    if (err.message[0] != '\0') {
        printf("  %s\n", err.message);
    }
    cf_ks_resolve(&ks);

    v = cf_ks_get(&ks, "a", 1);
    check(v != NULL && strcmp(v->u.t.p, "tab\there") == 0, "\\t");
    v = cf_ks_get(&ks, "b", 1);
    check(v != NULL && strcmp(v->u.t.p, "q\"q") == 0, "escaped quote");
    v = cf_ks_get(&ks, "c", 1);
    check(v != NULL && strcmp(v->u.t.p, "\xC3\xA7") == 0, "\\u to UTF-8");
    /* A surrogate pair is one code point, not two characters. */
    v = cf_ks_get(&ks, "d", 1);
    check(v != NULL && strcmp(v->u.t.p, "\xF0\x9F\x98\x80") == 0,
          "surrogate pair recombined");
    v = cf_ks_get(&ks, "e", 1);
    check(v != NULL && strcmp(v->u.t.p, "\\") == 0, "escaped backslash");
}

static void json_rejects(cf_arena* ar, const char* doc, const char* what) {
    cf_keyspace ks;
    cf_error    err = CF_ERROR_INIT;

    cf_ks_init(&ks, ar, 8);
    ++g_run;
    if (decode("json", doc, &ks, ar, 10, &err)) {
        ++g_fail;
        printf("FAIL: json accepted %s\n", what);
    } else if (err.message[0] == '\0') {
        ++g_fail;
        printf("FAIL: json rejected %s without saying why\n", what);
    }
}

static void test_json_errors(cf_arena* ar) {
    json_rejects(ar, "", "an empty document");
    json_rejects(ar, "[1,2]", "an array at the top level");
    json_rejects(ar, "{\"a\":1,}", "a trailing comma");
    /* RFC 8259's number grammar, which the JSON Test Suite found loose */
    json_rejects(ar, "{\"a\":01}", "a leading zero");
    json_rejects(ar, "{\"a\":-01}", "a negative leading zero");
    json_rejects(ar, "{\"a\":2.}", "a point with no digit after it");
    json_rejects(ar, "{\"a\":.5}", "a point with no digit before it");
    json_rejects(ar, "{\"a\":1e}", "an exponent with no digit");
    json_rejects(ar, "{\"a\":-}", "a sign alone");
    json_rejects(ar, "{\"a\":1", "an unclosed object");
    json_rejects(ar, "{\"a\":[1,2}", "a mismatched bracket");
    json_rejects(ar, "{\"a\":\"\\q\"}", "an unknown escape");
    json_rejects(ar, "{\"a\":\"\\ud83d\"}", "a lone high surrogate");
    json_rejects(ar, "{\"a\":\"\\udc00\"}", "a lone low surrogate");
    json_rejects(ar, "{\"a\":\"line\nbreak\"}", "a raw newline in a string");
    json_rejects(ar, "{\"a\":1} trailing", "text after the document");
    json_rejects(ar, "{\"a\":tru}", "a truncated literal");
    json_rejects(ar, "{\"A\":1}", "an uppercase key");
}

/* ---------------------------------------------------------------- */

#if CF_WITH_EMIT
static size_t encode(const char* fmt, const cf_keyspace* ks, char* buf, size_t cap,
                     cf_error* err) {
    const cf_codec* c = cf_codec_by_name(fmt);
    cf_doc_sink     sink;
    cf_sink         out;
    static char     state[2048];

    if (c == NULL) {
        printf("FAIL: no codec '%s'\n", fmt);
        return 0;
    }
    if (c->encoder_state_size > sizeof state) {
        printf("FAIL: encoder state for '%s' needs %u bytes\n", fmt,
               (unsigned)c->encoder_state_size);
        return 0;
    }
    cf_sink_init(&out, buf, cap);
    if (!c->encoder(&out, state, &sink)) {
        return 0;
    }
    if (!cf_doc_emit(ks, &sink, err)) {
        return 0;
    }
    cf_sink_terminate(&out);
    return out.len;
}

static void test_json_roundtrip(cf_arena* ar) {
    static const char* const doc      = "{\"server\":{\"host\":\"a b\",\"port\":8080,"
                                        "\"tags\":[\"x\",\"y\"]},\"on\":true}";
    static const char* const expected = "{\n"
                                        "  \"on\": true,\n"
                                        "  \"server\": {\n"
                                        "    \"host\": \"a b\",\n"
                                        "    \"port\": 8080,\n"
                                        "    \"tags\": [\n"
                                        "      \"x\",\n"
                                        "      \"y\"\n"
                                        "    ]\n"
                                        "  }\n"
                                        "}\n";

    cf_keyspace ks, ks2;
    cf_error    err = CF_ERROR_INIT;
    char        buf[1024];
    size_t      n;

    cf_ks_init(&ks, ar, 16);
    check(decode("json", doc, &ks, ar, 10, &err), "decode for round trip");
    cf_ks_resolve(&ks);

    n = encode("json", &ks, buf, sizeof buf, &err);
    check(n > 0, "encode produces output");
    /* Keys come out sorted: the document is the key space, and the key
       space is ordered. That is a property, not an accident. */
    eq(buf, expected, "encoded JSON");

    /* And it reads back to the same configuration. */
    cf_ks_init(&ks2, ar, 16);
    check(decode("json", buf, &ks2, ar, 10, &err), "re-decode");
    cf_ks_resolve(&ks2);
    check(cf_ks_digest(&ks) == cf_ks_digest(&ks2),
          "round trip preserves the configuration exactly");

    /* Measuring without a buffer must agree with writing. */
    check(encode("json", &ks, NULL, 0, &err) == n, "encoder can be measured first");
}

/* Room for a document at the deepest nesting: a line is indented by its
   level, so the text grows with the square of the limit. */
#define OUT_MAX (8192 + 4 * (CF_MAX_DEPTH + 2) * (CF_MAX_DEPTH + 2))

/* A document read, written and read again is the same document. A format
   typed by its contract (YAML, D-39) reads a plain 11 as text, and gets
   the number back the way INI does: by inference. */
static void round_trip(cf_arena* ar, const char* fmt, cf_bool by_contract,
                       const char* doc, const char* what) {
    static char buf[OUT_MAX];
    static char msg[160];
    cf_keyspace ks, ks2;
    cf_error    err = CF_ERROR_INIT;

    cf_ks_init(&ks, ar, 128);
    cf_fmt(msg, sizeof msg, "%s: the document reads", what);
    check(decode("json", doc, &ks, ar, 10, &err), msg);
    cf_ks_resolve(&ks);

    cf_fmt(msg, sizeof msg, "%s: %s writes it", what, fmt);
    check(encode(fmt, &ks, buf, sizeof buf, &err) > 0, msg);

    cf_ks_init(&ks2, ar, 128);
    cf_fmt(msg, sizeof msg, "%s: %s reads it back", what, fmt);
    check(decode(fmt, buf, &ks2, ar, 10, &err), msg);
    cf_ks_resolve(&ks2);
    if (by_contract) {
        cf_ks_infer_untyped(&ks);
        cf_ks_infer_untyped(&ks2);
    }

    cf_fmt(msg, sizeof msg, "%s: through %s it is the same configuration", what, fmt);
    check(cf_ks_digest(&ks) == cf_ks_digest(&ks2), msg);
}

/* What the JSON encoder writes for a JSON document, exactly. */
static void written_as(cf_arena* ar, const char* doc, const char* want,
                       const char* what) {
    static char buf[OUT_MAX];
    cf_keyspace ks;
    cf_error    err = CF_ERROR_INIT;

    cf_ks_init(&ks, ar, 128);
    check(decode("json", doc, &ks, ar, 10, &err), what);
    cf_ks_resolve(&ks);
    check(encode("json", &ks, buf, sizeof buf, &err) > 0, what);
    eq(buf, want, what);
}

static void round_trips(cf_arena* ar, const char* doc, const char* what) {
    round_trip(ar, "json", CF_FALSE, doc, what);
#if CF_WITH_TOML
    round_trip(ar, "toml", CF_FALSE, doc, what);
#endif
#if CF_WITH_YAML
    round_trip(ar, "yaml", CF_TRUE, doc, what);
#endif
}

/* The key space is in byte order, where "10" comes before "2", and the
   members of an array are its positions: written in key order, the
   third of twelve elements was the twelfth. A hundred and one elements
   have indexes of one, two and three digits. */
static void test_array_order(cf_arena* ar) {
    static char doc[2048];
    static char want[4096];
    size_t      d, w;
    int         i;

    d = cf_fmt(doc, sizeof doc, "{\"v\":[");
    w = cf_fmt(want, sizeof want, "{\n  \"v\": [\n");
    for (i = 0; i <= 100; ++i) {
        d = cf_fmt_append(doc, sizeof doc, d, "%s\"e%d\"", (i > 0) ? "," : "", i);
        w = cf_fmt_append(want, sizeof want, w, "    \"e%d\"%s\n", i,
                          (i < 100) ? "," : "");
    }
    cf_fmt_append(doc, sizeof doc, d, "]}");
    cf_fmt_append(want, sizeof want, w, "  ]\n}\n");

    written_as(ar, doc, want, "a hundred and one elements are written in order");
    round_trips(ar, doc, "a hundred and one elements");

    /* Twelve, the shortest array that shows it, and one nested in a
       table in an array, which is a second place the order could go. */
    written_as(ar,
               "{\"v\":[\"a\",\"b\",\"c\",\"d\",\"e\",\"f\",\"g\",\"h\",\"i\",\"j\","
               "\"k\",\"l\"]}",
               "{\n  \"v\": [\n    \"a\",\n    \"b\",\n    \"c\",\n    \"d\",\n"
               "    \"e\",\n    \"f\",\n    \"g\",\n    \"h\",\n    \"i\",\n"
               "    \"j\",\n    \"k\",\n    \"l\"\n  ]\n}\n",
               "twelve elements are written in order");
    round_trips(ar, "{\"s\":[{\"t\":[1,2,3,4,5,6,7,8,9,10,11]}]}",
                "an eleven element array in a table in an array");
}

/* Inside an array a table has no name - position is its name - and
   what is in the array keeps the order it had. Given "1" as a name, the
   JSON encoder wrote ["a", "1": {...}]: a key in the middle of a list. */
static void test_array_members(cf_arena* ar) {
    written_as(ar, "{\"m\":[\"a\",{\"x\":\"b\"},\"c\"]}",
               "{\n  \"m\": [\n    \"a\",\n    {\n      \"x\": \"b\"\n    },\n"
               "    \"c\"\n  ]\n}\n",
               "a table sits in a mixed array where it was");
    round_trips(ar, "{\"m\":[\"a\",{\"x\":\"b\"},\"c\"]}", "a mixed array");

    written_as(ar,
               "{\"g\":[[1,2],[3]],"
               "\"s\":[{\"host\":\"a\",\"port\":1},{\"host\":\"b\",\"port\":2}]}",
               "{\n  \"g\": [\n    [\n      1,\n      2\n    ],\n    [\n      3\n"
               "    ]\n  ],\n  \"s\": [\n    {\n      \"host\": \"a\",\n"
               "      \"port\": 1\n    },\n    {\n      \"host\": \"b\",\n"
               "      \"port\": 2\n    }\n  ]\n}\n",
               "arrays of arrays and of tables carry no names");
    round_trips(ar,
                "{\"g\":[[1,2],[3]],"
                "\"s\":[{\"host\":\"a\",\"port\":1},{\"host\":\"b\",\"port\":2}]}",
                "arrays of arrays and of tables");
}

/* An empty array is an array, and a table that has a member called
   `count` is a table: taken for an array, `pool` came out as `[]` with
   its size lost. */
static void test_count_is_a_key(cf_arena* ar) {
    written_as(ar, "{\"a\":[],\"t\":{\"b\":[]}}",
               "{\n  \"a\": [],\n  \"t\": {\n    \"b\": []\n  }\n}\n",
               "an empty array stays an array");
    round_trips(ar, "{\"a\":[],\"t\":{\"b\":[]}}", "empty arrays");

    written_as(ar, "{\"pool\":{\"count\":4,\"size\":10},\"retry\":{\"count\":3}}",
               "{\n  \"pool\": {\n    \"count\": 4,\n    \"size\": 10\n  },\n"
               "  \"retry\": {\n    \"count\": 3\n  }\n}\n",
               "a table with a count is a table");
    round_trips(ar, "{\"pool\":{\"count\":4,\"size\":10},\"retry\":{\"count\":3}}",
                "tables with a count");

    /* The flat model cannot tell {"0":"a","count":1} from ["a"], so that
       one is an array. A group with any other member is not an array
       in anyone's reading, and dropping `size` to make it one would be
       losing data. */
    written_as(ar, "{\"t\":{\"0\":\"a\",\"count\":1,\"size\":3}}",
               "{\n  \"t\": {\n    \"0\": \"a\",\n    \"count\": 1,\n    \"size\": 3\n"
               "  }\n}\n",
               "a table with index-like keys and others stays a table");
}

/* The deepest document there is: the document itself and CF_MAX_DEPTH - 1
   levels inside it, as much as any reader takes. A writer has to write
   all of that, or a configuration cannot be saved, and no more, or it
   writes a file that cannot be read: through TOML a table below the
   eighth was written and refused on the way back, and JSON wrote a level
   that its own reader turned away. The levels are tables, arrays, and
   the two in turn, because a format counts a table in an array in its
   own way. Whether such a document fits a key is the build's to say. */
#if 2 * CF_MAX_DEPTH + 8 <= CF_KEY_MAX

static cf_bool level_is_table(char shape, int level) {
    return (shape == 't' || (shape == 'm' && level % 2 == 0)) ? CF_TRUE : CF_FALSE;
}

/* The JSON text of a document with `levels` containers inside it. */
static const char* nested_doc(char* doc, size_t cap, char shape, int levels) {
    size_t at;
    int    i;

    at = cf_fmt(doc, cap, "{\"a\":");
    for (i = 1; i <= levels; ++i) {
        at = cf_fmt_append(doc, cap, at, "%s",
                           level_is_table(shape, i) ? "{\"a\":" : "[");
    }
    at = cf_fmt_append(doc, cap, at, "1");
    for (i = levels; i >= 1; --i) {
        at = cf_fmt_append(doc, cap, at, "%s", level_is_table(shape, i) ? "}" : "]");
    }
    cf_fmt_append(doc, cap, at, "}");
    return doc;
}

/* The same document as keys, put in directly: no reader reads one past
   the limit, and that is the one to hand a writer. */
static cf_bool nested_keys(cf_keyspace* ks, char shape, int levels) {
    char     path[CF_KEY_MAX];
    char     key[CF_KEY_MAX];
    cf_value one;
    cf_bool  ok = CF_TRUE;
    size_t   at;
    int      i;

    cf_value_int(&one, 1);
    at = cf_fmt(path, sizeof path, "a");
    for (i = 1; i <= levels; ++i) {
        if (!level_is_table(shape, i)) {
            /* The length the readers record for every array. */
            size_t n = cf_fmt(key, sizeof key, "%s.count", path);
            ok       = cf_ks_add(ks, key, n, 10, &one, NULL) && ok;
        }
        at = cf_fmt_append(path, sizeof path, at, "%s",
                           level_is_table(shape, i) ? ".a" : ".0");
    }
    return cf_ks_add_authored(ks, path, at, 10, &one, NULL) && ok;
}

/* One level too many: no writer writes it, and each says why. */
static void too_deep_for(const cf_keyspace* ks, const char* fmt, const char* what) {
    static char buf[OUT_MAX];
    static char msg[160];
    cf_error    err = CF_ERROR_INIT;

    cf_fmt(msg, sizeof msg, "%s: %s refuses to write it", what, fmt);
    check(encode(fmt, ks, buf, sizeof buf, &err) == 0, msg);
    cf_fmt(msg, sizeof msg, "%s: %s says the document nests too deep", what, fmt);
    check(err.code == CF_E_KEY && strstr(err.message, "nests deeper") != NULL, msg);
}

static void test_deepest_document(cf_arena* ar) {
    static const char        shapes[] = "tam";
    static const char* const names[]  = {"the deepest tables", "the deepest arrays",
                                         "the deepest tables and arrays"};
    static char              doc[(CF_MAX_DEPTH + 2) * 8 + 16];
    int                      s;

    for (s = 0; shapes[s] != '\0'; ++s) {
        cf_keyspace read, built;
        cf_error    err = CF_ERROR_INIT;

        /* The deepest one is read and written by every format, and
           comes back the same. */
        nested_doc(doc, sizeof doc, shapes[s], CF_MAX_DEPTH - 1);
        round_trips(ar, doc, names[s]);
        if (shapes[s] == 't') {
            round_trip(ar, "ini", CF_TRUE, doc, names[s]);
        }

        /* The keys put in directly are that document, so the one past
           it is the same shape and nothing else. */
        cf_ks_init(&read, ar, CF_MAX_DEPTH + 8);
        check(decode("json", doc, &read, ar, 10, &err), names[s]);
        cf_ks_resolve(&read);
        cf_ks_init(&built, ar, CF_MAX_DEPTH + 8);
        check(nested_keys(&built, shapes[s], CF_MAX_DEPTH - 1), names[s]);
        cf_ks_resolve(&built);
        check(cf_ks_digest(&read) == cf_ks_digest(&built),
              "the keys made directly are the keys that were read");

        cf_ks_init(&built, ar, CF_MAX_DEPTH + 8);
        check(nested_keys(&built, shapes[s], CF_MAX_DEPTH), names[s]);
        cf_ks_resolve(&built);
        too_deep_for(&built, "json", names[s]);
#if CF_WITH_TOML
        too_deep_for(&built, "toml", names[s]);
#endif
#if CF_WITH_YAML
        too_deep_for(&built, "yaml", names[s]);
#endif
        if (shapes[s] == 't') {
            too_deep_for(&built, "ini", names[s]);
        }
    }
}

/* Level `level` of a chain of tables, or of arrays, below the document. */
static cf_bool open_level(const cf_doc_sink* sink, cf_bool arrays, int level) {
    if (!arrays) {
        return sink->begin_table(sink->ctx, "a", 1);
    }
    return (level == 1) ? sink->begin_array(sink->ctx, "a", 1)
                        : sink->begin_array(sink->ctx, NULL, 0);
}

/* A writer used on its own, the way a program with a source of documents
   of its own would, keeps to the same limit as the walk does. */
static void test_writer_limit(void) {
    static const char* const formats[] = {"json", "toml", "yaml", "ini"};
    static char              state[CF_ENCODER_STATE_MAX];
    static char              msg[160];
    size_t                   f;

    for (f = 0; f < sizeof formats / sizeof formats[0]; ++f) {
        const cf_codec* c = cf_codec_by_name(formats[f]);
        int             arrays;

        if (c == NULL || (c->caps & CF_CODEC_WRITE) == 0) {
            continue;
        }
        for (arrays = 0; arrays < 2; ++arrays) {
            cf_doc_sink sink;
            cf_sink     out;
            cf_bool     within = CF_TRUE;
            int         level;

            if (arrays && (c->caps & CF_CODEC_ARRAYS) == 0) {
                continue;
            }
            cf_sink_init(&out, NULL, 0);
            check(c->encoder(&out, state, &sink), formats[f]);
            check(sink.begin_table(sink.ctx, NULL, 0), formats[f]);
            for (level = 1; level < CF_MAX_DEPTH; ++level) {
                within = open_level(&sink, arrays, level) && within;
            }
            cf_fmt(msg, sizeof msg, "%s writes %d levels of %s inside the document",
                   formats[f], CF_MAX_DEPTH - 1, arrays ? "arrays" : "tables");
            check(within, msg);
            cf_fmt(msg, sizeof msg, "%s does not write %d levels of %s", formats[f],
                   CF_MAX_DEPTH, arrays ? "arrays" : "tables");
            check(!open_level(&sink, arrays, CF_MAX_DEPTH), msg);
        }
    }
}
#endif

static void test_ini_roundtrip(cf_arena* ar) {
    /* Arrays too, with elements that need their quotes: a blank, a
       comment character, a quote of the other kind. */
    static const char* const doc = "top = 1\n"
                                   "tags[] = a\n"
                                   "tags[] = \"b c\"\n"
                                   "tags[] = \"#d\"\n"
                                   "tags[] = 'say \"hi\"'\n"
                                   "[server]\nhost = 0.0.0.0\nport = 8080\n"
                                   "ports[] = 80\nports[] = 443\n";
    cf_keyspace              ks, ks2;
    cf_error                 err = CF_ERROR_INIT;
    char                     buf[512];

    cf_ks_init(&ks, ar, 16);
    check(decode("ini", doc, &ks, ar, 10, &err), "ini decode");
    cf_ks_resolve(&ks);
    cf_ks_infer_untyped(&ks);

    check(encode("ini", &ks, buf, sizeof buf, &err) > 0, "ini encode");

    cf_ks_init(&ks2, ar, 16);
    check(decode("ini", buf, &ks2, ar, 10, &err), "ini re-decode");
    if (err.message[0] != '\0') {
        printf("  %s\n  ---\n%s---\n", err.message, buf);
    }
    cf_ks_resolve(&ks2);
    cf_ks_infer_untyped(&ks2);
    check(cf_ks_digest(&ks) == cf_ks_digest(&ks2), "ini round trip");
    if (cf_ks_digest(&ks) != cf_ks_digest(&ks2)) {
        printf("  written:\n%s", buf);
    }
    check(strstr(buf, "tags[] = \"b c\"\n") != NULL &&
              strstr(buf, "ports[] = 443\n") != NULL,
          "arrays written as name[] lines");
}
#endif

/* ---------------------------------------------------------------- */

static void test_registry(void) {
    check(cf_codec_by_name("json") != NULL, "json is compiled in");
    check(cf_codec_by_name("ini") != NULL, "ini is compiled in");
    check(cf_codec_by_name("nope") == NULL, "unknown format is NULL");

    check(cf_codec_by_path("a/b/app.json") != NULL, "by extension");
    check(cf_codec_by_path("APP.JSON") != NULL, "extension is case blind");
    check(cf_codec_by_path("app") == NULL, "no extension");
    check(cf_codec_by_path("a.json/b") == NULL,
          "a dot in a directory is not an extension");

    /* A format the library knows but this build left out must be
       nameable, so the error can say which option to turn on rather
       than claiming the file is unrecognisable. */
#ifdef ABSENT
    check(cf_codec_by_path("app." ABSENT) == NULL,
          "a format left out is not compiled in");
    eq(cf_format_of_path("app." ABSENT), ABSENT, "but it is still named");
    eq(cf_format_option(ABSENT), ABSENT_OPTION, "and so is its option");
#endif
#if CF_WITH_XML
    check(cf_codec_by_path("app.xml") != NULL, "xml is compiled in");
#endif
#if CF_WITH_TOML
    check(cf_codec_by_path("app.toml") != NULL, "toml is built in");
#endif
    check(cf_format_of_path("app.zzz") == NULL, "a truly unknown extension");
}

static void test_deep_nesting(cf_arena* ar) {
    static char doc[(CF_MAX_DEPTH + 8) * 8 + 16];
    cf_keyspace ks;
    cf_error    err = CF_ERROR_INIT;
    size_t      at;
    int         i;

    /* Past CF_MAX_DEPTH, whatever this build chose: it must refuse
       rather than overrun, and say so. */
    at = 0;
    for (i = 0; i < CF_MAX_DEPTH + 4; ++i) {
        at = cf_fmt_append(doc, sizeof doc, at, "{\"a\":");
    }
    at = cf_fmt_append(doc, sizeof doc, at, "1");
    for (i = 0; i < CF_MAX_DEPTH + 4; ++i) {
        at = cf_fmt_append(doc, sizeof doc, at, "}");
    }
    cf_ks_init(&ks, ar, 8);
    check(!decode("json", doc, &ks, ar, 10, &err), "nesting past the limit is refused");

    /* And a document within it is read: the limit is the limit, not
       somewhere below it. One level is the root object. */
    at = 0;
    for (i = 0; i < CF_MAX_DEPTH - 2; ++i) {
        at = cf_fmt_append(doc, sizeof doc, at, "{\"a\":");
    }
    at = cf_fmt_append(doc, sizeof doc, at, "1");
    for (i = 0; i < CF_MAX_DEPTH - 2; ++i) {
        at = cf_fmt_append(doc, sizeof doc, at, "}");
    }
    cf_ks_init(&ks, ar, 8);
    check(decode("json", doc, &ks, ar, 10, &err), "nesting within the limit is read");
}

static int g_reserves;

static char* deny_text_storage(void* ctx, size_t n) {
    (void)ctx;
    (void)n;
    ++g_reserves;
    return NULL;
}

static void test_reader_text_storage(void) {
    static const char* formats[] = {"json", "toml"};
    static char        doc[4096], expected[513], fixed[2048];
    cf_error           err = CF_ERROR_INIT;
    cf_keyspace        ks;
    cf_flatten         fl;
    cf_doc_sink        sink;
    cf_blob            blob;
    size_t             f, i, at;
    int                escaped, mode;

    memset(expected, 'a', sizeof expected - 1);
    expected[sizeof expected - 1] = '\0';
    for (f = 0; f < sizeof formats / sizeof formats[0]; ++f) {
        const cf_codec* codec = cf_codec_by_name(formats[f]);
        if (codec == NULL) {
            continue;
        }
        for (escaped = 0; escaped < 2; ++escaped) {
            at = cf_fmt(doc, sizeof doc, "%s", f == 0 ? "{\"k\":\"" : "k=\"");
            for (i = 0; i < sizeof expected - 1; ++i) {
                at =
                    cf_fmt_append(doc, sizeof doc, at, "%s", escaped ? "\\u0061" : "a");
            }
            cf_fmt_append(doc, sizeof doc, at, "%s", f == 0 ? "\"}" : "\"");
            cf_blob_mem(&blob, doc, strlen(doc), "storage");
            for (mode = 0; mode < 5; ++mode) {
                cf_arena* ar = cf_arena_new(NULL, 4096);
                cf_bool   read;
                if (ar == NULL) {
                    check(0, "text storage arena exists");
                    return;
                }
                cf_ks_init(&ks, ar, 8);
                cf_flatten_init(&fl, &ks, 10);
                sink = *cf_flatten_sink(&fl);
                if (mode < 4) {
                    sink.reserve_text = mode == 2 ? NULL : deny_text_storage;
                }
                if (mode == 0) {
                    sink.size = offsetof(cf_doc_sink, reserve_text);
                } else if (mode == 1) {
                    sink.size = offsetof(cf_doc_sink, reserve_text) +
                                sizeof sink.reserve_text - 1;
                }
                g_reserves = 0;
                cf_error_clear(&err);
                read = codec->decode(&blob, &sink, &err);
                check(read == (mode == 4),
                      "long strings require available advertised storage");
                check(g_reserves == (mode == 3 ? 1 : 0),
                      "legacy, partial-size and null callbacks are never invoked");
                if (mode < 4) {
                    check(err.code == (mode == 3 ? CF_E_MEMORY : CF_E_SOURCE),
                          "storage exhaustion and absent storage have explicit errors");
                    check(err.message[0] != '\0', "the storage error says why");
                } else {
                    const cf_value* value;
                    cf_ks_resolve(&ks);
                    value = cf_ks_get(&ks, "k", 1);
                    check(value != NULL && value->type == CF_TYPE_TEXT &&
                              value->u.t.n == sizeof expected - 1,
                          "long text keeps its decoded length");
                    if (value != NULL && value->type == CF_TYPE_TEXT) {
                        eq(value->u.t.p, expected,
                           "long text is decoded and NUL-terminated");
                    }
                }
                cf_arena_free(ar);
            }
            {
                cf_arena* ar = cf_arena_fixed(fixed, sizeof fixed);
                check(ar != NULL && cf_ks_init(&ks, ar, 1), "a fixed arena starts");
                if (ar == NULL) {
                    return;
                }
                cf_flatten_init(&fl, &ks, 10);
                cf_error_clear(&err);
                /* Ask for more than the entire fixed arena can hold. */
                check(cf_flatten_sink(&fl)->reserve_text(&fl, sizeof fixed + 1) == NULL,
                      "fixed workspace refuses exhaustion instead of using the heap");
                cf_arena_free(ar);
            }
        }
    }
}

#if CF_WITH_EMIT
static void test_encoder_sink_prefix(void) {
    size_t i;
    for (i = 0; i < cf_codec_count(); ++i) {
        const cf_codec* codec = cf_codec_at(i);
        cf_doc_sink     sink;
        cf_sink         out;
        unsigned char   tail[sizeof sink.reserve_text];
        union {
            cf_i64 align;
            char   bytes[CF_ENCODER_STATE_MAX];
        } state;
        if (codec->encoder == NULL) {
            continue;
        }
        memset(&sink, 0x5a, sizeof sink);
        memcpy(tail, &sink.reserve_text, sizeof tail);
        cf_sink_init(&out, NULL, 0);
        check(codec->encoder(&out, state.bytes, &sink),
              "an encoder initializes its sink");
        check(sink.size == offsetof(cf_doc_sink, reserve_text),
              "an encoder advertises only the 1.0 output prefix");
        check(memcmp(tail, &sink.reserve_text, sizeof tail) == 0,
              "an encoder never writes beyond a 1.0 caller's sink");
    }
}
#endif

int main(void) {
    cf_arena* ar = cf_arena_new(NULL, 1 << 16);

    test_sink();
    test_registry();
    test_ini(ar);
    test_ini_errors(ar);
    test_ini_arrays(ar);
    test_json(ar);
    test_json_strings(ar);
    test_json_errors(ar);
    test_reader_text_storage();
#if CF_WITH_EMIT
    test_encoder_sink_prefix();
    test_json_roundtrip(ar);
    test_array_order(ar);
    test_array_members(ar);
    test_count_is_a_key(ar);
#if 2 * CF_MAX_DEPTH + 8 <= CF_KEY_MAX
    test_deepest_document(ar);
    test_writer_limit();
#endif
    test_ini_roundtrip(ar);
#endif
    test_deep_nesting(ar);

    cf_arena_free(ar);
    return cf_test_done();
}
