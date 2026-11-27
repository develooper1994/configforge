/*
 * test_oom_decode.c - every codec when memory runs out (K-9b, D-237).
 *
 * cf_codec_decode reads a document into the events of a cf_doc_sink, and a
 * sink lends the reader its workspace (reserve_text, reserve_work) from an
 * arena. Here the arena's allocator refuses the 1st, 2nd, ... nth
 * allocation in turn (tests/support/cf_oom.h): the sink says no room, the
 * reader says CF_E_MEMORY with a message, nothing is outstanding, and the
 * run that is allowed everything reads the document it was given.
 *
 * Every codec this build has is swept over a document with a table, an
 * array, a long text and a number; a codec the build left out is skipped.
 */

/* The tests' directory defines CF_INTERNAL_API as 1; a program does not. */
#undef CF_INTERNAL_API
#define CF_INTERNAL_API 0

#include <configforge/cf_alloc.h>
#include <configforge/cf_serialize.h>

#include <stdio.h>
#include <string.h>

#include "cf_test.h"

#include "cf_oom.h"

typedef struct decode_case {
    const char* codec;
    const char* name; /* the document's name, for a codec chosen by extension */
    const char* text;
    size_t      n;
} decode_case;

typedef struct decode_state {
    cf_arena* arena;
    cf_u64    digest;
} decode_state;

static void mix(decode_state* s, const char* p, size_t n) {
    size_t i;

    s->digest = s->digest * 131u + (cf_u64)n;
    for (i = 0; p != NULL && i < n; ++i) {
        s->digest = s->digest * 131u + (unsigned char)p[i];
    }
}

static cf_bool on_table(void* ctx, const char* name, size_t n) {
    mix((decode_state*)ctx, name, n);
    return CF_TRUE;
}

static cf_bool on_array(void* ctx, const char* name, size_t n) {
    mix((decode_state*)ctx, name, n);
    return CF_TRUE;
}

static cf_bool on_scalar(void* ctx, const char* name, size_t n, const cf_value* v,
                         cf_bool textual, const cf_loc* at) {
    decode_state* s = (decode_state*)ctx;

    (void)textual;
    (void)at;
    mix(s, name, n);
    s->digest = s->digest * 131u + (cf_u64)v->type;
    if (v->type == CF_TYPE_TEXT) {
        mix(s, v->u.t.p, v->u.t.n);
    } else if (v->type == CF_TYPE_INT) {
        s->digest = s->digest * 131u + (cf_u64)v->u.i;
    }
    return CF_TRUE;
}

static cf_bool on_end(void* ctx) {
    ++((decode_state*)ctx)->digest;
    return CF_TRUE;
}

static char* on_reserve_text(void* ctx, size_t n) {
    return (char*)cf_arena_alloc(((decode_state*)ctx)->arena, n);
}

static void* on_reserve_work(void* ctx, size_t n) {
    return cf_arena_alloc(((decode_state*)ctx)->arena, n);
}

static int try_decode(void* ctx, const cf_allocator* a, cf_u64* digest, cf_error* err) {
    const decode_case* dc = (const decode_case*)ctx;
    const cf_codec*    c  = cf_codec_by_name(dc->codec);
    decode_state       st;
    cf_doc_sink        sink;
    cf_blob            in;
    cf_bool            ok;

    *digest   = 0;
    st.digest = 0;
    st.arena  = cf_arena_new(a, 0);
    if (st.arena == NULL) {
        cf_error_set(err, CF_E_MEMORY, NULL, NULL, "no room for an arena");
        return CF_OOM_FAILED;
    }
    memset(&sink, 0, sizeof sink);
    sink.size         = sizeof sink;
    sink.begin_table  = on_table;
    sink.begin_array  = on_array;
    sink.scalar       = on_scalar;
    sink.end          = on_end;
    sink.ctx          = &st;
    sink.reserve_text = on_reserve_text;
    sink.reserve_work = on_reserve_work;
    cf_blob_mem(&in, dc->text, dc->n, dc->name);
    ok      = cf_codec_decode(c, &in, &sink, err);
    *digest = st.digest;
    cf_arena_free(st.arena);
    return ok ? CF_OOM_DONE : CF_OOM_FAILED;
}

/* A text longer than any fixed reader buffer, so that a reader that
   lends workspace has to ask for it. */
#define LONG_TEXT 5000

static char g_json[2 * LONG_TEXT + 128];
static char g_toml[2 * LONG_TEXT + 128];
static char g_yaml[2 * LONG_TEXT + 128];
static char g_ini[2 * LONG_TEXT + 128];

/* head, a long text, mid, a second long text, tail: two texts, each bigger
   than the arena's first block, so the arena takes more of them. */
static void fill(char* out, const char* head, const char* mid, const char* tail) {
    size_t n = strlen(head);

    memcpy(out, head, n);
    memset(out + n, 'x', LONG_TEXT);
    n += LONG_TEXT;
    memcpy(out + n, mid, strlen(mid));
    n += strlen(mid);
    memset(out + n, 'y', LONG_TEXT);
    n += LONG_TEXT;
    memcpy(out + n, tail, strlen(tail) + 1u);
}

static void sweep_codec(const char* codec, const char* name, const char* text,
                        size_t n) {
    decode_case dc;
    char        what[64];

    if (cf_codec_by_name(codec) == NULL ||
        !cf_codec_can_read(cf_codec_by_name(codec))) {
        return; /* this build left the format out */
    }
    dc.codec = codec;
    dc.name  = name;
    dc.text  = text;
    dc.n     = n;
    cf_fmt(what, sizeof what, "decode %s", codec);
    (void)cf_oom_sweep(what, try_decode, &dc);
}

static void test_decode(void) {
    /* {"a": 1, "b": [1, 2]} */
    static const char cbor[] = "\xA2\x61"
                               "a\x01\x61"
                               "b\x82\x01\x02";

    fill(g_json,
         "{\"server\": {\"port\": 8080}, \"tags\": [\"a\", \"b\"], \"long\": \"",
         "\", \"more\": \"", "\"}");
    fill(g_toml, "[server]\nport = 8080\ntags = [\"a\", \"b\"]\nlong = \"",
         "\"\nmore = \"", "\"\n");
    fill(g_yaml, "server:\n  port: 8080\ntags:\n  - a\n  - b\nlong: \"", "\"\nmore: \"",
         "\"\n");
    fill(g_ini, "[server]\nport = 8080\nlong = ", "\nmore = ", "\n");

    sweep_codec("json", "oom.json", g_json, strlen(g_json));
    sweep_codec("json5", "oom.json5", g_json, strlen(g_json));
    sweep_codec("toml", "oom.toml", g_toml, strlen(g_toml));
    sweep_codec("yaml", "oom.yaml", g_yaml, strlen(g_yaml));
    sweep_codec("ini", "oom.ini", g_ini, strlen(g_ini));
    sweep_codec("properties", "oom.properties", "server.port=8080\nlong=abc\n", 26);
    sweep_codec("cbor", "oom.cbor", cbor, sizeof cbor - 1u);
}

int main(void) {
    test_decode();

    return cf_test_done();
}
