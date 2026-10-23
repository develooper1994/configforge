/*
 * blob.c - whole-document input and measure-or-write output. C89.
 */

#include "configforge/cf_serialize.h"

#include <stdarg.h>
#include <string.h>

#ifndef CF_NO_STDIO
#include <stdio.h>
#endif

void cf_blob_mem(cf_blob* b, const char* p, size_t n, const char* name) {
    if (b == NULL) {
        return;
    }
    b->p    = p;
    b->n    = n;
    b->name = (name != NULL) ? name : "<memory>";
}

#ifndef CF_NO_STDIO

cf_bool cf_blob_file(cf_blob* b, cf_arena* ar, const char* path, cf_error* err) {
    FILE*  f;
    char*  buf;
    size_t cap = 0;
    size_t len = 0;

    if (b == NULL || ar == NULL || path == NULL) {
        return cf_error_set(err, CF_E_SOURCE, NULL, path, "no path given");
    }
    f = fopen(path, "rb");
    if (f == NULL) {
        return cf_error_set(err, CF_E_SOURCE, NULL, path, "cannot open '%s'", path);
    }

    /* Read in chunks rather than seeking to the end. ftell on a file
       that is not seekable - a pipe, a device - reports nonsense, and
       a configuration file arriving on stdin is a normal thing to
       want. */
    cap = 4096;
    buf = (char*)cf_arena_alloc(ar, cap);
    if (buf == NULL) {
        (void)fclose(f);
        return cf_error_set(err, CF_E_MEMORY, NULL, path, "no room to read '%s'", path);
    }
    for (;;) {
        size_t got = fread(buf + len, 1, cap - len - 1, f);
        len += got;
        if (got == 0) {
            break;
        }
        if (len + 1 >= cap) {
            char*  nb;
            size_t ncap = cap * 2;
            nb          = (char*)cf_arena_alloc(ar, ncap);
            if (nb == NULL) {
                (void)fclose(f);
                return cf_error_set(err, CF_E_MEMORY, NULL, path,
                                    "'%s' is larger than the arena", path);
            }
            memcpy(nb, buf, len);
            buf = nb;
            cap = ncap;
        }
    }
    if (ferror(f)) {
        (void)fclose(f);
        return cf_error_set(err, CF_E_SOURCE, NULL, path, "error while reading '%s'",
                            path);
    }
    /* Read-only, and everything wanted was read: a failure to close
       cannot lose data, so there is nothing to report. */
    (void)fclose(f);

    buf[len] = '\0';
    b->p     = buf;
    b->n     = len;
    b->name  = path;
    return CF_TRUE;
}

#else

cf_bool cf_blob_file(cf_blob* b, cf_arena* ar, const char* path, cf_error* err) {
    (void)b;
    (void)ar;
    return cf_error_set(err, CF_E_CAPABILITY, NULL, path,
                        "this build has no file input");
}

#endif /* CF_NO_STDIO */

/* ------------------------------------------------------------------ *
 * Sink
 * ------------------------------------------------------------------ */

void cf_sink_init(cf_sink* s, char* buf, size_t cap) {
    s->buf = buf;
    s->cap = (buf != NULL) ? cap : 0;
    s->len = 0;
}

void cf_sink_write(cf_sink* s, const char* p, size_t n) {
    size_t i;

    if (s->buf != NULL) {
        for (i = 0; i < n; ++i) {
            if (s->len + i + 1 < s->cap) {
                s->buf[s->len + i] = p[i];
            }
        }
    }
    s->len += n;
}

void cf_sink_putc(cf_sink* s, char c) {
    if (s->buf != NULL && s->len + 1 < s->cap) {
        s->buf[s->len] = c;
    }
    s->len += 1;
}

void cf_sink_puts(cf_sink* s, const char* z) {
    if (z != NULL) {
        cf_sink_write(s, z, strlen(z));
    }
}

void cf_sink_fmt(cf_sink* s, const char* fmt, ...) {
    char    tmp[CF_MESSAGE_MAX];
    va_list ap;
    size_t  n;

    va_start(ap, fmt);
    n = cf_vfmt(tmp, sizeof tmp, fmt, ap);
    va_end(ap);

    if (n < sizeof tmp) {
        cf_sink_write(s, tmp, n);
    } else {
        /* Longer than the scratch buffer. Writing the truncation
           silently would corrupt a document; the length still
           accumulates so the overflow is visible. */
        cf_sink_write(s, tmp, sizeof tmp - 1);
        s->len += n - (sizeof tmp - 1);
    }
}

void cf_sink_terminate(cf_sink* s) {
    if (s->buf != NULL && s->cap > 0) {
        s->buf[(s->len < s->cap) ? s->len : s->cap - 1] = '\0';
    }
}

cf_bool cf_sink_overflowed(const cf_sink* s) {
    return (s->buf != NULL && s->len + 1 > s->cap) ? CF_TRUE : CF_FALSE;
}

/* ------------------------------------------------------------------ *
 * Locations
 * ------------------------------------------------------------------ */

size_t cf_loc_text(const cf_loc* at, char* buf, size_t cap) {
    if (at == NULL || at->file == NULL) {
        if (buf != NULL && cap > 0) {
            buf[0] = '\0';
        }
        return 0;
    }
    if (at->line == 0) {
        return cf_fmt(buf, cap, "%s", at->file);
    }
    return cf_fmt(buf, cap, "%s:%u:%u", at->file, at->line, at->col);
}
