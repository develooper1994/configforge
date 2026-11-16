/*
 * posix.c - the POSIX platform. C89 plus the POSIX calls that C89 has
 * no answer for.
 *
 * The whole body is guarded once at the top. That guard is not a
 * scattered #ifdef: the build normally compiles only this file, and the
 * guard exists so the amalgamation - one translation unit, no build to
 * choose for it - can carry every platform and let the preprocessor
 * select.
 */

/* Before any header, because it decides what the headers declare.
   lstat and getpwuid_r (D-83), and kqueue's timeout, a struct timespec,
   are what glibc hides in strict C89; on Linux kqueue exists only as
   libkqueue - which is how the BSD path is built and tested without a
   BSD. Real BSDs and macOS declare all three unconditionally and never
   take this branch. */
#if defined(__linux__) && !defined(_POSIX_C_SOURCE)
/* A feature-test macro is the one reserved name a program is meant to
   define, which bugprone-reserved-identifier cannot know. */
#define _POSIX_C_SOURCE 200112L /* NOLINT(bugprone-reserved-identifier) */
#endif

#include "configforge/cf_platform.h"

#if CF_PLATFORM_POSIX

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#if !defined(__wasi__)
#include <pwd.h>
#endif
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>

/* The sticky bit: XSI's name, which _POSIX_C_SOURCE alone does not
   show, and 01000 on every system that has it. */
#ifndef S_ISVTX
#define S_ISVTX 01000
#endif

/* The wait handle: inotify on Linux, kqueue on macOS and the BSDs, and
   on any other POSIX system a plain statement that there is none, where
   polling cf_changed works exactly as before.

   CF_WAKE_KQUEUE takes the kqueue path on Linux too, through libkqueue.
   That is what lets the BSD implementation be compiled and run on every
   commit by a machine that is not a BSD; it is a test build, not a
   recommendation. */
#if defined(CF_WAKE_KQUEUE) && CF_WAKE_KQUEUE
#define CF_POSIX_WAKE 2
#elif defined(__linux__)
#define CF_POSIX_WAKE 1
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) ||             \
    defined(__OpenBSD__) || defined(__DragonFly__)
#define CF_POSIX_WAKE 2
#else
#define CF_POSIX_WAKE 0
#endif

#if CF_POSIX_WAKE == 1
#include <errno.h>
#include <poll.h>
#include <sys/inotify.h>
#include <unistd.h>
#elif CF_POSIX_WAKE == 2
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/event.h>
#include <time.h>
#include <unistd.h>
#endif

/* Not C89, which is the whole reason this file exists. Declared here
   rather than pulled in from unistd.h so the translation unit stays
   readable about exactly what it is reaching for. */
extern char** environ;

const char* cf_platform_name(void) {
    return "posix";
}

cf_bool cf_platform_has_environ(void) {
    return CF_TRUE;
}

cf_bool cf_platform_has_dir_list(void) {
    return CF_TRUE;
}

cf_bool cf_platform_has_wide_args(void) {
    /* Arguments are already UTF-8 here, so there is nothing to widen
       and nothing that could have been lost. */
    return CF_FALSE;
}

char* const* cf_platform_environ(void) {
    return (char* const*)environ;
}

char* const* cf_platform_args(int argc, char* const* argv, cf_arena* arena,
                              int* out_argc) {
    (void)arena;
    *out_argc = argc;
    return argv;
}

/* A directory by what it holds. See cf_platform.h for why the
   directory's own time is not enough. */
static void posix_stamp_dir(const char* path, cf_stamp* out) {
    DIR*           d = opendir(path);
    struct dirent* e;
    char           full[CF_PLATFORM_NAME_MAX];
    struct stat    st;

    if (d == NULL) {
        return;
    }
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') {
            continue;
        }
        if (cf_fmt(full, sizeof full, "%s/%s", path, e->d_name) >= sizeof full ||
            stat(full, &st) != 0) {
            continue;
        }
        if ((cf_i64)st.st_mtime > out->mtime) {
            out->mtime = (cf_i64)st.st_mtime;
        }
        out->size += (cf_i64)st.st_size + 1;
    }
    closedir(d);
}

cf_bool cf_platform_stamp(const char* path, cf_stamp* out) {
    struct stat st;

    if (out == NULL) {
        return CF_FALSE;
    }
    out->mtime   = 0;
    out->size    = 0;
    out->present = CF_FALSE;
    if (path == NULL || stat(path, &st) != 0) {
        return CF_FALSE;
    }
    out->mtime   = (cf_i64)st.st_mtime;
    out->present = CF_TRUE;
    if (S_ISDIR(st.st_mode)) {
        posix_stamp_dir(path, out);
    } else {
        out->size = (cf_i64)st.st_size;
    }
    return CF_TRUE;
}

void* cf_platform_fopen(const char* path, const char* mode) {
    /* Paths are bytes here, and UTF-8 bytes are a path like any other. */
    return (path != NULL && mode != NULL) ? fopen(path, mode) : NULL;
}

/* There is no registry here, and a source that read none would look
   like an empty one. */
cf_bool cf_platform_has_registry(void) {
    return CF_FALSE;
}

int cf_platform_registry_walk(const char* root, cf_arena* arena, cf_platform_reg_fn fn,
                              void* ctx, cf_error* err) {
    (void)arena;
    (void)fn;
    (void)ctx;
    cf_error_set(err, CF_E_CAPABILITY, NULL, root,
                 "'%s' is in the Windows Registry, which exists only on Windows",
                 (root != NULL) ? root : "");
    return -1;
}

#if defined(__APPLE__) && CF_WITH_PREFERENCES

/* macOS's user defaults, through CoreFoundation: the current user's
   values on any host, the ones `defaults read <domain>` shows (D-87).
   Apple's headers are not C89, so with this option on macOS the build
   compiles this file as C99 and links the framework (CMakeLists.txt). */
#include <CoreFoundation/CoreFoundation.h>

cf_bool cf_platform_has_preferences(void) {
    return CF_TRUE;
}

typedef struct mac_prefs {
    cf_arena*           arena;
    cf_platform_pref_fn fn;
    void*               ctx;
    cf_error*           err;
    const char*         domain;
} mac_prefs;

static int mac_prefs_room(const mac_prefs* w) {
    cf_error_set(w->err, CF_E_MEMORY, NULL, w->domain, "no room to read '%s'",
                 w->domain);
    return -1;
}

/* A CFString as UTF-8 in the arena; NULL when there is no room. */
static char* mac_prefs_utf8(cf_arena* arena, CFStringRef s, size_t* out_len) {
    CFIndex cap = CFStringGetMaximumSizeForEncoding(CFStringGetLength(s),
                                                    kCFStringEncodingUTF8) +
                  1;
    char*   out = (cap > 0) ? (char*)cf_arena_alloc(arena, (size_t)cap) : NULL;

    if (out == NULL || !CFStringGetCString(s, out, cap, kCFStringEncodingUTF8)) {
        return NULL;
    }
    *out_len = strlen(out);
    return out;
}

/* `parent` and `leaf` joined by '/' in the arena; `leaf` alone under the
   domain. Paths live here and not in the recursion's frames. */
static const char* mac_prefs_path(cf_arena* arena, const char* parent, const char* leaf,
                                  size_t leaflen) {
    size_t pn  = strlen(parent);
    char*  out = (char*)cf_arena_alloc(arena, pn + leaflen + 2);

    if (out == NULL) {
        return NULL;
    }
    memcpy(out, parent, pn);
    if (pn > 0) {
        out[pn++] = '/';
    }
    memcpy(out + pn, leaf, leaflen);
    out[pn + leaflen] = '\0';
    return out;
}

static int mac_prefs_emit(const mac_prefs* w, const cf_platform_pref_event* e) {
    return w->fn(w->ctx, e) ? 0 : -1;
}

static int mac_prefs_value(const mac_prefs* w, CFTypeRef v, const char* name,
                           size_t namelen, const char* path, int depth);

/* An array's members in order, each named in its path by its index. */
static int mac_prefs_array(const mac_prefs* w, CFArrayRef a, const char* path, int depth) {
    CFIndex n = CFArrayGetCount(a), i;

    for (i = 0; i < n; ++i) {
        char        idx[24];
        size_t      k  = cf_fmt(idx, sizeof idx, "%U", (cf_u64)i);
        const char* at = mac_prefs_path(w->arena, path, idx, k);

        if (at == NULL) {
            return mac_prefs_room(w);
        }
        if (mac_prefs_value(w, CFArrayGetValueAtIndex(a, i), NULL, 0, at, depth) != 0) {
            return -1;
        }
    }
    return 0;
}

/* A dictionary's members by name, in byte order: the events, and the
   keys a test sees, do not depend on CFDictionary's order. An insertion
   sort over the arena's copies; a dictionary in configuration is small. */
static int mac_prefs_dict(const mac_prefs* w, CFDictionaryRef d, const char* path,
                          int depth) {
    CFIndex      n = CFDictionaryGetCount(d), i, j;
    const void** keys;
    const void** vals;
    const char** names;
    size_t*      lens;

    if (n <= 0) {
        return 0;
    }
    keys  = (const void**)cf_arena_alloc(w->arena, (size_t)n * sizeof *keys);
    vals  = (const void**)cf_arena_alloc(w->arena, (size_t)n * sizeof *vals);
    names = (const char**)cf_arena_alloc(w->arena, (size_t)n * sizeof *names);
    lens  = (size_t*)cf_arena_alloc(w->arena, (size_t)n * sizeof *lens);
    if (keys == NULL || vals == NULL || names == NULL || lens == NULL) {
        return mac_prefs_room(w);
    }
    CFDictionaryGetKeysAndValues(d, keys, vals);
    for (i = 0; i < n; ++i) {
        if (CFGetTypeID(keys[i]) != CFStringGetTypeID()) {
            cf_error_set(w->err, CF_E_KEY, NULL, w->domain,
                         "defaults %s %s: a key that is not a string", w->domain, path);
            return -1;
        }
        names[i] = mac_prefs_utf8(w->arena, (CFStringRef)keys[i], &lens[i]);
        if (names[i] == NULL) {
            return mac_prefs_room(w);
        }
    }
    for (i = 1; i < n; ++i) {
        const char* nm = names[i];
        size_t      ln = lens[i];
        const void* vv = vals[i];

        for (j = i; j > 0 && strcmp(names[j - 1], nm) > 0; --j) {
            names[j] = names[j - 1];
            lens[j]  = lens[j - 1];
            vals[j]  = vals[j - 1];
        }
        names[j] = nm;
        lens[j]  = ln;
        vals[j]  = vv;
    }
    for (i = 0; i < n; ++i) {
        const char* at = mac_prefs_path(w->arena, path, names[i], lens[i]);

        if (at == NULL) {
            return mac_prefs_room(w);
        }
        if (mac_prefs_value(w, vals[i], names[i], lens[i], at, depth) != 0) {
            return -1;
        }
    }
    return 0;
}

/* One value as an event; a dictionary or an array as its begin, its
   members one level down, and its end. */
static int mac_prefs_value(const mac_prefs* w, CFTypeRef v, const char* name,
                           size_t namelen, const char* path, int depth) {
    cf_platform_pref_event e;
    CFTypeID               t = CFGetTypeID(v);
    int                    r;

    memset(&e, 0, sizeof e);
    e.name    = name;
    e.namelen = namelen;
    e.path    = path;
    if (t == CFDictionaryGetTypeID() || t == CFArrayGetTypeID()) {
        if (depth >= CF_MAX_DEPTH) {
            cf_error_set(w->err, CF_E_SOURCE, NULL, w->domain,
                         "defaults %s: nests deeper than %d levels", w->domain,
                         CF_MAX_DEPTH);
            return -1;
        }
        e.kind = (t == CFDictionaryGetTypeID()) ? CF_PREF_BEGIN_DICT : CF_PREF_BEGIN_ARRAY;
        r      = mac_prefs_emit(w, &e);
        if (r == 0 && e.kind == CF_PREF_BEGIN_DICT) {
            r = mac_prefs_dict(w, (CFDictionaryRef)v, path, depth + 1);
        } else if (r == 0) {
            r = mac_prefs_array(w, (CFArrayRef)v, path, depth + 1);
        }
        if (r != 0) {
            return r;
        }
        memset(&e, 0, sizeof e);
        e.kind = CF_PREF_END;
        e.path = path;
        return mac_prefs_emit(w, &e);
    }
    if (t == CFStringGetTypeID()) {
        e.kind = CF_PREF_TEXT;
        e.text = mac_prefs_utf8(w->arena, (CFStringRef)v, &e.textlen);
        if (e.text == NULL) {
            return mac_prefs_room(w);
        }
    } else if (t == CFNumberGetTypeID() && CFNumberIsFloatType((CFNumberRef)v)) {
        e.kind = CF_PREF_REAL;
        (void)CFNumberGetValue((CFNumberRef)v, kCFNumberDoubleType, &e.r);
    } else if (t == CFNumberGetTypeID()) {
        e.kind = CF_PREF_INT;
        (void)CFNumberGetValue((CFNumberRef)v, kCFNumberSInt64Type, &e.i);
    } else if (t == CFBooleanGetTypeID()) {
        e.kind = CF_PREF_BOOL;
        e.b    = CFBooleanGetValue((CFBooleanRef)v) ? CF_TRUE : CF_FALSE;
    } else {
        e.kind = CF_PREF_OTHER;
        if (t == CFDateGetTypeID()) {
            e.text = "a date";
        } else if (t == CFDataGetTypeID()) {
            e.text = "data";
        } else {
            e.text = "of another type";
        }
        e.textlen = strlen(e.text);
    }
    return mac_prefs_emit(w, &e);
}

int cf_platform_preferences_walk(const char* domain, cf_arena* arena,
                                 cf_platform_pref_fn fn, void* ctx, cf_error* err) {
    CFStringRef     app;
    CFArrayRef      keys;
    CFDictionaryRef all;
    mac_prefs       w;
    int             r;

    w.arena  = arena;
    w.fn     = fn;
    w.ctx    = ctx;
    w.err    = err;
    w.domain = (domain != NULL) ? domain : "";
    app      = (domain != NULL) ? CFStringCreateWithCString(kCFAllocatorDefault, domain,
                                                            kCFStringEncodingUTF8)
                                : NULL;
    if (app == NULL) {
        cf_error_set(err, CF_E_SOURCE, NULL, w.domain,
                     "'%s' is not a domain: it is not UTF-8", w.domain);
        return -1;
    }
    /* So that values another process wrote, `defaults` among them, are
       seen and not this process's cached copy. */
    (void)CFPreferencesAppSynchronize(app);
    keys = CFPreferencesCopyKeyList(app, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
    if (keys == NULL || CFArrayGetCount(keys) == 0) {
        if (keys != NULL) {
            CFRelease(keys);
        }
        CFRelease(app);
        return 1;
    }
    all = CFPreferencesCopyMultiple(keys, app, kCFPreferencesCurrentUser,
                                    kCFPreferencesAnyHost);
    CFRelease(keys);
    CFRelease(app);
    if (all == NULL) {
        cf_error_set(err, CF_E_SOURCE, NULL, w.domain,
                     "defaults %s: its values could not be read", w.domain);
        return -1;
    }
    r = mac_prefs_value(&w, all, NULL, 0, "", 0);
    CFRelease(all);
    return r;
}

#else

/* No user defaults are read here, and a source that read none would
   look like an empty domain. */
cf_bool cf_platform_has_preferences(void) {
    return CF_FALSE;
}

int cf_platform_preferences_walk(const char* domain, cf_arena* arena,
                                 cf_platform_pref_fn fn, void* ctx, cf_error* err) {
#if CF_WITH_PREFERENCES
    static const char says[] =
        "'%s' is in macOS's user defaults, which exist only on macOS";
#else
    static const char says[] = "'%s' is in macOS's user defaults, and this build was "
                               "compiled without CF_WITH_PREFERENCES";
#endif
    (void)arena;
    (void)fn;
    (void)ctx;
    cf_error_set(err, CF_E_CAPABILITY, NULL, domain, says,
                 (domain != NULL) ? domain : "");
    return -1;
}

#endif /* user defaults */

int cf_platform_world_readable(const char* path) {
    struct stat st;

    if (path == NULL || stat(path, &st) != 0) {
        return -1;
    }
    return ((st.st_mode & S_IROTH) != 0) ? 1 : 0;
}

cf_bool cf_platform_has_private_create(void) {
    return CF_TRUE;
}

int cf_platform_create_private(const char* path, cf_error* err) {
    int fd;

    if (path == NULL) {
        cf_error_set(err, CF_E_SOURCE, NULL, NULL, "no file to create");
        return -1;
    }
    /* The mode goes in with the creation: a chmod afterwards leaves a
       window in which the file has the umask's mode. */
    fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0) {
        if (errno == EEXIST) {
            return 0;
        }
        cf_error_set(err, CF_E_SOURCE, NULL, path, "cannot create '%s': %s", path,
                     strerror(errno));
        return -1;
    }
    close(fd);
    return 1;
}

#if !defined(__wasi__)
/* Who may change what a directory hands over (D-83). A mode is printed
   by its three octal digits, since cf_fmt has no %o. */
static const char posix_writable[] =
    "'%s' can be written by users other than its owner and root (mode "
    "0%u%u%u); a %s read as configuration must not be, or CF_ANY_WRITER "
    "must say it may";
static const char posix_stranger[] =
    "'%s' belongs to user %U, in a directory others can write to; only "
    "root, the directory's owner and this program's user (%U) may put a "
    "file there, or CF_ANY_WRITER must say anyone may";

/* Whether `gid` is the private group of user `uid`: the group of that
   user's entry in the password database. A user with no entry has no
   group of their own, and neither has one whose entry is too long for
   the buffer (ERANGE): it is not known to be the user's own. The buffer
   is in this frame, not the listing's, which already holds a path. */
static cf_bool posix_own_group(uid_t uid, gid_t gid) {
    struct passwd  pw;
    struct passwd* found = NULL;
    char           buf[512];

    if (getpwuid_r(uid, &pw, buf, sizeof buf, &found) != 0 || found == NULL) {
        return CF_FALSE;
    }
    return (found->pw_gid == gid) ? CF_TRUE : CF_FALSE;
}

/* Whether a group that is not its owner's own may write to it. */
static cf_bool posix_group_writes(const struct stat* st) {
    if ((st->st_mode & S_IWGRP) == 0) {
        return CF_FALSE;
    }
    return posix_own_group(st->st_uid, st->st_gid) ? CF_FALSE : CF_TRUE;
}

/* The refusal of a directory or a file others may write to: -2. */
static int posix_refuse(const char* path, const struct stat* st, const char* what,
                        cf_error* err) {
    unsigned mode = (unsigned)st->st_mode;

    cf_error_set(err, CF_E_SOURCE, NULL, path, posix_writable, path, (mode >> 6) & 7u,
                 (mode >> 3) & 7u, mode & 7u, what);
    return -2;
}
#endif /* !__wasi__ */

/* The listing, and with `trusted` the check of D-83 in the same pass,
   so that nothing is listed once and checked again:
   1. the directory is refused when others, or a group that is not its
      owner's own, may write to it and it is not sticky;
   2. in a sticky directory they may write to, each entry as lstat
      sees it (the link, for a link) must belong to root, to the
      directory's owner or to the program's user: there nobody can
      replace another's entry;
   3. each file as stat sees it - what a link names - is refused when
      others may write to it, or a group that is not its owner's own.
   The directories on the way to a link's target are not checked. */
static int posix_dir_list(const char* path, char (*names)[CF_PLATFORM_NAME_MAX],
                          int max, cf_bool trusted, cf_error* err) {
    DIR*           d;
    struct dirent* e;
    char           full[CF_PLATFORM_NAME_MAX];
    struct stat    st;
#if !defined(__wasi__)
    struct stat lst;
    uid_t       owner  = 0;
    cf_bool     shared = CF_FALSE;
#endif
    int n = 0;

    if (path == NULL || names == NULL) {
        cf_error_set(err, CF_E_SOURCE, NULL, path, "no directory given");
        return -1;
    }
#if defined(__wasi__)
    /* No users, no pwd.h: nothing to check, as on Windows. */
    (void)trusted;
#else
    if (trusted) {
        if (stat(path, &st) != 0) {
            cf_error_set(err, CF_E_SOURCE, NULL, path, "cannot read '%s'", path);
            return -1;
        }
        /* Shared: others, or a group that is not its owner's own, may
           write here. Sticky, that is rule 2's to judge by each entry. */
        shared = ((st.st_mode & S_IWOTH) != 0) ? CF_TRUE : posix_group_writes(&st);
        if (shared && (st.st_mode & S_ISVTX) == 0) {
            return posix_refuse(path, &st, "directory", err);
        }
        owner = st.st_uid;
    }
#endif
    d = opendir(path);
    if (d == NULL) {
        cf_error_set(err, CF_E_SOURCE, NULL, path, "cannot read '%s'", path);
        return -1;
    }
    while ((e = readdir(d)) != NULL) {
        size_t len;
        if (e->d_name[0] == '.') {
            /* Skips . and .. and, deliberately, dotfiles: an editor's
               .swp and a package manager's .dpkg-new both land in a
               drop-in directory and neither is configuration. */
            continue;
        }
        len = strlen(e->d_name);
        if (len + 1 > CF_PLATFORM_NAME_MAX ||
            cf_fmt(full, sizeof full, "%s/%s", path, e->d_name) >= sizeof full) {
            cf_error_set(err, CF_E_SOURCE, NULL, path,
                         "a path in '%s' is too long: '%s'", path, e->d_name);
            n = -2;
            break;
        }
#if !defined(__wasi__)
        /* Rule 2: the entry itself, the link and not what it names. */
        if (shared) {
            uid_t me = geteuid();
            if (lstat(full, &lst) != 0) {
                cf_error_set(err, CF_E_SOURCE, NULL, full, "cannot read '%s'", full);
                n = -2;
                break;
            }
            if (lst.st_uid != 0 && lst.st_uid != owner && lst.st_uid != me) {
                cf_error_set(err, CF_E_SOURCE, NULL, full, posix_stranger, full,
                             (cf_u64)lst.st_uid, (cf_u64)me);
                n = -2;
                break;
            }
        }
#endif
        /* stat, not lstat: a Kubernetes mount is links to files, and
           what counts is what a link names. Asked before anything opens
           it, because opening a FIFO waits for a writer. */
        if (stat(full, &st) != 0) {
            cf_error_set(err, CF_E_SOURCE, NULL, full, "cannot read '%s'", full);
            n = -2;
            break;
        }
        if (S_ISDIR(st.st_mode)) {
            continue; /* as on Windows: a subdirectory is not a file of it */
        }
        if (!S_ISREG(st.st_mode)) {
            cf_error_set(err, CF_E_SOURCE, NULL, full,
                         "'%s' is not a file, and everything in the directory is read "
                         "as one",
                         full);
            n = -2;
            break;
        }
#if !defined(__wasi__)
        /* Rule 3: the file, as what a link names. */
        if (trusted && ((st.st_mode & S_IWOTH) != 0 || posix_group_writes(&st))) {
            n = posix_refuse(full, &st, "file", err);
            break;
        }
#endif
        if (n >= max) {
            cf_error_set(err, CF_E_SOURCE, NULL, path,
                         "'%s' holds more than %d files, the most a directory may",
                         path, max);
            n = -2;
            break;
        }
        memcpy(names[n], e->d_name, len + 1);
        ++n;
    }
    closedir(d);
    return n;
}

int cf_platform_dir_list(const char* path, char (*names)[CF_PLATFORM_NAME_MAX], int max,
                         cf_error* err) {
    return posix_dir_list(path, names, max, CF_FALSE, err);
}

cf_bool cf_platform_has_trusted_dir(void) {
#if defined(__wasi__)
    return CF_FALSE;
#else
    return CF_TRUE;
#endif
}

int cf_platform_dir_list_trusted(const char* path, char (*names)[CF_PLATFORM_NAME_MAX],
                                 int max, cf_error* err) {
#if defined(__wasi__)
    return posix_dir_list(path, names, max, CF_FALSE, err);
#else
    return posix_dir_list(path, names, max, CF_TRUE, err);
#endif
}

/* ------------------------------------------------------------------ *
 * The wait handle
 * ------------------------------------------------------------------ */

#if CF_POSIX_WAKE

typedef struct posix_target {
    char    path[CF_PLATFORM_NAME_MAX]; /* as the configuration named it */
    char    dir[CF_PLATFORM_NAME_MAX];  /* the directory that holds it */
    cf_bool direct; /* watched itself, rather than through an ancestor */
} posix_target;

struct cf_platform_wake {
    const cf_allocator* a;
    int                 fd; /* the inotify or kqueue descriptor */
    size_t              n;
    posix_target*       t;
    /* kqueue only: one open descriptor per watched directory and file,
       because a kqueue watches what it has open rather than a name. */
    int*   vfd;
    size_t nvfd;
    size_t capvfd;
};

/* One level up, in place. False at the top, where there is nowhere to
   go. "a/b" becomes "a", "a" becomes ".", "/a" becomes "/". */
static cf_bool posix_up(char* dir) {
    size_t len = strlen(dir);

    if (len == 0 || (len == 1 && (dir[0] == '/' || dir[0] == '.'))) {
        return CF_FALSE;
    }
    while (len > 0 && dir[len - 1] != '/') {
        --len;
    }
    if (len == 0) {
        memcpy(dir, ".", 2);
    } else if (len == 1) {
        dir[1] = '\0';
    } else {
        dir[len - 1] = '\0';
    }
    return CF_TRUE;
}

/* The directory a path lives in, or the path when it is a directory. */
static cf_bool posix_dir_of(const char* path, char* out, size_t cap) {
    struct stat st;
    size_t      len = strlen(path);

    if (len + 1 > cap) {
        return CF_FALSE;
    }
    memcpy(out, path, len + 1);
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        return CF_TRUE;
    }
    if (!posix_up(out)) {
        memcpy(out, ".", 2);
    }
    return CF_TRUE;
}

#endif /* CF_POSIX_WAKE */

#if CF_POSIX_WAKE == 1

/* Everything that can make a watched path different: written, replaced
   by a rename, created, removed, or the directory itself going away. */
#define POSIX_WAKE_MASK                                                                \
    (IN_CLOSE_WRITE | IN_MODIFY | IN_ATTRIB | IN_CREATE | IN_DELETE | IN_MOVED_FROM |  \
     IN_MOVED_TO | IN_DELETE_SELF | IN_MOVE_SELF)

static const char* posix_why(int e) {
    switch (e) {
    case ENOSPC:
        return "the inotify watch limit is reached";
    case EACCES:
        return "permission denied";
    case ENOMEM:
        return "the kernel is out of memory";
    default:
        return "inotify refused it";
    }
}

/* Watch the target itself, or the nearest ancestor that exists. */
static cf_bool posix_arm(cf_platform_wake* w, posix_target* t, cf_error* err) {
    char dir[CF_PLATFORM_NAME_MAX];
    int  e;

    if (inotify_add_watch(w->fd, t->dir, POSIX_WAKE_MASK) >= 0) {
        t->direct = CF_TRUE;
        return CF_TRUE;
    }
    e = errno;
    if (e != ENOENT && e != ENOTDIR) {
        return cf_error_set(err, CF_E_CAPABILITY, NULL, t->dir, "cannot watch '%s': %s",
                            t->dir, posix_why(e));
    }
    t->direct = CF_FALSE;
    memcpy(dir, t->dir, sizeof dir);
    while (posix_up(dir)) {
        if (inotify_add_watch(w->fd, dir, POSIX_WAKE_MASK) >= 0) {
            return CF_TRUE;
        }
        e = errno;
        if (e != ENOENT && e != ENOTDIR) {
            return cf_error_set(err, CF_E_CAPABILITY, NULL, dir,
                                "cannot watch '%s': %s", dir, posix_why(e));
        }
    }
    return cf_error_set(err, CF_E_CAPABILITY, NULL, t->dir,
                        "cannot watch '%s': no part of it exists", t->dir);
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
    w = (cf_platform_wake*)a->alloc(a->ctx, sizeof *w + n * sizeof(posix_target));
    if (w == NULL) {
        cf_error_set(err, CF_E_MEMORY, NULL, NULL, "no room to watch %u path(s)",
                     (unsigned)n);
        return NULL;
    }
    {
        void* rest = w + 1;
        w->t       = (posix_target*)rest;
    }
    w->a      = a;
    w->n      = n;
    w->vfd    = NULL;
    w->nvfd   = 0;
    w->capvfd = 0;
    w->fd     = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (w->fd < 0) {
        cf_error_set(err, CF_E_CAPABILITY, NULL, NULL, "inotify is unavailable: %s",
                     posix_why(errno));
        a->release(a->ctx, w);
        return NULL;
    }
    for (i = 0; i < n; ++i) {
        if (paths[i] == NULL ||
            !posix_dir_of(paths[i], w->t[i].dir, sizeof w->t[i].dir)) {
            cf_error_set(err, CF_E_CAPABILITY, NULL, paths[i],
                         "cannot watch a path this long");
            cf_platform_wake_close(w);
            return NULL;
        }
        if (!posix_arm(w, &w->t[i], err)) {
            cf_platform_wake_close(w);
            return NULL;
        }
    }
    return w;
}

int cf_platform_wake_fd(const cf_platform_wake* w) {
    return (w != NULL) ? w->fd : -1;
}

void* cf_platform_wake_handle(const cf_platform_wake* w) {
    (void)w;
    return NULL;
}

/* Linux's NAME_MAX, the longest name an inotify record carries. Spelled
   out because <limits.h> declares it only under POSIX feature macros,
   which a strict C89 compile does not set. */
#define CF_INOTIFY_NAME_MAX 255

cf_bool cf_platform_wake_drain(cf_platform_wake* w) {
    /* Aligned for the event records the kernel writes into it, and room
       for two of the largest: the loop below reads until the queue is
       empty however small the buffer, as long as one record fits, so
       anything larger is stack spent to save a system call on a path
       that runs once per change. */
    union {
        struct inotify_event e;
        char b[2 * (sizeof(struct inotify_event) + CF_INOTIFY_NAME_MAX + 1)];
    } buf;
    cf_bool lost = CF_FALSE;
    cf_bool ok   = CF_TRUE;
    size_t  i;

    if (w == NULL) {
        return CF_FALSE;
    }
    for (;;) {
        ssize_t got = read(w->fd, buf.b, sizeof buf.b);
        size_t  at  = 0;
        if (got <= 0) {
            break; /* EAGAIN: nothing left, which is the point */
        }
        /* A watch the kernel dropped - its directory was removed or
           renamed - has to be re-armed, and the event does not say
           which target it belonged to. Re-arming is idempotent for the
           ones still watched, so all of them are. */
        while (at + sizeof(struct inotify_event) <= (size_t)got) {
            struct inotify_event ev;
            memcpy(&ev, buf.b + at, sizeof ev);
            if ((ev.mask & (IN_IGNORED | IN_Q_OVERFLOW)) != 0) {
                lost = CF_TRUE;
            }
            at += sizeof(struct inotify_event) + ev.len;
        }
    }
    /* A target watched through an ancestor tries for itself again: the
       level that was missing may be what just appeared. */
    for (i = 0; i < w->n; ++i) {
        if (lost || !w->t[i].direct) {
            if (!posix_arm(w, &w->t[i], NULL)) {
                ok = CF_FALSE;
            }
        }
    }
    return ok;
}

int cf_platform_wake_wait(cf_platform_wake* w, long timeout_ms) {
    struct pollfd p;
    int           ms = -1;
    int           r;

    if (w == NULL) {
        return -1;
    }
    if (timeout_ms >= 0) {
        ms = (timeout_ms > 0x7fffffffL) ? 0x7fffffff : (int)timeout_ms;
    }
    p.fd      = w->fd;
    p.events  = POLLIN;
    p.revents = 0;
    r         = poll(&p, 1, ms);
    if (r < 0) {
        /* A signal is not a failure: nothing was seen, ask again. */
        return (errno == EINTR) ? 0 : -1;
    }
    if (r == 0) {
        return 0;
    }
    return cf_platform_wake_drain(w) ? 1 : -1;
}

void cf_platform_wake_close(cf_platform_wake* w) {
    if (w == NULL) {
        return;
    }
    if (w->fd >= 0) {
        close(w->fd);
    }
    w->a->release(w->a->ctx, w);
}

#elif CF_POSIX_WAKE == 2

/* kqueue watches what it has open, not a name, which shapes everything
   below. A directory's events say an entry was added, removed or
   renamed - never that a file in it was rewritten in place - so each
   file is watched as well as its directory. And a descriptor follows
   the file it was opened on: after an editor renames a new file over
   the old one, the descriptor watches the old one in the bin. So every
   drain closes everything and opens it again by name, which is a few
   system calls per watched file and is the only way to be watching what
   the names now mean.

   A change in the gap while that happens is not lost: drain, then ask,
   and cf_changed compares against the files themselves. */

/* NOTE_LINK because a directory created inside a watched one changes
   its link count, and that is how the missing level of a path arrives. */
#define KQ_VNODE_EVENTS                                                                \
    (NOTE_WRITE | NOTE_DELETE | NOTE_RENAME | NOTE_EXTEND | NOTE_ATTRIB | NOTE_LINK)

static const char* kq_why(int e) {
    switch (e) {
    case EMFILE:
    case ENFILE:
        return "too many open files: kqueue holds one per watched file";
    case EACCES:
        return "permission denied";
    case ENOMEM:
        return "the kernel is out of memory";
    default:
        return "kqueue refused it";
    }
}

/* Deleted before it is closed. A real kqueue drops a descriptor's events
   when it is closed; libkqueue cannot see a close, keeps the event, and
   attaches the next descriptor to reuse the number to the old file's
   watch - so the second drain watched nothing it named. Saying it
   explicitly is correct on both. */
static void kq_close_all(cf_platform_wake* w) {
    size_t i;
    for (i = 0; i < w->nvfd; ++i) {
        struct kevent ev;
        EV_SET(&ev, (uintptr_t)w->vfd[i], EVFILT_VNODE, EV_DELETE, 0, 0, NULL);
        (void)kevent(w->fd, &ev, 1, NULL, 0, NULL);
        close(w->vfd[i]);
    }
    w->nvfd = 0;
}

/* 1 watched, 0 absent, -1 failed with `err` set. */
static int kq_add(cf_platform_wake* w, const char* path, cf_error* err) {
    struct kevent ev;
    int           flags = O_RDONLY;
    int           fd;

#ifdef O_EVTONLY
    /* macOS: an event-only descriptor, which does not keep the volume
       it is on from being unmounted. */
    flags = O_EVTONLY;
#endif
#ifdef O_CLOEXEC
    flags |= O_CLOEXEC;
#endif
    /* Without O_NONBLOCK, opening a FIFO in conf.d waits for a writer,
       and the watch with it. It changes nothing for a file or a
       directory. No O_NOFOLLOW: a Kubernetes mount is links, and the
       watch is on what they name. */
    flags |= O_NONBLOCK;
    fd = open(path, flags);
    if (fd < 0) {
        int e = errno;
        if (e == ENOENT || e == ENOTDIR) {
            return 0;
        }
        cf_error_set(err, CF_E_CAPABILITY, NULL, path, "cannot watch '%s': %s", path,
                     kq_why(e));
        return -1;
    }
    {
        /* Only files and directories are watched. Anything else is left
           as if absent: the listing refuses it by name when the
           directory is read, which is where it is reported. */
        struct stat st;
        if (fstat(fd, &st) != 0 || (!S_ISREG(st.st_mode) && !S_ISDIR(st.st_mode))) {
            close(fd);
            return 0;
        }
    }
    if (w->nvfd == w->capvfd) {
        size_t ncap = (w->capvfd < 8) ? 8 : w->capvfd * 2;
        int*   nv   = (int*)w->a->alloc(w->a->ctx, ncap * sizeof(int));
        if (nv == NULL) {
            close(fd);
            cf_error_set(err, CF_E_MEMORY, NULL, path, "no room to watch '%s'", path);
            return -1;
        }
        if (w->nvfd > 0) {
            memcpy(nv, w->vfd, w->nvfd * sizeof(int));
        }
        if (w->vfd != NULL) {
            w->a->release(w->a->ctx, w->vfd);
        }
        w->vfd    = nv;
        w->capvfd = ncap;
    }
    EV_SET(&ev, (uintptr_t)fd, EVFILT_VNODE, EV_ADD | EV_CLEAR, KQ_VNODE_EVENTS, 0,
           NULL);
    if (kevent(w->fd, &ev, 1, NULL, 0, NULL) < 0) {
        int e = errno;
        close(fd);
        cf_error_set(err, CF_E_CAPABILITY, NULL, path, "cannot watch '%s': %s", path,
                     kq_why(e));
        return -1;
    }
    w->vfd[w->nvfd++] = fd;
    return 1;
}

/* Every file in a directory, as well as the directory: an edit in place
   inside conf.d moves nothing the directory's own events can see. */
static cf_bool kq_add_entries(cf_platform_wake* w, const char* dir, cf_error* err) {
    DIR*           d = opendir(dir);
    struct dirent* e;
    char           full[CF_PLATFORM_NAME_MAX];
    cf_bool        ok = CF_TRUE;

    if (d == NULL) {
        return CF_TRUE; /* gone since it was opened; the next drain sees why */
    }
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') {
            continue; /* skipped as the listing skips it */
        }
        if (cf_fmt(full, sizeof full, "%s/%s", dir, e->d_name) >= sizeof full) {
            continue;
        }
        if (kq_add(w, full, err) < 0) {
            ok = CF_FALSE;
            break;
        }
    }
    closedir(d);
    return ok;
}

/* Watch the target's directory, or the nearest ancestor that exists,
   and - when the directory itself is there - the target too. */
static cf_bool kq_arm(cf_platform_wake* w, posix_target* t, cf_error* err) {
    char        dir[CF_PLATFORM_NAME_MAX];
    struct stat st;
    int         r;

    memcpy(dir, t->dir, sizeof dir);
    t->direct = CF_TRUE;
    for (;;) {
        r = kq_add(w, dir, err);
        if (r < 0) {
            return CF_FALSE;
        }
        if (r > 0) {
            break;
        }
        t->direct = CF_FALSE;
        if (!posix_up(dir)) {
            return cf_error_set(err, CF_E_CAPABILITY, NULL, t->dir,
                                "cannot watch '%s': no part of it exists", t->dir);
        }
    }
    if (!t->direct || stat(t->path, &st) != 0) {
        return CF_TRUE; /* the file's arrival is the directory's event */
    }
    if (S_ISDIR(st.st_mode)) {
        return kq_add_entries(w, t->path, err);
    }
    return (kq_add(w, t->path, err) < 0) ? CF_FALSE : CF_TRUE;
}

static cf_bool kq_arm_all(cf_platform_wake* w, cf_error* err) {
    size_t  i;
    cf_bool ok = CF_TRUE;

    kq_close_all(w);
    for (i = 0; i < w->n; ++i) {
        if (!kq_arm(w, &w->t[i], err)) {
            ok  = CF_FALSE;
            err = NULL; /* the first failure is the useful one */
        }
    }
    return ok;
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
    w = (cf_platform_wake*)a->alloc(a->ctx, sizeof *w + n * sizeof(posix_target));
    if (w == NULL) {
        cf_error_set(err, CF_E_MEMORY, NULL, NULL, "no room to watch %u path(s)",
                     (unsigned)n);
        return NULL;
    }
    {
        void* rest = w + 1;
        w->t       = (posix_target*)rest;
    }
    w->a      = a;
    w->n      = n;
    w->vfd    = NULL;
    w->nvfd   = 0;
    w->capvfd = 0;
    w->fd     = kqueue();
    if (w->fd < 0) {
        cf_error_set(err, CF_E_CAPABILITY, NULL, NULL, "kqueue is unavailable: %s",
                     kq_why(errno));
        a->release(a->ctx, w);
        return NULL;
    }
    for (i = 0; i < n; ++i) {
        size_t len = (paths[i] != NULL) ? strlen(paths[i]) : 0;
        if (paths[i] == NULL || len + 1 > sizeof w->t[i].path ||
            !posix_dir_of(paths[i], w->t[i].dir, sizeof w->t[i].dir)) {
            cf_error_set(err, CF_E_CAPABILITY, NULL, paths[i],
                         "cannot watch a path this long");
            cf_platform_wake_close(w);
            return NULL;
        }
        memcpy(w->t[i].path, paths[i], len + 1);
    }
    if (!kq_arm_all(w, err)) {
        cf_platform_wake_close(w);
        return NULL;
    }
    return w;
}

/* On macOS and the BSDs a kqueue descriptor is readable when an event
   is pending, so it goes into an event loop like any other. libkqueue's
   is not, which is why cf_platform_wake_wait asks kevent rather than
   poll - and why the test build cannot vouch for this one call. */
int cf_platform_wake_fd(const cf_platform_wake* w) {
    return (w != NULL) ? w->fd : -1;
}

void* cf_platform_wake_handle(const cf_platform_wake* w) {
    (void)w;
    return NULL;
}

cf_bool cf_platform_wake_drain(cf_platform_wake* w) {
    struct kevent   ev[8];
    struct timespec zero;

    if (w == NULL) {
        return CF_FALSE;
    }
    zero.tv_sec  = 0;
    zero.tv_nsec = 0;
    while (kevent(w->fd, NULL, 0, ev, 8, &zero) > 0) {
    }
    return kq_arm_all(w, NULL);
}

int cf_platform_wake_wait(cf_platform_wake* w, long timeout_ms) {
    struct kevent   ev;
    struct timespec ts;
    int             r;

    if (w == NULL) {
        return -1;
    }
    if (timeout_ms >= 0) {
        ts.tv_sec  = (time_t)(timeout_ms / 1000);
        ts.tv_nsec = (long)(timeout_ms % 1000) * 1000000L;
    }
    r = kevent(w->fd, NULL, 0, &ev, 1, (timeout_ms >= 0) ? &ts : NULL);
    if (r < 0) {
        /* A signal is not a failure: nothing was seen, ask again. */
        return (errno == EINTR) ? 0 : -1;
    }
    if (r == 0) {
        return 0;
    }
    return cf_platform_wake_drain(w) ? 1 : -1;
}

void cf_platform_wake_close(cf_platform_wake* w) {
    if (w == NULL) {
        return;
    }
    kq_close_all(w);
    if (w->vfd != NULL) {
        w->a->release(w->a->ctx, w->vfd);
    }
    if (w->fd >= 0) {
        close(w->fd);
    }
    w->a->release(w->a->ctx, w);
}

#else /* no wait handle on this POSIX system */

cf_bool cf_platform_has_wake(void) {
    return CF_FALSE;
}

cf_platform_wake* cf_platform_wake_open(const char* const* paths, size_t n,
                                        const cf_allocator* a, cf_error* err) {
    (void)paths;
    (void)n;
    (void)a;
    cf_error_set(err, CF_E_CAPABILITY, NULL, NULL,
                 "this platform has no change handle yet; poll cf_changed instead");
    return NULL;
}

int cf_platform_wake_fd(const cf_platform_wake* w) {
    (void)w;
    return -1;
}

void* cf_platform_wake_handle(const cf_platform_wake* w) {
    (void)w;
    return NULL;
}

cf_bool cf_platform_wake_drain(cf_platform_wake* w) {
    (void)w;
    return CF_FALSE;
}

int cf_platform_wake_wait(cf_platform_wake* w, long timeout_ms) {
    (void)w;
    (void)timeout_ms;
    return -1;
}

void cf_platform_wake_close(cf_platform_wake* w) {
    (void)w;
}

#endif /* CF_POSIX_WAKE == ... */

#endif /* CF_PLATFORM_POSIX */
