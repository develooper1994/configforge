/*
 * cf_text.h - the text helpers every reader shares: UTF-8 decoded,
 * validated and encoded, a hexadecimal digit read, and the hash of a
 * reader's name table (D-102).
 *
 * INTERNAL (D-51): exported for the tests, and outside the 1.x promise - a
 * minor release may change it. Its functions are declared only under
 * CF_INTERNAL_API (D-51, D-88). A program uses cf_text_is_utf8 from
 * cf_serialize.h instead.
 */

#ifndef CONFIGFORGE_CF_TEXT_H
#define CONFIGFORGE_CF_TEXT_H

#include "configforge/cf_port.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CF_INTERNAL_API

/* The code point at p, and its length, or 0 when p does not start a
   whole, valid sequence before end, and 0 when p == end. *cp is written
   only when the length is not 0. Valid is RFC 3629's: no overlong form,
   no surrogate, nothing above U+10FFFF. */
CF_API size_t cf_text_utf8_decode(const char* p, const char* end, cf_u32* cp);

/* The length of the longest valid prefix of p[0..n), through lanes_scalar(). */
CF_API size_t cf_text_utf8_valid(const char* p, size_t n);

/* Writes cp into out[0..3] and returns 1 to 4; the caller has refused
   surrogates and values above 0x10FFFF. */
CF_API size_t cf_text_utf8_encode(cf_u32 cp, char out[4]);

/* 0-15 for a hex digit of either case, -1 otherwise. */
CF_API int cf_text_hex_value(int c);

/* The seed the readers' name tables use: made of two addresses, `where`
   and a local's, and never 0. */
CF_API size_t cf_text_seed(const void* where);

/* FNV-1a of n bytes from seed, in a size_t. The same in 32 bits agrees
   with its low 32 bits, so a bucket chosen by the low bits is the same. */
CF_API size_t cf_text_hash(size_t seed, const char* s, size_t n);

/* The line and column, both from 1, of the byte at offset `at` of p, in the
   original's units (D-108): bytes for `unit` 1, two-byte code units for
   CF_ENCODING_UTF16LE or CF_ENCODING_UTF16BE, a line ending at each LF
   unit. For a refusal of bytes that are not UTF-8 (A-5) and of a byte with
   no character in the encoding named (A-4). */
CF_API void cf_text_place(const char* p, size_t at, unsigned unit, cf_u32* line,
                          cf_u32* col);

#endif /* CF_INTERNAL_API */

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_CF_TEXT_H */
