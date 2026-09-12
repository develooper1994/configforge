/*
 * environ.c - reaching the platform environment.
 *
 * Isolated in its own file because it is the one place in the source
 * layer that has to know what a platform is. Everything else takes the
 * environment as a parameter.
 */

#include "configforge/cf_source.h"

#if defined(CF_NO_ENVIRON)

char* const* cf_environ(void) {
    return NULL;
}

#elif defined(_WIN32)

#include <stdlib.h>
extern char** _environ;

char* const* cf_environ(void) {
    return (char* const*)_environ;
}

#else

/* POSIX. Not C89, which is why it is declared here rather than
   included from a header the core would otherwise have to know about. */
extern char** environ;

char* const* cf_environ(void) {
    return (char* const*)environ;
}

#endif
