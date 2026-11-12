/*
 * test_cbor.c - CBOR (RFC 8949), read: the data model, the places the
 * values are explained by, the refusals by name, and the build that left
 * it out; and written: the exact bytes, and a literal ${ kept literal.
 * Documents are written as hex, the way the RFC's appendices write them.
 */

#include <configforge/cf_numeric.h>
#include <configforge/cf_serialize.h>
#include <configforge/configforge.h>

#include <stdio.h>
#include <string.h>

#include "cf_test.h"

#define FILE_NAME "cf_cbor_test.cbor"

#if CF_WITH_CBOR

static int hexval(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

/* "A1 61 61 01" as bytes; the count. */
static size_t unhex(const char* hex, unsigned char* out, size_t cap) {
    size_t n  = 0;
    int    hi = -1, v;

    for (; *hex != '\0'; ++hex) {
        v = hexval(*hex);
        if (v < 0) {
            continue;
        }
        if (hi < 0) {
            hi = v;
        } else if (n < cap) {
            out[n++] = (unsigned char)(hi * 16 + v);
            hi       = -1;
        }
    }
    return n;
}

static cf_config* read_bytes(const unsigned char* bytes, size_t n, cf_error* err) {
    cf_builder* b = cf_build(NULL);

    cf_builder_document(b, "doc", CF_RANK_USER_FILE, (const char*)bytes, n, "cbor", 0);
    return cf_builder_create(b, err);
}

static cf_config* read(const char* hex, cf_error* err) {
    unsigned char bytes[1024];
    size_t        n = unhex(hex, bytes, sizeof bytes);

    return read_bytes(bytes, n, err);
}

static const char g_places[] =
    "A1 66 73 65 72 76 65 72 A2 64 70 6F 72 74 19 1F 90 65 68 "
    "6F 73 74 73 82 61 61 61 62";

/* One document that must be refused, and the words its message holds. */
static void refused(const char* hex, const char* words, const char* what) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = read(hex, &err);

    check(c == NULL, what);
    has(err.message, words, what);
    cf_config_free(c);
}

static void test_scalars(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = read("A7 61 61 00 61 62 20 61 63 1A 00 0F 42 40 61 64 64 49 45 54 "
                          "46 61 65 F5 61 66 F4 61 67 F6",
                          &err);

    check(c != NULL, "scalars read");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(cf_get_int(c, "a", 9) == 0, "unsigned 0");
    check(cf_get_int(c, "b", 9) == -1, "negative -1");
    check(cf_get_int(c, "c", 9) == 1000000, "a four-byte integer");
    check_text(cf_get_text(c, "d", ""), "IETF", "text");
    check(cf_get_bool(c, "e", CF_FALSE) == CF_TRUE, "true");
    check(cf_get_bool(c, "f", CF_TRUE) == CF_FALSE, "false");
    check(cf_has(c, "g") && cf_type_of(c, "g") == CF_TYPE_NULL, "null");
    cf_config_free(c);
}

#if CF_WITH_REAL

static void test_floats(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = read("A4 61 68 F9 3E 00 61 69 FA 47 C3 50 00 61 6A FB 3F F1 99 99 "
                          "99 99 99 9A 61 6B F9 00 01",
                          &err);

    check(c != NULL, "floats read");
    if (c != NULL) {
        check(cf_get_real(c, "h", 0.0) == 1.5, "half precision");
        check(cf_get_real(c, "i", 0.0) == 100000.0, "single precision");
        check(cf_get_real(c, "j", 0.0) == 1.1, "double precision");
        check(cf_get_real(c, "k", 0.0) == 5.960464477539063e-08,
              "the smallest half-precision subnormal");
    }
    cf_config_free(c);

    c = read("A3 61 70 F9 7C 00 61 6E F9 FC 00 61 71 F9 7E 00", &err);
    check(c != NULL, "non-finite floats read");
    if (c != NULL) {
        check_text(cf_get_text(c, "p", ""), "inf", "infinity is the text inf");
        check_text(cf_get_text(c, "n", ""), "-inf", "minus infinity");
        check_text(cf_get_text(c, "q", ""), "nan", "NaN");
    }
    cf_config_free(c);

    c = read("A2 61 61 FA 7F 80 00 00 61 62 FB FF F0 00 00 00 00 00 00", &err);
    check(c != NULL, "non-finite singles and doubles read");
    if (c != NULL) {
        check_text(cf_get_text(c, "a", ""), "inf", "a single infinity");
        check_text(cf_get_text(c, "b", ""), "-inf", "a double minus infinity");
    }
    cf_config_free(c);
}

static void test_f16(void) {
    check(cf_f64_from_f16_bits(0x0000u) == 0.0, "half zero");
    check(cf_f64_from_f16_bits(0x3C00u) == 1.0, "half one");
    check(cf_f64_from_f16_bits(0xC000u) == -2.0, "half minus two");
    check(cf_f64_from_f16_bits(0x7BFFu) == 65504.0, "the largest half");
    check(cf_f64_from_f16_bits(0x0400u) == 6.103515625e-05, "the smallest normal half");
    check(cf_f64_from_f32_bits(0x3F800000u) == 1.0, "single one");
    check(cf_f64_from_f32_bits(0x00000001u) == 1.401298464324817e-45,
          "the smallest single");
    check(cf_f64_from_bits(cf_f64_bits(-0.5)) == -0.5, "bits and back");
}

#else

static void test_no_real(void) {
    refused("A1 61 68 F9 3E 00", "no floating point support", "a float without reals");
    refused("A1 61 68 F9 7C 00", "no floating point support", "infinity too");
}

#endif /* CF_WITH_REAL */

static void test_places(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_config*  c   = read(g_places, &err);
    static char ex[512];

    check(c != NULL, "nested maps and arrays read");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check(cf_get_int(c, "server.port", 0) == 8080, "a nested map");
    cf_explain(c, "server.port", ex, sizeof ex);
    has(ex, "doc, byte 14", "explain names the byte the value starts at");
    check(cf_get_int(c, "server.hosts.count", 0) == 2, "an array has its count");
    check_text(cf_get_text(c, "server.hosts.1", ""), "b", "an element");
    cf_explain(c, "server.hosts.1", ex, sizeof ex);
    has(ex, "doc, byte 26", "an element's byte");
    cf_config_free(c);
}

static void test_indefinite(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = read("BF 61 61 7F 65 73 74 72 65 61 64 6D 69 6E 67 FF 61 62 9F 01 "
                          "02 FF FF",
                          &err);

    check(c != NULL, "indefinite lengths read");
    if (c != NULL) {
        check_text(cf_get_text(c, "a", ""), "streaming", "chunks are joined");
        check(cf_get_int(c, "b.count", 0) == 2, "an indefinite array");
        check(cf_get_int(c, "b.1", 0) == 2, "its element");
    } else {
        printf("  %s\n", err.message);
    }
    cf_config_free(c);

    c = read("BF 7F 61 61 61 62 FF 01 FF", &err);
    check(c != NULL && cf_get_int(c, "ab", 0) == 1, "an indefinite key");
    cf_config_free(c);
}

static void test_tags(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c   = read("D9 D9 F7 A1 61 74 C0 74 32 30 31 33 2D 30 33 2D 32 31 54 32 "
                          "30 3A 30 34 3A 30 30 5A",
                          &err);

    check(c != NULL, "a self-described document with tag 0 reads");
    if (c != NULL) {
        check_text(cf_get_text(c, "t", ""), "2013-03-21T20:04:00Z",
                   "tag 0 is its text");
    } else {
        printf("  %s\n", err.message);
    }
    cf_config_free(c);

    c = read("A1 61 65 C1 1A 51 4B 67 B0", &err);
    check(c != NULL && cf_get_int(c, "e", 0) == 1363896240, "tag 1 is its number");
    cf_config_free(c);

    c = read("A1 61 75 D8 20 63 61 2F 62", &err);
    check(c != NULL && strcmp(cf_get_text(c, "u", ""), "a/b") == 0,
          "tag 32 is its text");
    cf_config_free(c);

    refused("A1 61 78 C4 82 21 19 6A B3", "tag 4", "a decimal fraction is refused");
    refused("A1 61 78 C4 82 21 19 6A B3", "which this reader does not read",
            "by what the reader does not do");
    refused("A1 61 74 C0 01", "must hold a text string", "tag 0 holds text");
    refused("A1 61 74 C1 61 61", "must hold a number", "tag 1 holds a number");
    refused("A1 61 74 C0 C0 60", "holds a tag", "a tag on a tag");
}

static void test_duplicates(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_config*  c   = read("A2 61 61 01 61 61 02", &err);
    static char ex[512];

    check(c != NULL, "duplicate keys read");
    if (c != NULL) {
        check(cf_get_int(c, "a", 0) == 2, "the later wins");
        cf_explain(c, "a", ex, sizeof ex);
        has(ex, "doc, byte 3", "the earlier is shown");
        has(ex, "doc, byte 6", "and the later");
    }
    cf_config_free(c);
}

static void test_refusals(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c;

    refused("A1 61 62 41 00", "a byte string", "a byte string");
    refused("A1 61 62 5F 41 00 FF", "a byte string", "a streamed byte string");
    refused("A1 41 61 01", "a byte string", "a byte string key");
    refused("A1 61 75 F7", "undefined", "undefined");
    refused("A1 61 73 F0", "simple value 16", "a simple value");
    refused("A1 61 73 F8 20", "simple value 32", "a two-byte simple value");
    refused("A1 01 02", "a map key that is an unsigned integer", "an integer key");
    refused("A1 20 02", "a map key that is a negative integer", "a negative key");
    refused("A1 80 02", "a map key that is an array", "an array key");
    refused("A1 61 62 1B FF FF FF FF FF FF FF FF", "past the 64-bit signed range",
            "an unsigned integer too large");
    refused("A1 61 6E 3B 80 00 00 00 00 00 00 00", "past the 64-bit signed range",
            "a negative integer too small");
    refused("82 01 02", "a map at the top, and this is an array",
            "an array at the top");
    refused("01", "a map at the top, and this is an unsigned integer", "a number");
    refused("C0 60", "a map at the top, and this is a tag", "a tag at the top");
    refused("", "the document is empty", "an empty document");
    refused("A1 61 61", "ends inside a data item", "a cut item");
    refused("A1 61", "a map of 1 pairs, and 1 bytes are left",
            "a map cut after a head");
    refused("A1 61 61 01 00", "bytes after the document", "a sequence");
    refused("A1 61 61 1C", "additional information 28 is reserved", "reserved");
    refused("A1 61 61 FF", "a break outside", "a stray break");
    refused("A1 61 61 F8 10", "a two-byte simple value below 32", "a short simple");
    refused("A1 61 61 62 C3 28", "not UTF-8", "bad UTF-8");
    refused("A1 62 C3 28 01", "not UTF-8", "bad UTF-8 in a key");
    refused("A1 61 61 9B 00 00 00 00 FF FF FF FF", "an array of 4294967295 items",
            "an array longer than the document");
    refused("A1 61 61 BB 00 00 00 00 FF FF FF FF", "a map of 4294967295 pairs",
            "a map longer than the document");
    refused("A1 61 61 7B 00 00 00 00 FF FF FF FF", "a length of 4294967295",
            "a string longer than the document");
    refused("BF 61 61 FF", "a key with no value", "an odd map");
    refused("A1 61 61 7F 41 00 FF", "a chunk of an indefinite-length string",
            "a chunk of the wrong type");
    refused("A1 61 61 7F 7F 60 FF FF", "a chunk of an indefinite-length string",
            "a chunk that is itself streamed");
    refused("A1 61 61 7F 62 C3 FF", "not UTF-8",
            "a chunk cut inside a character is refused by itself");
    refused("A1 61 61 3F", "major type 1 has no indefinite length", "major 1");
    refused("A1 61 61 DF", "major type 6 has no indefinite length", "major 6");
    refused("A1 61 61 1F", "major type 0 has no indefinite length", "major 0");
    refused("A1 61 61 5F", "a byte string", "a streamed byte string, cut");
    refused("EF BB BF A0", "a map at the top, and this is a simple value",
            "a byte order mark is not skipped");

    /* Head longer than it needs to be is well-formed. */
    c = read("A1 61 61 18 05", &err);
    check(c != NULL && cf_get_int(c, "a", 0) == 5, "a long head for 5 is read");
    cf_config_free(c);

    c = read("A1 61 6E 3B 7F FF FF FF FF FF FF FF", &err);
    check(c != NULL && cf_get_int(c, "n", 0) == CF_I64_MIN, "the least int64 is read");
    cf_config_free(c);

    c = read("A0", &err);
    check(c != NULL && cf_count(c) == 0, "an empty map is an empty configuration");
    cf_config_free(c);
}

static void test_depth(void) {
    unsigned char bytes[128];
    size_t        k, n;

    for (k = CF_MAX_DEPTH - 1; k <= (size_t)CF_MAX_DEPTH; ++k) {
        cf_error   err = CF_ERROR_INIT;
        cf_config* c;

        n          = 0;
        bytes[n++] = 0xA1;
        bytes[n++] = 0x61;
        bytes[n++] = 0x61;
        memset(bytes + n, 0x81, k);
        n += k;
        bytes[n++] = 0x00;
        c          = read_bytes(bytes, n, &err);
        if (k == (size_t)CF_MAX_DEPTH - 1) {
            /* The deepest array's count is the key "a", k times ".0" and
               ".count": a build whose keys are shorter cannot hold it. */
            if (2 * k + 6 < (size_t)CF_KEY_MAX) {
                check(c != NULL, "the deepest nesting reads");
            } else {
                check(c == NULL, "a key this build cannot hold is refused");
            }
        } else {
            check(c == NULL, "one level deeper is refused");
            has(err.message, "nests deeper than", "and says so");
        }
        cf_config_free(c);
    }
}

static void test_utf8(void) {
    static const char ok1[]   = "abc";
    static const char ok2[]   = "\xC3\xA9";
    static const char ok3[]   = "\xF0\x9F\x98\x80";
    static const char ok4[]   = "a\0b";
    static const char bad1[]  = "\xC0\x80";
    static const char bad2[]  = "\xED\xA0\x80";
    static const char bad3[]  = "\xF4\x90\x80\x80";
    static const char bad4[]  = "\xE2\x82";
    static const char bad5[]  = "\x80";
    static const char bad6[]  = "\xE0\x80\x80";
    static const char good1[] = "\xF4\x8F\xBF\xBF";

    check(cf_text_is_utf8("", 0), "empty");
    check(cf_text_is_utf8(ok1, 3), "ASCII");
    check(cf_text_is_utf8(ok2, 2), "two bytes");
    check(cf_text_is_utf8(ok3, 4), "four bytes");
    check(cf_text_is_utf8(ok4, 3), "a NUL is a character");
    check(cf_text_is_utf8(good1, 4), "U+10FFFF");
    check(!cf_text_is_utf8(bad1, 2), "overlong");
    check(!cf_text_is_utf8(bad2, 3), "a surrogate");
    check(!cf_text_is_utf8(bad3, 4), "past U+10FFFF");
    check(!cf_text_is_utf8(bad4, 2), "cut short");
    check(!cf_text_is_utf8(bad5, 1), "a continuation byte alone");
    check(!cf_text_is_utf8(bad6, 3), "overlong three bytes");
}

static void test_extension(void) {
    cf_error      err = CF_ERROR_INIT;
    cf_builder*   b   = cf_build(NULL);
    cf_config*    c;
    FILE*         f;
    unsigned char bytes[64];
    size_t        n = unhex(g_places, bytes, sizeof bytes);

    f = fopen(FILE_NAME, "wb");
    check(f != NULL, "wrote the file");
    if (f == NULL) {
        cf_builder_abort(b);
        return;
    }
    fwrite(bytes, 1, n, f);
    fclose(f);
    cf_builder_file(b, "file", CF_RANK_USER_FILE, FILE_NAME, NULL, 0);
    c = cf_builder_create(b, &err);
    check(c != NULL && cf_get_int(c, "server.port", 0) == 8080, "a .cbor file is read");
    cf_config_free(c);
    remove(FILE_NAME);
}

#if CF_WITH_EMIT

/* `n` bytes as "BF 61 ...", for a message. */
static void tohex(char* out, size_t cap, const unsigned char* p, size_t n) {
    static const char digits[] = "0123456789ABCDEF";
    size_t            i, o = 0;

    for (i = 0; i < n && o + 4 < cap; ++i) {
        out[o++] = digits[p[i] >> 4];
        out[o++] = digits[p[i] & 15u];
        out[o++] = ' ';
    }
    out[o] = '\0';
}

/* Whether `got` (n bytes) is the hex `want`, saying what it is when not. */
static void same_bytes(const unsigned char* got, size_t n, const char* want,
                       const char* what) {
    static unsigned char expect[512];
    static char          shown[1600];
    size_t               m = unhex(want, expect, sizeof expect);

    check(n == m && memcmp(got, expect, m) == 0, what);
    if (n != m || memcmp(got, expect, m) != 0) {
        tohex(shown, sizeof shown, got, n);
        printf("  got  %s\n  want %s\n", shown, want);
    }
}

/* A JSON document through the flattener into a key space, resolved, and
   written by the CBOR encoder as cf_doc_emit walks it, as
   test_roundtrip.c's encode() does: the bytes, compared with `want`. */
static void written(const char* json, const char* want, const char* what) {
    static union {
        cf_i64 align;
        char   b[CF_ENCODER_STATE_MAX];
    } state;
    static unsigned char got[512];
    const cf_codec*      cbor = cf_codec_by_name("cbor");
    const cf_codec*      js   = cf_codec_by_name("json");
    cf_arena*            ar;
    cf_error             err = CF_ERROR_INIT;
    cf_keyspace          ks;
    cf_flatten           fl;
    cf_blob              b;
    cf_doc_sink          sink;
    cf_sink              out;
    cf_bool              ok;

    if (js == NULL) {
        return; /* the document is JSON */
    }
    check(cbor != NULL && cbor->encoder != NULL, "the CBOR codec writes");
    if (cbor == NULL || cbor->encoder == NULL) {
        return;
    }
    ar = cf_arena_new(NULL, 1 << 14);
    if (ar == NULL) {
        check(0, "an arena");
        return;
    }
    cf_ks_init(&ks, ar, 32);
    cf_sink_init(&out, (char*)got, sizeof got);
    cf_blob_mem(&b, json, strlen(json), "json");
    cf_flatten_init(&fl, &ks, 10);
    ok = (js->decode(&b, cf_flatten_sink(&fl), &err) && !fl.failed && !fl.overflow)
             ? CF_TRUE
             : CF_FALSE;
    if (ok) {
        cf_ks_resolve(&ks);
        ok = (cbor->encoder(&out, state.b, &sink) && cf_doc_emit(&ks, &sink, &err))
                 ? CF_TRUE
                 : CF_FALSE;
    }
    check(ok, what);
    if (ok) {
        same_bytes(got, out.len, want, what);
    } else {
        printf("  %s\n", err.message);
    }
    cf_arena_free(ar);
}

static const char g_ints[] =
    "{\"b\":{\"yes\":true},\"n\":{\"zero\":0,\"small\":23,\"byte\":24,"
    "\"k\":1000,\"neg\":-1000,\"big\":1000000000000},\"t\":{\"s\":\"IETF\"}}";

static const char g_ints_cbor[] =
    "BF 61 62 BF 63 79 65 73 F5 FF 61 6E BF 63 62 69 67 1B 00 00 00 E8 D4 A5 "
    "10 00 64 62 79 74 65 18 18 61 6B 19 03 E8 63 6E 65 67 39 03 E7 65 73 6D "
    "61 6C 6C 17 64 7A 65 72 6F 00 FF 61 74 BF 61 73 64 49 45 54 46 FF FF";

#if CF_WITH_REAL

static const char g_reals[] =
    "{\"r\":{\"half\":1.5,\"single\":100000.0,\"double\":1.1}}";

static const char g_reals_cbor[] =
    "BF 61 72 BF 66 64 6F 75 62 6C 65 FB 3F F1 99 99 99 99 99 9A 64 68 61 6C "
    "66 F9 3E 00 66 73 69 6E 67 6C 65 FA 47 C3 50 00 FF FF";

#endif

static void test_write_bytes(void) {
    written(g_ints, g_ints_cbor,
            "integers, a boolean and text, each in its shortest head");
#if CF_WITH_REAL
    written(g_reals, g_reals_cbor, "a real in the fewest bytes that hold it");
#endif
}

/* cf_emit has the encoder double a ${ itself: the length is written before
   the text, so nothing can be doubled after it. */
static void test_write_refs(void) {
    static const cf_pair pairs[] = {{"t", "a$${b}"}};
    static char          buf[64];
    cf_error             err = CF_ERROR_INIT;
    cf_builder*          b   = cf_build(NULL);
    cf_config*           c;
    cf_config*           back;
    size_t               n;

    cf_builder_memory(b, "m", CF_RANK_USER_FILE, pairs, 1);
    c = cf_builder_create(b, &err);
    check(c != NULL, "a literal ${ builds");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    check_text(cf_get_text(c, "t", ""), "a${b}", "the value is the literal a${b}");
    n = cf_emit(c, "cbor", CF_EMIT_CONFIGURED, 0, buf, sizeof buf, &err);
    check(n == 11, "cf_emit writes 11 bytes");
    if (n == 0) {
        printf("  %s\n", err.message);
    }
    same_bytes((const unsigned char*)buf, n, "BF 61 74 66 61 24 24 7B 62 7D FF",
               "the ${ is doubled, and the length counts it");
    check(cf_emit(c, "cbor", CF_EMIT_CONFIGURED, 0, NULL, 0, &err) == 11,
          "measured without a buffer, the same length");
    back = read_bytes((const unsigned char*)buf, n, &err);
    check(back != NULL, "what was written reads");
    if (back != NULL) {
        check_text(cf_get_text(back, "t", ""), "a${b}", "and reads back as a${b}");
    }
    cf_config_free(back);
    cf_config_free(c);
}

#endif /* CF_WITH_EMIT */

#else /* !CF_WITH_CBOR */

static void test_off(void) {
    cf_error                   err = CF_ERROR_INIT;
    cf_builder*                b   = cf_build(NULL);
    cf_config*                 c;
    FILE*                      f;
    static const unsigned char doc[] = {0xA0};

    check(cf_codec_by_name("cbor") == NULL, "no codec");
    check(strcmp(cf_format_option("cbor"), "CF_WITH_CBOR") == 0, "the option is named");
    f = fopen(FILE_NAME, "wb");
    if (f != NULL) {
        fwrite(doc, 1, sizeof doc, f);
        fclose(f);
    }
    cf_builder_file(b, "file", CF_RANK_USER_FILE, FILE_NAME, NULL, 0);
    c = cf_builder_create(b, &err);
    check(c == NULL, "a .cbor file is refused without CF_WITH_CBOR");
    has(err.message, "CF_WITH_CBOR", "and the error names the option");
    cf_config_free(c);
    remove(FILE_NAME);
}

#endif /* CF_WITH_CBOR */

int main(void) {
#if CF_WITH_CBOR
    test_scalars();
#if CF_WITH_REAL
    test_floats();
    test_f16();
#else
    test_no_real();
#endif
    test_places();
    test_indefinite();
    test_tags();
    test_duplicates();
    test_refusals();
    test_depth();
    test_utf8();
    test_extension();
#if CF_WITH_EMIT
    test_write_bytes();
    test_write_refs();
#endif
#else
    test_off();
#endif
    return cf_test_done();
}
