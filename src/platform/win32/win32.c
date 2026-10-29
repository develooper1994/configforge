/*
 * win32.c - the Windows platform.
 *
 * Written without a Windows machine at first, and the first MSVC build
 * found the first bug: _environ declared by hand collided with the
 * CRT's own declaration. It is built and run under MSVC now, and
 * unit.test_lex.utf8 asserts the non-ASCII argument round trip that the
 * wide-argument path exists for.
 *
 * Paths are UTF-8 at the interface (D-15) and UTF-16 to Windows: every
 * one goes through win32_wide and into a "W" function. The "A"
 * functions this file used at first take the active code page, so a
 * configuration under a user name that page cannot spell could not be
 * opened at all.
 *
 * Guarded once at the top, for the reason given in posix.c.
 */

#include "configforge/cf_platform.h"

#if CF_PLATFORM_WIN32

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
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

/* UTF-8 in, UTF-16 out, refusing a malformed sequence rather than
   substituting for it: a path with a replacement character in it names
   a different file. False when it does not convert or does not fit. */
static cf_bool win32_wide(const char* utf8, wchar_t* out, int cap) {
    if (utf8 == NULL || out == NULL) {
        return CF_FALSE;
    }
    return (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, out, cap) > 0)
               ? CF_TRUE
               : CF_FALSE;
}

/* "<dir>\*", in UTF-16, for FindFirstFileW. */
static cf_bool win32_pattern(const char* dir, wchar_t* out, int cap) {
    size_t n;

    if (!win32_wide(dir, out, cap - 2)) {
        return CF_FALSE;
    }
    n          = wcslen(out);
    out[n]     = L'\\';
    out[n + 1] = L'*';
    out[n + 2] = L'\0';
    return CF_TRUE;
}

void* cf_platform_fopen(const char* path, const char* mode) {
    wchar_t wpath[CF_PLATFORM_NAME_MAX];
    wchar_t wmode[8];

    if (!win32_wide(path, wpath, CF_PLATFORM_NAME_MAX) || !win32_wide(mode, wmode, 8)) {
        return NULL;
    }
    return _wfopen(wpath, wmode);
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
    wchar_t          pattern[CF_PLATFORM_NAME_MAX];
    WIN32_FIND_DATAW fd;
    HANDLE           h;

    if (!win32_pattern(path, pattern, CF_PLATFORM_NAME_MAX)) {
        return;
    }
    h = FindFirstFileW(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        return;
    }
    do {
        cf_i64 t;
        if (fd.cFileName[0] == L'.') {
            continue;
        }
        t = win32_join(fd.ftLastWriteTime.dwHighDateTime,
                       fd.ftLastWriteTime.dwLowDateTime);
        if (t > out->mtime) {
            out->mtime = t;
        }
        out->size += win32_join(fd.nFileSizeHigh, fd.nFileSizeLow) + 1;
    } while (FindNextFileW(h, &fd) != 0);
    FindClose(h);
}

cf_bool cf_platform_stamp(const char* path, cf_stamp* out) {
    WIN32_FILE_ATTRIBUTE_DATA fa;
    wchar_t                   wpath[CF_PLATFORM_NAME_MAX];

    if (out == NULL) {
        return CF_FALSE;
    }
    out->mtime   = 0;
    out->size    = 0;
    out->present = CF_FALSE;
    if (!win32_wide(path, wpath, CF_PLATFORM_NAME_MAX) ||
        !GetFileAttributesExW(wpath, GetFileExInfoStandard, &fa)) {
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

int cf_platform_world_readable(const char* path) {
    (void)path;
    return -1; /* see cf_platform.h */
}

int cf_platform_dir_list(const char* path, char (*names)[CF_PLATFORM_NAME_MAX], int max,
                         cf_error* err) {
    wchar_t          pattern[CF_PLATFORM_NAME_MAX];
    WIN32_FIND_DATAW fd;
    HANDLE           h;
    int              n = 0;

    if (path == NULL || names == NULL) {
        cf_error_set(err, CF_E_SOURCE, NULL, path, "no directory given");
        return -1;
    }
    if (!win32_pattern(path, pattern, CF_PLATFORM_NAME_MAX)) {
        cf_error_set(err, CF_E_SOURCE, NULL, path,
                     "'%s' is too long, or is not valid UTF-8", path);
        return -1;
    }
    h = FindFirstFileW(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        cf_error_set(err, CF_E_SOURCE, NULL, path, "cannot read '%s'", path);
        return -1;
    }
    do {
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            continue;
        }
        if (fd.cFileName[0] == L'.') {
            continue;
        }
        if (n >= max) {
            break;
        }
        /* Back to UTF-8, which is what every name is above this layer.
           A name that does not fit is skipped, as on POSIX. */
        if (WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, names[n],
                                CF_PLATFORM_NAME_MAX, NULL, NULL) <= 0) {
            continue;
        }
        ++n;
    } while (FindNextFileW(h, &fd) != 0);
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
    size_t  len = strlen(path);
    DWORD   at  = INVALID_FILE_ATTRIBUTES;
    wchar_t wpath[CF_PLATFORM_NAME_MAX];

    if (len + 1 > cap) {
        return CF_FALSE;
    }
    memcpy(out, path, len + 1);
    if (win32_wide(path, wpath, CF_PLATFORM_NAME_MAX)) {
        at = GetFileAttributesW(wpath);
    }
    if (at != INVALID_FILE_ATTRIBUTES && (at & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return CF_TRUE;
    }
    if (!win32_up(out)) {
        memcpy(out, ".", 2);
    }
    return CF_TRUE;
}

static HANDLE win32_open_dir(const char* dir) {
    wchar_t wdir[CF_PLATFORM_NAME_MAX];

    if (!win32_wide(dir, wdir, CF_PLATFORM_NAME_MAX)) {
        SetLastError(ERROR_INVALID_NAME);
        return INVALID_HANDLE_VALUE;
    }
    return CreateFileW(wdir, FILE_LIST_DIRECTORY,
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
    w->ev = CreateEventW(NULL, TRUE, FALSE, NULL);
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

/* ------------------------------------------------------------------ *
 * The Registry
 *
 * One recursive walk, every name through UTF-16 and back. The buffers
 * are sized by RegQueryInfoKeyW for each key and taken from the
 * caller's arena, so a value of any length is read whole, and nothing
 * large sits on the stack of a recursion.
 * ------------------------------------------------------------------ */

cf_bool cf_platform_has_registry(void) {
    return CF_TRUE;
}

typedef struct win32_reg {
    cf_arena*          arena;
    cf_platform_reg_fn fn;
    void*              ctx;
    cf_error*          err;
    const char*        root;
    char               path[CF_PATH_MAX];
    size_t             pathlen;
} win32_reg;

/* The hive a root names, and where the subkey after it starts. */
static HKEY win32_reg_hive(const char* root, const char** rest) {
    static const struct {
        const char* name;
        HKEY        hive;
    } hives[] = {
        {"HKCU", HKEY_CURRENT_USER},   {"HKEY_CURRENT_USER", HKEY_CURRENT_USER},
        {"HKLM", HKEY_LOCAL_MACHINE},  {"HKEY_LOCAL_MACHINE", HKEY_LOCAL_MACHINE},
        {"HKCR", HKEY_CLASSES_ROOT},   {"HKEY_CLASSES_ROOT", HKEY_CLASSES_ROOT},
        {"HKU", HKEY_USERS},           {"HKEY_USERS", HKEY_USERS},
        {"HKCC", HKEY_CURRENT_CONFIG}, {"HKEY_CURRENT_CONFIG", HKEY_CURRENT_CONFIG}};
    size_t i;

    for (i = 0; i < sizeof hives / sizeof hives[0]; ++i) {
        size_t n = strlen(hives[i].name);
        if (_strnicmp(root, hives[i].name, n) == 0 &&
            (root[n] == '\0' || root[n] == '\\')) {
            *rest = root + n + (root[n] == '\\' ? 1 : 0);
            return hives[i].hive;
        }
    }
    return NULL;
}

/* UTF-16 of known length to UTF-8 in the arena, NUL-terminated; the
   length written goes to *out_len. Embedded NULs are kept, which is
   what a REG_MULTI_SZ is. */
static char* win32_reg_utf8(cf_arena* arena, const wchar_t* w, int wn,
                            size_t* out_len) {
    int need =
        (wn > 0) ? WideCharToMultiByte(CP_UTF8, 0, w, wn, NULL, 0, NULL, NULL) : 0;
    char* out = (char*)cf_arena_alloc(arena, (size_t)need + 1);

    if (out == NULL) {
        return NULL;
    }
    if (need > 0) {
        WideCharToMultiByte(CP_UTF8, 0, w, wn, out, need, NULL, NULL);
    }
    out[need] = '\0';
    *out_len  = (size_t)need;
    return out;
}

static int win32_reg_fail(win32_reg* w, LONG rc, const char* what) {
    cf_error_set(w->err, CF_E_SOURCE, NULL, w->root, "%s\\%s: %s failed (error %d)",
                 w->root, w->path, what, (int)rc);
    return -1;
}

/* One value, as the platform-neutral record the callback reads. */
static int win32_reg_value(win32_reg* w, const wchar_t* name, DWORD namelen, DWORD type,
                           const BYTE* data, DWORD datalen) {
    cf_platform_reg_value v;
    size_t                n = 0;

    memset(&v, 0, sizeof v);
    v.path = w->path;
    v.name = win32_reg_utf8(w->arena, name, (int)namelen, &n);
    if (v.name == NULL) {
        return win32_reg_fail(w, ERROR_OUTOFMEMORY, "reading a value name");
    }
    if (type == REG_SZ || type == REG_EXPAND_SZ || type == REG_MULTI_SZ) {
        const wchar_t* s  = (const wchar_t*)(const void*)data;
        int            wn = (int)(datalen / sizeof(wchar_t));
        int            k;

        /* Trailing NULs: one ends a string, two end a list. */
        while (wn > 0 && s[wn - 1] == L'\0') {
            --wn;
        }
        v.text = win32_reg_utf8(w->arena, s, wn, &v.textlen);
        if (v.text == NULL) {
            return win32_reg_fail(w, ERROR_OUTOFMEMORY, "reading a value");
        }
        if (type == REG_MULTI_SZ) {
            v.kind  = CF_REG_LIST;
            v.items = (wn > 0) ? 1 : 0;
            for (k = 0; k < wn; ++k) {
                if (s[k] == L'\0') {
                    ++v.items;
                }
            }
        } else {
            v.kind = CF_REG_TEXT;
        }
    } else if (type == REG_DWORD && datalen >= sizeof(DWORD)) {
        DWORD d;
        memcpy(&d, data, sizeof d);
        v.kind = CF_REG_INT;
        v.i    = (cf_i64)d;
    } else if (type == REG_QWORD && datalen >= sizeof(cf_i64)) {
        memcpy(&v.i, data, sizeof v.i);
        v.kind = CF_REG_INT;
    } else {
        v.kind = CF_REG_OTHER;
    }
    return w->fn(w->ctx, &v) ? 0 : -1;
}

static int win32_reg_key(win32_reg* w, HKEY h, int depth) {
    DWORD    nsub = 0, maxsub = 0, nval = 0, maxname = 0, maxdata = 0, i;
    wchar_t* wname;
    wchar_t* wsub;
    BYTE*    data;
    LONG     rc;

    rc = RegQueryInfoKeyW(h, NULL, NULL, NULL, &nsub, &maxsub, NULL, &nval, &maxname,
                          &maxdata, NULL, NULL);
    if (rc != ERROR_SUCCESS) {
        return win32_reg_fail(w, rc, "RegQueryInfoKeyW");
    }
    wname = (wchar_t*)cf_arena_alloc(w->arena, (maxname + 1) * sizeof(wchar_t));
    wsub  = (wchar_t*)cf_arena_alloc(w->arena, (maxsub + 1) * sizeof(wchar_t));
    data  = (BYTE*)cf_arena_alloc(w->arena, maxdata + sizeof(wchar_t) * 2);
    if (wname == NULL || wsub == NULL || data == NULL) {
        return win32_reg_fail(w, ERROR_OUTOFMEMORY, "allocating");
    }
    for (i = 0; i < nval; ++i) {
        DWORD namelen = maxname + 1, datalen = maxdata, type = 0;
        rc = RegEnumValueW(h, i, wname, &namelen, NULL, &type, data, &datalen);
        if (rc != ERROR_SUCCESS) {
            return win32_reg_fail(w, rc, "RegEnumValueW");
        }
        if (win32_reg_value(w, wname, namelen, type, data, datalen) != 0) {
            return -1;
        }
    }
    for (i = 0; i < nsub; ++i) {
        DWORD  sublen = maxsub + 1;
        HKEY   child;
        size_t keep = w->pathlen, n = 0;
        char*  name;
        int    r;

        rc = RegEnumKeyExW(h, i, wsub, &sublen, NULL, NULL, NULL, NULL);
        if (rc != ERROR_SUCCESS) {
            return win32_reg_fail(w, rc, "RegEnumKeyExW");
        }
        if (depth + 1 >= CF_MAX_DEPTH) {
            cf_error_set(w->err, CF_E_SOURCE, NULL, w->root,
                         "%s\\%s: deeper than %d keys", w->root, w->path, CF_MAX_DEPTH);
            return -1;
        }
        name = win32_reg_utf8(w->arena, wsub, (int)sublen, &n);
        if (name == NULL || keep + n + 2 > sizeof w->path) {
            return win32_reg_fail(w, ERROR_BUFFER_OVERFLOW, "naming a subkey");
        }
        if (keep > 0) {
            w->path[w->pathlen++] = '\\';
        }
        memcpy(w->path + w->pathlen, name, n);
        w->pathlen += n;
        w->path[w->pathlen] = '\0';

        rc = RegOpenKeyExW(h, wsub, 0, KEY_READ, &child);
        if (rc != ERROR_SUCCESS) {
            return win32_reg_fail(w, rc, "RegOpenKeyExW");
        }
        r = win32_reg_key(w, child, depth + 1);
        RegCloseKey(child);
        if (r != 0) {
            return r;
        }
        w->pathlen          = keep;
        w->path[w->pathlen] = '\0';
    }
    return 0;
}

int cf_platform_registry_walk(const char* root, cf_arena* arena, cf_platform_reg_fn fn,
                              void* ctx, cf_error* err) {
    wchar_t*    wide;
    win32_reg*  w;
    const char* rest = NULL;
    HKEY        hive = (root != NULL) ? win32_reg_hive(root, &rest) : NULL;
    HKEY        h;
    LONG        rc;
    int         r;

    if (hive == NULL) {
        cf_error_set(err, CF_E_SOURCE, NULL, root,
                     "'%s' does not start with a hive: HKCU, HKLM, HKCR, HKU or HKCC",
                     (root != NULL) ? root : "");
        return -1;
    }
    /* From the arena, not static: two configurations may be built at
       once on two threads (D-14). */
    wide = (wchar_t*)cf_arena_alloc(arena, CF_PATH_MAX * sizeof(wchar_t));
    if (wide == NULL || !win32_wide(rest, wide, CF_PATH_MAX)) {
        cf_error_set(err, CF_E_SOURCE, NULL, root,
                     "'%s' is not valid UTF-8, or too long", root);
        return -1;
    }
    w = (win32_reg*)cf_arena_calloc(arena, sizeof *w);
    if (w == NULL) {
        cf_error_set(err, CF_E_MEMORY, NULL, root, "no room to read '%s'", root);
        return -1;
    }
    w->arena = arena;
    w->fn    = fn;
    w->ctx   = ctx;
    w->err   = err;
    w->root  = root;

    rc = RegOpenKeyExW(hive, wide, 0, KEY_READ, &h);
    if (rc == ERROR_FILE_NOT_FOUND) {
        return 1;
    }
    if (rc != ERROR_SUCCESS) {
        cf_error_set(err, CF_E_SOURCE, NULL, root, "cannot open '%s' (error %d)", root,
                     (int)rc);
        return -1;
    }
    r = win32_reg_key(w, h, 0);
    RegCloseKey(h);
    return r;
}

#endif /* CF_PLATFORM_WIN32 */
