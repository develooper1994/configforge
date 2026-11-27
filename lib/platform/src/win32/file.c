/*
 * file.c (win32) - opening a file by a UTF-8 path, and replacing one whole
 * or not at all (D-224, D-127), on Windows.
 *
 * Apart from win32.c so that the serialization library, which opens
 * files and calls nothing else of the platform, carries this file
 * without it (docs/Architecture.md, "Layers").
 *
 * Guarded once at the top, for the reason given in posix.c.
 */

#include "configforge/cf_platform_file.h"

#if CF_PLATFORM_WIN32

#include "configforge/cf_build_options.h"
#include "configforge/cf_error.h"
#include "configforge/cf_file.h"
#include "configforge/cf_port.h"
#include "../file_prim.h"

#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>

/* win32.c's win32_wide, copied: UTF-8 in, UTF-16 out, refusing a
   malformed sequence rather than substituting for it - a path with a
   replacement character in it names a different file. False when it
   does not convert or does not fit. It is a copy because the
   serialization library carries this file without win32.c, and sharing
   the helper would make it an exported name of win32.c's. */
static cf_bool win32f_wide(const char* utf8, wchar_t* out, int cap) {
    if (utf8 == NULL || out == NULL) {
        return CF_FALSE;
    }
    return (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, out, cap) > 0)
               ? CF_TRUE
               : CF_FALSE;
}

void* cf_platform_fopen(const char* path, const char* mode) {
    wchar_t wpath[CF_PATH_MAX];
    wchar_t wmode[8];

    if (!win32f_wide(path, wpath, CF_PATH_MAX) || !win32f_wide(mode, wmode, 8)) {
        return NULL;
    }
    return _wfopen(wpath, wmode);
}

cf_bool cf_platform_has_files(void) {
    return CF_TRUE;
}

unsigned long cf_platform_now_ms(void) {
    return (unsigned long)GetTickCount64();
}

cf_bool cfi_file_sync(void* stream) {
    return FlushFileBuffers((HANDLE)_get_osfhandle(_fileno((FILE*)stream))) != 0
               ? CF_TRUE
               : CF_FALSE;
}

const char* cfi_file_why(char* buf, size_t cap) {
    (void)cf_fmt(buf, cap, "error %U", (cf_u64)GetLastError());
    return buf;
}

unsigned cf_platform_write_steps(void) {
    return 5u;
}

/* What file_core has made so far, for its one exit that undoes it. */
typedef struct replace_state {
    FILE*   fp;   /* the temporary's stream while it is open */
    int     made; /* the temporary exists */
    int     raw;  /* `why` is the whole message, not the reason of a "cannot write" */
    char    tmp[CF_PATH_MAX];
    wchar_t wtmp[CF_PATH_MAX];
    char why[256]; /* a reason, or a message that names the path (cut to fit: the error
                      carries the path whole) */
} replace_state;

/* A step that was told to fail. */
static int file_injected(replace_state* x, unsigned step) {
    (void)cf_fmt(x->why, sizeof x->why, "injected failure at step %u", step);
    return 0;
}

/* A step whose system call failed: the error code says why. */
static int file_failed(replace_state* x, DWORD code) {
    (void)cf_fmt(x->why, sizeof x->why, "error %U", (cf_u64)code);
    return 0;
}

/* A step whose C library call failed: errno says why. */
static int file_failed_stdio(replace_state* x) {
    (void)cfi_file_why_stdio(x->why, sizeof x->why);
    return 0;
}

/* The temporary made, written, synced and put in place: 1, or 0 with x->why
   set and whatever it holds still to be undone. Steps 1 to 5 are numbered as
   `fail_at` counts them. */
static int file_steps(replace_state* x, const char* path, const wchar_t* wpath,
                      const void* data, size_t size, int use_sync,
                      unsigned long retry_ms, unsigned    fail_at,
                      void (*on_retry)(void* user), void* user) {
    unsigned      k;
    HANDLE        h     = INVALID_HANDLE_VALUE;
    DWORD         le    = 0;
    unsigned long slept = 0UL;
    unsigned long delay = 10UL;
    unsigned long wait;
    int           fd;
    int           rc;

    /* 1: the temporary, exclusively, so that nobody else's is ever touched,
       and not inheritable by a child process (D-190) */
    if (fail_at == 1u) {
        return file_injected(x, 1u);
    }
    for (k = 0u; k < 100u; ++k) {
        if (!cfi_file_temp_name(path, k, x->tmp, sizeof x->tmp)) {
            (void)cf_fmt(x->why, sizeof x->why, "'%s' is not a regular file", path);
            x->raw = 1;
            return 0;
        }
        if (!win32f_wide(x->tmp, x->wtmp, CF_PATH_MAX)) {
            (void)cf_fmt(x->why, sizeof x->why, "the path is not valid UTF-8");
            return 0;
        }
        h = CreateFileW(x->wtmp, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                        FILE_ATTRIBUTE_NORMAL, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            break;
        }
        le = GetLastError();
        if (le != ERROR_FILE_EXISTS && le != ERROR_ALREADY_EXISTS) {
            break;
        }
    }
    if (h == INVALID_HANDLE_VALUE) {
        if (le == ERROR_FILE_EXISTS || le == ERROR_ALREADY_EXISTS) {
            (void)cf_fmt(x->why, sizeof x->why,
                         "no temporary name is free next to '%s'", path);
            x->raw = 1;
            return 0;
        }
        return file_failed(x, le);
    }
    x->made = 1;
    fd      = _open_osfhandle((intptr_t)h, _O_WRONLY | _O_BINARY | _O_NOINHERIT);
    if (fd < 0) {
        (void)CloseHandle(h);
        return file_failed_stdio(x);
    }
    x->fp = _fdopen(fd, "wb");
    if (x->fp == NULL) {
        rc = errno;
        (void)_close(fd);
        errno = rc;
        return file_failed_stdio(x);
    }

    /* 2: every byte, and out of the stream's buffer */
    if (fail_at == 2u) {
        return file_injected(x, 2u);
    }
    if ((size > 0u && fwrite(data, 1u, size, x->fp) != size) || fflush(x->fp) != 0) {
        return file_failed_stdio(x);
    }

    /* 3: the sync the options ask for (the step counts when they ask for none) */
    if (fail_at == 3u) {
        return file_injected(x, 3u);
    }
    if (use_sync && !cfi_file_sync(x->fp)) {
        return file_failed(x, GetLastError());
    }

    /* 4: the close, whose result is checked */
    rc    = fclose(x->fp);
    x->fp = NULL;
    if (fail_at == 4u) {
        return file_injected(x, 4u);
    }
    if (rc != 0) {
        return file_failed_stdio(x);
    }

    /* 5: one rename, retried while another handle holds the target without
       delete sharing (a reader's _wfopen), as Go's renameio does: the sleeps
       double from 10 ms to at most 200, and their sum is bound by retry_ms
       (a sum of sleeps, no clock). An injected failure is not retried. */
    if (fail_at == 5u) {
        return file_injected(x, 5u);
    }
    for (;;) {
        if (MoveFileExW(x->wtmp, wpath,
                        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            break;
        }
        le = GetLastError();
        if ((le != ERROR_ACCESS_DENIED && le != ERROR_SHARING_VIOLATION) ||
            slept >= retry_ms) {
            return file_failed(x, le);
        }
        wait = retry_ms - slept;
        if (delay < wait) {
            wait = delay;
        }
        if (on_retry != NULL) {
            on_retry(user);
        }
        Sleep((DWORD)wait);
        slept += wait;
        delay = (delay * 2UL > 200UL) ? 200UL : delay * 2UL;
    }
    x->made = 0;
    return 1;
}

/* The static core of cf_platform_write_file_with. */
static int file_core(const char* path, const void* data, size_t size,
                     const cf_file_options* o, unsigned  fail_at,
                     void (*on_retry)(void* user), void* user, cf_error* err) {
    cf_file_options r;
    replace_state   x;
    wchar_t         wpath[CF_PATH_MAX];
    DWORD           attrs;
    DWORD           le;
    int             use_sync;

    cf_error_clear(err);
    if (path == NULL || path[0] == '\0' || (data == NULL && size > 0u)) {
        (void)cf_error_set(err, CF_E_SOURCE, NULL, path, "no file to write");
        return 0;
    }
    if (!cfi_file_options(o, &r, path, err)) {
        return 0;
    }
    /* The layer's own is a sync before the rename; one write makes "every"
       and "each" the same as it. */
    use_sync = (r.sync != CF_SYNC_PLATFORM) ? 1 : 0;
    if (!win32f_wide(path, wpath, CF_PATH_MAX)) {
        (void)cfi_file_fail(err, path, "the path is not valid UTF-8 or is too long");
        return 0;
    }

    /* What is there: a directory is not ours to replace, nor is a read-only
       file, and no file at all is fine. */
    attrs = GetFileAttributesW(wpath);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        le = GetLastError();
        if (le != ERROR_FILE_NOT_FOUND && le != ERROR_PATH_NOT_FOUND) {
            (void)cf_fmt(x.why, sizeof x.why, "error %U", (cf_u64)le);
            (void)cfi_file_fail(err, path, x.why);
            return 0;
        }
    } else if ((attrs & FILE_ATTRIBUTE_DIRECTORY) != 0u) {
        (void)cf_error_set(err, CF_E_SOURCE, NULL, path, "'%s' is not a regular file",
                           path);
        return 0;
    } else if ((attrs & FILE_ATTRIBUTE_READONLY) != 0u) {
        (void)cf_error_set(err, CF_E_SOURCE, NULL, path, "'%s' is read-only", path);
        return 0;
    }

    x.fp     = NULL;
    x.made   = 0;
    x.raw    = 0;
    x.tmp[0] = '\0';
    x.why[0] = '\0';
    if (file_steps(&x, path, wpath, data, size, use_sync, r.retry_ms, fail_at, on_retry,
                   user)) {
        return 1;
    }

    /* Undo: the stream, then the temporary; the old file was never touched. */
    if (x.fp != NULL) {
        (void)fclose(x.fp);
    }
    if (x.made) {
        (void)DeleteFileW(x.wtmp);
    }
    if (x.raw) {
        (void)cf_error_set(err, CF_E_SOURCE, NULL, path, "%s", x.why);
    } else {
        (void)cfi_file_fail(err, path, x.why);
    }
    return 0;
}

int cf_platform_write_file_with(const char* path, const void* data, size_t size,
                                const cf_file_options* o, unsigned  fail_at,
                                void (*on_retry)(void* user), void* user,
                                cf_error* err) {
    return file_core(path, data, size, o, fail_at, on_retry, user, err);
}

#endif /* CF_PLATFORM_WIN32 */
