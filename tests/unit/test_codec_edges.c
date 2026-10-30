/*
 * test_codec_edges.c - every codec fed the documents at its edges.
 *
 * The fuzzer's contract, applied to a list a person wrote: escapes of
 * every kind and the malformed ones, numbers at each form and past their
 * range, nesting to the limit and beyond it, the syntax a document gets
 * wrong. For each, the codec returns; a document it refuses comes with a
 * reason; one it accepts can be resolved and written back out. Then a
 * sink that refuses at each event in turn, because a codec has a path
 * for its consumer saying no, and every one of them must stop cleanly.
 *
 * The documents are in tests/unit/data/edges.<format>, separated by a
 * line of five dashes, so that what they spell is what the file says -
 * no C escapes between the reader and the bytes.
 *
 *     test_codec_edges <directory of edges.*>
 */

#include <configforge/cf_flatten.h>
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

static char g_text[1 << 16];
static char g_arena[1 << 18];
static char g_out[1 << 15];

/* A sink that passes events to a real one until its budget runs out. */
typedef struct refusing {
    const cf_doc_sink* inner;
    int                budget;
} refusing;

static cf_bool rf_spend(refusing* r) {
    return (r->budget-- > 0) ? CF_TRUE : CF_FALSE;
}
static cf_bool rf_table(void* ctx, const char* name, size_t n) {
    refusing* r = (refusing*)ctx;
    return rf_spend(r) && r->inner->begin_table(r->inner->ctx, name, n);
}
static cf_bool rf_array(void* ctx, const char* name, size_t n) {
    refusing* r = (refusing*)ctx;
    return rf_spend(r) && r->inner->begin_array(r->inner->ctx, name, n);
}
static cf_bool rf_scalar(void* ctx, const char* name, size_t n, const cf_value* v,
                         cf_bool textual, const cf_loc* at) {
    refusing* r = (refusing*)ctx;
    return rf_spend(r) && r->inner->scalar(r->inner->ctx, name, n, v, textual, at);
}
static cf_bool rf_end(void* ctx) {
    refusing* r = (refusing*)ctx;
    return rf_spend(r) && r->inner->end(r->inner->ctx);
}

/* One document through one codec, with a sink that allows `budget`
   events (-1 for all). Returns 1 accepted, 0 refused. */
static int feed(const cf_codec* codec, const char* doc, size_t n, int budget) {
    cf_arena*   ar = cf_arena_fixed(g_arena, sizeof g_arena);
    cf_keyspace ks;
    cf_flatten  fl;
    cf_blob     b;
    cf_error    err = CF_ERROR_INIT;
    refusing    r;
    cf_doc_sink sink;
    int         ok;

    if (ar == NULL || !cf_ks_init(&ks, ar, 32)) {
        return 0;
    }
    cf_blob_mem(&b, doc, n, "edges");
    cf_flatten_init(&fl, &ks, 0);
    r.inner          = cf_flatten_sink(&fl);
    r.budget         = (budget < 0) ? 1 << 30 : budget;
    sink.size        = sizeof(cf_doc_sink);
    sink.begin_table = rf_table;
    sink.begin_array = rf_array;
    sink.scalar      = rf_scalar;
    sink.end         = rf_end;
    sink.ctx         = &r;
    ok               = codec->decode(&b, &sink, &err) ? 1 : 0;
    if (!ok) {
        check(err.message[0] != '\0', "a refusal says why");
    } else {
        cf_ks_resolve(&ks);
        cf_ks_infer_untyped(&ks);
        (void)cf_ks_digest(&ks);
    }
    return ok;
}

static void run_file(const char* dir, const char* format) {
    const cf_codec* codec = cf_codec_by_name(format);
    static char     path[512]; /* static: feed() is inlined here, and its
                                  frame and this one share the budget */
    FILE*  f;
    size_t n, start = 0, i, docs = 0, accepted = 0;

    if (codec == NULL) {
        return; /* a build without it */
    }
    cf_fmt(path, sizeof path, "%s/edges.%s", dir, format);
    f = fopen(path, "rb");
    check(f != NULL, path);
    if (f == NULL) {
        return;
    }
    n = fread(g_text, 1, sizeof g_text - 1, f);
    fclose(f);
    g_text[n] = '\0';

    for (i = 0; i <= n; ++i) {
        if (i == n || (strncmp(g_text + i, "-----\n", 6) == 0 &&
                       (i == 0 || g_text[i - 1] == '\n'))) {
            size_t len = i - start;
            int    budget;
            ++docs;
            accepted += (size_t)feed(codec, g_text + start, len, -1);
            /* The consumer saying no at each event in turn. */
            for (budget = 0; budget < 64; ++budget) {
                (void)feed(codec, g_text + start, len, budget);
            }
            start = i + 6;
            i += 5;
        }
    }
    printf("  %s: %u documents, %u accepted\n", format, (unsigned)docs,
           (unsigned)accepted);
    check(docs > 5, "the documents were read");
    check(accepted > 0 && accepted < docs, "some accepted and some refused");
}

/* What the encoders write for text that needs escaping, back through the
   decoders: a round trip of the awkward characters. */
static void test_encoders(void) {
    static const cf_pair pairs[] = {
        {"a.quote", "say \"hi\""}, {"a.back", "C:\\dir"}, {"a.ctrl", "tab\tnl\ncr\r"},
        {"a.uni", "caf\xc3\xa9"},  {"b.list.0", "x"},     {"b.list.1", "y"},
        {"b.list.count", "2"},     {"c.empty", ""}};
    static const char* const formats[] = {"json", "toml", "ini", NULL};
    cf_error                 err       = CF_ERROR_INIT;
    cf_builder*              b         = cf_build(NULL);
    cf_config*               c;
    size_t                   i;

    cf_builder_memory(b, "m", 0, pairs, sizeof pairs / sizeof pairs[0]);
    c = cf_builder_create(b, &err);
    check(c != NULL, "a configuration to write out");
    if (c == NULL) {
        return;
    }
    for (i = 0; formats[i] != NULL; ++i) {
        const cf_codec* codec = cf_codec_by_name(formats[i]);
        size_t          n;
        if (codec == NULL) {
            continue;
        }
        n = cf_emit(c, formats[i], CF_EMIT_ALL, 0, g_out, sizeof g_out, &err);
        if (n == 0) {
            continue; /* a format that cannot hold one of these says so */
        }
        check(feed(codec, g_out, n, -1) == 1, "what an encoder wrote reads back");
        check(cf_emit(c, formats[i], CF_EMIT_ALL, 0, g_out, 8, &err) == n,
              "and it is measured past a short buffer");
    }
    cf_config_free(c);
}

int main(int argc, char** argv) {
    const char* dir = (argc > 1) ? argv[1] : CF_EDGES_DIR;

    run_file(dir, "json");
    run_file(dir, "toml");
    run_file(dir, "ini");
    run_file(dir, "properties");
    test_encoders();
    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
