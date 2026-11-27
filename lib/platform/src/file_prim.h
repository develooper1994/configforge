/*
 * file_prim.h - what the file layer's platform-independent part
 * (file_layer.c) and the three platform files (posix/file.c, win32/file.c,
 * none/file.c) share. Private to lib/platform/src: no CF_API, not
 * installed, not a promise.
 *
 * The streams are void*, as cf_platform_fopen's are, so that this header
 * does not include <stdio.h> for the one build that has none.
 */

#ifndef CONFIGFORGE_FILE_PRIM_H
#define CONFIGFORGE_FILE_PRIM_H

#include <stddef.h>

#include "configforge/cf_error.h"
#include "configforge/cf_file.h"
#include "configforge/cf_port.h"

/* Where cf_file_options ends in 1.9: a smaller `size` is refused. */
#define CFI_FILE_OPTIONS_FLOOR (offsetof(cf_file_options, append) + sizeof(cf_bool))

/* ---- file_layer.c ---- */

/* `o` (NULL: the defaults) into `out`, member by member and as far as
   o->size reaches; false, with err set, when it is below the floor or a
   member holds no such mode. `path` is the error's source. */
cf_bool cfi_file_options(const cf_file_options* o, cf_file_options* out,
                         const char* path, cf_error* err);

/* CF_E_SOURCE, `cannot write '<path>': <why>`; returns CF_FALSE. */
cf_bool cfi_file_fail(cf_error* err, const char* path, const char* why);

/* `<dir>/.<name>.cf-tmp-<k>` (k below 100) for `path`, in `out`; false when
   `path` names no file (empty, or ends in a separator) or the name does not
   fit in `cap`. */
cf_bool cfi_file_temp_name(const char* path, unsigned k, char* out, size_t cap);

/* `<dir>/.<name>.cf-lock` for `path`, in `out`: the sidecar's name (D-227); false
   as cfi_file_temp_name is. */
cf_bool cfi_file_lock_name(const char* path, char* out, size_t cap);

/* strerror(errno), into `buf`, for the failure of a C library call. */
const char* cfi_file_why_stdio(char* buf, size_t cap);

/* What a replace checks just before it puts the temporary in place
   (cf_platform_write_file_if, D-186): `expect` is what the file must still
   hold (NULL: the file must not exist), `between(user)` is called once before
   the check when not NULL. */
typedef struct cfi_file_guard {
    const void* expect;
    size_t      expect_size;
    void (*between)(void* user);
    void* user;
} cfi_file_guard;

/* True when `path` can be read and holds exactly `size` bytes of `expect`
   (a NULL `expect` with `size` 0 is an empty file). False for anything
   else, a file that cannot be read included. */
cf_bool cfi_file_holds(const char* path, const void* expect, size_t size);

/* ---- posix/file.c, win32/file.c, none/file.c ---- */

/* The stream's file forced to the device: true, or false with the reason
   in cfi_file_why. */
cf_bool cfi_file_sync(void* stream);

/* The reason of the last failure of a system call (not of the C library's:
   errno's text is cfi_file_why_stdio): POSIX strerror(errno), Windows
   `error <GetLastError()>`. */
const char* cfi_file_why(char* buf, size_t cap);

#endif /* CONFIGFORGE_FILE_PRIM_H */
