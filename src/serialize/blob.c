/*
 * blob.c - whole-document input and measure-or-write output. C89.
 */

#include "configforge/cf_platform.h"
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

cf_arena* cf_blob_arena(cf_arena* ar) {
    if (ar == NULL || cf_arena_is_fixed(ar)) {
        return ar;
    }
    return cf_arena_new(cf_arena_allocator(ar), 0);
}

void cf_blob_arena_done(cf_arena* bytes, cf_arena* ar) {
    if (bytes != NULL && bytes != ar) {
        cf_arena_free(bytes); /* which zeroes first: a file may hold a secret */
    }
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
    /* Through the platform, so a path is UTF-8 on every system. */
    f = cf_platform_fopen(path, "rb");
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

void cf_sink_json_string(cf_sink* out, const char* p, size_t n) {
    size_t i;

    cf_sink_putc(out, '"');
    for (i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)p[i];
        switch (c) {
        case '"':
            cf_sink_puts(out, "\\\"");
            break;
        case '\\':
            cf_sink_puts(out, "\\\\");
            break;
        case '\b':
            cf_sink_puts(out, "\\b");
            break;
        case '\f':
            cf_sink_puts(out, "\\f");
            break;
        case '\n':
            cf_sink_puts(out, "\\n");
            break;
        case '\r':
            cf_sink_puts(out, "\\r");
            break;
        case '\t':
            cf_sink_puts(out, "\\t");
            break;
        default:
            if (c < 0x20u) {
                cf_sink_fmt(out, "\\u00%c%c", "0123456789abcdef"[(c >> 4) & 0xF],
                            "0123456789abcdef"[c & 0xF]);
            } else {
                cf_sink_putc(out, (char)c);
            }
            break;
        }
    }
    cf_sink_putc(out, '"');
}

void cf_sink_json_value(cf_sink* out, const cf_value* v) {
    char tmp[CF_VALUE_TEXT_MAX];

    switch (v->type) {
    case CF_TYPE_TEXT:
        cf_sink_json_string(out, v->u.t.p, v->u.t.n);
        break;
    case CF_TYPE_NULL:
        cf_sink_puts(out, "null");
        break;
    case CF_TYPE_BOOL:
        cf_sink_puts(out, v->u.b ? "true" : "false");
        break;
    case CF_TYPE_INT:
    case CF_TYPE_REAL:
        cf_sink_write(out, tmp, cf_value_to_text(v, tmp, sizeof tmp));
        break;
    }
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
