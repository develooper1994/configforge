/*
 * file_layer.c - the file layer's platform-independent part (D-224,
 * D-127): the options and their defaults, the writer's buffering and
 * sync policy, and what the three platform files share.
 *
 * The platform's part - the temporary, the sync, the rename, the clock - is
 * posix/file.c, win32/file.c and none/file.c, reached through file_prim.h
 * and cf_platform_file.h. C89.
 */

#include "configforge/cf_file.h"
#include "configforge/cf_alloc.h"
#include "configforge/cf_build_options.h"
#include "configforge/cf_error.h"
#include "configforge/cf_platform_file.h"
#include "configforge/cf_port.h"
#include "file_prim.h"

#include <string.h>

#ifndef CF_NO_STDIO
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#endif

#define FILE_NO_FILES "this build has no files to write"

void cf_file_options_init(cf_file_options* o) {
    if (o != NULL) {
        (void)memset(o, 0, sizeof *o);
        o->size       = sizeof(cf_file_options);
        o->buffering  = CF_BUF_PLATFORM;
        o->sync       = CF_SYNC_DEFAULT;
        o->sync_bytes = 1048576UL;
        o->sync_ms    = 1000UL;
        o->retry_ms   = 2000UL;
        o->append     = CF_FALSE;
    }
}

cf_bool cfi_file_options(const cf_file_options* o, cf_file_options* out,
                         const char* path, cf_error* err) {
    cf_file_options_init(out);
    if (o == NULL) {
        return CF_TRUE;
    }
    if (o->size < CFI_FILE_OPTIONS_FLOOR) {
        return cf_error_set(err, CF_E_SOURCE, NULL, path,
                            "file options of an unknown size");
    }
    if ((unsigned)o->buffering > (unsigned)CF_BUF_NONE) {
        return cf_error_set(err, CF_E_SOURCE, NULL, path,
                            "file options with an unknown buffering");
    }
    if ((unsigned)o->sync > (unsigned)CF_SYNC_EACH) {
        return cf_error_set(err, CF_E_SOURCE, NULL, path,
                            "file options with an unknown sync");
    }
    out->buffering   = o->buffering;
    out->buffer_size = o->buffer_size;
    out->sync        = o->sync;
    out->sync_bytes  = o->sync_bytes;
    out->sync_ms     = o->sync_ms;
    out->retry_ms    = o->retry_ms;
    out->append      = o->append;
    if (o->size >= offsetof(cf_file_options, allocator) + sizeof o->allocator) {
        out->allocator = o->allocator;
    }
    return CF_TRUE;
}

cf_bool cfi_file_fail(cf_error* err, const char* path, const char* why) {
    return cf_error_set(err, CF_E_SOURCE, NULL, path, "cannot write '%s': %s", path,
                        why);
}

#if CF_PLATFORM_WIN32
#define FILE_IS_SEP(c) ((c) == '/' || (c) == '\\' || (c) == ':')
#else
#define FILE_IS_SEP(c) ((c) == '/')
#endif

/* `<dir>/.<name><tag><num>` for `path`: the directory, ".", the name, the tag
   and the digits (none when `num` is NULL). */
static cf_bool file_dot_name(const char* path, const char* tag, const char* num,
                             char* out, size_t cap) {
    size_t len, cut = 0, i, n = 0, base, tl = strlen(tag), nl = 0;

    if (path == NULL || out == NULL) {
        return CF_FALSE;
    }
    if (num != NULL) {
        nl = strlen(num);
    }
    len = strlen(path);
    for (i = 0; i < len; ++i) {
        if (FILE_IS_SEP(path[i])) {
            cut = i + 1;
        }
    }
    base = len - cut;
    if (base == 0) {
        return CF_FALSE;
    }
    /* the directory, ".", the name, the tag, the digits, the NUL */
    if (cut + 1u + base + tl + nl + 1u > cap) {
        return CF_FALSE;
    }
    (void)memcpy(out, path, cut);
    n        = cut;
    out[n++] = '.';
    (void)memcpy(out + n, path + cut, base);
    n += base;
    (void)memcpy(out + n, tag, tl);
    n += tl;
    if (nl != 0) {
        (void)memcpy(out + n, num, nl);
        n += nl;
    }
    out[n] = '\0';
    return CF_TRUE;
}

cf_bool cfi_file_temp_name(const char* path, unsigned k, char* out, size_t cap) {
    char   num[8];
    size_t nl = 0;

    if (k >= 100u) {
        return CF_FALSE;
    }
    if (k >= 10u) {
        num[nl++] = (char)('0' + (int)(k / 10u));
    }
    num[nl++] = (char)('0' + (int)(k % 10u));
    num[nl]   = '\0';
    return file_dot_name(path, ".cf-tmp-", num, out, cap);
}

cf_bool cfi_file_lock_name(const char* path, char* out, size_t cap) {
    return file_dot_name(path, ".cf-lock", NULL, out, cap);
}

/* A platform with no files to write says so once, here, for every call. */
static cf_bool file_available(const char* path, cf_error* err) {
    if (cf_platform_write_steps() == 0u) {
        return cf_error_set(err, CF_E_CAPABILITY, NULL, path, FILE_NO_FILES);
    }
    return CF_TRUE;
}

cf_bool cf_file_replace(const char* path, const void* data, size_t size,
                        const cf_file_options* o, cf_error* err) {
    return cf_platform_write_file_with(path, data, size, o, 0u, NULL, NULL, err) == 1
               ? CF_TRUE
               : CF_FALSE;
}

cf_bool cfi_file_holds(const char* path, const void* expect, size_t size) {
#ifdef CF_NO_STDIO
    (void)path;
    (void)expect;
    (void)size;
    return CF_FALSE;
#else
    char   buf[256];
    FILE*  f;
    size_t done = 0u;
    int    same = 1;

    f = (FILE*)cf_platform_fopen(path, "rb");
    if (f == NULL) {
        return CF_FALSE;
    }
    while (same && done < size) {
        size_t want = size - done;
        size_t got;

        if (want > sizeof buf) {
            want = sizeof buf;
        }
        got = fread(buf, 1u, want, f);
        if (got != want || memcmp(buf, (const char*)expect + done, got) != 0) {
            same = 0;
        }
        done += got;
    }
    /* one byte more than `size` and the file is longer; a read error is no
       answer either */
    if (same && (fread(buf, 1u, 1u, f) != 0u || ferror(f) != 0)) {
        same = 0;
    }
    (void)fclose(f);
    return same ? CF_TRUE : CF_FALSE;
#endif
}

int cf_platform_write_file_if_hook(const char* path, const void* data, size_t size,
                                   const void* expect, size_t expect_size,
                                   const cf_file_options* o,
                                   void (*between)(void* user), void* user,
                                   cf_error* err) {
    return cf_platform_write_file_if_with(path, data, size, expect, expect_size, o, 0u,
                                          between, user, err);
}

int cf_platform_write_file_if(const char* path, const void* data, size_t size,
                              const void* expect, size_t expect_size,
                              const cf_file_options* o, cf_error* err) {
    return cf_platform_write_file_if_with(path, data, size, expect, expect_size, o, 0u,
                                          NULL, NULL, err);
}

#ifdef CF_NO_STDIO

/* No stdio: no writer. The calls exist so that a program built for this
   target still links (D-220). */

cf_file_writer* cf_file_writer_open(const char* path, const cf_file_options* o,
                                    cf_error* err) {
    (void)o;
    cf_error_clear(err);
    (void)file_available(path, err);
    return NULL;
}

cf_bool cf_file_writer_write(cf_file_writer* w, const void* data, size_t size,
                             cf_error* err) {
    (void)w;
    (void)data;
    (void)size;
    cf_error_clear(err);
    return file_available(NULL, err);
}

cf_bool cf_file_writer_flush(cf_file_writer* w, cf_error* err) {
    (void)w;
    cf_error_clear(err);
    return file_available(NULL, err);
}

cf_bool cf_file_writer_sync(cf_file_writer* w, cf_error* err) {
    (void)w;
    cf_error_clear(err);
    return file_available(NULL, err);
}

cf_bool cf_file_writer_close(cf_file_writer* w, cf_error* err) {
    (void)w;
    cf_error_clear(err);
    return file_available(NULL, err);
}

unsigned long cf_platform_writer_syncs(const cf_file_writer* w) {
    (void)w;
    return 0UL;
}

cf_bool cf_platform_writer_close_counted(cf_file_writer* w, unsigned long* syncs,
                                         cf_error* err) {
    (void)w;
    if (syncs != NULL) {
        *syncs = 0UL;
    }
    cf_error_clear(err);
    return file_available(NULL, err);
}

#else /* stdio */

const char* cfi_file_why_stdio(char* buf, size_t cap) {
    (void)cf_fmt(buf, cap, "%s", strerror(errno));
    return buf;
}

struct cf_file_writer {
    void*               fp;
    const cf_allocator* a;
    char*               buf;  /* the stream's buffer, when buffer_size asked for one */
    cf_sync_mode        sync; /* the options', CF_SYNC_DEFAULT resolved */
    unsigned long       sync_bytes;
    unsigned long       sync_ms;
    unsigned long       syncs;   /* how many this writer issued */
    unsigned long       pending; /* bytes written since the last sync */
    unsigned long       last_ms; /* the clock at the last sync, or at the open */
    char                path[CF_PATH_MAX];
};

static cf_bool writer_arg(const cf_file_writer* w, cf_error* err) {
    if (w == NULL) {
        return cf_error_set(err, CF_E_SOURCE, NULL, NULL, "no file to write");
    }
    return CF_TRUE;
}

/* Everything written goes to the system; then to the device. */
static cf_bool writer_sync(cf_file_writer* w, cf_error* err) {
    char why[96];

    if (fflush((FILE*)w->fp) != 0) {
        return cfi_file_fail(err, w->path, cfi_file_why_stdio(why, sizeof why));
    }
    if (!cfi_file_sync(w->fp)) {
        return cfi_file_fail(err, w->path, cfi_file_why(why, sizeof why));
    }
    ++w->syncs;
    w->pending = 0UL;
    w->last_ms = cf_platform_now_ms();
    return CF_TRUE;
}

/* The bytes of a write that count toward the next sync, without wrapping. */
static void writer_count(cf_file_writer* w, size_t n) {
    if ((size_t)(ULONG_MAX - w->pending) < n) {
        w->pending = ULONG_MAX;
    } else {
        w->pending += (unsigned long)n;
    }
}

cf_file_writer* cf_file_writer_open(const char* path, const cf_file_options* o,
                                    cf_error* err) {
    cf_file_options     r;
    const cf_allocator* a;
    cf_file_writer*     w;
    char                why[96];

    cf_error_clear(err);
    if (!file_available(path, err)) {
        return NULL;
    }
    if (path == NULL || path[0] == '\0') {
        (void)cf_error_set(err, CF_E_SOURCE, NULL, path, "no file to write");
        return NULL;
    }
    if (strlen(path) >= sizeof w->path) {
        (void)cf_error_set(err, CF_E_SOURCE, NULL, NULL,
                           "the path is too long to write to");
        return NULL;
    }
    if (!cfi_file_options(o, &r, path, err)) {
        return NULL;
    }
    if (r.sync == CF_SYNC_DEFAULT) {
        r.sync = CF_SYNC_PLATFORM;
    }
    if (r.sync == CF_SYNC_EVERY && r.sync_ms != 0UL &&
        cf_platform_now_ms() == (unsigned long)-1) {
        (void)cf_error_set(err, CF_E_CAPABILITY, NULL, path,
                           "this platform has no clock for sync_ms");
        return NULL;
    }
    a = (r.allocator != NULL) ? r.allocator : cf_default_allocator();
    if (a == NULL) {
        (void)cf_error_set(err, CF_E_CAPABILITY, NULL, path,
                           "this build has no allocator for a file writer");
        return NULL;
    }
    w = (cf_file_writer*)a->alloc(a->ctx, sizeof *w);
    if (w == NULL) {
        (void)cf_error_set(err, CF_E_MEMORY, NULL, path, "out of memory");
        return NULL;
    }
    (void)memset(w, 0, sizeof *w);
    w->a          = a;
    w->sync       = r.sync;
    w->sync_bytes = r.sync_bytes;
    w->sync_ms    = r.sync_ms;
    (void)memcpy(w->path, path, strlen(path) + 1u);

    w->fp = cf_platform_fopen(path, r.append ? "ab" : "wb");
    if (w->fp == NULL) {
        (void)cfi_file_fail(err, path, cfi_file_why_stdio(why, sizeof why));
        a->release(a->ctx, w);
        return NULL;
    }

    if (r.buffering != CF_BUF_PLATFORM) {
        size_t want = r.buffer_size != 0u ? r.buffer_size : (size_t)BUFSIZ;
        int    mode = _IOFBF;
        if (r.buffering == CF_BUF_LINE) {
            mode = _IOLBF;
        } else if (r.buffering == CF_BUF_NONE) {
            mode = _IONBF;
            want = 0u;
        }
        if (want != 0u && r.buffer_size != 0u) {
            w->buf = (char*)a->alloc(a->ctx, want);
            if (w->buf == NULL) {
                (void)cf_error_set(err, CF_E_MEMORY, NULL, path, "out of memory");
                (void)fclose((FILE*)w->fp);
                a->release(a->ctx, w);
                return NULL;
            }
        }
        if (setvbuf((FILE*)w->fp, w->buf, mode, want) != 0) {
            (void)cfi_file_fail(err, path, "setvbuf refused the buffering");
            (void)fclose((FILE*)w->fp);
            if (w->buf != NULL) {
                a->release(a->ctx, w->buf);
            }
            a->release(a->ctx, w);
            return NULL;
        }
    }
    w->last_ms = cf_platform_now_ms();
    return w;
}

cf_bool cf_file_writer_write(cf_file_writer* w, const void* data, size_t size,
                             cf_error* err) {
    char    why[96];
    cf_bool due = CF_FALSE;

    cf_error_clear(err);
    if (!file_available(NULL, err) || !writer_arg(w, err)) {
        return CF_FALSE;
    }
    if (data == NULL && size > 0u) {
        return cf_error_set(err, CF_E_SOURCE, NULL, w->path, "no file to write");
    }
    if (size > 0u && fwrite(data, 1u, size, (FILE*)w->fp) != size) {
        return cfi_file_fail(err, w->path, cfi_file_why_stdio(why, sizeof why));
    }
    writer_count(w, size);
    if (w->sync == CF_SYNC_EACH) {
        due = CF_TRUE;
    } else if (w->sync == CF_SYNC_EVERY) {
        /* Either trigger: the bytes written since the last sync, or the time. */
        due = ((w->sync_bytes != 0UL && w->pending >= w->sync_bytes) ||
               (w->sync_ms != 0UL && cf_platform_now_ms() - w->last_ms >= w->sync_ms))
                  ? CF_TRUE
                  : CF_FALSE;
    }
    return due ? writer_sync(w, err) : CF_TRUE;
}

cf_bool cf_file_writer_flush(cf_file_writer* w, cf_error* err) {
    char why[96];

    cf_error_clear(err);
    if (!file_available(NULL, err) || !writer_arg(w, err)) {
        return CF_FALSE;
    }
    if (fflush((FILE*)w->fp) != 0) {
        return cfi_file_fail(err, w->path, cfi_file_why_stdio(why, sizeof why));
    }
    return CF_TRUE;
}

cf_bool cf_file_writer_sync(cf_file_writer* w, cf_error* err) {
    cf_error_clear(err);
    if (!file_available(NULL, err) || !writer_arg(w, err)) {
        return CF_FALSE;
    }
    return writer_sync(w, err);
}

unsigned long cf_platform_writer_syncs(const cf_file_writer* w) {
    return w != NULL ? w->syncs : 0UL;
}

cf_bool cf_platform_writer_close_counted(cf_file_writer* w, unsigned long* syncs,
                                         cf_error* err) {
    cf_bool ok = CF_TRUE;
    char    why[96];

    if (syncs != NULL) {
        *syncs = 0UL;
    }
    cf_error_clear(err);
    if (!file_available(NULL, err)) {
        return CF_FALSE;
    }
    if (w == NULL) {
        return CF_TRUE;
    }
    if (w->sync == CF_SYNC_COMMIT || (w->sync == CF_SYNC_EVERY && w->pending > 0UL)) {
        ok = writer_sync(w, err);
    }
    if (fclose((FILE*)w->fp) != 0 && ok) {
        ok = cfi_file_fail(err, w->path, cfi_file_why_stdio(why, sizeof why));
    }
    if (syncs != NULL) {
        *syncs = w->syncs;
    }
    if (w->buf != NULL) {
        w->a->release(w->a->ctx, w->buf);
    }
    w->a->release(w->a->ctx, w);
    return ok;
}

cf_bool cf_file_writer_close(cf_file_writer* w, cf_error* err) {
    return cf_platform_writer_close_counted(w, NULL, err);
}

#endif /* stdio */
