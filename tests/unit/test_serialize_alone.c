/*
 * test_serialize_alone.c - the serialization library on its own (D-90).
 *
 * Linked to configforge_serialize and nothing else, and including one
 * header: a program that wants documents and not a configuration. It
 * reads a JSON document into a sink of its own, checks the version the
 * library reports, and writes the same events back. A function the
 * library lacks is an unresolved symbol at link time, named by the
 * linker, so building this file is most of the test.
 */

/* The tests' directory defines CF_INTERNAL_API as 1; a program does not. */
#undef CF_INTERNAL_API
#define CF_INTERNAL_API 0

#include <configforge/cf_serialize.h>

#include <stdio.h>
#include <string.h>

static int g_failed;

static void check(int ok, const char* what) {
    if (!ok) {
        printf("FAIL: %s\n", what);
        g_failed = 1;
    }
}

#if CF_WITH_JSON

typedef struct counts {
    int    tables;
    int    arrays;
    int    scalars;
    int    ends;
    cf_i64 port;
    int    got_port;
} counts;

static cf_bool on_table(void* ctx, const char* name, size_t n) {
    (void)name;
    (void)n;
    ++((counts*)ctx)->tables;
    return CF_TRUE;
}

static cf_bool on_array(void* ctx, const char* name, size_t n) {
    (void)name;
    (void)n;
    ++((counts*)ctx)->arrays;
    return CF_TRUE;
}

static cf_bool on_scalar(void* ctx, const char* name, size_t n, const cf_value* v,
                         cf_bool textual, const cf_loc* at) {
    counts* c = (counts*)ctx;

    (void)textual;
    (void)at;
    ++c->scalars;
    if (name != NULL && n == 4 && memcmp(name, "port", 4) == 0 &&
        v->type == CF_TYPE_INT) {
        c->port     = v->u.i;
        c->got_port = 1;
    }
    return CF_TRUE;
}

static cf_bool on_end(void* ctx) {
    ++((counts*)ctx)->ends;
    return CF_TRUE;
}

static void test_reads(void) {
    static const char doc[] = "{\"server\": {\"port\": 8080}, \"tags\": [\"a\", \"b\"]}";
    const cf_codec*   json  = cf_codec_by_name("json");
    cf_blob           in;
    cf_doc_sink       sink;
    counts            c;
    cf_error          err = CF_ERROR_INIT;

    check(json != NULL, "the json codec is registered");
    if (json == NULL) {
        return;
    }
    memset(&c, 0, sizeof c);
    memset(&sink, 0, sizeof sink);
    sink.size        = sizeof sink;
    sink.begin_table = on_table;
    sink.begin_array = on_array;
    sink.scalar      = on_scalar;
    sink.end         = on_end;
    sink.ctx         = &c;
    cf_blob_mem(&in, doc, sizeof doc - 1, "alone.json");
    check(cf_codec_decode(json, &in, &sink, &err), "the document is read");
    check(c.tables == 2, "two tables");
    check(c.arrays == 1, "one array");
    check(c.scalars == 3, "three scalars");
    check(c.ends == 3, "three ends");
    check(c.got_port && c.port == 8080, "port is 8080");
}

static void test_writes(void) {
    const cf_codec* json = cf_codec_by_name("json");
    cf_doc_sink     enc;
    cf_sink         out;
    cf_value        v;
    char            state[CF_ENCODER_STATE_MAX];
    char            buf[256];

    if (json == NULL || json->encoder == NULL) {
        return;
    }
    check(json->encoder_state_size <= sizeof state, "the encoder's state fits");
    memset(&enc, 0, sizeof enc);
    cf_sink_init(&out, buf, sizeof buf);
    check(json->encoder(&out, state, &enc), "the encoder starts");
    cf_value_int(&v, 8080);
    check(enc.begin_table(enc.ctx, NULL, 0), "the document");
    check(enc.begin_table(enc.ctx, "server", 6), "a table");
    check(enc.scalar(enc.ctx, "port", 4, &v, CF_FALSE, NULL), "a scalar");
    check(enc.end(enc.ctx), "the table ends");
    check(enc.end(enc.ctx), "the document ends");
    cf_sink_terminate(&out);
    check(!cf_sink_overflowed(&out), "the output fits");
    check(strstr(buf, "\"port\"") != NULL, "\"port\" is written");
    check(strstr(buf, "8080") != NULL, "8080 is written");
}

#endif /* CF_WITH_JSON */

static void test_version(void) {
    char want[32];

    check(cf_serialize_version_number() == CF_SERIALIZE_VERSION_NUMBER,
          "the version number is the header's");
    sprintf(want, "%d.%d.%d", CF_SERIALIZE_VERSION_MAJOR, CF_SERIALIZE_VERSION_MINOR,
            CF_SERIALIZE_VERSION_PATCH);
    check(strcmp(cf_serialize_version(), want) == 0,
          "the version string is built from the macros");
}

int main(void) {
#if CF_WITH_JSON
    test_reads();
    test_writes();
#endif
    test_version();
    if (g_failed) {
        return 1;
    }
    printf("serialize_alone: ok\n");
    return 0;
}
