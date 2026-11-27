/*
 * cf_file.h - the file layer: a file replaced whole or not at all, and a
 * writer for a program that writes continuously (D-224, D-127).
 *
 * It knows nothing of configuration and any program can use it alone:
 * a write that goes through here is never half a file that the next
 * start reads. Three layers, each usable without the one above:
 *
 *   cf_file_options  how a file is written: buffering, durability, and the
 *                    wait of a rename on Windows. Every setting has a
 *                    default, and a NULL `options` anywhere below is those
 *                    defaults.
 *   cf_file_writer   streaming, in place, no temporary: records, logs.
 *   cf_file_replace  a file made to hold exactly the bytes given, by a
 *                    temporary in the same directory, synced once and put
 *                    in place by one rename.
 *
 * (The third, cf_emit_file, writes a configuration and is cf_emit.h's.)
 *
 * Buffering is C's setvbuf, and durability is the platform's own sync
 * (fsync, FlushFileBuffers). Paths are UTF-8 everywhere (D-15). A build
 * for the `none` platform has no files to write: every call but
 * cf_file_options_init fails with CF_E_CAPABILITY, `this build has no
 * files to write`, and a program built for it still links.
 */

#ifndef CONFIGFORGE_CF_FILE_H
#define CONFIGFORGE_CF_FILE_H

#include "configforge/cf_error.h"
#include "configforge/cf_port.h"

#ifdef __cplusplus
extern "C" {
#endif

/* How a file's stream is buffered. CF_BUF_PLATFORM leaves what the C
   library does alone (setvbuf is not called); the others are setvbuf's
   _IOFBF, _IOLBF and _IONBF. */
typedef enum cf_buf_mode {
    CF_BUF_PLATFORM = 0,
    CF_BUF_FULL,
    CF_BUF_LINE,
    CF_BUF_NONE
} cf_buf_mode;

/* When the bytes are forced to the device.
     CF_SYNC_DEFAULT   the layer's own: cf_file_replace takes CF_SYNC_COMMIT,
                       so that "whole or not at all" holds across a power
                       loss, as SQLite and Git do; cf_file_writer takes
                       CF_SYNC_PLATFORM.
     CF_SYNC_PLATFORM  forces nothing: the system writes when it does.
     CF_SYNC_COMMIT    once, before the file is closed (for cf_file_replace,
                       before the rename).
     CF_SYNC_EVERY     when sync_bytes bytes or sync_ms milliseconds have
                       passed since the last sync, checked at each write,
                       whichever comes first; a trigger of 0 never fires.
                       The tail still unsynced at the close is synced then.
     CF_SYNC_EACH      after every write.
   cf_file_replace writes its bytes in one go, so CF_SYNC_EVERY and
   CF_SYNC_EACH act as CF_SYNC_COMMIT there. */
typedef enum cf_sync_mode {
    CF_SYNC_DEFAULT = 0,
    CF_SYNC_PLATFORM,
    CF_SYNC_COMMIT,
    CF_SYNC_EVERY,
    CF_SYNC_EACH
} cf_sync_mode;

typedef struct cf_file_options {
    size_t        size;      /* sizeof(cf_file_options): set by cf_file_options_init */
    cf_buf_mode   buffering; /* CF_BUF_PLATFORM: setvbuf is not called */
    size_t        buffer_size; /* 0: BUFSIZ; used with CF_BUF_FULL and CF_BUF_LINE */
    cf_sync_mode  sync;        /* CF_SYNC_DEFAULT: the layer's own (above) */
    unsigned long sync_bytes;  /* CF_SYNC_EVERY: after this many bytes; 0: no byte
                                  trigger; default 1048576 */
    unsigned long sync_ms;  /* CF_SYNC_EVERY: after this many milliseconds; 0: no time
                               trigger; default 1000 */
    unsigned long retry_ms; /* Windows: how long a rename waits for another handle on
                               the target; POSIX: ignored; default 2000 */
    cf_bool append; /* cf_file_writer only: append to the file instead of truncating it;
                       default CF_FALSE */
} cf_file_options;

/* Sets every member to its default and `size`. A NULL `o` does nothing, and
   a NULL `options` in any call below is these defaults. A `size` below the
   members above is CF_E_SOURCE, `file options of an unknown size`.
   `sync_ms` needs a clock: a platform with none refuses a writer that asks
   for CF_SYNC_EVERY with sync_ms != 0 (CF_E_CAPABILITY, `this platform has
   no clock for sync_ms`). `buffer_size` above 0 is a buffer taken from
   cf_default_allocator() and released at the close; a build with no
   allocator refuses it (CF_E_CAPABILITY). */
CF_API void cf_file_options_init(cf_file_options* o);

/* ------------------------------------------------------------------ *
 * The writer: streaming, in place
 *
 * cf_file_writer_open opens `path` for writing (truncated, or appended
 * to with options->append) - no temporary: a crash leaves what was
 * written. A failure of any call is CF_E_SOURCE, `cannot write '%s':
 * <reason>`, source = path. One writer, one thread (D-128).
 * ------------------------------------------------------------------ */

typedef struct cf_file_writer cf_file_writer;

/* clang-format off */
CF_NODISCARD CF_API cf_file_writer* cf_file_writer_open(const char* path, const cf_file_options* o, cf_error* err);
/* clang-format on */
CF_NODISCARD CF_API cf_bool cf_file_writer_write(cf_file_writer* w, const void* data,
                                                 size_t size, cf_error* err);
/* To the system, not forced to the device. */
CF_NODISCARD CF_API cf_bool cf_file_writer_flush(cf_file_writer* w, cf_error* err);
/* Flush, then forced to the device. */
CF_NODISCARD CF_API cf_bool cf_file_writer_sync(cf_file_writer* w, cf_error* err);
/* The last flush, the sync the policy asks for, the close. Always frees
   `w`, whether it returns true or not; NULL is fine. */
CF_NODISCARD CF_API cf_bool cf_file_writer_close(cf_file_writer* w, cf_error* err);

/* ------------------------------------------------------------------ *
 * The replace: a file whole or not at all
 *
 * True when `path` holds exactly `size` bytes of `data`; false with `err`
 * set (CF_E_SOURCE, source = path) and the old file, or its absence, as it
 * was, and no temporary left. The temporary is `.<name>.cf-tmp-<k>` in the
 * same directory, written, synced (by default: see CF_SYNC_DEFAULT) and put
 * in place by one rename. On Windows a rename fails while another handle
 * holds the target without delete sharing, and is retried for `retry_ms`.
 *
 * A symbolic link is refused (name the file it points to), as is a path
 * that is not a regular file and, on Windows, a read-only one. The mode of
 * the file replaced is kept on POSIX; on Windows no attribute or ACL is
 * copied and the new file takes the directory's.
 * ------------------------------------------------------------------ */

CF_NODISCARD CF_API cf_bool cf_file_replace(const char* path, const void* data,
                                            size_t size, const cf_file_options* o,
                                            cf_error* err);

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_CF_FILE_H */
