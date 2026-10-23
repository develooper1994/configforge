/*
 * test_serialize.c - the serialization layer.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <string.h>

static int g_fail = 0, g_run = 0;

static void check(int c, const char* w) {
    ++g_run;
    if (!c) {
        ++g_fail;
        printf("FAIL: %s\n", w);
    }
}

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
    check(e != NULL && e->chain->at != NULL, "location recorded");
    eq(e->chain->at, "doc:6:1", "line and column are exact");
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

static void test_ini_roundtrip(cf_arena* ar) {
    static const char* const doc = "top = 1\n[server]\nhost = 0.0.0.0\nport = 8080\n";
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
}

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
    check(cf_codec_by_path("app.xml") == NULL, "xml is not compiled in");
    eq(cf_format_of_path("app.xml"), "xml", "but it is still named");
    eq(cf_format_option("xml"), "CF_WITH_XML", "and so is its option");
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

int main(void) {
    cf_arena* ar = cf_arena_new(NULL, 1 << 16);

    test_sink();
    test_registry();
    test_ini(ar);
    test_ini_errors(ar);
    test_json(ar);
    test_json_strings(ar);
    test_json_errors(ar);
    test_json_roundtrip(ar);
    test_ini_roundtrip(ar);
    test_deep_nesting(ar);

    cf_arena_free(ar);
    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
