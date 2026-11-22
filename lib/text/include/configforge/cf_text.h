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

/* One letter after the backslash that stands for one code point. */
typedef struct cf_text_esc_simple {
    char   letter;
    cf_u32 cp;
} cf_text_esc_simple;

/* Where a refusal of the digits is placed, as an offset from the
   letter: at the letter, at the first digit, or at the first byte
   that is not a digit (or the end of the input). */
enum { CF_ESC_AT_LETTER = 0, CF_ESC_AT_DIGITS = 1, CF_ESC_AT_BAD = 2 };

/* Family flags. PAIRS: a four-digit escape is UTF-16, so a high half must
   be followed by a second four-digit escape that is a low half, and the
   two make one code point, and a lone half is refused. NO_OCTAL: '0' not before a
   decimal digit is U+0000, and any digit '1' to '9' (or '0' before a digit) is refused,
   as ECMAScript's octal escapes are in JSON5. */
enum { CF_ESC_PAIRS = 1u, CF_ESC_NO_OCTAL = 2u };
enum { CF_ESC_NL_NONE = 0, CF_ESC_NL_JSON5, CF_ESC_NL_ANY };

typedef struct cf_text_esc_family {
    const cf_text_esc_simple* simple; /* ended by letter 0 */
    const char*               hex2;   /* letters taking 2 digits: "x" or "" */
    const char*               hex4;   /* ... 4: "u" */
    const char*               hex8;   /* ... 8: "U" or "" */
    unsigned                  flags;  /* CF_ESC_PAIRS, CF_ESC_NO_OCTAL */
    unsigned                  place;  /* CF_ESC_AT_* */
    unsigned                  breaks; /* CF_ESC_NL_*: the escaped line
                                         breaks the family has */
} cf_text_esc_family;

enum {
    CF_ESC_CP = 0,      /* cp and used are set */
    CF_ESC_BREAK,       /* an escaped line break; used is its length */
    CF_ESC_OTHER,       /* no escape of this family: the caller decides */
    CF_ESC_E_DIGITS,    /* too few or non-hexadecimal digits; letter is set */
    CF_ESC_E_HIGH,      /* a high half with no low half after it */
    CF_ESC_E_LOW,       /* a low half alone */
    CF_ESC_E_SCALAR,    /* a surrogate or a value above U+10FFFF */
    CF_ESC_E_NUL_DIGIT, /* '0' before a decimal digit (NO_OCTAL) */
    CF_ESC_E_DIGIT      /* '1' to '9' (NO_OCTAL) */
};

typedef struct cf_text_esc {
    int    kind;
    cf_u32 cp;
    size_t used;   /* CF_ESC_CP, CF_ESC_BREAK: bytes from the letter, it
                      included; 0 for the others */
    size_t at;     /* a refusal: offset from the letter of its place */
    char   letter; /* CF_ESC_E_DIGITS: the letter, for the message */
} cf_text_esc;

/* What to escape on writing. `named` is pairs of bytes: the byte, and
   the letter written after the backslash for it. Every other code
   point below `below`, and each from `from` to `to` inclusive
   (from > to: none; `to` at most 0xFFFF), is written as a backslash, u
   and four hexadecimal digits - or, with CF_ESCAPER_X and below 0x100,
   as a backslash, x and two. With CF_ESCAPER_NONCHARS U+FFFE and U+FFFF
   are written with the four digits too. Hex digits are lowercase. */
enum { CF_ESCAPER_X = 1u, CF_ESCAPER_NONCHARS = 2u };
typedef struct cf_text_escaper {
    const char* named;
    cf_u32      below;
    cf_u32      from, to;
    unsigned    flags;
} cf_text_escaper;

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

/* Decodes the escape whose letter is at p (the byte after the
   backslash), reading nothing at or past end. Moves nothing. */
CF_API void cf_text_esc_decode(const cf_text_esc_family* f, const char* p,
                               const char* end, cf_text_esc* r);

/* The escape for cp into out (at most 6 bytes are written; out holds
   8) and its length, or 0 when cp is written as it is. */
CF_API size_t cf_text_escape(const cf_text_escaper* e, cf_u32 cp, char out[8]);

/* The "${" of a reference. How many of them n bytes hold. */
CF_API size_t cf_text_ref_count(const char* p, size_t n);
/* The index of the '$' of the first "${" at or after `from`, or n. */
CF_API size_t cf_text_ref_next(const char* p, size_t n, size_t from);
/* "$${" read as "${", into out, which holds n bytes; the length. */
CF_API size_t cf_text_ref_unescape(const char* p, size_t n, char* out);
/* buf[from..end) widened in place, each "${" written "$${": from the
   back, with extra = cf_text_ref_count(buf + from, end - from); a
   byte is stored only where its new place is below `limit`. */
CF_API void cf_text_ref_widen(char* buf, size_t from, size_t end, size_t limit);

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
