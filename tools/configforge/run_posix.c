/*
 * run_posix.c - finds configforge-<command> and replaces this program
 * with it.
 *
 * Beside this program first, then on the PATH. When argv[0] has no `/`
 * the shell found this program on the PATH, and the same search finds
 * its siblings installed beside it. The current directory is searched
 * only where the PATH names it: an empty entry is skipped.
 *
 * The PATH is searched here rather than by execvp, which answers
 * EACCES for a command that is not there when one directory it passed
 * could not be read (a mounted foreign file system does), and "not a
 * command" and "cannot run it" would then be told apart by luck.
 */

/* Before any header: glibc hides execv and access in strict C89. */
#define _POSIX_C_SOURCE 200112L

#include "configforge/cf_platform.h"

#if CF_PLATFORM_POSIX

#include "front.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int out_of_memory(void) {
    fputs("configforge: out of memory\n", stderr);
    return 2;
}

/* A file that may be executed, and is not a directory. */
static int runnable(const char* path) {
    struct stat st;
    if (access(path, X_OK) != 0) {
        return 0;
    }
    if (stat(path, &st) != 0) {
        return 0;
    }
    return S_ISDIR(st.st_mode) ? 0 : 1;
}

/* `<dir>/<program>` in `path`, which is big enough, for the `n` bytes
   of `dir`. */
static void join(char* path, const char* dir, size_t n, const char* program,
                 size_t plen) {
    memcpy(path, dir, n);
    path[n] = '/';
    memcpy(path + n + 1, program, plen + 1);
}

int front_run(const char* program, int argc, char** argv) {
    size_t      plen = strlen(program);
    const char* env  = getenv("PATH");
    const char* slash;
    const char* p;
    char**      args;
    char*       name;
    char*       path;
    int         failed = 0;
    int         i;

    /* program, argv[2] to argv[argc - 1], NULL: argc entries. */
    args = (char**)malloc((size_t)argc * sizeof *args);
    name = (char*)malloc(plen + 1);
    if (env == NULL) {
        env = "/usr/bin:/bin";
    }
    /* Room for the longest directory the PATH or argv[0] can name. */
    path = (char*)malloc(strlen(env) + strlen(argv[0]) + plen + 3);
    if (args == NULL || name == NULL || path == NULL) {
        free(args);
        free(name);
        free(path);
        return out_of_memory();
    }
    memcpy(name, program, plen + 1);
    args[0] = name;
    for (i = 2; i < argc; i++) {
        args[i - 1] = argv[i];
    }
    args[argc - 1] = NULL;

    slash = strrchr(argv[0], '/');
    if (slash != NULL) {
        join(path, argv[0], (size_t)(slash - argv[0]), program, plen);
        if (runnable(path)) {
            execv(path, args);
            failed = 1;
        }
    }
    for (p = env; *p != '\0';) {
        const char* end = strchr(p, ':');
        size_t      n   = (end != NULL) ? (size_t)(end - p) : strlen(p);
        if (n > 0) {
            join(path, p, n, program, plen);
            if (runnable(path)) {
                execv(path, args);
                failed = 1;
            }
        }
        p += n;
        if (*p == ':') {
            p++;
        }
    }

    /* Here only when nothing was run. */
    if (failed) {
        fprintf(stderr, "configforge: cannot run %s\n", program);
    } else {
        fprintf(stderr, "configforge: '%s' is not a configforge command ",
                program + 12);
        fprintf(stderr, "(no %s beside this program or on the PATH); ", program);
        fputs("configforge help lists them\n", stderr);
    }
    free(args);
    free(name);
    free(path);
    return 2;
}

#else

typedef int front_run_posix_unused;

#endif /* CF_PLATFORM_POSIX */
