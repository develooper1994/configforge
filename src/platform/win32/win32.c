/*
 * win32.c - the Windows platform.
 *
 * Written without a Windows machine at first, and the first MSVC build
 * found the first bug: _environ declared by hand collided with the
 * CRT's own declaration. It is built and run under MSVC now, and
 * unit.test_lex.utf8 asserts the non-ASCII argument round trip that the
 * wide-argument path exists for.
 *
 * Paths go through the ANSI ("A") functions, as everywhere else in this
 * file. A configuration path outside the active code page is a gap that
 * belongs to D-15 and is not closed here.
 *
 * Guarded once at the top, for the reason given in posix.c.
 */

#include "configforge/cf_platform.h"

#if CF_PLATFORM_WIN32

#include <stdlib.h>
#include <string.h>
#include <windows.h>

/* _environ is MSVC's spelling, and it has to come from <stdlib.h>: the
   CRT defines it as a macro over __p__environ(), so declaring it by
   hand produces a second, conflicting declaration of that function. */

const char* cf_platform_name(void) {
    return "win32";
}

cf_bool cf_platform_has_environ(void) {
    return CF_TRUE;
}

cf_bool cf_platform_has_dir_list(void) {
    return CF_TRUE;
}

cf_bool cf_platform_has_wide_args(void) {
    return CF_TRUE;
}

char* const* cf_platform_environ(void) {
    return (char* const*)_environ;
}

char* const* cf_platform_args(int argc, char* const* argv, cf_arena* arena,
                              int* out_argc) {
    LPWSTR* wide;
    int     n = 0;
    char**  out;
    int     i;

    *out_argc = argc;
    if (arena == NULL) {
        return argv;
    }
    /* The wide command line rather than a conversion of argv: argv has
       already lost anything the active code page cannot represent, and
       what is lost cannot be converted back. */
    wide = CommandLineToArgvW(GetCommandLineW(), &n);
    if (wide == NULL || n <= 0) {
        return argv;
    }
    out = (char**)cf_arena_alloc(arena, (size_t)(n + 1) * sizeof(char*));
    if (out == NULL) {
        LocalFree(wide);
        return argv;
    }
    for (i = 0; i < n; ++i) {
        int   need = WideCharToMultiByte(CP_UTF8, 0, wide[i], -1, NULL, 0, NULL, NULL);
        char* buf;
        if (need <= 0) {
            LocalFree(wide);
            return argv;
        }
        buf = (char*)cf_arena_alloc(arena, (size_t)need);
        if (buf == NULL) {
            LocalFree(wide);
            return argv;
        }
        WideCharToMultiByte(CP_UTF8, 0, wide[i], -1, buf, need, NULL, NULL);
        out[i] = buf;
    }
    out[n] = NULL;
    LocalFree(wide);
    *out_argc = n;
    return (char* const*)out;
}

/* Two 32-bit halves, joined. A function rather than an expression at
   each site because one shift-and-or spread over two lines is laid out
   differently by every version of clang-format. */
static cf_i64 win32_join(DWORD hi, DWORD lo) {
    cf_i64 v = (cf_i64)hi;
    return (v << 32) | (cf_i64)lo;
}

/* A directory by what it holds. See cf_platform.h for why the
   directory's own time is not enough. */
static void win32_stamp_dir(const char* path, cf_stamp* out) {
    char             pattern[CF_PLATFORM_NAME_MAX];
    WIN32_FIND_DATAA fd;
    HANDLE           h;

    if (cf_fmt(pattern, sizeof pattern, "%s\\*", path) >= sizeof pattern) {
        return;
    }
    h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        return;
    }
    do {
        cf_i64 t;
        if (fd.cFileName[0] == '.') {
            continue;
        }
        t = win32_join(fd.ftLastWriteTime.dwHighDateTime,
                       fd.ftLastWriteTime.dwLowDateTime);
        if (t > out->mtime) {
            out->mtime = t;
        }
        out->size += win32_join(fd.nFileSizeHigh, fd.nFileSizeLow) + 1;
    } while (FindNextFileA(h, &fd) != 0);
    FindClose(h);
}

cf_bool cf_platform_stamp(const char* path, cf_stamp* out) {
    WIN32_FILE_ATTRIBUTE_DATA fa;

    if (out == NULL) {
        return CF_FALSE;
    }
    out->mtime   = 0;
    out->size    = 0;
    out->present = CF_FALSE;
    if (path == NULL || !GetFileAttributesExA(path, GetFileExInfoStandard, &fa)) {
        return CF_FALSE;
    }
    out->mtime =
        win32_join(fa.ftLastWriteTime.dwHighDateTime, fa.ftLastWriteTime.dwLowDateTime);
    out->present = CF_TRUE;
    if ((fa.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        win32_stamp_dir(path, out);
    } else {
        out->size = win32_join(fa.nFileSizeHigh, fa.nFileSizeLow);
    }
    return CF_TRUE;
}

int cf_platform_dir_list(const char* path, char (*names)[CF_PLATFORM_NAME_MAX], int max,
                         cf_error* err) {
    char             pattern[CF_PLATFORM_NAME_MAX];
    WIN32_FIND_DATAA fd;
    HANDLE           h;
    int              n = 0;

    if (path == NULL || names == NULL) {
        cf_error_set(err, CF_E_SOURCE, NULL, path, "no directory given");
        return -1;
    }
    if (cf_fmt(pattern, sizeof pattern, "%s\\*", path) >= sizeof pattern) {
        cf_error_set(err, CF_E_SOURCE, NULL, path, "path is too long");
        return -1;
    }
    h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        cf_error_set(err, CF_E_SOURCE, NULL, path, "cannot read '%s'", path);
        return -1;
    }
    do {
        size_t len;
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            continue;
        }
        if (fd.cFileName[0] == '.') {
            continue;
        }
        if (n >= max) {
            break;
        }
        len = strlen(fd.cFileName);
        if (len + 1 > CF_PLATFORM_NAME_MAX) {
            continue;
        }
        memcpy(names[n], fd.cFileName, len + 1);
        ++n;
    } while (FindNextFileA(h, &fd) != 0);
    FindClose(h);
    return n;
}

/* ------------------------------------------------------------------ *
 * The wait handle
 *
 * One overlapped ReadDirectoryChangesW per directory, all completing
 * into one manual-reset event. One handle is what an event loop wants:
 * FindFirstChangeNotification would have been simpler and handed back
 * a handle per directory, which every caller would then have to wait
 * on as a set.
 *
 * The notifications themselves are never parsed. A completion means
 * "ask now"; which file it named does not change that answer, and a
 * buffer overflow - a completion with no records - means the same.
 * ------------------------------------------------------------------ */

#define WIN32_WAKE_FILTER                                                              \
    (FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |                      \
     FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE |                         \
     FILE_NOTIFY_CHANGE_CREATION)

typedef struct win32_target {
    char       dir[CF_PLATFORM_NAME_MAX];
    HANDLE     h; /* INVALID_HANDLE_VALUE when nothing is open */
    OVERLAPPED ov;
    cf_bool    direct; /* watched itself, rather than through an ancestor */
    /* DWORD, for the alignment ReadDirectoryChangesW requires. */
    DWORD buf[256];
} win32_target;

struct cf_platform_wake {
    const cf_allocator* a;
    HANDLE              ev;
    size_t              n;
    win32_target*       t;
};

static cf_bool win32_is_sep(char c) {
    return (c == '\\' || c == '/') ? CF_TRUE : CF_FALSE;
}

/* One level up, in place. False at the top: ".", "C:\" and "\" have
   nowhere to go. */
static cf_bool win32_up(char* dir) {
    size_t len = strlen(dir);

    while (len > 1 && win32_is_sep(dir[len - 1]) && dir[len - 2] != ':') {
        dir[--len] = '\0';
    }
    if (len == 0 || (len == 1 && (dir[0] == '.' || win32_is_sep(dir[0]))) ||
        (len == 3 && dir[1] == ':' && win32_is_sep(dir[2]))) {
        return CF_FALSE;
    }
    while (len > 0 && !win32_is_sep(dir[len - 1]) && dir[len - 1] != ':') {
        --len;
    }
    if (len == 0) {
        memcpy(dir, ".", 2);
    } else if (dir[len - 1] == ':' || len == 1 || dir[len - 2] == ':') {
        dir[len] = '\0'; /* keep "C:", "C:\" and "\" whole */
    } else {
        dir[len - 1] = '\0';
    }
    return CF_TRUE;
}

/* The directory a path lives in, or the path when it is a directory. */
static cf_bool win32_dir_of(const char* path, char* out, size_t cap) {
    size_t len = strlen(path);
    DWORD  at;

    if (len + 1 > cap) {
        return CF_FALSE;
    }
    memcpy(out, path, len + 1);
    at = GetFileAttributesA(path);
    if (at != INVALID_FILE_ATTRIBUTES && (at & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return CF_TRUE;
    }
    if (!win32_up(out)) {
        memcpy(out, ".", 2);
    }
    return CF_TRUE;
}

static HANDLE win32_open_dir(const char* dir) {
    return CreateFileA(dir, FILE_LIST_DIRECTORY,
                       FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                       OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                       NULL);
}

/* Stop the read and wait for the kernel to let go of the buffer, which
   it may still be writing into until the cancellation completes.

   GetOverlappedResult alone is not that wait here: it waits on the
   event, the event is shared, and another directory's completion can
   have set it already - so it can return while this read is still in
   flight. The loop asks the overlapped structure itself. */
static void win32_disarm(win32_target* t) {
    DWORD got;

    if (t->h == INVALID_HANDLE_VALUE) {
        return;
    }
    CancelIoEx(t->h, &t->ov);
    while (!HasOverlappedIoCompleted(&t->ov)) {
        GetOverlappedResult(t->h, &t->ov, &got, TRUE);
        if (!HasOverlappedIoCompleted(&t->ov)) {
            Sleep(1);
        }
    }
    CloseHandle(t->h);
    t->h = INVALID_HANDLE_VALUE;
}

static cf_bool win32_issue(cf_platform_wake* w, win32_target* t) {
    memset(&t->ov, 0, sizeof t->ov);
    t->ov.hEvent = w->ev;
    return ReadDirectoryChangesW(t->h, t->buf, (DWORD)sizeof t->buf, FALSE,
                                 WIN32_WAKE_FILTER, NULL, &t->ov, NULL)
               ? CF_TRUE
               : CF_FALSE;
}

/* Watch the target itself, or the nearest ancestor that exists. */
static cf_bool win32_arm(cf_platform_wake* w, win32_target* t, cf_error* err) {
    char dir[CF_PLATFORM_NAME_MAX];

    win32_disarm(t);
    memcpy(dir, t->dir, sizeof dir);
    t->direct = CF_TRUE;
    for (;;) {
        t->h = win32_open_dir(dir);
        if (t->h != INVALID_HANDLE_VALUE) {
            break;
        }
        if (GetLastError() != ERROR_FILE_NOT_FOUND &&
            GetLastError() != ERROR_PATH_NOT_FOUND) {
            return cf_error_set(err, CF_E_CAPABILITY, NULL, dir,
                                "cannot watch '%s': it cannot be opened", dir);
        }
        t->direct = CF_FALSE;
        if (!win32_up(dir)) {
            return cf_error_set(err, CF_E_CAPABILITY, NULL, t->dir,
                                "cannot watch '%s': no part of it exists", t->dir);
        }
    }
    if (!win32_issue(w, t)) {
        win32_disarm(t);
        return cf_error_set(err, CF_E_CAPABILITY, NULL, dir,
                            "cannot watch '%s': ReadDirectoryChangesW refused it", dir);
    }
    return CF_TRUE;
}

cf_bool cf_platform_has_wake(void) {
    return CF_TRUE;
}

cf_platform_wake* cf_platform_wake_open(const char* const* paths, size_t n,
                                        const cf_allocator* a, cf_error* err) {
    cf_platform_wake* w;
    size_t            i;

    if (a == NULL || a->alloc == NULL) {
        cf_error_set(err, CF_E_MEMORY, NULL, NULL, "no allocator to watch with");
        return NULL;
    }
    /* One block: the targets follow the header, whose size is already a
       multiple of the stricter alignment of the two. */
    w = (cf_platform_wake*)a->alloc(a->ctx, sizeof *w + n * sizeof(win32_target));
    if (w == NULL) {
        cf_error_set(err, CF_E_MEMORY, NULL, NULL, "no room to watch %u path(s)",
                     (unsigned)n);
        return NULL;
    }
    {
        void* rest = w + 1;
        w->t       = (win32_target*)rest;
    }
    w->a = a;
    w->n = n;
    for (i = 0; i < n; ++i) {
        w->t[i].h = INVALID_HANDLE_VALUE;
    }
    w->ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (w->ev == NULL) {
        cf_error_set(err, CF_E_CAPABILITY, NULL, NULL,
                     "cannot create an event to wait on");
        a->release(a->ctx, w);
        return NULL;
    }
    for (i = 0; i < n; ++i) {
        if (paths[i] == NULL ||
            !win32_dir_of(paths[i], w->t[i].dir, sizeof w->t[i].dir)) {
            cf_error_set(err, CF_E_CAPABILITY, NULL, paths[i],
                         "cannot watch a path this long");
            cf_platform_wake_close(w);
            return NULL;
        }
        if (!win32_arm(w, &w->t[i], err)) {
            cf_platform_wake_close(w);
            return NULL;
        }
    }
    return w;
}

int cf_platform_wake_fd(const cf_platform_wake* w) {
    (void)w;
    return -1;
}

void* cf_platform_wake_handle(const cf_platform_wake* w) {
    return (w != NULL) ? (void*)w->ev : NULL;
}

cf_bool cf_platform_wake_drain(cf_platform_wake* w) {
    cf_bool ok = CF_TRUE;
    size_t  i;

    if (w == NULL) {
        return CF_FALSE;
    }
    /* Reset first, then look. A read that completes after its check
       sets the event again and the caller simply wakes once more; the
       other order could lose one. */
    ResetEvent(w->ev);
    for (i = 0; i < w->n; ++i) {
        win32_target* t = &w->t[i];
        DWORD         got;
        if (t->h != INVALID_HANDLE_VALUE &&
            !GetOverlappedResult(t->h, &t->ov, &got, FALSE) &&
            GetLastError() == ERROR_IO_INCOMPLETE) {
            continue; /* still waiting, which is the normal case */
        }
        /* Completed, failed, or watched through an ancestor: a fresh
           read either way, and the ancestor case tries for the target
           itself again because the missing level may have appeared. */
        if (t->direct && t->h != INVALID_HANDLE_VALUE && win32_issue(w, t)) {
            continue;
        }
        if (!win32_arm(w, t, NULL)) {
            ok = CF_FALSE;
        }
    }
    return ok;
}

int cf_platform_wake_wait(cf_platform_wake* w, long timeout_ms) {
    DWORD ms = INFINITE;
    DWORD r;

    if (w == NULL) {
        return -1;
    }
    if (timeout_ms >= 0) {
        ms = (DWORD)timeout_ms;
    }
    r = WaitForSingleObject(w->ev, ms);
    if (r == WAIT_TIMEOUT) {
        return 0;
    }
    if (r != WAIT_OBJECT_0) {
        return -1;
    }
    return cf_platform_wake_drain(w) ? 1 : -1;
}

void cf_platform_wake_close(cf_platform_wake* w) {
    size_t i;

    if (w == NULL) {
        return;
    }
    for (i = 0; i < w->n; ++i) {
        win32_disarm(&w->t[i]);
    }
    if (w->ev != NULL) {
        CloseHandle(w->ev);
    }
    w->a->release(w->a->ctx, w);
}

#endif /* CF_PLATFORM_WIN32 */
