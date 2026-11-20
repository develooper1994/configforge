/*
 * spawn_posix.c - finds configforge-gen-<lang> and runs it with the
 * contract on its standard input (D-110).
 *
 * posix_spawn and not fork and _exit(127): a program that cannot be
 * started is then the call's own error, and no exit code of a generator is
 * mistaken for one. The PATH is searched here, as the front door's
 * run_posix.c does, so "not there" and "cannot run it" are told apart by
 * what was found, not by an errno some directory happened to give.
 */

/* Before any header: glibc hides posix_spawn in strict C89. */
#define _POSIX_C_SOURCE 200112L /* NOLINT(bugprone-reserved-identifier) */

#include "configforge/cf_platform.h"

#if CF_PLATFORM_POSIX

#include "spawn.h"

#include <errno.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

/* A regular file that may be executed. */
static int runnable(const char* path) {
    struct stat st;

    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
        return 0;
    }
    return (access(path, X_OK) == 0) ? 1 : 0;
}

/* `<dir>/<program>`, malloc'd, for the first `n` bytes of `dir`. A directory
   that ends with a `/` gives two, which every system reads as one. */
static char* join(const char* dir, size_t n, const char* program) {
    size_t plen = strlen(program);
    char*  path;

    /* The size is a directory's own, from argv[0] or the PATH. */
    /* NOLINTNEXTLINE(clang-analyzer-optin.taint.TaintedAlloc) */
    path = (char*)malloc(n + 1 + plen + 1);
    if (path == NULL) {
        return NULL;
    }
    (void)sprintf(path, "%.*s/%s", (int)n, dir, program);
    return path;
}

/* The program in `dir[0..n)`: 1 found (and *path set), 0 not there, -1 no
   room. */
static int try_dir(const char* dir, size_t n, const char* program, char** path) {
    char* candidate = join(dir, n, program);

    if (candidate == NULL) {
        return -1;
    }
    if (!runnable(candidate)) {
        free(candidate);
        return 0;
    }
    *path = candidate;
    return 1;
}

char* spawn_find(const char* argv0, const char* lang, char** dir) {
    static const char prefix[] = "configforge-gen-";
    size_t            llen     = strlen(lang);
    char*             program  = (char*)malloc(sizeof prefix + llen);
    const char*       slash    = strrchr(argv0, '/');
    const char*       env      = getenv("PATH");
    char*             found    = NULL;
    size_t            pos;
    size_t            elen;

    *dir = NULL;
    if (program == NULL) {
        return NULL;
    }
    memcpy(program, prefix, sizeof prefix - 1);
    memcpy(program + sizeof prefix - 1, lang, llen + 1);
    if (slash != NULL) {
        /* The program's own directory; "/" for one directly under it. */
        size_t n = (slash == argv0) ? 1 : (size_t)(slash - argv0);

        *dir = (char*)malloc(n + 1);
        if (*dir != NULL) {
            memcpy(*dir, argv0, n);
            (*dir)[n] = '\0';
            if (try_dir(*dir, n, program, &found) != 0) {
                free(program);
                if (found != NULL) {
                    free(*dir);
                    *dir = NULL;
                }
                return found;
            }
        }
    }
    if (env == NULL) {
        env = "/usr/bin:/bin";
    }
    elen = strlen(env);
    for (pos = 0; pos < elen;) {
        const char* p   = env + pos;
        const char* end = strchr(p, ':');
        size_t      n   = (end != NULL) ? (size_t)(end - p) : strlen(p);

        /* An empty entry is the current directory, which is never searched. */
        if (n > 0 && try_dir(p, n, program, &found) != 0) {
            break;
        }
        if (end == NULL) {
            break;
        }
        pos = (size_t)(end - env) + 1;
    }
    free(program);
    if (found != NULL) {
        free(*dir);
        *dir = NULL;
    }
    return found;
}

/* What build_args made: the copy of the path it holds first, and the array. */
static void free_args(char** args) {
    free(args[0]);
    free((void*)args);
}

/* The arguments to pass: `path`, then argv[1..argc-1] but the dropped
   names, each with the argument after it; NULL-terminated, malloc'd. */
static char** build_args(const char* path, int argc, char** argv,
                         const char* const* drop) {
    char** args = (char**)malloc(((size_t)argc + 2) * sizeof *args);
    char*  copy = (char*)malloc(strlen(path) + 1);
    size_t at   = 0;
    int    i;

    if (args == NULL || copy == NULL) {
        free((void*)args);
        free(copy);
        return NULL;
    }
    memcpy(copy, path, strlen(path) + 1);
    args[at++] = copy;
    for (i = 1; i < argc; i += 2) {
        const char* const* d;
        int                dropped = 0;

        for (d = drop; *d != NULL; ++d) {
            if (strcmp(argv[i], *d) == 0) {
                dropped = 1;
            }
        }
        if (!dropped) {
            args[at++] = argv[i];
            if (i + 1 < argc) {
                args[at++] = argv[i + 1];
            }
        }
    }
    args[at] = NULL;
    return args;
}

/* `n` bytes to `fd`, over interrupted and short writes; a reader that has
   gone is not an error here: the generator's own code says how it went. */
static void write_all(int fd, const char* p, size_t n) {
    size_t off = 0;

    while (off < n) {
        ssize_t w = write(fd, p + off, n - off);

        if (w < 0 && errno == EINTR) {
            continue;
        }
        if (w <= 0) {
            return;
        }
        off += (size_t)w;
    }
}

int spawn_run(const char* path, int argc, char** argv, const char* const* drop,
              const char* input, size_t n, int* code) {
    char**                     args = build_args(path, argc, argv, drop);
    posix_spawn_file_actions_t fa;
    void (*old_pipe)(int);
    pid_t pid = 0;
    int   fds[2];
    int   status = 0;
    int   rc;
    int   w;

    if (args == NULL) {
        return 1;
    }
    if (pipe(fds) != 0) {
        free_args(args);
        return 1;
    }
    if (posix_spawn_file_actions_init(&fa) != 0) {
        (void)close(fds[0]);
        (void)close(fds[1]);
        free_args(args);
        return 1;
    }
    rc = posix_spawn_file_actions_adddup2(&fa, fds[0], 0);
    if (rc == 0) {
        rc = posix_spawn_file_actions_addclose(&fa, fds[0]);
    }
    if (rc == 0) {
        rc = posix_spawn_file_actions_addclose(&fa, fds[1]);
    }
    if (rc == 0) {
        rc = posix_spawn(&pid, path, &fa, NULL, args, environ);
    }
    (void)posix_spawn_file_actions_destroy(&fa);
    free_args(args);
    (void)close(fds[0]);
    if (rc != 0) {
        (void)close(fds[1]);
        return 1;
    }

    /* The child has the default SIGPIPE; here a closed pipe is an error
       code, dropped, and not a signal. */
    old_pipe = signal(SIGPIPE, SIG_IGN);
    write_all(fds[1], input, n);
    (void)close(fds[1]);
    if (old_pipe != SIG_ERR) {
        (void)signal(SIGPIPE, old_pipe);
    }

    do {
        w = waitpid(pid, &status, 0);
    } while (w < 0 && errno == EINTR);
    if (w < 0) {
        return 1;
    }
    if (WIFEXITED(status)) {
        *code = WEXITSTATUS(status);
        return 0;
    }
    *code = WIFSIGNALED(status) ? WTERMSIG(status) : 0;
    return 2;
}

#else

typedef int spawn_posix_unused;

#endif /* CF_PLATFORM_POSIX */
