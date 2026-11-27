/*
 * file.c (posix) - opening a file by a UTF-8 path, and replacing one whole
 * or not at all (D-224, D-127), on POSIX. C89 plus the POSIX calls.
 *
 * Apart from posix.c so that the serialization library, which opens
 * files and calls nothing else of the platform, carries this file
 * without it (docs/Architecture.md, "Layers").
 *
 * Guarded once at the top, for the reason given in posix.c.
 */

/* Before any header, because it decides what the headers declare: lstat,
   fdopen, fileno, fchmod and clock_gettime are what glibc hides in strict
   C89 (the reason is posix.c's). O_CLOEXEC is not among them, and
   file_core asks FD_CLOEXEC of the descriptor when it is missing. */
#if defined(__linux__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200112L /* NOLINT(bugprone-reserved-identifier) */
#endif

#include "configforge/cf_platform_file.h"

#if CF_PLATFORM_POSIX

#include "configforge/cf_build_options.h"
#include "configforge/cf_error.h"
#include "configforge/cf_file.h"
#include "configforge/cf_port.h"
#include "../file_prim.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

void* cf_platform_fopen(const char* path, const char* mode) {
    /* Paths are bytes here, and UTF-8 bytes are a path like any other. */
    return (path != NULL && mode != NULL) ? fopen(path, mode) : NULL;
}

cf_bool cf_platform_has_files(void) {
    return CF_TRUE;
}

unsigned long cf_platform_now_ms(void) {
#if defined(CLOCK_MONOTONIC)
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (unsigned long)ts.tv_sec * 1000UL +
               (unsigned long)(ts.tv_nsec / 1000000L);
    }
#endif
    return (unsigned long)-1;
}

cf_bool cfi_file_sync(void* stream) {
    return fsync(fileno((FILE*)stream)) == 0 ? CF_TRUE : CF_FALSE;
}

const char* cfi_file_why(char* buf, size_t cap) {
    return cfi_file_why_stdio(buf, cap);
}

unsigned cf_platform_write_steps(void) {
    return 6u;
}

/* What file_core has made so far, for its one exit that undoes it. */
typedef struct replace_state {
    FILE* fp;   /* the temporary's stream while it is open */
    int   made; /* the temporary exists */
    int   raw;  /* `why` is the whole message, not the reason of a "cannot write" */
    char  tmp[CF_PATH_MAX];
    char  why[256]; /* a reason, or a message that names the path (cut to fit: the error
                       carries the path whole) */
} replace_state;

/* A step that was told to fail. */
static int file_injected(replace_state* x, unsigned step) {
    (void)cf_fmt(x->why, sizeof x->why, "injected failure at step %u", step);
    return 0;
}

/* A step whose system call failed: errno says why. */
static int file_failed(replace_state* x) {
    (void)cfi_file_why(x->why, sizeof x->why);
    return 0;
}

/* The temporary made, written, synced, given the original's mode and put in
   place: 1, or 0 with x->why set and whatever it holds still to be undone.
   Steps 1 to 6 are numbered as `fail_at` counts them. */
static int file_steps(replace_state* x, const char* path, const void* data, size_t size,
                      int use_sync, const struct stat* old, int exists,
                      unsigned fail_at) {
    unsigned k;
    int      fd    = -1;
    int      flags = O_WRONLY | O_CREAT | O_EXCL;
    int      rc;

    /* 1: the temporary, exclusively, so that nobody else's is ever touched */
    if (fail_at == 1u) {
        return file_injected(x, 1u);
    }
#if defined(O_CLOEXEC)
    flags |= O_CLOEXEC;
#endif
    for (k = 0u; k < 100u; ++k) {
        if (!cfi_file_temp_name(path, k, x->tmp, sizeof x->tmp)) {
            (void)cf_fmt(x->why, sizeof x->why, "'%s' is not a regular file", path);
            x->raw = 1;
            return 0;
        }
        fd = open(x->tmp, flags, (mode_t)(exists ? 0600 : 0666));
        if (fd >= 0 || errno != EEXIST) {
            break;
        }
    }
    if (fd < 0) {
        if (errno == EEXIST) {
            (void)cf_fmt(x->why, sizeof x->why,
                         "no temporary name is free next to '%s'", path);
            x->raw = 1;
            return 0;
        }
        return file_failed(x);
    }
    x->made = 1;
#if !defined(O_CLOEXEC)
    (void)fcntl(fd, F_SETFD, FD_CLOEXEC);
#endif
    x->fp = fdopen(fd, "wb");
    if (x->fp == NULL) {
        rc = errno;
        (void)close(fd);
        errno = rc;
        return file_failed(x);
    }

    /* 2: every byte, and out of the stream's buffer */
    if (fail_at == 2u) {
        return file_injected(x, 2u);
    }
    if ((size > 0u && fwrite(data, 1u, size, x->fp) != size) || fflush(x->fp) != 0) {
        return file_failed(x);
    }

    /* 3: the sync the options ask for (the step counts when they ask for none) */
    if (fail_at == 3u) {
        return file_injected(x, 3u);
    }
    if (use_sync && !cfi_file_sync(x->fp)) {
        return file_failed(x);
    }

    /* 4: the original's mode (trivially done when there is no original) */
    if (fail_at == 4u) {
        return file_injected(x, 4u);
    }
    if (exists && fchmod(fileno(x->fp), old->st_mode & 07777) != 0) {
        return file_failed(x);
    }

    /* 5: the close, whose result is checked: a full disk may say so only here */
    rc    = fclose(x->fp);
    x->fp = NULL;
    if (fail_at == 5u) {
        return file_injected(x, 5u);
    }
    if (rc != 0) {
        return file_failed(x);
    }

    /* 6: one rename, and the file is the new one or the old one */
    if (fail_at == 6u) {
        return file_injected(x, 6u);
    }
    if (rename(x->tmp, path) != 0) {
        return file_failed(x);
    }
    x->made = 0;
    return 1;
}

/* After the rename, and not a step: the directory's own entry made durable.
   Every failure is ignored - the file is in place, and saying otherwise would
   tell the caller that nothing changed. `dir` is the caller's scratch (the
   temporary's name, no longer needed), so that the core's frame holds one
   path buffer, not two, under the minimal build's stack budget. */
static void file_sync_directory(const char* path, char* dir, size_t cap) {
    const char* slash = strrchr(path, '/');
    int         fd;

    if (slash == NULL) {
        dir[0] = '.';
        dir[1] = '\0';
    } else {
        size_t n = (slash == path) ? 1u : (size_t)(slash - path);
        if (n >= cap) {
            return;
        }
        (void)memcpy(dir, path, n);
        dir[n] = '\0';
    }
    fd = open(dir, O_RDONLY);
    if (fd >= 0) {
        (void)fsync(fd);
        (void)close(fd);
    }
}

/* The static core of cf_platform_write_file_with. */
static int file_core(const char* path, const void* data, size_t size,
                     const cf_file_options* o, unsigned fail_at, cf_error* err) {
    cf_file_options r;
    replace_state   x;
    struct stat     st;
    int             exists = 0;
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

    /* What is there: a link is the caller's to follow, a directory or a
       device is not ours to replace, and no file at all is fine. */
    if (lstat(path, &st) == 0) {
        if (S_ISLNK(st.st_mode)) {
            (void)cf_error_set(err, CF_E_SOURCE, NULL, path,
                               "'%s' is a symbolic link; write the file it points to",
                               path);
            return 0;
        }
        if (!S_ISREG(st.st_mode)) {
            (void)cf_error_set(err, CF_E_SOURCE, NULL, path,
                               "'%s' is not a regular file", path);
            return 0;
        }
        exists = 1;
    } else if (errno != ENOENT) {
        (void)cfi_file_why(x.why, sizeof x.why);
        (void)cfi_file_fail(err, path, x.why);
        return 0;
    }

    x.fp     = NULL;
    x.made   = 0;
    x.raw    = 0;
    x.tmp[0] = '\0';
    x.why[0] = '\0';
    if (file_steps(&x, path, data, size, use_sync, &st, exists, fail_at)) {
        if (use_sync) {
            file_sync_directory(path, x.tmp, sizeof x.tmp);
        }
        return 1;
    }

    /* Undo: the stream, then the temporary; the old file was never touched. */
    if (x.fp != NULL) {
        (void)fclose(x.fp);
    }
    if (x.made) {
        (void)unlink(x.tmp);
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
    /* The retry of the rename is Windows's: a rename over an open file
       succeeds here. */
    (void)on_retry;
    (void)user;
    return file_core(path, data, size, o, fail_at, err);
}

#endif /* CF_PLATFORM_POSIX */
