/*
 * test_tlv.c - TLV by a layout the program gives (D-93): a layout's
 * checks, a flat document read through the codec made of it, each value
 * placed by the byte its tag starts at, and the refusals by name. Every
 * document is written here from the layout; no device's bytes are kept.
 */

#include <configforge/cf_serialize.h>
#include <configforge/configforge.h>

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "cf_test.h"

#define FILE_NAME "cf_tlv_test.bin"

/* One cf_error for the whole test, made fresh for each use: kept off the
   frames, which -O3 inlines into main past the stack budget, as the
   documents are kept static. */
static cf_error* fresh_error(void) {
    static const cf_error fresh = CF_ERROR_INIT;
    static cf_error       err;

    err = fresh;
    return &err;
}

#if CF_WITH_TLV

/* A device's layout: two-byte tags and lengths, little-endian, ending at
   FFFF. Its tags in ascending order. */
static const cf_tlv_tag dev_tags[] = {
    {0x0001, "name", CF_TLV_TEXT, 0, 0, 0, NULL, 0, NULL},
    {0x0002, "net.port", CF_TLV_UINT, 2, 0, 0, NULL, 0, NULL},
    {0x0003, "net.dhcp", CF_TLV_BOOL, 1, 0, 0, NULL, 0, NULL},
    {0x0004, "offset", CF_TLV_INT, 4, CF_TLV_BIG, 0, NULL, 0, NULL},
#if CF_WITH_REAL
    {0x0005, "gain", CF_TLV_REAL, 4, 0, 0, NULL, 0, NULL},
#endif
    {0x0010, NULL, CF_TLV_RECORD, 12, 0, CF_TLV_SKIP, NULL, 0, NULL},
    {0x0011, NULL, CF_TLV_RECORD, 8, 0, 0, NULL, 0, NULL}};

static cf_tlv_layout dev_layout(void) {
    cf_tlv_layout l = CF_TLV_LAYOUT_INIT;

    l.name         = "devcfg";
    l.tag_width    = 2;
    l.length_width = 2;
    l.order        = CF_TLV_LITTLE;
    l.flags        = CF_TLV_END;
    l.end_tag      = 0xFFFFu;
    l.tags         = dev_tags;
    l.ntags        = sizeof dev_tags / sizeof dev_tags[0];
    return l;
}

/* One element: the tag and the length little-endian, then the value. */
static void put(unsigned char* doc, size_t* n, cf_u32 tag, const void* value,
                size_t len) {
    doc[(*n)++] = (unsigned char)(tag & 0xFFu);
    doc[(*n)++] = (unsigned char)((tag >> 8) & 0xFFu);
    doc[(*n)++] = (unsigned char)(len & 0xFFu);
    doc[(*n)++] = (unsigned char)((len >> 8) & 0xFFu);
    if (len > 0) {
        memcpy(doc + *n, value, len);
    }
    *n += len;
}

static void put_end(unsigned char* doc, size_t* n) {
    doc[(*n)++] = 0xFF;
    doc[(*n)++] = 0xFF;
}

/* The document every reading test starts from. */
static size_t dev_document(unsigned char* doc) {
    static const unsigned char name[]   = {'u', 'n', 'i', 't', '-', '7', 0, 0};
    static const unsigned char port[]   = {0x90, 0x01};
    static const unsigned char dhcp[]   = {0x01};
    static const unsigned char offset[] = {0xFF, 0xFF, 0xFF, 0xFE};
    static const unsigned char record[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    size_t                     n        = 0;

    put(doc, &n, 0x0001, name, sizeof name);
    put(doc, &n, 0x0002, port, sizeof port);
    put(doc, &n, 0x0003, dhcp, sizeof dhcp);
    put(doc, &n, 0x0004, offset, sizeof offset);
#if CF_WITH_REAL
    {
        static const unsigned char gain[] = {0x00, 0x00, 0xC0, 0x3F};
        put(doc, &n, 0x0005, gain, sizeof gain);
    }
#endif
    put(doc, &n, 0x0010, record, sizeof record);
    put_end(doc, &n);
    return n;
}

static cf_codec      g_devcfg;
static cf_tlv_layout g_dev;

static cf_config* read_doc(const unsigned char* doc, size_t n, cf_error* err) {
    cf_builder* b = cf_build(NULL);

    cf_builder_codec(b, &g_devcfg);
    cf_builder_document(b, "dev.cfg", CF_RANK_USER_FILE, (const char*)doc, n, "devcfg",
                        0);
    return cf_builder_create(b, err);
}

static void check_dev(cf_config* c, const char* what) {
    static char ex[512];

    check(c != NULL, what);
    if (c == NULL) {
        return;
    }
    check_text(cf_get_text(c, "name", ""), "unit-7", "text, its padding dropped");
    check(cf_get_int(c, "net.port", 0) == 400, "an unsigned integer, little-endian");
    check(cf_get_bool(c, "net.dhcp", CF_FALSE) == CF_TRUE, "a bool");
    check(cf_get_int(c, "offset", 0) == -2, "a signed integer in its own order");
#if CF_WITH_REAL
    check(cf_get_real(c, "gain", 0.0) == 1.5, "a single-precision real");
#endif
    cf_explain(c, "net.port", ex, sizeof ex);
    has(ex, "dev.cfg, byte 12", "explain names the byte of the value's tag");
}

static void test_flat(void) {
    cf_error*            err = fresh_error();
    static unsigned char doc[128];
    size_t               n = dev_document(doc);
    cf_config*           c = read_doc(doc, n, err);

    if (c == NULL) {
        printf("  %s\n", err->message);
    }
    check_dev(c, "a flat document reads");
    cf_config_free(c);
}

static void test_file(void) {
    cf_error*            err = fresh_error();
    static unsigned char doc[128];
    size_t               n = dev_document(doc);
    FILE*                f = fopen(FILE_NAME, "wb");
    cf_builder*          b;
    cf_config*           c;

    check(f != NULL, "the file opens");
    if (f == NULL) {
        return;
    }
    fwrite(doc, 1, n, f);
    fclose(f);
    b = cf_build(NULL);
    cf_builder_codec(b, &g_devcfg);
    cf_builder_file(b, "file", CF_RANK_USER_FILE, FILE_NAME, "devcfg", 0);
    c = cf_builder_create(b, err);
    if (c == NULL) {
        printf("  %s\n", err->message);
    } else {
        check(cf_get_int(c, "net.port", 0) == 400, "a file named for the layout reads");
    }
    cf_config_free(c);
    remove(FILE_NAME);
}

/* One document refused, and the words its message holds. */
static void refused(const unsigned char* doc, size_t n, const char* words,
                    const char* what) {
    cf_error*  err = fresh_error();
    cf_config* c   = read_doc(doc, n, err);

    check(c == NULL, what);
    has(err->message, words, what);
    cf_config_free(c);
}

static void test_read_refusals(void) {
    static const unsigned char eight[] = {1, 2, 3, 4, 5, 6, 7, 8};
    static const unsigned char three[] = {1, 2, 3};
    static const unsigned char two[]   = {2};
    static const unsigned char bad[]   = {0xC3, 0x28};
    static const unsigned char nul[]   = {0x61, 0x00, 0x62};
    static unsigned char       doc[64];
    size_t                     n;

    n = 0;
    put(doc, &n, 0x0011, eight, sizeof eight);
    put_end(doc, &n);
    refused(doc, n, "a record of 8 bytes", "a record the layout does not read");

    n = 0;
    put(doc, &n, 0x0099, two, sizeof two);
    put_end(doc, &n);
    refused(doc, n, "tag 0x0099 is not in the layout 'devcfg'",
            "a tag not in the layout");

    n = 0;
    put(doc, &n, 0x0002, three, sizeof three);
    put_end(doc, &n);
    refused(doc, n, "is 3 bytes, and its entry says 2", "a fixed width not kept");

    n = 0;
    put(doc, &n, 0x0001, eight, 4);
    doc[2] = 0x2C; /* a length of 300 */
    doc[3] = 0x01;
    refused(doc, n, "says 300 bytes, and 4 are left", "a length past the end");

    n = 0;
    put(doc, &n, 0x0003, two, sizeof two);
    put_end(doc, &n);
    refused(doc, n, "a bool is 0 or 1", "a bool that is neither");

    n = 0;
    put(doc, &n, 0x0001, bad, sizeof bad);
    put_end(doc, &n);
    refused(doc, n, "not UTF-8", "text that is not UTF-8");

    n = 0;
    put(doc, &n, 0x0001, nul, sizeof nul);
    put_end(doc, &n);
    refused(doc, n, "a NUL inside", "text with a NUL inside");

    n = 0;
    put(doc, &n, 0x0003, eight, 1);
    refused(doc, n, "without the end tag 0xFFFF", "no end tag");

    n = 0;
    put_end(doc, &n);
    doc[n++] = 0;
    refused(doc, n, "1 bytes after the end tag", "a byte after the end");

    refused(doc, 0, "the document is empty", "an empty document");

    doc[0] = 0x01;
    refused(doc, 1, "a tag cut short", "a tag cut short");
}

/* One layout refused, and the words its message holds. */
static void bad_layout(const cf_tlv_layout* l, const char* words, const char* what) {
    cf_error* err = fresh_error();
    cf_codec  c;

    check(!cf_tlv_codec(&c, l, err), what);
    has(err->message, words, what);
}

static void test_layout_refusals(void) {
    cf_tlv_layout l;
    cf_tlv_tag    t[2];
    cf_codec      old;
    cf_error*     err = fresh_error();

    l           = dev_layout();
    l.tag_width = 3;
    bad_layout(&l, "1, 2 or 4 bytes wide, not 3", "a tag three bytes wide");

    l       = dev_layout();
    t[0]    = dev_tags[1];
    t[1]    = dev_tags[0];
    l.tags  = t;
    l.ntags = 2;
    bad_layout(&l, "is listed after", "tags out of order");

    t[0] = dev_tags[1];
    t[1] = dev_tags[1];
    bad_layout(&l, "is listed after", "a tag listed twice");

    t[0]     = dev_tags[0];
    t[1]     = dev_tags[1];
    t[1].key = "Net.Port";
    bad_layout(&l, "is not a valid key", "a key that is not one");

    t[1]       = dev_tags[1];
    t[1].type  = CF_TLV_INT;
    t[1].width = 3;
    bad_layout(&l, "an int of 3 bytes", "an integer three bytes wide");

    t[1]      = dev_tags[1];
    t[1].more = &l;
    bad_layout(&l, "has `more` set", "a tag with more");

    l      = dev_layout();
    l.size = 0;
    bad_layout(&l, "CF_TLV_LAYOUT_INIT", "a layout without its size");

    l = dev_layout();
    check(!cf_tlv_codec_sized(&old, offsetof(cf_codec, decode_self), &l, err),
          "a codec from before 1.5");
    has(err->message, "from before 1.5", "has no room for a layout");
}

/* ------------------------------------------------------------------ *
 * Nesting, lists and unknown tags: a network layout of one-byte tags
 * and lengths, big-endian, with no end tag.
 * ------------------------------------------------------------------ */

static const cf_tlv_tag servers_tags[] = {
    {0x01, "addr", CF_TLV_TEXT, 0, 0, 0, NULL, 0, NULL},
    {0x02, "port", CF_TLV_UINT, 2, 0, 0, NULL, 0, NULL}};

static const cf_tlv_tag net_tags[] = {
    {0x01, "host", CF_TLV_TEXT, 0, 0, 0, NULL, 0, NULL},
    {0x02, "servers", CF_TLV_NEST, 0, 0, CF_TLV_LIST, servers_tags, 2, NULL},
    {0x03, "amount", CF_TLV_BCD, 0, 0, 0, NULL, 0, NULL},
    {0x04, "mac", CF_TLV_HEX, 6, 0, 0, NULL, 0, NULL}};

static cf_tlv_layout net_layout(int unknown) {
    cf_tlv_layout l = CF_TLV_LAYOUT_INIT;

    l.name         = "net";
    l.tag_width    = 1;
    l.length_width = 1;
    l.order        = CF_TLV_BIG;
    l.unknown      = unknown;
    l.tags         = net_tags;
    l.ntags        = sizeof net_tags / sizeof net_tags[0];
    return l;
}

/* One element of one-byte tag and length. */
static void put1(unsigned char* doc, size_t* n, unsigned tag, const void* value,
                 size_t len) {
    doc[(*n)++] = (unsigned char)tag;
    doc[(*n)++] = (unsigned char)len;
    if (len > 0) {
        memcpy(doc + *n, value, len);
    }
    *n += len;
}

/* A server's nest: its address and its port. */
static void put_server(unsigned char* doc, size_t* n, const char* addr, unsigned port) {
    static unsigned char inner[32];
    unsigned char        p[2];
    size_t               k = 0;

    p[0] = (unsigned char)(port >> 8);
    p[1] = (unsigned char)(port & 0xFFu);
    put1(inner, &k, 0x01, addr, strlen(addr));
    put1(inner, &k, 0x02, p, 2);
    put1(doc, n, 0x02, inner, k);
}

static size_t net_document(unsigned char* doc, cf_bool apart) {
    static const unsigned char amount[] = {0x00, 0x00, 0x00, 0x01, 0x23, 0x45};
    static const unsigned char mac[]    = {0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
    size_t                     n        = 0;

    put1(doc, &n, 0x01, "web", 3);
    put_server(doc, &n, "10.0.0.1", 8080);
    if (apart) {
        put1(doc, &n, 0x03, amount, sizeof amount);
    }
    put_server(doc, &n, "10.0.0.2", 9104);
    put1(doc, &n, 0x03, amount, sizeof amount);
    put1(doc, &n, 0x04, mac, sizeof mac);
    return n;
}

static cf_config* read_with(const cf_tlv_layout* l, const unsigned char* doc, size_t n,
                            cf_error* err) {
    static cf_codec codec;
    cf_builder*     b = cf_build(NULL);

    if (!cf_tlv_codec(&codec, l, err)) {
        cf_builder_abort(b);
        return NULL;
    }
    cf_builder_codec(b, &codec);
    cf_builder_document(b, "net.bin", CF_RANK_USER_FILE, (const char*)doc, n, l->name,
                        0);
    return cf_builder_create(b, err);
}

static void test_nested(void) {
    cf_tlv_layout        l   = net_layout(CF_TLV_UNKNOWN_REFUSE);
    cf_error*            err = fresh_error();
    static unsigned char doc[128];
    size_t               n = net_document(doc, CF_FALSE);
    cf_config*           c = read_with(&l, doc, n, err);
    static char          ex[512];

    check(c != NULL, "nests, a list, BCD and hex read");
    if (c == NULL) {
        printf("  %s\n", err->message);
        return;
    }
    check_text(cf_get_text(c, "host", ""), "web", "text beside them");
    check(cf_get_int(c, "servers.count", 0) == 2, "a list of nests is an array");
    check(cf_get_int(c, "servers.1.port", 0) == 9104, "each a table");
    check_text(cf_get_text(c, "servers.0.addr", ""), "10.0.0.1", "with its members");
    check(cf_get_int(c, "amount", 0) == 12345, "BCD, its leading zeros digits");
    check_text(cf_get_text(c, "mac", ""), "001a2b3c4d5e", "hex, lowercase");
    cf_explain(c, "servers.0.addr", ex, sizeof ex);
    has(ex, "net.bin, byte 7", "a member is placed in the document, not in its nest");
    cf_config_free(c);
}

static void net_refused(const cf_tlv_layout* l, const unsigned char* doc, size_t n,
                        const char* words, const char* what) {
    cf_error*  err = fresh_error();
    cf_config* c   = read_with(l, doc, n, err);

    check(c == NULL, what);
    has(err->message, words, what);
    cf_config_free(c);
}

/* A nest whose tags are its own table: as deep as the document makes it. */
static const cf_tlv_tag deep_tags[1] = {
    {0x01, "d", CF_TLV_NEST, 0, 0, 0, deep_tags, 1, NULL}};

static void test_nested_refusals(void) {
    static const unsigned char nibble[] = {0x0A};
    static const unsigned char two[]    = {0x01, 0x02};
    cf_tlv_layout              l        = net_layout(CF_TLV_UNKNOWN_REFUSE);
    cf_error*                  err      = fresh_error();
    static unsigned char       doc[128];
    size_t                     n = net_document(doc, CF_TRUE);
    cf_config*                 c;
    int                        i;

    net_refused(&l, doc, n, "continues a list that ended at byte",
                "a list in two runs");

    n = 0;
    put1(doc, &n, 0x03, nibble, 1);
    net_refused(&l, doc, n, "a nibble that is not a digit", "BCD that is not decimal");

    n = 0;
    put1(doc, &n, 0x09, two, 2);
    net_refused(&l, doc, n, "tag 0x09 is not in the layout 'net'",
                "an unknown tag refused");
    l = net_layout(CF_TLV_UNKNOWN_SKIP);
    c = read_with(&l, doc, n, err);
    check(c != NULL && !cf_has(c, "tag-09"), "skipped");
    cf_config_free(c);
    l = net_layout(CF_TLV_UNKNOWN_KEEP);
    c = read_with(&l, doc, n, err);
    check(c != NULL, "or kept");
    if (c != NULL) {
        check_text(cf_get_text(c, "tag-09", ""), "0102", "under tag-<hex>, as hex");
    }
    cf_config_free(c);

    /* 33 nests, each the only member of the one around it. */
    l.unknown = CF_TLV_UNKNOWN_REFUSE;
    l.tags    = deep_tags;
    l.ntags   = 1;
    n         = 0;
    for (i = 0; i < 33; ++i) {
        doc[n++] = 0x01;
        doc[n++] = (unsigned char)(2 * (32 - i));
    }
    net_refused(&l, doc, n, "nests deeper than", "nests past the depth");
}

/* ------------------------------------------------------------------ *
 * BER-TLV, as a payment card's record has it. The numbers are made up
 * for the test: a PAN no card has.
 * ------------------------------------------------------------------ */

static const cf_tlv_tag emv_issuer[] = {
    {0x50, "label", CF_TLV_TEXT, 0, 0, 0, NULL, 0, NULL}};

static const cf_tlv_tag emv_record[] = {
    {0x5A, "pan", CF_TLV_BCD, 0, 0, 0, NULL, 0, NULL},
    {0x5F24, "expiry", CF_TLV_BCD, 3, 0, 0, NULL, 0, NULL},
    {0x9F02, "amount", CF_TLV_BCD, 6, 0, 0, NULL, 0, NULL},
    {0xBF0C, "issuer", CF_TLV_NEST, 0, 0, 0, emv_issuer, 1, NULL}};

static const cf_tlv_tag emv_tags[] = {
    {0x70, "record", CF_TLV_NEST, 0, 0, 0, emv_record, 4, NULL}};

static cf_tlv_layout emv_layout(void) {
    cf_tlv_layout l = CF_TLV_LAYOUT_INIT;

    l.name  = "emv";
    l.kind  = CF_TLV_BER;
    l.flags = CF_TLV_PADDING;
    l.tags  = emv_tags;
    l.ntags = 1;
    return l;
}

static size_t emv_document(unsigned char* doc) {
    static const unsigned char record[] = {
        0x5A, 0x08, 0x47, 0x61, 0x73, 0x90, 0x01, 0x01, 0x00, 0x10, /* pan    */
        0x5F, 0x24, 0x03, 0x25, 0x12, 0x31,                         /* expiry */
        0x9F, 0x02, 0x06, 0x00, 0x00, 0x00, 0x01, 0x23, 0x45,       /* amount */
        0xBF, 0x0C, 0x06, 0x50, 0x04, 'T',  'E',  'S',  'T'};       /* issuer */
    size_t n = 0;

    doc[n++] = 0x00; /* padding */
    doc[n++] = 0x00;
    doc[n++] = 0x70;
    doc[n++] = 0x81; /* a long-form length, on purpose */
    doc[n++] = (unsigned char)sizeof record;
    memcpy(doc + n, record, sizeof record);
    n += sizeof record;
    doc[n++] = 0xFF; /* padding */
    return n;
}

static void test_ber(void) {
    cf_tlv_layout        l   = emv_layout();
    cf_error*            err = fresh_error();
    static unsigned char doc[64];
    size_t               n = emv_document(doc);
    cf_config*           c = read_with(&l, doc, n, err);
    static char          ex[512];

    check(c != NULL, "a BER-TLV record reads");
    if (c == NULL) {
        printf("  %s\n", err->message);
        return;
    }
    /* 4761739001010010, as an expression: C89 has no long long literal */
    check(cf_get_int(c, "record.pan", 0) == (cf_i64)47617390 * 100000000 + 1010010,
          "a BCD PAN");
    check(cf_get_int(c, "record.expiry", 0) == 251231, "a two-byte tag");
    check(cf_get_int(c, "record.amount", 0) == 12345, "EMV's amount");
    check_text(cf_get_text(c, "record.issuer.label", ""), "TEST",
               "a constructed tag inside");
    cf_explain(c, "record.amount", ex, sizeof ex);
    has(ex, "net.bin, byte 21", "a value placed at its tag's first byte");
    cf_config_free(c);
}

static void test_ber_refusals(void) {
    static const unsigned char long_tag[] = {0x9F, 0x82, 0x80, 0x80, 0x80, 0x01};
    static const unsigned char indef[]    = {0x70, 0x80, 0x00, 0x00};
    static const unsigned char wide[]     = {0x5A, 0x85, 0x00, 0x00,
                                             0x00, 0x00, 0x01, 0x00};
    cf_tlv_layout              l          = emv_layout();
    cf_tlv_tag                 prim[1];
    cf_codec                   codec;
    cf_error*                  err = fresh_error();
    static unsigned char       doc[64];
    size_t                     n;

    net_refused(&l, long_tag, sizeof long_tag, "a tag longer than 4 bytes",
                "a long tag");
    net_refused(&l, indef, sizeof indef, "an indefinite length",
                "an indefinite length");
    net_refused(&l, wide, sizeof wide, "a length of 5 bytes",
                "a length past four bytes");

    prim[0]        = emv_tags[0];
    prim[0].type   = CF_TLV_TEXT;
    prim[0].inner  = NULL;
    prim[0].ninner = 0;
    l.tags         = prim;
    check(!cf_tlv_codec(&codec, &l, err), "a constructed tag that is not a nest");
    has(err->message, "constructed bit", "is refused by its constructed bit");

    l       = emv_layout();
    l.flags = 0;
    n       = emv_document(doc);
    net_refused(&l, doc, n, "tag 0x00 is not in the layout",
                "padding without the flag");
}

int main(void) {
    cf_error* err = fresh_error();

    g_dev = dev_layout();
    check(cf_tlv_codec(&g_devcfg, &g_dev, err), "the layout makes a codec");
    if (err->message[0] != '\0') {
        printf("  %s\n", err->message);
    }
    {
        char kind[160];
        cf_codec_kind(&g_devcfg, kind, sizeof kind);
        check_text(kind, "binary, schema-bound, typed, arrays, read",
                   "a layout's codec is a binary one that needs the layout");
    }
    test_flat();
    test_file();
    test_read_refusals();
    test_layout_refusals();
    test_nested();
    test_nested_refusals();
    test_ber();
    test_ber_refusals();
    return cf_test_done();
}

#else /* !CF_WITH_TLV */

static void test_off(void) {
    cf_tlv_layout l   = CF_TLV_LAYOUT_INIT;
    cf_error*     err = fresh_error();
    cf_codec      c;

    l.name = "devcfg";
    check(!cf_tlv_codec(&c, &l, err), "a build without TLV makes no codec");
    has(err->message, "CF_WITH_TLV", "and names the option");
}

int main(void) {
    test_off();
    return cf_test_done();
}

#endif /* CF_WITH_TLV */
