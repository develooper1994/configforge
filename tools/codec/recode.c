/*
 * recode.c - `configforge codec recode` (D-108, rule 6): a file converted
 * to UTF-8 once, its bytes recoded and nothing else changed, for a person
 * to review as a diff; and `--check`, whether a file is UTF-8 and, when it
 * is not, what it may be. Only cf_encoding.h's public functions. C89.
 */

#include "codec.h"

#include "configforge/cf_encoding.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static int rc_usage(void) {
    fputs("codec: recode <file> --from <encoding> [--out <file>]\n"
          "       recode <file> --check\n",
          stderr);
    return 2;
}

static cf_bool rc_is_utf16(unsigned enc) {
    return (enc == (unsigned)CF_ENCODING_UTF16LE ||
            enc == (unsigned)CF_ENCODING_UTF16BE)
               ? CF_TRUE
               : CF_FALSE;
}

/* The line and column, from 1, of the byte at p[at]: in bytes for the
   single-byte encodings, in code units for UTF-16, a new line after each
   LF. */
static void rc_place(const char* p, size_t at, unsigned enc, unsigned long* line,
                     unsigned long* col) {
    size_t i;
    size_t step = rc_is_utf16(enc) ? 2u : 1u;

    *line = 1;
    *col  = 1;
    for (i = 0; i + step <= at; i += step) {
        unsigned unit = (unsigned)(unsigned char)p[i];

        if (step == 2) {
            unsigned other = (unsigned)(unsigned char)p[i + 1];

            unit = (enc == (unsigned)CF_ENCODING_UTF16BE) ? ((unit << 8) | other)
                                                          : ((other << 8) | unit);
        }
        if (unit == 10u) {
            ++*line;
            *col = 1;
        } else {
            ++*col;
        }
    }
}

/* The refusal of the byte at p[bad], as the readers write one, with the
   command's prefix. */
static void rc_refuse(const char* path, unsigned enc, const char* p, size_t n,
                      size_t bad) {
    unsigned long line, col;
    unsigned      unit;

    if (!rc_is_utf16(enc)) {
        rc_place(p, bad, enc, &line, &col);
        fprintf(stderr, "codec: %s:%lu:%lu: byte 0x%02X has no character in %s\n", path,
                line, col, (unsigned)(unsigned char)p[bad], cf_encoding_name(enc));
        return;
    }
    if (n - bad < 2) {
        fprintf(stderr, "codec: %s: ends in half a UTF-16 code unit\n", path);
        return;
    }
    rc_place(p, bad, enc, &line, &col);
    if (enc == (unsigned)CF_ENCODING_UTF16BE) {
        unit = ((unsigned)(unsigned char)p[bad] << 8) |
               (unsigned)(unsigned char)p[bad + 1];
    } else {
        unit = ((unsigned)(unsigned char)p[bad + 1] << 8) |
               (unsigned)(unsigned char)p[bad];
    }
    fprintf(stderr, "codec: %s:%lu:%lu: an unpaired UTF-16 surrogate 0x%04X\n", path,
            line, col, unit);
}

/* The encoding the first bytes declare: UTF-16's byte-order mark, or none
   (UTF-8), with the bytes it takes; CF_FALSE when they are UTF-32's. */
static cf_bool rc_mark(const cf_blob* in, unsigned* enc, size_t* skip) {
    const unsigned char* u = (const unsigned char*)in->p;

    *enc  = CF_ENCODING_UTF8;
    *skip = 0;
    if (in->n >= 4 && ((u[0] == 0xFF && u[1] == 0xFE && u[2] == 0 && u[3] == 0) ||
                       (u[0] == 0 && u[1] == 0 && u[2] == 0xFE && u[3] == 0xFF))) {
        return CF_FALSE;
    }
    if (in->n >= 2 && u[0] == 0xFF && u[1] == 0xFE) {
        *enc  = CF_ENCODING_UTF16LE;
        *skip = 2;
    } else if (in->n >= 2 && u[0] == 0xFE && u[1] == 0xFF) {
        *enc  = CF_ENCODING_UTF16BE;
        *skip = 2;
    }
    return CF_TRUE;
}

static int rc_utf32(const char* path) {
    fprintf(stderr,
            "codec: '%s' is UTF-32, which this library does not read; convert it to "
            "UTF-8\n",
            path);
    return 1;
}

/* `data` to `path`, or to standard output (binary) when `path` is NULL. */
static int rc_write(const char* path, const char* data, size_t n) {
    FILE*   f = stdout;
    cf_bool ok;

    if (path != NULL) {
        f = fopen(path, "wb");
        if (f == NULL) {
            fprintf(stderr, "codec: cannot write %s\n", path);
            return 2;
        }
    } else {
#ifdef _WIN32
        (void)_setmode(_fileno(stdout), _O_BINARY);
#endif
    }
    ok = (n == 0 || fwrite(data, 1, n, f) == n) ? CF_TRUE : CF_FALSE;
    if (path != NULL) {
        if (fclose(f) != 0) {
            ok = CF_FALSE;
        }
    } else if (fflush(f) != 0) {
        ok = CF_FALSE;
    }
    if (!ok) {
        fprintf(stderr, "codec: cannot write %s\n",
                (path != NULL) ? path : "standard output");
        return 2;
    }
    return 0;
}

/* `--check`: exit 0 on standard output when UTF-8, else 1 on standard
   error. */
static int rc_check(const char* path, const cf_blob* blob) {
    unsigned enc;
    size_t   skip;
    cf_error err = CF_ERROR_INIT;

    if (!rc_mark(blob, &enc, &skip)) {
        return rc_utf32(path);
    }
    if (skip != 0) {
        fprintf(
            stderr,
            "codec: '%s' is %s (by its byte-order mark): recode it with --from %s\n",
            path, (enc == (unsigned)CF_ENCODING_UTF16LE) ? "UTF-16LE" : "UTF-16BE",
            cf_encoding_name(enc));
        return 1;
    }
    if (!cf_encoding_check(path, blob->p, blob->n, &err)) {
        fprintf(stderr, "codec: %s\n", err.message);
        codec_report(&err);
        return 1;
    }
    printf("'%s' is UTF-8\n", path);
    return 0;
}

/* The bytes of `blob` after the mark, recoded in `enc`, to `out`. */
static int rc_recode(const char* path, const cf_blob* blob, size_t skip, unsigned enc,
                     const char* out) {
    size_t n = blob->n - skip;
    size_t cap, wrote, bad = 0;
    char*  buf;
    int    rc;

    if (n > ((size_t)-1 - 1) / 3) {
        fprintf(stderr, "codec: '%s' is too large\n", path);
        return 2;
    }
    cap = 3 * n + ((n == 0) ? 1u : 0u);
    buf = (char*)malloc(cap);
    if (buf == NULL) {
        fprintf(stderr, "codec: out of memory\n");
        return 2;
    }
    wrote = cf_encoding_recode(enc, blob->p + skip, n, buf, cap, &bad);
    if (wrote == (size_t)-1) {
        rc_refuse(path, enc, blob->p + skip, n, bad);
        rc = 1;
    } else {
        rc = rc_write(out, buf, wrote);
    }
    free(buf);
    return rc;
}

int codec_recode(int argc, char** argv) {
    const char* path  = NULL;
    const char* from  = NULL;
    const char* out   = NULL;
    cf_bool     check = CF_FALSE;
    unsigned    enc   = CF_ENCODING_UTF8;
    unsigned    mark_enc;
    size_t      skip;
    cf_arena*   ar;
    cf_blob     blob;
    cf_error    err = CF_ERROR_INIT;
    int         a, rc;

    for (a = 0; a < argc; ++a) {
        if (strcmp(argv[a], "--check") == 0) {
            check = CF_TRUE;
        } else if (strcmp(argv[a], "--from") == 0 && a + 1 < argc && from == NULL) {
            from = argv[++a];
        } else if (strcmp(argv[a], "--out") == 0 && a + 1 < argc && out == NULL) {
            out = argv[++a];
        } else if (argv[a][0] != '-' && path == NULL) {
            path = argv[a];
        } else {
            return rc_usage();
        }
    }
    if (path == NULL || (check && (from != NULL || out != NULL))) {
        return rc_usage();
    }
    if (from != NULL) {
        if (!cf_encoding_by_name(from, &enc)) {
            fprintf(stderr,
                    "codec: no encoding '%s'; there are: utf-8, iso-8859-1, "
                    "windows-1252, windows-1254, iso-8859-9, utf-16le, utf-16be\n",
                    from);
            return 2;
        }
        if (enc == (unsigned)CF_ENCODING_UTF8) {
            fprintf(stderr, "codec: '%s' is to be UTF-8 already; use --check\n", path);
            return 2;
        }
    }
    if (out != NULL && strcmp(out, path) == 0) {
        fprintf(stderr, "codec: write to a new file and review the difference\n");
        return 2;
    }

    ar = cf_arena_new(NULL, 0);
    if (ar == NULL) {
        fprintf(stderr, "codec: out of memory\n");
        return 2;
    }
    if (!cf_blob_file(&blob, ar, path, &err)) {
        fprintf(stderr, "codec: cannot read %s\n", path);
        cf_arena_free(ar);
        return 2;
    }
    if (check) {
        rc = rc_check(path, &blob);
    } else if (!rc_mark(&blob, &mark_enc, &skip)) {
        rc = rc_utf32(path);
    } else if (skip != 0 && from != NULL && enc != mark_enc) {
        fprintf(stderr,
                "codec: '%s' starts with a %s byte-order mark; leave out --from\n",
                path,
                (mark_enc == (unsigned)CF_ENCODING_UTF16LE) ? "UTF-16LE" : "UTF-16BE");
        rc = 2;
    } else if (skip == 0 && from == NULL) {
        rc = rc_usage();
    } else {
        rc = rc_recode(path, &blob, skip, (skip != 0) ? mark_enc : enc, out);
    }
    cf_arena_free(ar);
    return rc;
}
