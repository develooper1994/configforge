/*
 * spawn_none.c - where a program cannot run another (CF_PLATFORM=none,
 * WASI): no generator outside the tree is found, so none is run (D-110).
 */

#include "configforge/cf_platform.h"

#if !CF_PLATFORM_POSIX && !CF_PLATFORM_WIN32

#include "spawn.h"

char* spawn_find(const char* argv0, const char* lang, char** dir) {
    (void)argv0;
    (void)lang;
    *dir = NULL;
    return NULL;
}

int spawn_run(const char* path, int argc, char** argv, const char* const* drop,
              const char* input, size_t n, int* code) {
    (void)path;
    (void)argc;
    (void)argv;
    (void)drop;
    (void)input;
    (void)n;
    *code = 0;
    return 1;
}

#else

typedef int spawn_none_unused;

#endif /* neither platform */
