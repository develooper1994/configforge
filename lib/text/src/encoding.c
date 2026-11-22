/*
 * encoding.c - another encoding recoded to UTF-8, and its names (D-108). C89.
 *
 * The single-byte encodings are three tables of 128 entries, 0x80 to 0xFF to
 * a code point, 0 for no character, written from the Unicode Consortium's
 * mappings (MAPPINGS/ISO8859/8859-9.TXT and MAPPINGS/VENDORS/MICSFT/WINDOWS/
 * CP1252.TXT and CP1254.TXT). ISO-8859-1 is the identity, and needs none.
 * Nothing here guesses and nothing is replaced: a byte with no character is
 * refused where it stands.
 */

#include "configforge/cf_encoding.h"
#include "configforge/cf_error.h"
#include "configforge/cf_port.h"
#include "configforge/cf_text.h"

#include <string.h>

/* Windows-1252. */
static const unsigned short cp1252[128] = {
    0x20AC, 0,      0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, /* 80 */
    0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0,      0x017D, 0,      /* 88 */
    0,      0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, /* 90 */
    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0,      0x017E, 0x0178, /* 98 */
    0x00A0, 0x00A1, 0x00A2, 0x00A3, 0x00A4, 0x00A5, 0x00A6, 0x00A7, /* A0 */
    0x00A8, 0x00A9, 0x00AA, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x00AF, /* A8 */
    0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x00B4, 0x00B5, 0x00B6, 0x00B7, /* B0 */
    0x00B8, 0x00B9, 0x00BA, 0x00BB, 0x00BC, 0x00BD, 0x00BE, 0x00BF, /* B8 */
    0x00C0, 0x00C1, 0x00C2, 0x00C3, 0x00C4, 0x00C5, 0x00C6, 0x00C7, /* C0 */
    0x00C8, 0x00C9, 0x00CA, 0x00CB, 0x00CC, 0x00CD, 0x00CE, 0x00CF, /* C8 */
    0x00D0, 0x00D1, 0x00D2, 0x00D3, 0x00D4, 0x00D5, 0x00D6, 0x00D7, /* D0 */
    0x00D8, 0x00D9, 0x00DA, 0x00DB, 0x00DC, 0x00DD, 0x00DE, 0x00DF, /* D8 */
    0x00E0, 0x00E1, 0x00E2, 0x00E3, 0x00E4, 0x00E5, 0x00E6, 0x00E7, /* E0 */
    0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC, 0x00ED, 0x00EE, 0x00EF, /* E8 */
    0x00F0, 0x00F1, 0x00F2, 0x00F3, 0x00F4, 0x00F5, 0x00F6, 0x00F7, /* F0 */
    0x00F8, 0x00F9, 0x00FA, 0x00FB, 0x00FC, 0x00FD, 0x00FE, 0x00FF  /* F8 */
};

/* Windows-1254: 1252's, but 0x8E and 0x9E have no character, and Turkish at
   0xD0, 0xDD, 0xDE, 0xF0, 0xFD and 0xFE. */
static const unsigned short cp1254[128] = {
    0x20AC, 0,      0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, /* 80 */
    0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0,      0,      0,      /* 88 */
    0,      0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, /* 90 */
    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0,      0,      0x0178, /* 98 */
    0x00A0, 0x00A1, 0x00A2, 0x00A3, 0x00A4, 0x00A5, 0x00A6, 0x00A7, /* A0 */
    0x00A8, 0x00A9, 0x00AA, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x00AF, /* A8 */
    0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x00B4, 0x00B5, 0x00B6, 0x00B7, /* B0 */
    0x00B8, 0x00B9, 0x00BA, 0x00BB, 0x00BC, 0x00BD, 0x00BE, 0x00BF, /* B8 */
    0x00C0, 0x00C1, 0x00C2, 0x00C3, 0x00C4, 0x00C5, 0x00C6, 0x00C7, /* C0 */
    0x00C8, 0x00C9, 0x00CA, 0x00CB, 0x00CC, 0x00CD, 0x00CE, 0x00CF, /* C8 */
    0x011E, 0x00D1, 0x00D2, 0x00D3, 0x00D4, 0x00D5, 0x00D6, 0x00D7, /* D0 */
    0x00D8, 0x00D9, 0x00DA, 0x00DB, 0x00DC, 0x0130, 0x015E, 0x00DF, /* D8 */
    0x00E0, 0x00E1, 0x00E2, 0x00E3, 0x00E4, 0x00E5, 0x00E6, 0x00E7, /* E0 */
    0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC, 0x00ED, 0x00EE, 0x00EF, /* E8 */
    0x011F, 0x00F1, 0x00F2, 0x00F3, 0x00F4, 0x00F5, 0x00F6, 0x00F7, /* F0 */
    0x00F8, 0x00F9, 0x00FA, 0x00FB, 0x00FC, 0x0131, 0x015F, 0x00FF  /* F8 */
};

/* ISO-8859-9: ISO-8859-1's, but for the same six Turkish letters. */
static const unsigned short iso8859_9[128] = {
    0x0080, 0x0081, 0x0082, 0x0083, 0x0084, 0x0085, 0x0086, 0x0087, /* 80 */
    0x0088, 0x0089, 0x008A, 0x008B, 0x008C, 0x008D, 0x008E, 0x008F, /* 88 */
    0x0090, 0x0091, 0x0092, 0x0093, 0x0094, 0x0095, 0x0096, 0x0097, /* 90 */
    0x0098, 0x0099, 0x009A, 0x009B, 0x009C, 0x009D, 0x009E, 0x009F, /* 98 */
    0x00A0, 0x00A1, 0x00A2, 0x00A3, 0x00A4, 0x00A5, 0x00A6, 0x00A7, /* A0 */
    0x00A8, 0x00A9, 0x00AA, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x00AF, /* A8 */
    0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x00B4, 0x00B5, 0x00B6, 0x00B7, /* B0 */
    0x00B8, 0x00B9, 0x00BA, 0x00BB, 0x00BC, 0x00BD, 0x00BE, 0x00BF, /* B8 */
    0x00C0, 0x00C1, 0x00C2, 0x00C3, 0x00C4, 0x00C5, 0x00C6, 0x00C7, /* C0 */
    0x00C8, 0x00C9, 0x00CA, 0x00CB, 0x00CC, 0x00CD, 0x00CE, 0x00CF, /* C8 */
    0x011E, 0x00D1, 0x00D2, 0x00D3, 0x00D4, 0x00D5, 0x00D6, 0x00D7, /* D0 */
    0x00D8, 0x00D9, 0x00DA, 0x00DB, 0x00DC, 0x0130, 0x015E, 0x00DF, /* D8 */
    0x00E0, 0x00E1, 0x00E2, 0x00E3, 0x00E4, 0x00E5, 0x00E6, 0x00E7, /* E0 */
    0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC, 0x00ED, 0x00EE, 0x00EF, /* E8 */
    0x011F, 0x00F1, 0x00F2, 0x00F3, 0x00F4, 0x00F5, 0x00F6, 0x00F7, /* F0 */
    0x00F8, 0x00F9, 0x00FA, 0x00FB, 0x00FC, 0x0131, 0x015F, 0x00FF  /* F8 */
};

typedef struct enc_name {
    const char* name;
    unsigned    flags;
    cf_bool     canonical; /* the name cf_encoding_name gives */
} enc_name;

static const enc_name enc_names[] = {
    {"utf-8", CF_ENCODING_UTF8, CF_TRUE},
    {"iso-8859-1", CF_ENCODING_ISO_8859_1, CF_TRUE},
    {"latin1", CF_ENCODING_ISO_8859_1, CF_FALSE},
    {"windows-1252", CF_ENCODING_WINDOWS_1252, CF_TRUE},
    {"cp1252", CF_ENCODING_WINDOWS_1252, CF_FALSE},
    {"windows-1254", CF_ENCODING_WINDOWS_1254, CF_TRUE},
    {"cp1254", CF_ENCODING_WINDOWS_1254, CF_FALSE},
    {"iso-8859-9", CF_ENCODING_ISO_8859_9, CF_TRUE},
    {"latin5", CF_ENCODING_ISO_8859_9, CF_FALSE},
    {"utf-16le", CF_ENCODING_UTF16LE, CF_TRUE},
    {"utf-16be", CF_ENCODING_UTF16BE, CF_TRUE}};

/* Equal, the first of the two read in ASCII lower case. */
static cf_bool enc_same(const char* a, const char* lower) {
    for (; *a != '\0' && *lower != '\0'; ++a, ++lower) {
        char c = *a;

        if (c >= 'A' && c <= 'Z') {
            c = (char)(c - 'A' + 'a');
        }
        if (c != *lower) {
            return CF_FALSE;
        }
    }
    return (*a == '\0' && *lower == '\0') ? CF_TRUE : CF_FALSE;
}

cf_bool cf_encoding_by_name(const char* name, unsigned* flags) {
    size_t i;

    if (name == NULL || flags == NULL) {
        return CF_FALSE;
    }
    for (i = 0; i < sizeof enc_names / sizeof enc_names[0]; ++i) {
        if (enc_same(name, enc_names[i].name)) {
            *flags = enc_names[i].flags;
            return CF_TRUE;
        }
    }
    return CF_FALSE;
}

const char* cf_encoding_name(unsigned flags) {
    size_t   i;
    unsigned enc = flags & CF_ENCODING_MASK;

    if (enc > (unsigned)CF_ENCODING_LAST) {
        return NULL;
    }
    for (i = 0; i < sizeof enc_names / sizeof enc_names[0]; ++i) {
        if (enc_names[i].canonical && enc_names[i].flags == enc) {
            return enc_names[i].name;
        }
    }
    return NULL;
}

/* A refusal: where, and nothing more. */
static size_t enc_refuse(size_t* bad, size_t at) {
    if (bad != NULL) {
        *bad = at;
    }
    return (size_t)-1;
}

/* One code point at out[*w], the room checked; false when there is none. */
static cf_bool enc_put(cf_u32 cp, char* out, size_t cap, size_t* w) {
    char   four[4];
    size_t k = cf_text_utf8_encode(cp, four);

    if (cap - *w < k) {
        return CF_FALSE;
    }
    memcpy(out + *w, four, k);
    *w += k;
    return CF_TRUE;
}

static size_t enc_single(const unsigned short* table, const unsigned char* u, size_t n,
                         char* out, size_t cap, size_t* bad) {
    size_t i, w = 0;

    for (i = 0; i < n; ++i) {
        cf_u32 cp = u[i];

        if (cp >= 0x80u && table != NULL) {
            cp = table[cp - 0x80u];
            if (cp == 0) {
                return enc_refuse(bad, i);
            }
        }
        if (!enc_put(cp, out, cap, &w)) {
            return enc_refuse(bad, n);
        }
    }
    return w;
}

/* The 16-bit unit at u[i]. */
static cf_u32 enc_unit(const unsigned char* u, size_t i, cf_bool big) {
    return big ? (((cf_u32)u[i] << 8) | (cf_u32)u[i + 1])
               : (((cf_u32)u[i + 1] << 8) | (cf_u32)u[i]);
}

static size_t enc_utf16(const unsigned char* u, size_t n, cf_bool big, char* out,
                        size_t cap, size_t* bad) {
    size_t i = 0, w = 0;

    while (i < n) {
        cf_u32 cp;

        if (n - i < 2) {
            return enc_refuse(bad, i); /* half a code unit */
        }
        cp = enc_unit(u, i, big);
        if (cp >= 0xD800u && cp <= 0xDBFFu) {
            cf_u32 low = (n - i >= 4) ? enc_unit(u, i + 2, big) : 0;

            if (low < 0xDC00u || low > 0xDFFFu) {
                return enc_refuse(bad, i); /* a high surrogate with no low one */
            }
            cp = 0x10000u + ((cp - 0xD800u) << 10) + (low - 0xDC00u);
            i += 4;
        } else if (cp >= 0xDC00u && cp <= 0xDFFFu) {
            return enc_refuse(bad, i); /* a low one alone */
        } else {
            i += 2;
        }
        if (!enc_put(cp, out, cap, &w)) {
            return enc_refuse(bad, n);
        }
    }
    return w;
}

size_t cf_encoding_recode(unsigned from, const char* in, size_t n, char* out,
                          size_t cap, size_t* bad) {
    const unsigned char* u = (const unsigned char*)in;

    switch (from & CF_ENCODING_MASK) {
    case CF_ENCODING_ISO_8859_1:
        return enc_single(NULL, u, n, out, cap, bad);
    case CF_ENCODING_WINDOWS_1252:
        return enc_single(cp1252, u, n, out, cap, bad);
    case CF_ENCODING_WINDOWS_1254:
        return enc_single(cp1254, u, n, out, cap, bad);
    case CF_ENCODING_ISO_8859_9:
        return enc_single(iso8859_9, u, n, out, cap, bad);
    case CF_ENCODING_UTF16LE:
        return enc_utf16(u, n, CF_FALSE, out, cap, bad);
    case CF_ENCODING_UTF16BE:
        return enc_utf16(u, n, CF_TRUE, out, cap, bad);
    default:
        /* UTF-8 is read as it is, and no other value is known. */
        return enc_refuse(bad, n);
    }
}

/* What the bytes of the whole input that are not part of a valid sequence
   look like: 0 for nothing to say, else 1 to 3 as enc_hints is indexed. A
   document that also holds valid multi-byte UTF-8 mixes encodings, and a
   guess at one of them would be a guess. */
static size_t enc_look(const char* p, size_t n) {
    const char* end  = p + n;
    cf_bool     turk = CF_FALSE;
    cf_bool     win  = CF_FALSE;
    size_t      i    = 0;

    while (i < n) {
        unsigned char b = (unsigned char)p[i];
        cf_u32        cp;
        size_t        k;

        if (b < 0x80u) {
            ++i;
            continue;
        }
        k = cf_text_utf8_decode(p + i, end, &cp);
        if (k != 0) {
            return 0; /* a valid multi-byte sequence: it mixes encodings */
        }
        if (b == 0xD0u || b == 0xDDu || b == 0xDEu || b == 0xF0u || b == 0xFDu ||
            b == 0xFEu) {
            turk = CF_TRUE;
        } else if (b <= 0x9Fu) {
            win = CF_TRUE;
        }
        ++i;
    }
    if (turk) {
        return 1;
    }
    return win ? 2 : 3;
}

/* The three encodings a refusal may name: its name, a word after it, and
   the constant that reads it. */
static const struct {
    const char* name;
    const char* note;
    const char* constant;
} enc_hints[] = {{"", "", ""},
                 {"windows-1254", " (Turkish)", "CF_ENCODING_WINDOWS_1254"},
                 {"windows-1252", "", "CF_ENCODING_WINDOWS_1252"},
                 {"iso-8859-1", "", "CF_ENCODING_ISO_8859_1"}};

cf_bool cf_encoding_check(const char* origin, const char* p, size_t n, cf_error* err) {
    static const char digits[] = "0123456789ABCDEF";
    char              hint[200];
    char              hex[3];
    size_t            at, look;
    cf_u32            line = 1, col = 1;

    at = (n == 0) ? 0 : cf_text_utf8_valid(p, n);
    if (at == n) {
        return CF_TRUE;
    }
    if (origin == NULL) {
        origin = "input";
    }
    hex[0] = digits[((unsigned char)p[at] >> 4) & 15u];
    hex[1] = digits[(unsigned char)p[at] & 15u];
    hex[2] = '\0';
    cf_text_place(p, at, 1, &line, &col);
    look    = enc_look(p, n);
    hint[0] = '\0';
    if (look != 0) {
        cf_fmt(hint, sizeof hint,
               "; it may be %s%s: read it with %s, or the encoding %s in the tools",
               enc_hints[look].name, enc_hints[look].note, enc_hints[look].constant,
               enc_hints[look].name);
    }
    return cf_text_fail(err, CF_E_SOURCE, origin, (unsigned int)line, (unsigned int)col,
                        CF_REASON_UTF8, "byte 0x%s is not valid UTF-8%s", hex, hint);
}
