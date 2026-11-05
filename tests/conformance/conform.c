/*
 * conform.c - one document through one of the library's readers, for
 * the conformance run (scripts/conformance.py): exit 0 when the reader
 * accepts it, 1 when it refuses it, 2 when it could not be tried.
 *
 *     conform <format> <file> [--keys]
 *
 * With --keys, the keys it read and their values, one a line, sorted,
 * as "key<TAB>type<TAB>text" - what the run compares with a suite's
 * expected values.
 */

#include <configforge/configforge.h>
#include <configforge/cf_flatten.h>
#include <configforge/cf_serialize.h>

#include <stdio.h>
#include <string.h>

/* A sink that takes every event: the grammar is what is measured, not
   the configuration model's rule that a document is a table at its root
   (a suite's valid documents are as often an array or a lone value). */
static cf_bool any_open(void* ctx, const char* name, size_t n) {
    (void)ctx;
    (void)name;
    (void)n;
    return CF_TRUE;
}
static cf_bool any_scalar(void* ctx, const char* name, size_t n, const cf_value* v,
                          cf_bool textual, const cf_loc* at) {
    (void)ctx;
    (void)name;
    (void)n;
    (void)v;
    (void)textual;
    (void)at;
    return CF_TRUE;
}
static cf_bool any_end(void* ctx) {
    (void)ctx;
    return CF_TRUE;
}
static char  g_text[1 << 20];
static char* any_reserve(void* ctx, size_t n) {
    (void)ctx;
    return (n <= sizeof g_text) ? g_text : NULL;
}

/* Static, as the tests keep what is large: the build's stack budget
   (-Wstack-usage) holds for every program in the tree. */
static cf_keyspace ks;
static cf_flatten  fl;
static char        g_value[4096];

int main(int argc, char** argv) {
    const cf_codec* codec;
    cf_arena*       ar;
    cf_blob         blob;
    cf_error        err = CF_ERROR_INIT;
    cf_bool         ok;

    if (argc < 3) {
        fprintf(stderr, "usage: conform <format> <file> [--keys]\n");
        return 2;
    }
    codec = cf_codec_by_name(argv[1]);
    ar    = cf_arena_new(NULL, 0);
    if (codec == NULL || ar == NULL || !cf_ks_init(&ks, ar, 64)) {
        fprintf(stderr, "conform: no reader for '%s'\n", argv[1]);
        return 2;
    }
    if (!cf_blob_file(&blob, ar, argv[2], &err)) {
        fprintf(stderr, "conform: %s\n", err.message);
        cf_arena_free(ar);
        return 2;
    }
    /* JSON's grammar allows any value at the root, and the configuration
       model, enforced by the reader, a table only; a suite's lone value
       or array is measured as the member of one, `{"v": ...}`, which
       keeps an invalid document invalid. */
    if (strcmp(argv[1], "json") == 0) {
        size_t k = 0;
        while (k < blob.n && (blob.p[k] == ' ' || blob.p[k] == '\t' ||
                              blob.p[k] == '\n' || blob.p[k] == '\r')) {
            ++k;
        }
        if (k >= blob.n || blob.p[k] != '{') {
            static char wrapped[1 << 20];
            if (blob.n + 7 > sizeof wrapped) {
                cf_arena_free(ar);
                return 2;
            }
            memcpy(wrapped, "{\"v\":", 5);
            memcpy(wrapped + 5, blob.p, blob.n);
            wrapped[5 + blob.n] = '}';
            cf_blob_mem(&blob, wrapped, blob.n + 6, argv[2]);
        }
    }
    if (argc > 3 && strcmp(argv[3], "--keys") == 0) {
        if (!cf_flatten_init(&fl, &ks, 10)) {
            cf_arena_free(ar);
            return 2;
        }
        ok = codec->decode(&blob, cf_flatten_sink(&fl), &err) && !fl.failed &&
             !fl.overflow;
    } else {
        cf_doc_sink any;
        memset(&any, 0, sizeof any);
        any.size         = sizeof any;
        any.begin_table  = any_open;
        any.begin_array  = any_open;
        any.scalar       = any_scalar;
        any.end          = any_end;
        any.reserve_text = any_reserve;
        ok               = codec->decode(&blob, &any, &err);
    }
    if (!ok) {
        printf("refused: %s\n", err.message);
        cf_arena_free(ar);
        return 1;
    }
    if (argc > 3 && strcmp(argv[3], "--keys") == 0) {
        size_t i;
        cf_ks_resolve(&ks);
        for (i = 0; i < cf_ks_count(&ks); ++i) {
            const cf_entry* e = cf_ks_at(&ks, i);
            if (e == NULL || e->selected == NULL) {
                continue;
            }
            (void)cf_value_to_text(&e->selected->value, g_value, sizeof g_value);
            printf("%.*s\t%s\t%s\n", (int)e->keylen, e->key,
                   cf_type_name(e->selected->value.type), g_value);
        }
    }
    cf_arena_free(ar);
    return 0;
}
