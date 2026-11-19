/*
 * dump.c - `configforge codec dump`: every event of a document, one line
 * each, with the place the reader gives it. The reader's own place is
 * the one explain() names, so what the dump shows is what a report will
 * say. Nothing is kept: the sink prints as the events come. C89.
 */

#include "codec.h"

#include <stdio.h>
#include <string.h>

/* The place of a value is written into a buffer of this many bytes. */
#define DP_PLACE_MAX 160u
/* The path of the event, and how much of it fits. */
#define DP_PATH_MAX ((size_t)CF_KEY_MAX * 2u)

/* One open table or array: the path as it was before it, and, for an
   array, the index its next member takes. */
typedef struct dp_frame {
    size_t   len;
    cf_bool  cut;
    cf_bool  array;
    unsigned next;
} dp_frame;

typedef struct dp_ctx {
    cf_arena* ar;
    char      path[DP_PATH_MAX];
    size_t    len;   /* bytes of path in use */
    cf_bool   cut;   /* the path did not fit: print it ending in "..." */
    size_t    depth; /* frames in use */
    dp_frame  stack[CF_MAX_DEPTH + 1];
} dp_ctx;

/* Append `n` bytes of `s`, after a '.' unless the path is empty. Past
   the buffer the path is marked cut and nothing more is added. */
static void dp_put(dp_ctx* c, const char* s, size_t n) {
    size_t need = n + ((c->len > 0) ? 1u : 0u);

    if (c->cut) {
        return;
    }
    if (c->len + need >= DP_PATH_MAX) {
        c->cut = CF_TRUE;
        return;
    }
    if (c->len > 0) {
        c->path[c->len++] = '.';
    }
    memcpy(c->path + c->len, s, n);
    c->len += n;
}

/* The path of the member now arriving: the parent's, and its name, or
   its index when the parent is an array. */
static void dp_member(dp_ctx* c, const char* name, size_t n) {
    if (c->depth == 0) {
        return;
    }
    if (c->stack[c->depth - 1].array) {
        char idx[16];

        n = (size_t)sprintf(idx, "%u", c->stack[c->depth - 1].next++);
        dp_put(c, idx, n);
    } else {
        dp_put(c, name, n);
    }
}

static void dp_path(const dp_ctx* c) {
    if (c->len == 0 && !c->cut) {
        fputc('.', stdout);
        return;
    }
    fwrite(c->path, 1, c->len, stdout);
    if (c->cut) {
        fputs("...", stdout);
    }
}

static cf_bool dp_open(void* ctx, const char* name, size_t n, const char* kind,
                       cf_bool array) {
    dp_ctx*  c = (dp_ctx*)ctx;
    dp_frame f;

    f.len   = c->len;
    f.cut   = c->cut;
    f.array = array;
    f.next  = 0;
    dp_member(c, name, n);
    fputs("-\t", stdout);
    dp_path(c);
    fputc('\t', stdout);
    fputs(kind, stdout);
    fputc('\n', stdout);
    if (c->depth >= sizeof c->stack / sizeof c->stack[0]) {
        return CF_FALSE;
    }
    c->stack[c->depth++] = f;
    return CF_TRUE;
}

static cf_bool dp_table(void* ctx, const char* name, size_t n) {
    return dp_open(ctx, name, n, "table", CF_FALSE);
}

static cf_bool dp_array(void* ctx, const char* name, size_t n) {
    return dp_open(ctx, name, n, "array", CF_TRUE);
}

/* A text value, quoted; nothing a file holds starts a line of its own. */
static void dp_quoted(const char* p, size_t n) {
    static const char hex[] = "0123456789abcdef";
    size_t            i;

    fputc('"', stdout);
    for (i = 0; i < n; ++i) {
        unsigned char ch = (unsigned char)p[i];

        if (ch == '\\') {
            fputs("\\\\", stdout);
        } else if (ch == '"') {
            fputs("\\\"", stdout);
        } else if (ch == '\n') {
            fputs("\\n", stdout);
        } else if (ch == '\r') {
            fputs("\\r", stdout);
        } else if (ch == '\t') {
            fputs("\\t", stdout);
        } else if (ch < 0x20 || ch == 0x7f) {
            fputs("\\x", stdout);
            fputc(hex[ch >> 4], stdout);
            fputc(hex[ch & 15], stdout);
        } else {
            fputc((int)ch, stdout);
        }
    }
    fputc('"', stdout);
}

static cf_bool dp_scalar(void* ctx, const char* name, size_t n, const cf_value* v,
                         cf_bool textual, const cf_loc* at) {
    dp_ctx* c = (dp_ctx*)ctx;
    char    place[DP_PLACE_MAX];
    size_t  len = c->len;
    cf_bool cut = c->cut;

    dp_member(c, name, n);
    if (cf_loc_text(at, place, sizeof place) == 0) {
        place[0] = '-';
        place[1] = '\0';
    }
    fputs(place, stdout);
    fputc('\t', stdout);
    dp_path(c);
    fputc('\t', stdout);
    fputs(cf_type_name(v->type), stdout);
    fputc('\t', stdout);
    if (v->type == CF_TYPE_TEXT) {
        dp_quoted(v->u.t.p, v->u.t.n);
    } else {
        char text[CF_VALUE_TEXT_MAX + 1];

        text[cf_value_to_text(v, text, sizeof text)] = '\0';
        fputs(text, stdout);
    }
    if (textual) {
        fputs(" (textual)", stdout);
    }
    fputc('\n', stdout);
    c->len = len;
    c->cut = cut;
    return CF_TRUE;
}

static cf_bool dp_end(void* ctx) {
    dp_ctx* c = (dp_ctx*)ctx;

    if (c->depth > 0) {
        --c->depth;
        c->len = c->stack[c->depth].len;
        c->cut = c->stack[c->depth].cut;
    }
    return CF_TRUE;
}

/* The room a reader asks for is lent from the file's arena. */
static char* dp_text(void* ctx, size_t n) {
    return (char*)cf_arena_alloc(((dp_ctx*)ctx)->ar, n);
}

static void* dp_work(void* ctx, size_t n) {
    return cf_arena_alloc(((dp_ctx*)ctx)->ar, n);
}

/* `dump <file> [--as <format>]`: the file and the format, once each. */
static cf_bool dp_args(int argc, char** argv, const char** file, const char** as) {
    int a;
    int files = 0;
    int names = 0;

    for (a = 0; a < argc; ++a) {
        if (strcmp(argv[a], "--as") == 0) {
            if (names++ > 0 || a + 1 >= argc) {
                return CF_FALSE;
            }
            ++a;
            *as = argv[a];
        } else {
            if (files++ > 0) {
                return CF_FALSE;
            }
            *file = argv[a];
        }
    }
    return files == 1;
}

int codec_dump(int argc, char** argv) {
    const char*     as   = NULL;
    const char*     file = NULL;
    cf_arena*       ar;
    cf_blob         blob;
    const cf_codec* c;
    cf_detect       how;
    cf_doc_sink     sink;
    cf_error        err = CF_ERROR_INIT;
    dp_ctx*         ctx;
    int             rc;

    if (!dp_args(argc, argv, &file, &as)) {
        fprintf(stderr, "codec: dump <file> [--as <format>]\n");
        return 2;
    }
    ar = cf_arena_new(NULL, 0);
    if (ar == NULL) {
        fprintf(stderr, "codec: out of memory\n");
        return 2;
    }
    rc  = codec_read(file, as, ar, &blob, &c, &how);
    ctx = (rc == 0) ? (dp_ctx*)cf_arena_alloc(ar, sizeof *ctx) : NULL;
    if (rc == 0 && ctx == NULL) {
        fprintf(stderr, "codec: out of memory\n");
        rc = 2;
    }
    if (rc == 0) {
        memset(ctx, 0, sizeof *ctx);
        ctx->ar = ar;
        memset(&sink, 0, sizeof sink);
        sink.size         = sizeof sink;
        sink.begin_table  = dp_table;
        sink.begin_array  = dp_array;
        sink.scalar       = dp_scalar;
        sink.end          = dp_end;
        sink.ctx          = ctx;
        sink.reserve_text = dp_text;
        sink.reserve_work = dp_work;
        if (!cf_codec_decode(c, &blob, &sink, &err)) {
            fflush(stdout);
            fprintf(stderr, "codec: %s\n", err.message);
            rc = 1;
        }
    }
    cf_arena_free(ar);
    return rc;
}
