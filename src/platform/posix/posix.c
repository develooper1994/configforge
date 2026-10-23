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
   kqueue's timeout is a struct timespec, which glibc hides in strict
   C89, and on Linux kqueue exists only as libkqueue - which is how the
   BSD path is built and tested without a BSD. Real BSDs and macOS
   declare it unconditionally and never take this branch. */
#if defined(CF_WAKE_KQUEUE) && CF_WAKE_KQUEUE && defined(__linux__) &&                 \
    !defined(_POSIX_C_SOURCE)
/* A feature-test macro is the one reserved name a program is meant to
   define, which bugprone-reserved-identifier cannot know. */
#define _POSIX_C_SOURCE 200112L /* NOLINT(bugprone-reserved-identifier) */
#endif

#include "configforge/cf_platform.h"

#if CF_PLATFORM_POSIX

#include <dirent.h>
#include <sys/stat.h>
#include <string.h>

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

int cf_platform_dir_list(const char* path, char (*names)[CF_PLATFORM_NAME_MAX], int max,
                         cf_error* err) {
    DIR*           d;
    struct dirent* e;
    int            n = 0;

    if (path == NULL || names == NULL) {
        cf_error_set(err, CF_E_SOURCE, NULL, path, "no directory given");
        return -1;
    }
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
        if (n >= max) {
            break;
        }
        len = strlen(e->d_name);
        if (len + 1 > CF_PLATFORM_NAME_MAX) {
            continue;
        }
        memcpy(names[n], e->d_name, len + 1);
        ++n;
    }
    closedir(d);
    return n;
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

cf_bool cf_platform_wake_drain(cf_platform_wake* w) {
    /* Aligned for the event records the kernel writes into it. A
       kilobyte rather than the customary four: the library's stack
       budget is two, and the loop below reads until the queue is empty
       however small the buffer, as long as one record fits. */
    union {
        struct inotify_event e;
        char                 b[1024];
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
