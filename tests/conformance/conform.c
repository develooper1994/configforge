/*
 * conform.c - one document through one of the library's readers, for
 * the conformance run (scripts/check/conformance.py): exit 0 when the reader
 * accepts it, 1 when it refuses it, 2 when it could not be tried.
 *
 *     conform <format> <file> [--keys] [--codec <library> <symbol>]
 *     conform --detect <file>
 *
 * With --detect, the file is not read as a format but asked which one
 * its content is (cf_codec_detect, no name): the format's name on a
 * line and exit 0, or exit 1 when none is chosen.
 *
 * With --codec, the document is read by a codec from a shared library -
 * <symbol> a `const cf_codec* const` it exports - rather than by this
 * library's reader for <format>, which still says how the suite's
 * documents are measured: a format of one's own scored against its
 * standard's suite with the same floors (D-68).
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

/* Windows writes a text stream's line feed as CR LF, which would end up
   in every value; the output is bytes, so stdout is made binary there. */
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#define CONFORM_BINARY_STDOUT() (void)_setmode(_fileno(stdout), _O_BINARY)
#else
#define CONFORM_BINARY_STDOUT() ((void)0)
#endif
#if defined(CONFORM_DLOPEN)
#include <dlfcn.h>
#endif

/* A codec a shared library exports, or NULL with the reason printed.
   The library stays loaded until the program ends. */
static const cf_codec* conform_load(const char* lib, const char* symbol) {
#if defined(_WIN32)
    HMODULE                m = LoadLibraryA(lib);
    FARPROC                f;
    const cf_codec* const* p;

    if (m == NULL) {
        fprintf(stderr, "conform: cannot load %s\n", lib);
        return NULL;
    }
    f = GetProcAddress(m, symbol);
    if (f == NULL) {
        fprintf(stderr, "conform: %s has no %s\n", lib, symbol);
        return NULL;
    }
    /* The address of data, returned as a function's: the same bytes. */
    memcpy(&p, &f, sizeof p);
    return *p;
#elif defined(CONFORM_DLOPEN)
    void*       m = dlopen(lib, RTLD_NOW | RTLD_LOCAL);
    const void* sym;

    if (m == NULL) {
        fprintf(stderr, "conform: %s\n", dlerror());
        return NULL;
    }
    sym = dlsym(m, symbol);
    if (sym == NULL) {
        fprintf(stderr, "conform: %s has no %s\n", lib, symbol);
        return NULL;
    }
    return *(const cf_codec* const*)sym;
#else
    (void)lib;
    (void)symbol;
    fprintf(stderr, "conform: this platform loads no shared library\n");
    return NULL;
#endif
}

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
    /* Joined by a dot below the root, even after an empty name: TOML
       allows "" as a key, and "a" and the "" table's "a" are two keys. */
    need = c->plen + (c->depth >= 1 ? 1 : 0) + n;
    if (need >= sizeof c->path) {
        return CF_FALSE;
    }
    if (c->depth >= 1) {
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

/* Bytes with the line's own separators escaped - backslash, tab, line
   feed, carriage return and NUL - so that a key or a value holding them
   stays one line; scripts/check/conformance.py undoes it. */
static void conform_escaped(const char* p, size_t n) {
    size_t i;
    for (i = 0; i < n; ++i) {
        switch (p[i]) {
        case '\\':
            fputs("\\\\", stdout);
            break;
        case '\t':
            fputs("\\t", stdout);
            break;
        case '\n':
            fputs("\\n", stdout);
            break;
        case '\r':
            fputs("\\r", stdout);
            break;
        case '\0':
            fputs("\\0", stdout);
            break;
        default:
            putchar(p[i]);
            break;
        }
    }
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
    (void)at;
    if (!conform_name(c, name, n)) {
        return CF_FALSE;
    }
    if (c->print) {
        const char* p = g_value;
        size_t      k;
        if (v->type == CF_TYPE_TEXT) {
            p = v->u.t.p;
            k = v->u.t.n;
        } else {
            k = cf_value_to_text(v, g_value, sizeof g_value);
        }
        conform_escaped(c->path, c->plen); /* a key may hold a NUL */
        /* Text whose type a definition decides - YAML's plain scalars,
           TOML's dates - is "untyped": the comparison reads it as the
           reference's value would be written. */
        printf("\t%s\t", (textual && v->type == CF_TYPE_TEXT) ? "untyped"
                                                              : cf_type_name(v->type));
        conform_escaped(p, k);
        putchar('\n');
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
        conform_escaped(c->path, c->plen);
        printf(".count\tint\t%lu\n", f->index);
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

/* The format the content of a file says, with no name to go by (D-73):
   its name on a line, exit 0; exit 1 when no format is chosen, and 2
   when the file could not be tried. */
static int conform_detect(const char* path) {
    cf_arena*       ar = cf_arena_new(NULL, 0);
    cf_arena*       trial;
    cf_blob         blob;
    cf_error        err = CF_ERROR_INIT;
    const cf_codec* codec;
    int             rc;

    if (ar == NULL || !cf_blob_file(&blob, ar, path, &err)) {
        fprintf(stderr, "conform: cannot read %s\n", path);
        cf_arena_free(ar);
        return 2;
    }
    trial = cf_blob_arena(ar);
    codec = cf_codec_detect(NULL, 0, &blob, NULL, trial, NULL);
    if (codec != NULL) {
        printf("%s\n", codec->name);
    }
    rc = (codec != NULL) ? 0 : 1;
    cf_blob_arena_done(trial, ar);
    cf_arena_free(ar);
    return rc;
}

int main(int argc, char** argv) {
    const cf_codec* codec;
    cf_arena*       ar;
    cf_blob         blob;
    cf_error        err = CF_ERROR_INIT;
    cf_doc_sink     any;
    cf_bool         keys = CF_FALSE;
    int             i;

    if (argc == 3 && strcmp(argv[1], "--detect") == 0) {
        return conform_detect(argv[2]);
    }
    if (argc < 3) {
        fprintf(stderr, "usage: conform <format> <file> [--keys] "
                        "[--codec <library> <symbol>]\n"
                        "       conform --detect <file>\n");
        return 2;
    }
    CONFORM_BINARY_STDOUT();
    codec = cf_codec_by_name(argv[1]);
    for (i = 3; i < argc; ++i) {
        if (strcmp(argv[i], "--keys") == 0) {
            keys = CF_TRUE;
        } else if (strcmp(argv[i], "--codec") == 0 && i + 2 < argc) {
            codec = conform_load(argv[i + 1], argv[i + 2]);
            if (codec == NULL) {
                return 2;
            }
            i += 2;
        } else {
            fprintf(stderr, "conform: unknown argument %s\n", argv[i]);
            return 2;
        }
    }
    ar = cf_arena_new(NULL, 0);
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
       keeps an invalid document invalid. JSON5's is always wrapped: a
       comment may come before the root, and one may end the last line,
       so the brace closing the wrapper goes on a line of its own. */
    if (strcmp(argv[1], "json5") == 0) {
        static char wrapped[1 << 20];
        if (blob.n + 8 > sizeof wrapped) {
            cf_arena_free(ar);
            return 2;
        }
        memcpy(wrapped, "{\"v\":", 5);
        memcpy(wrapped + 5, blob.p, blob.n);
        memcpy(wrapped + 5 + blob.n, "\n}", 2);
        cf_blob_mem(&blob, wrapped, blob.n + 7, argv[2]);
    } else if (strcmp(argv[1], "cbor") == 0) {
        /* A vector is mostly a lone value and a configuration a map: it
           is measured as the value of the member "v" (A1 61 76 before
           it), which keeps a malformed item malformed - the map's value
           is cut short, or holds a stray break. */
        static char wrapped[1 << 20];
        if (blob.n + 3 > sizeof wrapped) {
            cf_arena_free(ar);
            return 2;
        }
        memcpy(wrapped, "\xA1\x61\x76", 3);
        memcpy(wrapped + 3, blob.p, blob.n);
        cf_blob_mem(&blob, wrapped, blob.n + 3, argv[2]);
    } else if (strcmp(argv[1], "json") == 0) {
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
    g_sink.print = keys;
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
