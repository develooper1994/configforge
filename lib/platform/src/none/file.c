/*
 * file.c (none) - opening a file by a UTF-8 path, on a target with no
 * platform; and the file layer's calls, which exist (D-220) and refuse.
 *
 * Apart from none.c so that the serialization library, which opens
 * files and calls nothing else of the platform, carries this file
 * without it (docs/Architecture.md, "Layers").
 *
 * Guarded once at the top, for the reason given in posix.c.
 */

#include "configforge/cf_platform_file.h"

#if CF_PLATFORM_NONE

#include "configforge/cf_error.h"
#include "configforge/cf_file.h"
#include "../file_prim.h"

#ifndef CF_NO_STDIO
#include <stdio.h>
#endif

void* cf_platform_fopen(const char* path, const char* mode) {
#ifndef CF_NO_STDIO
    /* No operating system, but there may still be a C library with
       stdio; a target without one builds with CF_NO_STDIO. */
    return (path != NULL && mode != NULL) ? fopen(path, mode) : NULL;
#else
    (void)path;
    (void)mode;
    return NULL;
#endif
}

cf_bool cf_platform_has_files(void) {
#ifndef CF_NO_STDIO
    return CF_TRUE;
#else
    return CF_FALSE;
#endif
}

/* No files to write: the program built for this target still links, and
   every call says what it lacks. */

int cf_platform_write_file_with(const char* path, const void* data, size_t size,
                                const cf_file_options* o, unsigned  fail_at,
                                void (*on_retry)(void* user), void* user,
                                cf_error* err) {
    (void)data;
    (void)size;
    (void)o;
    (void)fail_at;
    (void)on_retry;
    (void)user;
    cf_error_clear(err);
    (void)cf_error_set(err, CF_E_CAPABILITY, NULL, path,
                       "this build has no files to write");
    return 0;
}

int cf_platform_write_file_if_with(const char* path, const void* data, size_t size,
                                   const void* expect, size_t expect_size,
                                   const cf_file_options* o, unsigned fail_at,
                                   void (*between)(void* user), void* user,
                                   cf_error* err) {
    (void)expect;
    (void)expect_size;
    (void)between;
    (void)user;
    return cf_platform_write_file_with(path, data, size, o, fail_at, NULL, NULL, err);
}

unsigned cf_platform_write_steps(void) {
    return 0u;
}

unsigned long cf_platform_now_ms(void) {
    return (unsigned long)-1;
}

cf_bool cfi_file_sync(void* stream) {
    (void)stream;
    return CF_FALSE;
}

const char* cfi_file_why(char* buf, size_t cap) {
    if (buf != NULL && cap > 0u) {
        buf[0] = '\0';
    }
    return "this build has no files to write";
}

#endif /* CF_PLATFORM_NONE */
