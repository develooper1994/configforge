/*
 * dump.c - `configforge codec dump`: every event of a document, one line
 * each, with the place the reader gives it. The reader's own place is
 * the one explain() names, so what the dump shows is what a report will
 * say. Nothing is kept: the sink prints as the events come.
 *
 * `configforge codec textconv` (N-16) is the same lines without the place,
 * for Git's textconv driver: the document is read once with the output
 * muted, so a refusal in the middle prints nothing, and then again with it
 * on; a file the reader refuses is printed as it is, exit 0. C89.
 */

#include "codec.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

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
    cf_bool   plain; /* no place column and no "-" lead (textconv) */
    cf_bool   mute;  /* print nothing: the first reading of textconv */
    size_t    depth; /* frames in use */
    dp_frame  stack[CF_MAX_DEPTH + 1];
} dp_ctx;

/* Everything the sink prints goes through these, so that a muted reading
   prints nothing. */
static void dp_out_n(const dp_ctx* c, const char* p, size_t n) {
    if (!c->mute) {
        fwrite(p, 1, n, stdout);
    }
}

static void dp_out_s(const dp_ctx* c, const char* s) {
    dp_out_n(c, s, strlen(s));
}

static void dp_out_c(const dp_ctx* c, int ch) {
    char b = (char)ch;

    dp_out_n(c, &b, 1);
}

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
        dp_out_c(c, '.');
        return;
    }
    dp_out_n(c, c->path, c->len);
    if (c->cut) {
        dp_out_s(c, "...");
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
    if (!c->plain) {
        dp_out_s(c, "-\t");
    }
    dp_path(c);
    dp_out_c(c, '\t');
    dp_out_s(c, kind);
    dp_out_c(c, '\n');
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
static void dp_quoted(const dp_ctx* c, const char* p, size_t n) {
    static const char hex[] = "0123456789abcdef";
    size_t            i;

    dp_out_c(c, '"');
    for (i = 0; i < n; ++i) {
        unsigned char ch = (unsigned char)p[i];

        if (ch == '\\') {
            dp_out_s(c, "\\\\");
        } else if (ch == '"') {
            dp_out_s(c, "\\\"");
        } else if (ch == '\n') {
            dp_out_s(c, "\\n");
        } else if (ch == '\r') {
            dp_out_s(c, "\\r");
        } else if (ch == '\t') {
            dp_out_s(c, "\\t");
        } else if (ch < 0x20 || ch == 0x7f) {
            dp_out_s(c, "\\x");
            dp_out_c(c, (int)hex[ch >> 4]);
            dp_out_c(c, (int)hex[ch & 15]);
        } else {
            dp_out_c(c, (int)ch);
        }
    }
    dp_out_c(c, '"');
}

static cf_bool dp_scalar(void* ctx, const char* name, size_t n, const cf_value* v,
                         cf_bool textual, const cf_loc* at) {
    dp_ctx* c   = (dp_ctx*)ctx;
    size_t  len = c->len;
    cf_bool cut = c->cut;

    dp_member(c, name, n);
    if (!c->plain) {
        char place[DP_PLACE_MAX];

        if (cf_loc_text(at, place, sizeof place) == 0) {
            place[0] = '-';
            place[1] = '\0';
        }
        dp_out_s(c, place);
        dp_out_c(c, '\t');
    }
    dp_path(c);
    dp_out_c(c, '\t');
    dp_out_s(c, cf_type_name(v->type));
    dp_out_c(c, '\t');
    if (v->type == CF_TYPE_TEXT) {
        dp_quoted(c, v->u.t.p, v->u.t.n);
    } else {
        char text[CF_VALUE_TEXT_MAX + 1];

        text[cf_value_to_text(v, text, sizeof text)] = '\0';
        dp_out_s(c, text);
    }
    if (textual) {
        dp_out_s(c, " (textual)");
    }
    dp_out_c(c, '\n');
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

/* One reading of `blob` into the printing sink. `ctx` keeps its arena,
   `plain` and `mute` and starts from an empty path, so it can be read
   again. */
static cf_bool dp_run(const cf_codec* codec, const cf_blob* blob, dp_ctx* ctx,
                      cf_error* err) {
    cf_doc_sink sink;
    cf_arena*   ar    = ctx->ar;
    cf_bool     plain = ctx->plain;
    cf_bool     mute  = ctx->mute;

    memset(ctx, 0, sizeof *ctx);
    ctx->ar    = ar;
    ctx->plain = plain;
    ctx->mute  = mute;
    memset(&sink, 0, sizeof sink);
    sink.size         = sizeof sink;
    sink.begin_table  = dp_table;
    sink.begin_array  = dp_array;
    sink.scalar       = dp_scalar;
    sink.end          = dp_end;
    sink.ctx          = ctx;
    sink.reserve_text = dp_text;
    sink.reserve_work = dp_work;
    return cf_codec_decode(codec, blob, &sink, err);
}

int codec_dump(int argc, char** argv) {
    const char*     as   = NULL;
    const char*     file = NULL;
    cf_arena*       ar;
    cf_blob         blob;
    const cf_codec* c;
    cf_detect       how;
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
        if (!dp_run(c, &blob, ctx, &err)) {
            fflush(stdout);
            fprintf(stderr, "codec: %s\n", err.message);
            codec_report(&err);
            rc = 1;
        }
    }
    cf_arena_free(ar);
    return rc;
}

/* The file's own bytes, unchanged: what textconv prints for a file the
   readers refuse. 0, or 2 when standard output cannot take them. */
static int dp_raw(const cf_blob* blob) {
    if ((blob->n == 0 || fwrite(blob->p, 1, blob->n, stdout) == blob->n) &&
        fflush(stdout) == 0) {
        return 0;
    }
    fprintf(stderr, "codec: cannot write the file to standard output\n");
    return 2;
}

int codec_textconv(int argc, char** argv) {
    const char*     as   = NULL;
    const char*     file = NULL;
    cf_arena*       ar;
    cf_blob         blob;
    const cf_codec* c;
    cf_detect       how;
    cf_error        err = CF_ERROR_INIT;
    dp_ctx*         ctx;
    int             rc;

    if (!dp_args(argc, argv, &file, &as)) {
        fprintf(stderr, "codec: textconv <file> [--as <format>]\n");
        return 2;
    }
#ifdef _WIN32
    (void)_setmode(_fileno(stdout), _O_BINARY);
#endif
    ar = cf_arena_new(NULL, 0);
    if (ar == NULL) {
        fprintf(stderr, "codec: out of memory\n");
        return 2;
    }
    memset(&blob, 0, sizeof blob);
    rc = codec_read(file, as, ar, &blob, &c, &how);
    if (rc == 1) {
        /* The format cannot be told: the file as it is. */
        rc = dp_raw(&blob);
    } else if (rc == 0) {
        ctx = (dp_ctx*)cf_arena_alloc(ar, sizeof *ctx);
        if (ctx == NULL) {
            fprintf(stderr, "codec: out of memory\n");
            rc = 2;
        } else {
            memset(ctx, 0, sizeof *ctx);
            ctx->ar   = ar;
            ctx->mute = CF_TRUE;
            if (!dp_run(c, &blob, ctx, &err)) {
                fprintf(stderr, "codec: %s\n", err.message);
                codec_report(&err);
                rc = dp_raw(&blob);
            } else {
                ctx->mute  = CF_FALSE;
                ctx->plain = CF_TRUE;
                if (!dp_run(c, &blob, ctx, &err)) {
                    fflush(stdout);
                    fprintf(stderr,
                            "codec: textconv: the second reading failed (a bug)\n");
                    rc = 3;
                }
            }
        }
    }
    cf_arena_free(ar);
    return rc;
}
