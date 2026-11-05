/*
 * conform.c - one document through one of the library's readers, for
 * the conformance run (scripts/conformance.py): exit 0 when the reader
 * accepts it, 1 when it refuses it, 2 when it could not be tried.
 *
 *     conform <format> <file> [--keys]
 *
 * With --keys, what it read, one value a line, as
 * "key<TAB>type<TAB>text", with an array's length as "key.count" - the
 * library's flat keys, built here from the reader's events rather than
 * by the key space, so that what is measured is the reader and not the
 * configuration model's rules for key names (which, for one, has no
 * capital letters, where CMakePresets.json has many).
 */

#include <configforge/configforge.h>
#include <configforge/cf_serialize.h>

#include <stdio.h>
#include <string.h>

/* The flat key the events have reached, and for each level open its
   length before it and, for an array, the next element's number. */
#define CONFORM_DEPTH 256

typedef struct conform_frame {
    size_t        saved;
    unsigned long index;
    int           is_array;
} conform_frame;

typedef struct conform_sink {
    cf_bool       print;
    char          path[4096];
    size_t        plen;
    conform_frame stack[CONFORM_DEPTH];
    int           depth;
} conform_sink;

static conform_sink g_sink;
static char         g_value[4096];

/* The key of the next member or element, appended to the path; false
   when it does not fit, which refuses the document as unmeasurable. */
static cf_bool conform_name(conform_sink* c, const char* name, size_t n) {
    char   num[32];
    size_t need;

    if (c->depth >= 0 && c->stack[c->depth].is_array) {
        n    = (size_t)sprintf(num, "%lu", c->stack[c->depth].index++);
        name = num;
    } else if (name == NULL) {
        return CF_TRUE; /* the root */
    }
    need = c->plen + (c->plen > 0 ? 1 : 0) + n;
    if (need >= sizeof c->path) {
        return CF_FALSE;
    }
    if (c->plen > 0) {
        c->path[c->plen++] = '.';
    }
    memcpy(c->path + c->plen, name, n);
    c->plen          = need;
    c->path[c->plen] = '\0';
    return CF_TRUE;
}

static cf_bool conform_open(conform_sink* c, const char* name, size_t n, int is_array) {
    size_t saved = c->plen;
    if (c->depth + 1 >= CONFORM_DEPTH || !conform_name(c, name, n)) {
        return CF_FALSE;
    }
    ++c->depth;
    c->stack[c->depth].saved    = saved;
    c->stack[c->depth].index    = 0;
    c->stack[c->depth].is_array = is_array;
    return CF_TRUE;
}

static cf_bool any_table(void* ctx, const char* name, size_t n) {
    return conform_open((conform_sink*)ctx, name, n, 0);
}
static cf_bool any_array(void* ctx, const char* name, size_t n) {
    return conform_open((conform_sink*)ctx, name, n, 1);
}
static cf_bool any_scalar(void* ctx, const char* name, size_t n, const cf_value* v,
                          cf_bool textual, const cf_loc* at) {
    conform_sink* c     = (conform_sink*)ctx;
    size_t        saved = c->plen;
    (void)textual;
    (void)at;
    if (!conform_name(c, name, n)) {
        return CF_FALSE;
    }
    if (c->print) {
        (void)cf_value_to_text(v, g_value, sizeof g_value);
        printf("%s\t%s\t%s\n", c->path, cf_type_name(v->type), g_value);
    }
    c->plen          = saved;
    c->path[c->plen] = '\0';
    return CF_TRUE;
}
static cf_bool any_end(void* ctx) {
    conform_sink*        c = (conform_sink*)ctx;
    const conform_frame* f;
    if (c->depth < 0) {
        return CF_FALSE;
    }
    f = &c->stack[c->depth];
    if (f->is_array && c->print) {
        printf("%s.count\tint\t%lu\n", c->path, f->index);
    }
    c->plen          = f->saved;
    c->path[c->plen] = '\0';
    --c->depth;
    return CF_TRUE;
}

static char  g_text[1 << 20];
static char* any_reserve(void* ctx, size_t n) {
    (void)ctx;
    return (n <= sizeof g_text) ? g_text : NULL;
}
/* The reader's bookkeeping, as the loader lends it: new memory each
   call, kept until the program ends. A double array keeps it aligned. */
static double g_work[1 << 20];
static size_t g_work_used;
static void*  any_work(void* ctx, size_t n) {
    size_t words = (n + sizeof g_work[0] - 1) / sizeof g_work[0];
    void*  p;
    (void)ctx;
    if (words > sizeof g_work / sizeof g_work[0] - g_work_used) {
        return NULL;
    }
    p = &g_work[g_work_used];
    g_work_used += words;
    return p;
}

int main(int argc, char** argv) {
    const cf_codec* codec;
    cf_arena*       ar;
    cf_blob         blob;
    cf_error        err = CF_ERROR_INIT;
    cf_doc_sink     any;

    if (argc < 3) {
        fprintf(stderr, "usage: conform <format> <file> [--keys]\n");
        return 2;
    }
    codec = cf_codec_by_name(argv[1]);
    ar    = cf_arena_new(NULL, 0);
    if (codec == NULL || ar == NULL) {
        fprintf(stderr, "conform: no reader for '%s'\n", argv[1]);
        cf_arena_free(ar);
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
    g_sink.print = (argc > 3 && strcmp(argv[3], "--keys") == 0) ? CF_TRUE : CF_FALSE;
    g_sink.depth = -1;
    memset(&any, 0, sizeof any);
    any.size         = sizeof any;
    any.begin_table  = any_table;
    any.begin_array  = any_array;
    any.scalar       = any_scalar;
    any.end          = any_end;
    any.ctx          = &g_sink;
    any.reserve_text = any_reserve;
    any.reserve_work = any_work;
    if (!codec->decode(&blob, &any, &err)) {
        printf("refused: %s\n", err.message);
        cf_arena_free(ar);
        return 1;
    }
    cf_arena_free(ar);
    return 0;
}
