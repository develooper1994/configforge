/*
 * detect.c - `configforge codec detect` and `configforge codec check`,
 * and the one way both read a file (codec_read), which the commands
 * that follow them read with too. They ask of bytes what format they
 * are and whether they are one; neither needs a contract or any
 * configuration. C89.
 */

#include "codec.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

/* Standard input is read in chunks of this many bytes. */
#define STDIN_CHUNK 65536u

/* Standard input, whole, into `ar`, named `stdin`. False when there is
   no room. */
static cf_bool read_stdin(cf_arena* ar, cf_blob* out) {
    size_t cap = STDIN_CHUNK;
    size_t n   = 0;
    char*  buf = (char*)cf_arena_alloc(ar, cap);

#ifdef _WIN32
    (void)_setmode(_fileno(stdin), _O_BINARY);
#endif
    if (buf == NULL) {
        return CF_FALSE;
    }
    for (;;) {
        size_t got;

        if (cap - n < STDIN_CHUNK) {
            char* bigger = (char*)cf_arena_alloc(ar, cap * 2);

            if (bigger == NULL) {
                return CF_FALSE;
            }
            memcpy(bigger, buf, n);
            buf = bigger;
            cap *= 2;
        }
        got = fread(buf + n, 1, STDIN_CHUNK, stdin);
        n += got;
        if (got < STDIN_CHUNK) {
            break;
        }
    }
    cf_blob_mem(out, buf, n, "stdin");
    return CF_TRUE;
}

int codec_read(const char* path, const char* as, cf_arena* ar, cf_blob* out,
               const cf_codec** codec, cf_detect* how) {
    const cf_codec* c;

    memset(how, 0, sizeof *how);
    *codec = NULL;
    if (strcmp(path, "-") == 0) {
        if (!read_stdin(ar, out)) {
            fprintf(stderr, "codec: cannot read %s\n", path);
            return 2;
        }
    } else {
        cf_error err = CF_ERROR_INIT;

        if (!cf_blob_file(out, ar, path, &err)) {
            fprintf(stderr, "codec: cannot read %s\n", path);
            return 2;
        }
    }

    if (as != NULL) {
        c = cf_codec_by_name(as);
        if (c == NULL) {
            const char* option = cf_format_option(as);

            if (option != NULL) {
                fprintf(stderr, "codec: format '%s' is not compiled into this build; ",
                        as);
                fprintf(stderr, "turn on %s\n", option);
            } else {
                fprintf(stderr, "codec: no format called '%s'\n", as);
            }
            return 2;
        }
        how->how = CF_DETECT_NAMED;
    } else {
        c = cf_codec_detect(NULL, 0, out, (strcmp(path, "-") == 0) ? NULL : path, NULL,
                            how);
        if (c == NULL) {
            if (how->first != NULL && how->second != NULL) {
                fprintf(stderr, "codec: %s reads as %s and as %s, differently; ", path,
                        how->first->name, how->second->name);
                fputs("name its format with --as\n", stderr);
            } else {
                fprintf(stderr, "codec: cannot tell the format of %s; ", path);
                fputs("name it with --as\n", stderr);
            }
            return 1;
        }
    }
    if (!cf_codec_can_read(c)) {
        fprintf(stderr, "codec: format '%s' can be written but not read\n", c->name);
        return 2;
    }
    *codec = c;
    return 0;
}

int codec_detect(int argc, char** argv) {
    cf_arena*       ar;
    cf_blob         blob;
    const cf_codec* c;
    cf_detect       how;
    int             rc;

    if (argc != 1) {
        fprintf(stderr, "codec: detect <file>\n");
        return 2;
    }
    ar = cf_arena_new(NULL, 0);
    if (ar == NULL) {
        fprintf(stderr, "codec: out of memory\n");
        return 2;
    }
    rc = codec_read(argv[0], NULL, ar, &blob, &c, &how);
    if (rc == 0) {
        printf("%s (%s)\n", c->name,
               (how.how == CF_DETECT_EXTENSION) ? "extension" : "content");
    }
    cf_arena_free(ar);
    return rc;
}

/* A sink that keeps nothing: every event is welcome, and the room a
   reader asks for is lent from the file's arena, so that none refuses a
   long string for want of it. */
static cf_bool keep_table(void* ctx, const char* name, size_t n) {
    (void)ctx;
    (void)name;
    (void)n;
    return CF_TRUE;
}

static cf_bool keep_scalar(void* ctx, const char* name, size_t n, const cf_value* v,
                           cf_bool textual, const cf_loc* at) {
    (void)ctx;
    (void)name;
    (void)n;
    (void)v;
    (void)textual;
    (void)at;
    return CF_TRUE;
}

static cf_bool keep_end(void* ctx) {
    (void)ctx;
    return CF_TRUE;
}

static char* lend_text(void* ctx, size_t n) {
    return (char*)cf_arena_alloc((cf_arena*)ctx, n);
}

static void* lend_work(void* ctx, size_t n) {
    return cf_arena_alloc((cf_arena*)ctx, n);
}

int codec_check(int argc, char** argv) {
    const char* as    = NULL;
    int         worst = 0;
    int         files = 0;
    int         a;

    for (a = 0; a < argc; ++a) {
        if (strcmp(argv[a], "--as") == 0) {
            if (a + 1 >= argc) {
                fprintf(stderr, "codec: check <file>... [--as <format>]\n");
                return 2;
            }
            as = argv[++a];
        } else {
            ++files;
        }
    }
    if (files == 0) {
        fprintf(stderr, "codec: check <file>... [--as <format>]\n");
        return 2;
    }

    for (a = 0; a < argc; ++a) {
        cf_arena*       ar;
        cf_blob         blob;
        const cf_codec* c;
        cf_detect       how;
        cf_doc_sink     sink;
        cf_error        err = CF_ERROR_INIT;
        int             rc;

        if (strcmp(argv[a], "--as") == 0) {
            ++a;
            continue;
        }
        ar = cf_arena_new(NULL, 0);
        if (ar == NULL) {
            fprintf(stderr, "codec: out of memory\n");
            return 2;
        }
        rc = codec_read(argv[a], as, ar, &blob, &c, &how);
        if (rc == 0) {
            memset(&sink, 0, sizeof sink);
            sink.size         = sizeof sink;
            sink.begin_table  = keep_table;
            sink.begin_array  = keep_table;
            sink.scalar       = keep_scalar;
            sink.end          = keep_end;
            sink.ctx          = ar;
            sink.reserve_text = lend_text;
            sink.reserve_work = lend_work;
            if (cf_codec_decode(c, &blob, &sink, &err)) {
                printf("%s: ok\n", argv[a]);
            } else {
                printf("%s: %s\n", argv[a], err.message);
                rc = 1;
            }
        }
        if (rc > worst) {
            worst = rc;
        }
        cf_arena_free(ar);
    }
    return worst;
}
