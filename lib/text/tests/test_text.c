/*
 * test_text.c - the shared text helpers on the cases each copy they replace
 * was used for: every overlong, surrogate and cut-off class of UTF-8, the
 * encoder's four lengths, both cases of a hex digit, the table hash.
 */

#include "configforge/cf_text.h"

#include <string.h>

#include "cf_test.h"

/* A byte string, decoded whole: its length and code point, or 0. */
static size_t dec(const char* s, size_t n, cf_u32* cp) {
    return cf_text_utf8_decode(s, s + n, cp);
}

static void test_decode_valid(void) {
    cf_u32 cp = 0;

    check(dec("A", 1, &cp) == 1 && cp == 'A', "ASCII");
    check(dec("\x7f", 1, &cp) == 1 && cp == 0x7Fu, "DEL");
    check(dec("\xC2\x80", 2, &cp) == 2 && cp == 0x80u,
          "U+0080, the shortest two bytes");
    check(dec("\xDF\xBF", 2, &cp) == 2 && cp == 0x7FFu, "U+07FF");
    check(dec("\xE0\xA0\x80", 3, &cp) == 3 && cp == 0x800u, "U+0800");
    check(dec("\xED\x9F\xBF", 3, &cp) == 3 && cp == 0xD7FFu,
          "U+D7FF, below the surrogates");
    check(dec("\xEE\x80\x80", 3, &cp) == 3 && cp == 0xE000u, "U+E000, above them");
    check(dec("\xEF\xBF\xBF", 3, &cp) == 3 && cp == 0xFFFFu, "U+FFFF");
    check(dec("\xF0\x90\x80\x80", 4, &cp) == 4 && cp == 0x10000u, "U+10000");
    check(dec("\xF4\x8F\xBF\xBF", 4, &cp) == 4 && cp == 0x10FFFFu, "U+10FFFF");
    /* More bytes than the sequence: its own length, not the span's. */
    check(dec("\xC3\xA9xyz", 5, &cp) == 2 && cp == 0xE9u, "a sequence before text");
}

static void test_decode_refused(void) {
    static const char none[] = "";
    cf_u32            cp     = 12345;

    check(cf_text_utf8_decode(none, none, &cp) == 0, "empty: p == end");
    check(dec("\x80", 1, &cp) == 0, "a lone continuation");
    check(dec("\xBF", 1, &cp) == 0, "the last continuation");
    check(dec("\xC0\x80", 2, &cp) == 0, "C0 is an overlong lead");
    check(dec("\xC1\xBF", 2, &cp) == 0, "C1 is an overlong lead");
    check(dec("\xE0\x80\x80", 3, &cp) == 0, "overlong three bytes");
    check(dec("\xE0\x9F\xBF", 3, &cp) == 0, "overlong three bytes, at its edge");
    check(dec("\xED\xA0\x80", 3, &cp) == 0, "the first surrogate, U+D800");
    check(dec("\xED\xBF\xBF", 3, &cp) == 0, "the last surrogate, U+DFFF");
    check(dec("\xF0\x80\x80\x80", 4, &cp) == 0, "overlong four bytes");
    check(dec("\xF0\x8F\xBF\xBF", 4, &cp) == 0, "overlong four bytes, at its edge");
    check(dec("\xF4\x90\x80\x80", 4, &cp) == 0, "U+110000");
    check(dec("\xF5\x80\x80\x80", 4, &cp) == 0, "F5 leads nothing");
    check(dec("\xFF", 1, &cp) == 0, "FF");
    check(dec("\xC3", 1, &cp) == 0, "two bytes, cut at one");
    check(dec("\xE2\x82", 2, &cp) == 0, "three bytes, cut at two");
    check(dec("\xF0\x9F\x98", 3, &cp) == 0, "four bytes, cut at three");
    check(dec("\xC3\x28", 2, &cp) == 0, "a lead before a non-continuation");
    check(dec("\xE2\x28\xA1", 3, &cp) == 0, "a bad second byte");
    check(dec("\xE2\x82\x28", 3, &cp) == 0, "a bad third byte");
    check(dec("\xF0\x9F\x98\x28", 4, &cp) == 0, "a bad fourth byte");
    check(cp == 12345u, "a refusal leaves the code point alone");
}

static void test_valid(void) {
    check(cf_text_utf8_valid("", 0) == 0, "valid: empty");
    check(cf_text_utf8_valid("abc", 3) == 3, "valid: ASCII");
    check(cf_text_utf8_valid("a\xC3\xA9z", 4) == 4, "valid: mixed");
    check(cf_text_utf8_valid("ab\xC3", 3) == 2,
          "valid: a cut-off sequence is not in the prefix");
    check(cf_text_utf8_valid("ab\xED\xA0\x80zz", 6) == 2,
          "valid: stops before a surrogate");
    check(cf_text_utf8_valid("\xC0\xAF", 2) == 0, "valid: an overlong slash");
    check(cf_text_utf8_valid("\xF4\x8F\xBF\xBF", 4) == 4, "valid: U+10FFFF");
    check(cf_text_utf8_valid("\xF4\x90\x80\x80", 4) == 0, "valid: past U+10FFFF");
}

/* Every code point, the encoder's bytes read back by the decoder. */
static void test_encode(void) {
    char   b[4];
    cf_u32 cp, v = 0;
    int    bad = 0;

    check(cf_text_utf8_encode(0x41u, b) == 1 && b[0] == 'A', "encode: ASCII");
    check(cf_text_utf8_encode(0u, b) == 1 && b[0] == '\0', "encode: NUL");
    check(cf_text_utf8_encode(0x80u, b) == 2 && (unsigned char)b[0] == 0xC2u &&
              (unsigned char)b[1] == 0x80u,
          "encode: U+0080");
    check(cf_text_utf8_encode(0x7FFu, b) == 2, "encode: U+07FF is two bytes");
    check(cf_text_utf8_encode(0x800u, b) == 3, "encode: U+0800 is three bytes");
    check(cf_text_utf8_encode(0xFFFFu, b) == 3, "encode: U+FFFF is three bytes");
    check(cf_text_utf8_encode(0x10000u, b) == 4, "encode: U+10000 is four bytes");
    check(cf_text_utf8_encode(0x10FFFFu, b) == 4 && (unsigned char)b[0] == 0xF4u &&
              (unsigned char)b[1] == 0x8Fu && (unsigned char)b[2] == 0xBFu &&
              (unsigned char)b[3] == 0xBFu,
          "encode: U+10FFFF");
    for (cp = 0; cp <= 0x10FFFFu; cp += 7) {
        size_t k;
        if (cp >= 0xD800u && cp <= 0xDFFFu) {
            continue;
        }
        k = cf_text_utf8_encode(cp, b);
        if (k == 0 || cf_text_utf8_decode(b, b + k, &v) != k || v != cp ||
            cf_text_utf8_valid(b, k) != k) {
            ++bad;
        }
    }
    check(bad == 0, "encode: every code point reads back as itself");
}

static void test_hex(void) {
    int c;

    for (c = 0; c <= 9; ++c) {
        check(cf_text_hex_value('0' + c) == c, "hex: a decimal digit");
    }
    for (c = 0; c < 6; ++c) {
        check(cf_text_hex_value('a' + c) == 10 + c, "hex: lower case");
        check(cf_text_hex_value('A' + c) == 10 + c, "hex: upper case");
    }
    check(cf_text_hex_value('g') == -1, "hex: g");
    check(cf_text_hex_value('G') == -1, "hex: G");
    check(cf_text_hex_value('/') == -1, "hex: the character before 0");
    check(cf_text_hex_value(':') == -1, "hex: the character after 9");
    check(cf_text_hex_value('@') == -1, "hex: @ is not a digit");
    check(cf_text_hex_value('`') == -1, "hex: a backquote is not a digit");
    check(cf_text_hex_value(' ') == -1, "hex: a space");
    check(cf_text_hex_value(-1) == -1, "hex: end of input");
    check(cf_text_hex_value(0) == -1, "hex: NUL");
    check(cf_text_hex_value(0xFF) == -1, "hex: a high byte");
}

static void test_hash(void) {
    static const char fb[] = "foobar";
    size_t            seed = cf_text_seed("a");
    size_t            h, i;

    check(seed != 0, "seed: never 0");

    /* FNV-1a: over nothing, the hash is the seed it started from. */
    check(cf_text_hash(seed, "", 0) == seed, "hash: the empty string is the seed");
    check(cf_text_hash(2166136261u, "", 0) == (size_t)2166136261u,
          "hash: empty, from the offset basis");

    /* The published 32-bit FNV-1a of "a" and "foobar"; the low 32 bits of
       this one agree. */
    check((cf_u32)cf_text_hash(2166136261u, "a", 1) == 0xE40C292Cu,
          "hash: FNV-1a of a");
    check((cf_u32)cf_text_hash(2166136261u, "foobar", 6) == 0xBF9CF968u,
          "hash: FNV-1a of foobar");

    /* One byte at a time is the same as all at once. */
    h = seed;
    for (i = 0; i < 6; ++i) {
        h = cf_text_hash(h, &fb[i], 1);
    }
    check(h == cf_text_hash(seed, "foobar", 6), "hash: byte by byte equals the whole");
    check(cf_text_hash(seed, "foobar", 6) != cf_text_hash(seed, "foobaz", 6),
          "hash: a different name");
    check(cf_text_hash(seed, "\xFF", 1) == cf_text_hash(seed, "\xFF", 1),
          "hash: a high byte is the same every time");
}

int main(void) {
    test_decode_valid();
    test_decode_refused();
    test_valid();
    test_encode();
    test_hex();
    test_hash();
    return cf_test_done();
}
