/*
 * cf_encoding.h - another encoding, read only by name (D-108).
 *
 * Every text inside the library is UTF-8. A file in another encoding is
 * read when the program names the encoding: a CF_ENCODING_* value in the
 * flags of its source, or its name in the tools and the bindings. The
 * bytes are recoded to UTF-8 as they are read. A byte-order mark is a
 * declaration, not a guess: UTF-16's, in either order, makes the file
 * UTF-16.
 *
 * Public, so that the serialization library's own program reaches it as
 * the configuration library does.
 */

#ifndef CONFIGFORGE_CF_ENCODING_H
#define CONFIGFORGE_CF_ENCODING_H

#include "configforge/cf_port.h"
#include "configforge/cf_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The encoding of a source, in bits 24 to 27 of its flags. UTF-8, the
   default, is 0: a source with no encoding named reads UTF-8. */
#define CF_ENCODING_SHIFT 24
#define CF_ENCODING_MASK (15u << CF_ENCODING_SHIFT)
enum {
    CF_ENCODING_UTF8         = 0u, /* the default */
    CF_ENCODING_ISO_8859_1   = 1u << CF_ENCODING_SHIFT,
    CF_ENCODING_WINDOWS_1252 = 2u << CF_ENCODING_SHIFT,
    CF_ENCODING_WINDOWS_1254 = 3u << CF_ENCODING_SHIFT,
    CF_ENCODING_ISO_8859_9   = 4u << CF_ENCODING_SHIFT,
    CF_ENCODING_UTF16LE      = 5u << CF_ENCODING_SHIFT,
    CF_ENCODING_UTF16BE      = 6u << CF_ENCODING_SHIFT,
    CF_ENCODING_LAST         = CF_ENCODING_UTF16BE
};

/* CF_TRUE and *flags the encoding's value for one of: "utf-8",
   "iso-8859-1" or "latin1", "windows-1252" or "cp1252",
   "windows-1254" or "cp1254", "iso-8859-9" or "latin5",
   "utf-16le", "utf-16be"; ASCII case ignored. CF_FALSE otherwise.
   Guarantee: strong. `flags` is written only when the name is known. */
CF_NODISCARD CF_API cf_bool cf_encoding_by_name(const char* name, unsigned* flags);

/* The canonical name ("windows-1254") of the encoding in `flags`,
   or NULL for a value above CF_ENCODING_LAST.
   Guarantee: nothrow. NULL for a value above CF_ENCODING_LAST. */
CF_API const char* cf_encoding_name(unsigned flags);

/* Recodes n bytes of `from` (any value but CF_ENCODING_UTF8) into
   UTF-8 at out, which holds cap bytes. Returns the bytes written, or
   (size_t)-1 with *bad the offset of the first byte of what has no
   character: Windows-1252's 0x81, 0x8D, 0x8F, 0x90, 0x9D;
   Windows-1254's those and 0x8E, 0x9E; an unpaired UTF-16
   surrogate; a last half code unit. Enough room: 3 * n for the
   single-byte encodings (0x80 to 0x9F of the Windows pages are
   mostly U+20xx, three bytes), 3 * (n / 2) for UTF-16. With less
   room it returns (size_t)-1 with *bad == n, an offset no byte has,
   having written nothing past cap.
   Guarantee: strong. Only `out` and `*bad` are written; `out` is unspecified after a
   failure, and nothing is written past `cap`. */
CF_API size_t cf_encoding_recode(unsigned from, const char* in, size_t n, char* out,
                                 size_t cap, size_t* bad);

/* CF_TRUE when the n bytes at p are UTF-8. Otherwise CF_FALSE, and
   err set (CF_E_SOURCE) to
   "<origin>:<line>:<column>: byte 0x<XX> is not valid UTF-8<hint>",
   the place of the first byte that is not, counted as every reader
   counts (lines by LF, columns in bytes from 1). The hint, when the bytes
   that are not part of a valid sequence look like another encoding and
   the input has no multi-byte UTF-8 of its own, says which and how to
   read it so: "; it may be windows-1254 (Turkish): read it with
   CF_ENCODING_WINDOWS_1254, or the encoding windows-1254 in the tools".
   It never changes a value. A NULL origin is "input". (D-108)
   Guarantee: strong. The input is never changed; on a refusal only `err` is written. */
CF_NODISCARD CF_API cf_bool cf_encoding_check(const char* origin, const char* p,
                                              size_t n, cf_error* err);

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_CF_ENCODING_H */
