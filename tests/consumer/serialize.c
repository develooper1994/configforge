/*
 * serialize.c - a program that knows only the serialization library.
 *
 * It includes the one header of that library and links it alone: what
 * the library's target exports, and what its install carries, is
 * checked here and nowhere else.
 */

#include <configforge/cf_serialize.h>

#include <stdio.h>
#include <string.h>

static cf_bool on_open(void* ctx, const char* name, size_t n) {
    (void)ctx;
    (void)name;
    (void)n;
    return 1;
}

static cf_bool on_scalar(void* ctx, const char* name, size_t n,
                         const cf_value* v, cf_bool textual,
                         const cf_loc* at) {
    (void)name;
    (void)n;
    (void)v;
    (void)textual;
    (void)at;
    ++*(int*)ctx;
    return 1;
}

static cf_bool on_end(void* ctx) {
    (void)ctx;
    return 1;
}

int main(void) {
    static const char doc[] = "{\"a\": {\"b\": 1}}";
    const cf_codec*   json  = cf_codec_by_name("json");
    cf_blob           in;
    cf_doc_sink       sink;
    cf_error          err   = CF_ERROR_INIT;
    int               count = 0;

    if (json == NULL) {
        printf("serialize: no json in this build\n");
        return 0;
    }
    memset(&sink, 0, sizeof sink);
    sink.size        = sizeof sink;
    sink.begin_table = on_open;
    sink.begin_array = on_open;
    sink.scalar      = on_scalar;
    sink.end         = on_end;
    sink.ctx         = &count;
    cf_blob_mem(&in, doc, sizeof doc - 1, "consumer.json");
    if (!json->decode(&in, &sink, &err)) {
        fprintf(stderr, "serialize: %s\n", err.message);
        return 1;
    }
    printf("serialize ok: %s %d value(s)\n", cf_serialize_version(), count);
    return count == 1 ? 0 : 1;
}
