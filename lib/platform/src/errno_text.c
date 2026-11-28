/*
 * errno_text.c - the text of a system error from a table of our own
 * (K-9k, D-239), in place of the C library's text, whose buffer threads may share
 * (CERT CON33-C). The same text on every platform and in every locale.
 *
 * Guarantee: nothrow, Thread: safe, Signal: safe, Alloc: none. It reads
 * a constant table and writes `buf`; it holds no state, lock or allocator.
 * C89.
 */

#include "configforge/cf_build_options.h"
#include "configforge/cf_port.h"
#include "file_prim.h"

#include <stddef.h>

#ifndef CF_NO_STDIO
#include <errno.h>
#endif

#ifndef CF_NO_STDIO
typedef struct cfi_errno_row {
    int         code;
    const char* text;
} cfi_errno_row;

/* Lower case, no final stop, as the library's own messages are. A code the
   libc lacks leaves its row out. */
static const cfi_errno_row cfi_errno_rows[] = {
#ifdef ENOENT
    {ENOENT, "no such file or directory"},
#endif
#ifdef EACCES
    {EACCES, "permission denied"},
#endif
#ifdef EEXIST
    {EEXIST, "file exists"},
#endif
#ifdef ENOTDIR
    {ENOTDIR, "not a directory"},
#endif
#ifdef EISDIR
    {EISDIR, "is a directory"},
#endif
#ifdef ENOSPC
    {ENOSPC, "no space left on device"},
#endif
#ifdef EROFS
    {EROFS, "read-only file system"},
#endif
#ifdef EMFILE
    {EMFILE, "too many open files"},
#endif
#ifdef ENAMETOOLONG
    {ENAMETOOLONG, "file name too long"},
#endif
#ifdef EINVAL
    {EINVAL, "invalid argument"},
#endif
#ifdef EIO
    {EIO, "input/output error"},
#endif
#ifdef ENOMEM
    {ENOMEM, "out of memory"},
#endif
#ifdef EPERM
    {EPERM, "operation not permitted"},
#endif
#ifdef EBUSY
    {EBUSY, "device or resource busy"},
#endif
#ifdef EXDEV
    {EXDEV, "cross-device link"},
#endif
#ifdef ELOOP
    {ELOOP, "too many levels of symbolic links"},
#endif
    {0, NULL}};
#endif /* CF_NO_STDIO */

const char* cfi_errno_text(int code, char* buf, size_t cap) {
#ifndef CF_NO_STDIO
    size_t i;

    for (i = 0u; cfi_errno_rows[i].text != NULL; ++i) {
        if (cfi_errno_rows[i].code == code) {
            (void)cf_fmt(buf, cap, "%s", cfi_errno_rows[i].text);
            return buf;
        }
    }
#endif
    (void)cf_fmt(buf, cap, "error %d", code);
    return buf;
}
