/*
 * test_encoding.c - another encoding, read only by name (D-108, rules 3 and 4).
 *
 * Part one is cf_encoding_recode and cf_encoding_by_name alone: every
 * table at every position where it differs from ISO-8859-1 and at every
 * position with no character, UTF-16 in both orders. Part two goes
 * through the builders: a Windows-1254 file read by name, a UTF-16 file
 * by its mark, and what is refused.
 */

#define _POSIX_C_SOURCE 200112L /* setenv */

#include <configforge/configforge.h>
#include "configforge/cf_encoding.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cf_test.h"

/* One byte, its code point where it differs from ISO-8859-1. */
typedef struct diff {
    unsigned char byte;
    cf_u32        cp;
} diff;

/* Windows-1252, 0x80 to 0x9F where it has a character (the rest of the
   page is ISO-8859-1's), as CP1252.TXT gives them. */
static const diff d1252[] = {
    {0x80, 0x20AC}, {0x82, 0x201A}, {0x83, 0x0192}, {0x84, 0x201E}, {0x85, 0x2026},
    {0x86, 0x2020}, {0x87, 0x2021}, {0x88, 0x02C6}, {0x89, 0x2030}, {0x8A, 0x0160},
    {0x8B, 0x2039}, {0x8C, 0x0152}, {0x8E, 0x017D}, {0x91, 0x2018}, {0x92, 0x2019},
    {0x93, 0x201C}, {0x94, 0x201D}, {0x95, 0x2022}, {0x96, 0x2013}, {0x97, 0x2014},
    {0x98, 0x02DC}, {0x99, 0x2122}, {0x9A, 0x0161}, {0x9B, 0x203A}, {0x9C, 0x0153},
    {0x9E, 0x017E}, {0x9F, 0x0178}};
static const unsigned char n1252[] = {0x81, 0x8D, 0x8F, 0x90, 0x9D};

/* Windows-1254: 1252's, but for 0x8E and 0x9E, and Turkish at six places. */
static const diff d1254[] = {
    {0x80, 0x20AC}, {0x82, 0x201A}, {0x83, 0x0192}, {0x84, 0x201E}, {0x85, 0x2026},
    {0x86, 0x2020}, {0x87, 0x2021}, {0x88, 0x02C6}, {0x89, 0x2030}, {0x8A, 0x0160},
    {0x8B, 0x2039}, {0x8C, 0x0152}, {0x91, 0x2018}, {0x92, 0x2019}, {0x93, 0x201C},
    {0x94, 0x201D}, {0x95, 0x2022}, {0x96, 0x2013}, {0x97, 0x2014}, {0x98, 0x02DC},
    {0x99, 0x2122}, {0x9A, 0x0161}, {0x9B, 0x203A}, {0x9C, 0x0153}, {0x9F, 0x0178},
    {0xD0, 0x011E}, {0xDD, 0x0130}, {0xDE, 0x015E}, {0xF0, 0x011F}, {0xFD, 0x0131},
    {0xFE, 0x015F}};
static const unsigned char n1254[] = {0x81, 0x8D, 0x8E, 0x8F, 0x90, 0x9D, 0x9E};

/* ISO-8859-9: ISO-8859-1's but at the same six places; no byte without a
   character. */
static const diff d8859_9[] = {{0xD0, 0x011E}, {0xDD, 0x0130}, {0xDE, 0x015E},
                               {0xF0, 0x011F}, {0xFD, 0x0131}, {0xFE, 0x015F}};

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

/* The code point one byte means in `enc`, -1 for a refusal, -2 for what is
   not a single code point. */
static long one_byte(unsigned enc, unsigned b) {
    char   in[1];
    char   out[8];
    size_t bad = 99;
    size_t n;
    cf_u32 cp = 0;

    in[0] = (char)b;
    n     = cf_encoding_recode(enc, in, 1, out, sizeof out, &bad);
    if (n == (size_t)-1) {
        return (bad == 0) ? -1 : -3;
    }
    if (n == 0 || n > 3) {
        return -2;
    }
    /* Decode the UTF-8 written: 1, 2 or 3 bytes. */
    if (n == 1) {
        cp = (cf_u32)(unsigned char)out[0];
    } else if (n == 2) {
        cp = (cf_u32)(((unsigned char)out[0] & 0x1Fu) << 6) |
             (cf_u32)((unsigned char)out[1] & 0x3Fu);
    } else {
        cp = (cf_u32)(((unsigned char)out[0] & 0x0Fu) << 12) |
             (cf_u32)(((unsigned char)out[1] & 0x3Fu) << 6) |
             (cf_u32)((unsigned char)out[2] & 0x3Fu);
    }
    return (long)cp;
}

/* Every byte 0x80 to 0xFF of `enc` against the differences and the empty
   places given, and ISO-8859-1's code point for the rest. */
static void table(unsigned enc, const char* name, const diff* d, size_t nd,
                  const unsigned char* none, size_t nn) {
    char     msg[96];
    unsigned b;
    size_t   i;

    for (b = 0x80; b <= 0xFF; ++b) {
        long want = (long)b;
        long got  = one_byte(enc, b);

        for (i = 0; i < nd; ++i) {
            if (d[i].byte == b) {
                want = (long)d[i].cp;
            }
        }
        for (i = 0; i < nn; ++i) {
            if (none[i] == b) {
                want = -1;
            }
        }
        sprintf(msg, "%s: byte 0x%02X is %ld, got %ld", name, b, want, got);
        check(got == want, msg);
    }
    for (b = 0; b < 0x80; ++b) {
        sprintf(msg, "%s: ASCII byte 0x%02X is itself", name, b);
        check(one_byte(enc, b) == (long)b, msg);
    }
}

static void test_tables(void) {
    table(CF_ENCODING_ISO_8859_1, "iso-8859-1", NULL, 0, NULL, 0);
    table(CF_ENCODING_WINDOWS_1252, "windows-1252", d1252, COUNT(d1252), n1252,
          COUNT(n1252));
    table(CF_ENCODING_WINDOWS_1254, "windows-1254", d1254, COUNT(d1254), n1254,
          COUNT(n1254));
    table(CF_ENCODING_ISO_8859_9, "iso-8859-9", d8859_9, COUNT(d8859_9), NULL, 0);
}

/* A text with a bad byte in it: the offset the refusal names. */
static void test_bad_offset(void) {
    char   out[64];
    size_t bad = 0;

    check(cf_encoding_recode(CF_ENCODING_WINDOWS_1252,
                             "ab\x81"
                             "c",
                             4, out, sizeof out, &bad) == (size_t)-1 &&
              bad == 2,
          "a byte with no character in Windows-1252 is named at its offset");
    check(cf_encoding_recode(CF_ENCODING_WINDOWS_1254, "\x9E", 1, out, sizeof out,
                             &bad) == (size_t)-1 &&
              bad == 0,
          "0x9E has no character in Windows-1254, though it has in 1252");
    check(cf_encoding_recode(CF_ENCODING_WINDOWS_1252, "\x9E", 1, out, sizeof out,
                             &bad) == 2,
          "and 0x9E has in Windows-1252: U+017E, two bytes");
}

/* The longest output: 0x80 bytes, each three bytes of UTF-8, in a buffer of
   exactly 3 * n. */
static void test_room(void) {
    char   in[0x80];
    char   out[3 * 0x80];
    size_t i, n, bad = 0;

    for (i = 0; i < sizeof in; ++i) {
        in[i] = '\x95'; /* U+2022 in Windows-1252: three bytes */
    }
    n = cf_encoding_recode(CF_ENCODING_WINDOWS_1252, in, sizeof in, out, sizeof out,
                           &bad);
    check(n == sizeof out, "0x80 bytes of U+2022 need 3 * n and fit in it");
    check(memcmp(out, "\xE2\x80\xA2\xE2\x80\xA2", 6) == 0, "and are U+2022 each");
    check(cf_encoding_recode(CF_ENCODING_WINDOWS_1252, in, sizeof in, out,
                             sizeof out - 1, &bad) == (size_t)-1,
          "a buffer one byte short is refused, not overrun");
}

static void test_utf16(void) {
    char   out[32];
    size_t bad = 0;
    size_t n;

    n = cf_encoding_recode(CF_ENCODING_UTF16LE, "A\0\xE9\0\xAC\x20", 6, out, sizeof out,
                           &bad);
    check(n == 6 && memcmp(out, "A\xC3\xA9\xE2\x82\xAC", 6) == 0,
          "UTF-16LE: A, e acute, euro");
    n = cf_encoding_recode(CF_ENCODING_UTF16BE, "\0A\0\xE9\x20\xAC", 6, out, sizeof out,
                           &bad);
    check(n == 6 && memcmp(out, "A\xC3\xA9\xE2\x82\xAC", 6) == 0, "UTF-16BE: the same");
    /* U+1F600 is D83D DE00. */
    n = cf_encoding_recode(CF_ENCODING_UTF16LE, "\x3D\xD8\x00\xDE", 4, out, sizeof out,
                           &bad);
    check(n == 4 && memcmp(out, "\xF0\x9F\x98\x80", 4) == 0,
          "UTF-16LE: a surrogate pair");
    n = cf_encoding_recode(CF_ENCODING_UTF16BE, "\xD8\x3D\xDE\x00", 4, out, sizeof out,
                           &bad);
    check(n == 4 && memcmp(out, "\xF0\x9F\x98\x80", 4) == 0,
          "UTF-16BE: a surrogate pair");
    check(cf_encoding_recode(CF_ENCODING_UTF16LE, "", 0, out, sizeof out, &bad) == 0,
          "UTF-16: nothing is nothing");

    bad = 99;
    check(cf_encoding_recode(CF_ENCODING_UTF16LE, "A\0\x3D\xD8Z\0", 6, out, sizeof out,
                             &bad) == (size_t)-1 &&
              bad == 2,
          "a high surrogate with no low one is refused at its unit");
    bad = 99;
    check(cf_encoding_recode(CF_ENCODING_UTF16LE, "A\0\x00\xDEZ\0", 6, out, sizeof out,
                             &bad) == (size_t)-1 &&
              bad == 2,
          "a low surrogate alone is refused at its unit");
    bad = 99;
    check(cf_encoding_recode(CF_ENCODING_UTF16BE, "\xD8\x3D", 2, out, sizeof out,
                             &bad) == (size_t)-1 &&
              bad == 0,
          "a high surrogate at the end is refused");
    bad = 99;
    check(cf_encoding_recode(CF_ENCODING_UTF16LE, "A\0B", 3, out, sizeof out, &bad) ==
                  (size_t)-1 &&
              bad == 2,
          "an odd length is refused at the half unit");
}

static void test_names(void) {
    static const struct {
        const char* name;
        unsigned    flags;
    } names[]                        = {{"utf-8", CF_ENCODING_UTF8},
                                        {"iso-8859-1", CF_ENCODING_ISO_8859_1},
                                        {"latin1", CF_ENCODING_ISO_8859_1},
                                        {"windows-1252", CF_ENCODING_WINDOWS_1252},
                                        {"cp1252", CF_ENCODING_WINDOWS_1252},
                                        {"windows-1254", CF_ENCODING_WINDOWS_1254},
                                        {"cp1254", CF_ENCODING_WINDOWS_1254},
                                        {"iso-8859-9", CF_ENCODING_ISO_8859_9},
                                        {"latin5", CF_ENCODING_ISO_8859_9},
                                        {"utf-16le", CF_ENCODING_UTF16LE},
                                        {"utf-16be", CF_ENCODING_UTF16BE},
                                        {"UTF-8", CF_ENCODING_UTF8},
                                        {"Windows-1254", CF_ENCODING_WINDOWS_1254},
                                        {"LATIN5", CF_ENCODING_ISO_8859_9},
                                        {"Utf-16Be", CF_ENCODING_UTF16BE}};
    static const char* const wrong[] = {
        "",       "utf8",   "utf-16",  "windows-1253", "latin2",     "ascii",
        "utf-8 ", " utf-8", "cp-1254", "utf-32",       "windows-125"};
    char     msg[96];
    unsigned flags = 12345;
    size_t   i;

    for (i = 0; i < COUNT(names); ++i) {
        flags = 12345;
        sprintf(msg, "name '%s' is known", names[i].name);
        check(cf_encoding_by_name(names[i].name, &flags) && flags == names[i].flags,
              msg);
    }
    for (i = 0; i < COUNT(wrong); ++i) {
        flags = 12345;
        sprintf(msg, "name '%s' is not an encoding", wrong[i]);
        check(!cf_encoding_by_name(wrong[i], &flags) && flags == 12345u, msg);
    }
    check(!cf_encoding_by_name(NULL, &flags), "NULL is not a name");

    check_text(cf_encoding_name(CF_ENCODING_UTF8), "utf-8", "the name of UTF-8");
    check_text(cf_encoding_name(CF_ENCODING_ISO_8859_1), "iso-8859-1", "its Latin-1");
    check_text(cf_encoding_name(CF_ENCODING_WINDOWS_1252), "windows-1252", "1252");
    check_text(cf_encoding_name(CF_ENCODING_WINDOWS_1254), "windows-1254", "1254");
    check_text(cf_encoding_name(CF_ENCODING_ISO_8859_9), "iso-8859-9", "Latin-5");
    check_text(cf_encoding_name(CF_ENCODING_UTF16LE), "utf-16le", "UTF-16LE");
    check_text(cf_encoding_name(CF_ENCODING_UTF16BE), "utf-16be", "UTF-16BE");
    check(cf_encoding_name(7u << CF_ENCODING_SHIFT) == NULL,
          "a value above the last has no name");
    check(cf_encoding_name(CF_ENCODING_LAST) != NULL, "the last has one");
}

/* ================================================================== *
 * Part two: through the builders
 * ================================================================== */

/* What a one-key read came to: whether it built, the key's value, the error
   when it did not, and the encoding its source says it read. */
typedef struct got {
    int  ok;
    char value[64];
    char error[512];
    char encoding[32];
} got;

/* The encoding named by the first source that says one. */
static void encoding_of(const cf_config* c, char* out, size_t cap) {
    const cf_source_view* sv = cf_sources(c);
    size_t                i;

    out[0] = '\0';
    for (i = 0; i < sv->count && out[0] == '\0'; ++i) {
        const char* e = cf_source_file_encoding(sv->items[i]);

        if (e != NULL && strlen(e) < cap) {
            strcpy(out, e);
        }
    }
}

static void finish(got* g, cf_builder* b, const char* key) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c;

    cf_builder_text(b, key, "", 0);
    c = cf_builder_create(b, &err);
    if (c == NULL) {
        sprintf(g->error, "%.500s", err.message);
        return;
    }
    g->ok = 1;
    sprintf(g->value, "%.60s", cf_get_text(c, key, "?"));
    encoding_of(c, g->encoding, sizeof g->encoding);
    cf_config_free(c);
}

/* `bytes` as a file at `path`, read with `format` and `flags`; the text of
   `key`. */
static void from_file(got* g, const char* format, unsigned flags, const char* path,
                      const char* bytes, size_t n, const char* key) {
    cf_builder* b = cf_build(NULL);

    memset(g, 0, sizeof *g);
    check(spill_n(path, bytes, n), "wrote the file");
    cf_builder_file(b, "file", CF_RANK_USER_FILE, path, format, flags);
    finish(g, b, key);
    remove(path);
}

/* A UTF-16 text of ASCII, in `big` or little order, appended at out[*n]. */
static void wide(char* out, size_t* n, const char* ascii, int big) {
    for (; *ascii != '\0'; ++ascii) {
        out[(*n)++] = big ? '\0' : *ascii;
        out[(*n)++] = big ? *ascii : '\0';
    }
}

/* One UTF-16 code unit. */
static void unit(char* out, size_t* n, unsigned v, int big) {
    out[(*n)++] = (char)(big ? (v >> 8) : (v & 0xFFu));
    out[(*n)++] = (char)(big ? (v & 0xFFu) : (v >> 8));
}

/* The byte-order mark of either order. */
static void mark(char* out, size_t* n, int big) {
    unit(out, n, 0xFEFFu, big);
}

#define S_CEDILLA "\xC5\x9E"                    /* U+015E in UTF-8 */
#define SUKRU_UTF8 "\xC5\x9E\xC3\xBCkr\xC3\xBC" /* Sukru with its Turkish letters */

static void test_single_byte(void) {
    got g;

    /* ad=Sukru, with the Turkish letters, in Windows-1254: S-cedilla is 0xDE,
     * u-diaeresis 0xFC. */
    from_file(&g, NULL, CF_ENCODING_WINDOWS_1254, "cf_encoding_a.ini",
              "ad=\xDE\xFC"
              "kr\xFC\n",
              9, "ad");
    check(g.ok, "a Windows-1254 INI is read when it is named");
    check_text(g.value, SUKRU_UTF8, "as UTF-8");
    check_text(g.encoding, "windows-1254", "and the source says what it read");

    /* The same bytes in ISO-8859-9, which has the same letters. */
    from_file(&g, NULL, CF_ENCODING_ISO_8859_9, "cf_encoding_b.ini",
              "ad=\xDE\xFC"
              "kr\xFC\n",
              9, "ad");
    check(g.ok && strcmp(g.value, SUKRU_UTF8) == 0, "ISO-8859-9 reads it the same");
    check_text(g.encoding, "iso-8859-9", "and says so");

    /* Windows-1252 reads 0xDE as thorn: a different, valid text. */
    from_file(&g, NULL, CF_ENCODING_WINDOWS_1252, "cf_encoding_c.ini",
              "ad=\xDE\xFC"
              "kr\xFC\n",
              9, "ad");
    check(g.ok && strcmp(g.value, "\xC3\x9E\xC3\xBCkr\xC3\xBC") == 0,
          "Windows-1252 reads 0xDE as another letter: nothing is guessed");

    /* No encoding named: the bytes are not UTF-8, and are refused (A-5). */
    from_file(&g, NULL, 0, "cf_encoding_d.ini",
              "ad=\xDE\xFC"
              "kr\xFC\n",
              9, "ad");
    check(!g.ok, "without the flag the file is refused");
    has(g.error, "cf_encoding_d.ini:1:4: byte 0xDE is not valid UTF-8",
        "at its place, and says why");
#if CF_MESSAGE_MAX >= 256 /* a short message is cut before the hint ends */
    has(g.error, "CF_ENCODING_WINDOWS_1254", "and what to name to read it");
#endif

    /* The euro sign is three bytes in UTF-8. */
    from_file(&g, NULL, CF_ENCODING_WINDOWS_1252, "cf_encoding_e.ini", "ad=\x80\n", 5,
              "ad");
    check(g.ok && strcmp(g.value, "\xE2\x82\xAC") == 0,
          "0x80 is the euro sign in 1252");
}

static void test_refusals(void) {
    got g;

    from_file(&g, NULL, CF_ENCODING_WINDOWS_1252, "cf_encoding_f.ini", "a=1\nb=\x81\n",
              8, "b");
    check(!g.ok, "a byte with no character is refused");
    has(g.error, "cf_encoding_f.ini:2:3: byte 0x81 has no character in windows-1252",
        "at its line and column, in the encoding's name");

    from_file(&g, NULL, CF_ENCODING_WINDOWS_1254, "cf_encoding_g.ini", "a=\x9E\n", 4,
              "a");
    has(g.error, ":1:3: byte 0x9E has no character in windows-1254",
        "0x9E has none in 1254");

    /* An unpaired surrogate and half a unit, in UTF-16 with a mark. */
    {
        char   buf[64];
        size_t n = 0;

        mark(buf, &n, 0);
        wide(buf, &n, "{\"ad\": \"", 0);
        unit(buf, &n, 0xD800u, 0);
        wide(buf, &n, "\"}", 0);
        from_file(&g, NULL, 0, "cf_encoding_h.json", buf, n, "ad");
        check(!g.ok, "an unpaired surrogate is refused");
        has(g.error, "cf_encoding_h.json:1:9: an unpaired UTF-16 surrogate 0xD800",
            "at its code unit, counted in units");

        n = 0;
        mark(buf, &n, 1);
        wide(buf, &n, "{\n", 1);
        unit(buf, &n, 0xDC00u, 1);
        from_file(&g, NULL, 0, "cf_encoding_i.json", buf, n, "ad");
        has(g.error, "cf_encoding_i.json:2:1: an unpaired UTF-16 surrogate 0xDC00",
            "a low surrogate alone, on the second line, in big-endian");

        n = 0;
        mark(buf, &n, 0);
        wide(buf, &n, "{}", 0);
        buf[n++] = '\n';
        from_file(&g, NULL, 0, "cf_encoding_j.json", buf, n, "ad");
        has(g.error, "cf_encoding_j.json: ends in half a UTF-16 code unit",
            "an odd length is half a code unit");
    }

    /* UTF-32's mark is refused by name. */
    from_file(&g, NULL, 0, "cf_encoding_k.json", "\xFF\xFE\x00\x00{\x00\x00\x00", 8,
              "ad");
    check(!g.ok, "a UTF-32 byte-order mark is refused");
    has(g.error,
        "'cf_encoding_k.json' is UTF-32, which this library does not read; "
        "convert it to UTF-8",
        "by name");
    from_file(&g, NULL, 0, "cf_encoding_l.json", "\x00\x00\xFE\xFF\x00\x00\x00{", 8,
              "ad");
    has(g.error, "is UTF-32", "in either order");
}

#if CF_WITH_JSON
static void test_utf16_by_mark(void) {
    char   buf[96];
    size_t n = 0;
    got    g;

    mark(buf, &n, 0);
    wide(buf, &n, "{\"ad\": \"\\u015E\", \"b\": \"", 0);
    unit(buf, &n, 0x015Eu, 0);
    wide(buf, &n, "\"}", 0);
    from_file(&g, NULL, 0, "cf_encoding_m.json", buf, n, "b");
    check(g.ok, "a UTF-16LE JSON file with its mark is read");
    check_text(g.value, S_CEDILLA, "its letters recoded");
    check_text(g.encoding, "utf-16le", "and the source says what the mark said");

    n = 0;
    mark(buf, &n, 1);
    wide(buf, &n, "{\"ad\": \"", 1);
    unit(buf, &n, 0x015Eu, 1);
    wide(buf, &n, "\"}", 1);
    from_file(&g, NULL, 0, "cf_encoding_n.json", buf, n, "ad");
    check(g.ok && strcmp(g.value, S_CEDILLA) == 0, "and the big-endian one");
    check_text(g.encoding, "utf-16be", "which says so");

    /* Named, with no mark: UTF-16LE is the encoding given. */
    n = 0;
    wide(buf, &n, "{\"ad\": \"", 0);
    unit(buf, &n, 0x015Eu, 0);
    wide(buf, &n, "\"}", 0);
    from_file(&g, NULL, CF_ENCODING_UTF16LE, "cf_encoding_o.json", buf, n, "ad");
    check(g.ok && strcmp(g.value, S_CEDILLA) == 0,
          "UTF-16LE named reads a file with no mark");
    check_text(g.encoding, "utf-16le", "and says so");

    /* A UTF-8 file with its mark is as it was. */
    from_file(&g, NULL, 0, "cf_encoding_p.json", "\xEF\xBB\xBF{\"ad\": \"x\"}", 14,
              "ad");
    check(g.ok && strcmp(g.value, "x") == 0, "a UTF-8 mark is skipped, as before");
    check_text(g.encoding, "utf-8", "and the encoding is UTF-8");
}
#endif

#if CF_WITH_XML
static void test_xml(void) {
    char   buf[160];
    size_t n = 0;
    got    g;
    int    big;

    for (big = 0; big < 2; ++big) {
        n = 0;
        mark(buf, &n, big);
        wide(buf, &n, "<?xml version=\"1.0\" encoding=\"UTF-16\"?><config><ad>", big);
        unit(buf, &n, 0x015Eu, big);
        wide(buf, &n, "</ad></config>", big);
        from_file(&g, NULL, 0, "cf_encoding_q.xml", buf, n, "ad");
        check(g.ok, "a UTF-16 XML file with its mark and declaration is read by expat");
        check_text(g.value, S_CEDILLA, "its text is UTF-8 in the key space");
    }

    /* The format left to the content: the mark recodes, XML reads the
       original bytes. */
    n = 0;
    mark(buf, &n, 0);
    wide(buf, &n, "<?xml version=\"1.0\"?><config><ad>", 0);
    unit(buf, &n, 0x015Eu, 0);
    wide(buf, &n, "</ad></config>", 0);
    from_file(&g, NULL, 0, "cf_encoding_r", buf, n, "ad");
    check(g.ok, "a UTF-16 XML file named by its content is read");
    check_text(g.value, S_CEDILLA, "as UTF-8");
    check_text(g.encoding, "utf-16le", "and says UTF-16LE");

    /* The same with the declaration UTF-16 documents carry. */
    n = 0;
    mark(buf, &n, 1);
    wide(buf, &n,
         "<?xml version=\"1.0\" encoding=\"UTF-16\"?><config><ad>x</ad></config>", 1);
    from_file(&g, NULL, 0, "cf_encoding_r2", buf, n, "ad");
    check(g.ok && strcmp(g.value, "x") == 0,
          "and one that declares UTF-16, its format left to the content");
    check_text(g.encoding, "utf-16be", "which says UTF-16BE");

    /* An encoding named for XML is refused: it declares its own. */
    from_file(&g, "xml", CF_ENCODING_WINDOWS_1252, "cf_encoding_s.xml",
              "<config><ad>x</ad></config>", 27, "ad");
    check(!g.ok, "an encoding named for XML is refused");
    has(g.error, "xml declares its own encoding or has none; name no encoding for it",
        "by its own words");
}
#endif

#if CF_WITH_CBOR
static void test_binary(void) {
    got g;

    from_file(&g, "cbor", CF_ENCODING_ISO_8859_1, "cf_encoding_t.cbor", "\xA0", 1,
              "ad");
    check(!g.ok, "an encoding named for CBOR is refused");
    has(g.error, "cbor declares its own encoding or has none; name no encoding for it",
        "by its own words");
    from_file(&g, "cbor", 0, "cf_encoding_u.cbor",
              "\xA1\x62"
              "ad\x61x",
              6, "ad");
    check(g.ok && strcmp(g.value, "x") == 0,
          "and CBOR with no encoding named is read as before");
}
#endif

/* The flags a builder passes to its source. */
static void test_masks(void) {
    cf_builder* b;
    got         g;

    /* A secrets directory takes no encoding: its bytes are opaque. */
    b = cf_build(NULL);
    cf_builder_secrets(b, "secrets", CF_RANK_ENV, "cf_encoding_nodir", NULL,
                       CF_ENCODING_WINDOWS_1254);
    memset(&g, 0, sizeof g);
    finish(&g, b, "ad");
    check(!g.ok, "an encoding on a secrets directory is refused");
    has(g.error, "are not source flags this library knows", "as an unknown flag is");

    /* Neither do credentials, memory, the registry, the store. */
    b = cf_build(NULL);
    cf_builder_credentials(b, "creds", CF_RANK_ENV, NULL, CF_ENCODING_UTF16LE);
    memset(&g, 0, sizeof g);
    finish(&g, b, "ad");
    check(!g.ok, "and on credentials");

    b = cf_build(NULL);
    cf_builder_store(b, "store", CF_RANK_ENV, "cf_encoding_store",
                     CF_ENCODING_ISO_8859_1);
    memset(&g, 0, sizeof g);
    finish(&g, b, "ad");
    check(!g.ok, "and on a store");

    /* A value above the last one lies in the mask, and is refused by the
       sources that take an encoding. */
    b = cf_build(NULL);
    cf_builder_file(b, "file", CF_RANK_USER_FILE, "cf_encoding_x.ini", NULL,
                    7u << CF_ENCODING_SHIFT);
    memset(&g, 0, sizeof g);
    finish(&g, b, "ad");
    check(!g.ok, "a value 7 in the encoding field of a file is refused");
    has(g.error, "are not source flags this library knows", "as an unknown flag is");

    b = cf_build(NULL);
    cf_builder_document(b, "doc", CF_RANK_USER_FILE, "a=1", 3, "ini",
                        15u << CF_ENCODING_SHIFT);
    memset(&g, 0, sizeof g);
    finish(&g, b, "a");
    check(!g.ok, "and a value 15 in a document's");

    b = cf_build(NULL);
    cf_builder_dir(b, "dir", CF_RANK_USER_FILE, "cf_encoding.d", NULL,
                   7u << CF_ENCODING_SHIFT);
    memset(&g, 0, sizeof g);
    finish(&g, b, "ad");
    check(!g.ok, "and in a drop-in directory's");

    b = cf_build(NULL);
    cf_builder_dotenv(b, "env", CF_RANK_USER_FILE, "cf_encoding.env", "APP_",
                      9u << CF_ENCODING_SHIFT);
    memset(&g, 0, sizeof g);
    finish(&g, b, "ad");
    check(!g.ok, "and in a .env file's");

    /* A source used directly refuses it the same way. */
    {
        cf_source s;
        cf_arena* ar = cf_arena_new(NULL, 0);
        void*     st = cf_arena_calloc(ar, cf_source_file_state_size());

        check(st != NULL, "room for a file source's state");
        if (st == NULL) {
            cf_arena_free(ar);
            return;
        }
        check(!cf_source_file(&s, "f", 0, "p.ini", NULL, 7u << CF_ENCODING_SHIFT, st,
                              cf_source_file_state_size()),
              "cf_source_file refuses a value 7");
        check(cf_source_file(&s, "f", 0, "p.ini", NULL, CF_ENCODING_UTF16BE, st,
                             cf_source_file_state_size()),
              "and takes the last one");
        check(cf_source_file_encoding(&s) == NULL, "which says nothing before a load");
        cf_arena_free(ar);
    }
}

static void test_document(void) {
    cf_builder* b = cf_build(NULL);
    got         g;

    memset(&g, 0, sizeof g);
    cf_builder_document(b, "doc.ini", CF_RANK_USER_FILE,
                        "ad=\xDE\xFC"
                        "kr\xFC\n",
                        9, NULL, CF_ENCODING_WINDOWS_1254);
    finish(&g, b, "ad");
    check(g.ok && strcmp(g.value, SUKRU_UTF8) == 0,
          "a document held in memory is recoded too");
    check_text(g.encoding, "windows-1254", "and says so");

    b = cf_build(NULL);
    memset(&g, 0, sizeof g);
    cf_builder_document(b, "doc2.ini", CF_RANK_USER_FILE,
                        "ad=\xDE\xFC"
                        "kr\xFC\n",
                        9, NULL, 0);
    finish(&g, b, "ad");
    check(!g.ok, "a document with none named is refused: it is not UTF-8");
    has(g.error, "doc2.ini:1:4: byte 0xDE is not valid UTF-8", "at its place");
}

#if CF_WITH_DOTENV
static void test_dotenv(void) {
    cf_builder* b = cf_build(NULL);
    got         g;
    char        buf[64];
    size_t      n = 0;

    check(spill_n("cf_encoding_1.env",
                  "APP_AD=\xDE\xFC"
                  "kr\xFC\n",
                  13),
          "wrote a .env file");
    cf_builder_dotenv(b, "env", CF_RANK_USER_FILE, "cf_encoding_1.env", "APP_",
                      CF_ENCODING_WINDOWS_1254);
    memset(&g, 0, sizeof g);
    finish(&g, b, "ad");
    check(g.ok && strcmp(g.value, SUKRU_UTF8) == 0, "a .env file is recoded");
    remove("cf_encoding_1.env");

    mark(buf, &n, 1);
    wide(buf, &n, "APP_AD=x\n", 1);
    check(spill_n("cf_encoding_2.env", buf, n), "wrote a UTF-16BE .env file");
    b = cf_build(NULL);
    cf_builder_dotenv(b, "env", CF_RANK_USER_FILE, "cf_encoding_2.env", "APP_", 0);
    memset(&g, 0, sizeof g);
    finish(&g, b, "ad");
    check(g.ok && strcmp(g.value, "x") == 0, "and one in UTF-16 by its mark");
    remove("cf_encoding_2.env");

    check(spill_n("cf_encoding_3.env", "APP_AD=\x81\n", 9), "wrote a bad .env file");
    b = cf_build(NULL);
    cf_builder_dotenv(b, "env", CF_RANK_USER_FILE, "cf_encoding_3.env", "APP_",
                      CF_ENCODING_WINDOWS_1252);
    memset(&g, 0, sizeof g);
    finish(&g, b, "ad");
    check(!g.ok, "a byte with no character is refused in it");
    has(g.error, "cf_encoding_3.env:1:8: byte 0x81 has no character in windows-1252",
        "at its place");
    remove("cf_encoding_3.env");
}
#endif

#if CF_WITH_DIR
static void test_dir(void) {
    cf_builder* b = cf_build(NULL);
    got         g;

    check(spill_n("cf_encoding.d/10-a.ini",
                  "ad=\xDE\xFC"
                  "kr\xFC\n",
                  8),
          "wrote a file of a drop-in directory");
    cf_builder_dir(b, "dir", CF_RANK_USER_FILE, "cf_encoding.d", NULL,
                   CF_ENCODING_WINDOWS_1254);
    memset(&g, 0, sizeof g);
    finish(&g, b, "ad");
    check(g.ok && strcmp(g.value, SUKRU_UTF8) == 0,
          "every file of a drop-in directory is recoded");
    remove("cf_encoding.d/10-a.ini");
}
#endif

static void set_var(const char* name, const char* value) {
#if defined(_WIN32)
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

static void test_env_document(void) {
    cf_builder* b = cf_build(NULL);
    got         g;

    set_var("CF_ENCODING_DOC", "ad=x\n");
    memset(&g, 0, sizeof g);
    cf_builder_env_document(b, "envdoc", CF_RANK_ENV, "CF_ENCODING_DOC", "ini",
                            CF_ENCODING_ISO_8859_1);
    finish(&g, b, "ad");
    check(g.ok && strcmp(g.value, "x") == 0,
          "an environment document takes an encoding");
    check_text(g.encoding, "iso-8859-1", "and says so");
}

#if CF_WITH_REPORT
static void test_report(void) {
    cf_error       err = CF_ERROR_INIT;
    cf_builder*    b   = cf_build(NULL);
    cf_config*     c;
    cf_report_info info = CF_REPORT_INFO_INIT;
    static char    buf[8192];

    cf_builder_document(b, "latin", CF_RANK_USER_FILE, "ad=x\n", 5, "ini",
                        CF_ENCODING_WINDOWS_1254);
    cf_builder_document(b, "plain", CF_RANK_SYS_FILE, "ad=x\n", 5, "ini", 0);
    cf_builder_text(b, "ad", "", 0);
    c = cf_builder_create(b, &err);
    check(c != NULL, "a configuration of two documents");
    if (c == NULL) {
        return;
    }
    info.generated = "2026-11-20T00:00:00Z";
    check(cf_report(c, &info, buf, sizeof buf) > 0, "its report");
    has(buf, "\"format_how\": \"named\", \"encoding\": \"windows-1254\"",
        "says the encoding of a file read in another");
    {
        const char* first = strstr(buf, "\"encoding\"");

        check(first != NULL, "the report names an encoding");
        if (first != NULL) {
            check(strstr(first + 1, "\"encoding\"") == NULL,
                  "and only for that one: a UTF-8 file adds nothing");
        }
    }
    cf_config_free(c);
}
#endif

int main(void) {
    test_tables();
    test_bad_offset();
    test_room();
    test_utf16();
    test_names();
    test_single_byte();
    test_refusals();
#if CF_WITH_JSON
    test_utf16_by_mark();
#endif
#if CF_WITH_XML
    test_xml();
#endif
#if CF_WITH_CBOR
    test_binary();
#endif
    test_masks();
    test_document();
#if CF_WITH_DOTENV
    test_dotenv();
#endif
#if CF_WITH_DIR
    test_dir();
#endif
    test_env_document();
#if CF_WITH_REPORT
    test_report();
#endif
    return cf_test_done();
}
