/*
 * test_fuzz.c - malformed input, systematically.
 *
 * There are a dozen hand-written refusal cases in the codec tests, and
 * they cover the mistakes someone thought of. This covers the ones
 * nobody did: take a valid document, corrupt it, and require that the
 * parser either succeeds or returns an error. Never a crash, never a
 * read past the end, never a hang.
 *
 * Deterministic on purpose. A fuzzer that uses rand() finds a different
 * bug on every machine and none of them twice; here the seed is the
 * bug report, and the generator is written out because rand() differs
 * between implementations and is not reproducible across platforms.
 *
 * The most valuable target is the binary image: it is the one reader
 * that dereferences offsets taken from the input, so a corrupt one is
 * the only input here that could reach memory that is not there.
 *
 * Under the sanitize preset this is also an AddressSanitizer run, which
 * is where it earns its place - the checks below would pass on a read
 * one byte past the end of a buffer, and ASan would not.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cf_test.h"

/* xorshift64*, written out rather than taken from the C library: the
   sequence has to be the same on every platform for a failing seed to
   mean anything to the person who receives it. */
static cf_u64 g_state;

static void seed(cf_u64 s) {
    /* 88172645463325252 and, below, 2685821657736338717, as expressions:
       on a 32-bit long the literals are long longs, which C89 has not. */
    g_state = (s != 0) ? s : (((cf_u64)20529293U << 32) | (cf_u64)3418323524U);
}

static cf_u64 next(void) {
    g_state ^= g_state >> 12;
    g_state ^= g_state << 25;
    g_state ^= g_state >> 27;
    return g_state * (((cf_u64)625341585U << 32) | (cf_u64)1332534557U);
}

static size_t pick(size_t n) {
    return (n == 0) ? 0 : (size_t)(next() % (cf_u64)n);
}

/* ---------------------------------------------------------------- *
 * Mutation
 * ---------------------------------------------------------------- */

#define MAX_INPUT 8192

/* What a reader may spend on an input (H-4, D-82): four times what the
   library promises it makes of the key space, since the key space spends
   about 100 bytes on a key where the budget charges 64, and doubles its
   arrays as they grow; plus the floor, and the cost of an empty input. A
   reader over it makes a multiple of its input, and that is a finding
   whether or not the reader stopped in time.

   FUZZ_COST_EMPTY: the largest was 5376 bytes, TOML's, on 2026-11-13 (gcc
   Debug, Linux); rounded up to a power of two and doubled. */
#define FUZZ_COST_EMPTY 16384u
#define FUZZ_COST_FACTOR (4u * CF_DECODE_FACTOR)
#define FUZZ_COST_FLOOR (CF_DECODE_FLOOR + FUZZ_COST_EMPTY)

typedef struct blob {
    char   b[MAX_INPUT];
    size_t n;
} blob;

static void mutate(blob* v) {
    int kind = (int)pick(6);

    if (v->n == 0) {
        return;
    }
    switch (kind) {
    case 0: /* flip a bit */
        v->b[pick(v->n)] ^= (char)(1u << pick(8));
        break;
    case 1: /* set a byte, favouring the structural characters */ {
        static const char interesting[] = "{}[]\",:=\n\0\xff.0123456789";
        v->b[pick(v->n)]                = interesting[pick(sizeof interesting - 1)];
        break;
    }
    case 2: /* truncate: the commonest real corruption there is */
        v->n = pick(v->n);
        break;
    case 3: /* duplicate a run */ {
        size_t at  = pick(v->n);
        size_t len = pick(v->n - at) + 1;
        if (v->n + len < MAX_INPUT) {
            memmove(v->b + at + len, v->b + at, v->n - at);
            v->n += len;
        }
        break;
    }
    case 4: /* delete a run */ {
        size_t at  = pick(v->n);
        size_t len = pick(v->n - at) + 1;
        memmove(v->b + at, v->b + at + len, v->n - at - len);
        v->n -= len;
        break;
    }
    default: /* swap two bytes */ {
        size_t i = pick(v->n), j = pick(v->n);
        char   t = v->b[i];
        v->b[i]  = v->b[j];
        v->b[j]  = t;
        break;
    }
    }
}

static void load(blob* v, const char* text) {
    v->n = strlen(text);
    if (v->n > MAX_INPUT) {
        v->n = MAX_INPUT;
    }
    memcpy(v->b, text, v->n);
}

/* A corpus of bytes, which may hold a zero: a binary format's. */
static void load_bytes(blob* v, const unsigned char* p, size_t n) {
    v->n = (n > MAX_INPUT) ? MAX_INPUT : n;
    memcpy(v->b, p, v->n);
}

/* ---------------------------------------------------------------- *
 * Targets
 *
 * Every run uses a fresh fixed arena, so a parser that leaks or
 * over-allocates runs out rather than growing until the machine
 * notices.
 * ---------------------------------------------------------------- */

static char g_arena[1 << 16];

/* feed_codec's own arena, large enough that a costly input is measured
   rather than cut off: with inputs of at most MAX_INPUT bytes the largest
   cost allowed below is under 4 MiB, so an input over it either fits and
   is measured or exhausts the arena, and is then over it too. */
static char g_codec_arena[(size_t)1 << 22];

/* The bytes read through `codec`, by cf_codec_decode: a built-in one, or
   one made from a layout (feed_tlv). */
static void feed_through(const cf_codec* codec, const blob* v) {
    cf_arena*   ar;
    cf_keyspace ks;
    cf_flatten  fl;
    cf_blob     b;
    cf_error    err = CF_ERROR_INIT;
    size_t      used;

    if (codec == NULL) {
        return;
    }
    ar = cf_arena_fixed(g_codec_arena, sizeof g_codec_arena);
    if (ar == NULL || !cf_ks_init(&ks, ar, 32)) {
        return;
    }
    cf_blob_mem(&b, v->b, v->n, "fuzz");
    cf_flatten_init(&fl, &ks, 0);

    /* The only contract: it returns. Success and failure are both fine;
       a parser that refuses a corrupted document is doing its job, and
       one that accepts it has produced a configuration from garbage,
       which is also legal - the garbage was a valid document. */
    if (cf_codec_decode(codec, &b, cf_flatten_sink(&fl), &err)) {
        cf_ks_resolve(&ks);
        cf_ks_infer_untyped(&ks);
        /* Reading what it produced is part of the target: a key space
           built from nonsense must still be searchable. */
        (void)cf_ks_get(&ks, "a", 1);
        (void)cf_ks_digest(&ks);
    } else {
        check(err.message[0] != '\0', "a refusal always says why");
    }
    used = cf_arena_used(ar);
    if (used > (size_t)FUZZ_COST_FACTOR * v->n + (size_t)FUZZ_COST_FLOOR) {
        printf("  %s: %lu bytes of input cost %lu bytes\n", codec->name,
               (unsigned long)v->n, (unsigned long)used);
        check(0, "no reader spends a multiple of its input");
    }
}

static void feed_codec(const char* format, const blob* v) {
    feed_through(cf_codec_by_name(format), v);
}

#if CF_WITH_TLV
/* Two layouts of the fuzzer's own (D-93), each made into a codec once:
   one of fixed widths with a tag of each type, a list of nests among
   them, and unknown tags kept, so that every path of the reader has a
   way in; and a BER-TLV one with padding, a nest and BCD. */
static const cf_tlv_tag fz_server_tags[] = {
    {0x0001, "addr", CF_TLV_TEXT, 0, 0, 0, NULL, 0, NULL},
    {0x0002, "port", CF_TLV_UINT, 2, 0, 0, NULL, 0, NULL}};

static const cf_tlv_tag fz_tlv_tags[] = {
    {0x0001, "name", CF_TLV_TEXT, 0, 0, 0, NULL, 0, NULL},
    {0x0002, "net.port", CF_TLV_UINT, 2, 0, 0, NULL, 0, NULL},
    {0x0003, "net.dhcp", CF_TLV_BOOL, 1, 0, 0, NULL, 0, NULL},
    {0x0004, "offset", CF_TLV_INT, 4, CF_TLV_BIG, 0, NULL, 0, NULL},
#if CF_WITH_REAL
    {0x0005, "gain", CF_TLV_REAL, 4, 0, 0, NULL, 0, NULL},
#endif
    {0x0006, "amount", CF_TLV_BCD, 0, 0, 0, NULL, 0, NULL},
    {0x0007, "mac", CF_TLV_HEX, 0, 0, 0, NULL, 0, NULL},
    {0x0008, "servers", CF_TLV_NEST, 0, 0, CF_TLV_LIST, fz_server_tags, 2, NULL},
    {0x0010, NULL, CF_TLV_RECORD, 12, 0, CF_TLV_SKIP, NULL, 0, NULL},
    {0x0011, NULL, CF_TLV_RECORD, 8, 0, 0, NULL, 0, NULL}};

static const cf_tlv_tag fz_issuer_tags[] = {
    {0x50, "label", CF_TLV_TEXT, 0, 0, 0, NULL, 0, NULL}};

static const cf_tlv_tag fz_record_tags[] = {
    {0x5A, "pan", CF_TLV_BCD, 0, 0, 0, NULL, 0, NULL},
    {0x5F24, "expiry", CF_TLV_BCD, 3, 0, 0, NULL, 0, NULL},
    {0x9F02, "amount", CF_TLV_BCD, 6, 0, 0, NULL, 0, NULL},
    {0xBF0C, "issuer", CF_TLV_NEST, 0, 0, 0, fz_issuer_tags, 1, NULL}};

static const cf_tlv_tag fz_ber_tags[] = {
    {0x70, "record", CF_TLV_NEST, 0, 0, 0, fz_record_tags, 4, NULL}};

static const cf_codec* fz_tlv(void) {
    static cf_tlv_layout l = CF_TLV_LAYOUT_INIT;
    static cf_codec      c;
    static int           made = 0;

    if (!made) {
        cf_error err   = CF_ERROR_INIT;
        l.name         = "fz-tlv";
        l.tag_width    = 2;
        l.length_width = 2;
        l.order        = CF_TLV_LITTLE;
        l.flags        = CF_TLV_END;
        l.end_tag      = 0xFFFFu;
        l.unknown      = CF_TLV_UNKNOWN_KEEP;
        l.tags         = fz_tlv_tags;
        l.ntags        = sizeof fz_tlv_tags / sizeof fz_tlv_tags[0];
        made           = cf_tlv_codec(&c, &l, &err) ? 1 : -1;
        check(made == 1, "the fuzzer's TLV layout makes a codec");
    }
    return (made == 1) ? &c : NULL;
}

static const cf_codec* fz_ber(void) {
    static cf_tlv_layout l = CF_TLV_LAYOUT_INIT;
    static cf_codec      c;
    static int           made = 0;

    if (!made) {
        cf_error err = CF_ERROR_INIT;
        l.name       = "fz-ber";
        l.kind       = CF_TLV_BER;
        l.flags      = CF_TLV_PADDING;
        l.tags       = fz_ber_tags;
        l.ntags      = 1;
        made         = cf_tlv_codec(&c, &l, &err) ? 1 : -1;
        check(made == 1, "the fuzzer's BER-TLV layout makes a codec");
    }
    return (made == 1) ? &c : NULL;
}
#else
static const cf_codec* fz_tlv(void) {
    return NULL;
}

static const cf_codec* fz_ber(void) {
    return NULL;
}
#endif

/* The bytes through a codec made from a layout. */
static void feed_tlv(const cf_codec* c, const blob* v) {
    feed_through(c, v);
}

static void feed_dotenv(const blob* v) {
    cf_arena*   ar = cf_arena_fixed(g_arena, sizeof g_arena);
    cf_keyspace ks;
    cf_keysink  out;
    cf_keymap   map;
    cf_error    err = CF_ERROR_INIT;

    if (ar == NULL || !cf_ks_init(&ks, ar, 32)) {
        return;
    }
    cf_keysink_init(&out, &ks, 0);
    cf_keymap_init(&map, CF_KEYMAP_ENV, NULL);
    (void)cf_lex_dotenv(v->b, v->n, "fuzz", &map, cf_keysink_of(&out), &err);
}

/* The image is the one reader that dereferences offsets taken from the
   input, so a corrupt one is the only input here that could reach
   memory that is not there. */
static void feed_image(const blob* v) {
    cf_image img;
    cf_error err = CF_ERROR_INIT;
    size_t   i;

    /* Aligned, because an unaligned buffer is refused before anything
       is read and that would test nothing. */
    static union {
        cf_i64 align;
        char   b[MAX_INPUT];
    } aligned;

    memcpy(aligned.b, v->b, v->n);
    if (!cf_image_open(&img, aligned.b, v->n, &err)) {
        check(err.message[0] != '\0', "a refused image says why");
        return;
    }
    /* It opened, so every accessor has to survive whatever the header
       claimed about itself. */
    for (i = 0; i < cf_image_count(&img) && i < 64; ++i) {
        const char* key;
        size_t      klen;
        cf_value    val;
        (void)cf_image_at(&img, i, &key, &klen, &val);
    }
    {
        cf_value got;
        (void)cf_image_get(&img, "server.port", 11, &got);
    }
}

/* The flat lexers. argv and the environment are user input too, and a
   lexer that walks off the end of a malformed one is the same bug in a
   less interesting place. */
static void feed_argv(const blob* v) {
    static char  storage[MAX_INPUT + 1];
    static char* argv[64];
    int          argc = 1;
    size_t       at   = 0;
    cf_arena*    ar;
    cf_keyspace  ks;
    cf_keysink   out;
    cf_keymap    map;
    cf_error     err = CF_ERROR_INIT;

    /* The mutated blob, split on NUL and newline into arguments. */
    memcpy(storage, v->b, v->n);
    storage[v->n] = '\0';
    argv[0]       = storage;
    while (at < v->n && argc < 63) {
        if (storage[at] == '\0' || storage[at] == '\n') {
            storage[at] = '\0';
            if (at + 1 < v->n) {
                argv[argc++] = storage + at + 1;
            }
        }
        ++at;
    }
    argv[argc] = NULL;

    ar = cf_arena_fixed(g_arena, sizeof g_arena);
    if (ar == NULL || !cf_ks_init(&ks, ar, 32)) {
        return;
    }
    cf_keysink_init(&out, &ks, 0);
    cf_keymap_init(&map, CF_KEYMAP_DIRECT, NULL);
    (void)cf_lex_cli(argc, argv, &map, NULL, cf_keysink_of(&out), &err);

    /* And the same bytes read as an environment, where the prefix and
       the escape give the key mapping more to get wrong. */
    cf_keymap_init(&map, CF_KEYMAP_ENV, "MYAPP_");
    (void)cf_lex_env(argv, &map, cf_keysink_of(&out), &err);
}

/* A provenance report is untrusted input the moment it arrives from
   another machine, and replay reads a structure out of it rather than
   just a document. */
static void feed_replay(const blob* v) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b;
    FILE*       f = fopen("cf_fuzz_report.json", "wb");

    if (f == NULL) {
        return;
    }
    fwrite(v->b, 1, v->n, f);
    fclose(f);

    b = cf_build(NULL);
    cf_builder_replay(b, "replay", CF_RANK_ENV, "cf_fuzz_report.json", 0);
    {
        cf_config* c = cf_builder_create(b, &err);
        if (c != NULL) {
            /* It accepted the corrupted report, so everything built on
               top of it has to hold as well. */
            char buf[1024];
            (void)cf_inspect(c, buf, sizeof buf);
            (void)cf_digest(c);
            cf_config_free(c);
        } else {
            check(err.message[0] != '\0', "a refused replay says why");
        }
    }
}

/* Response files are read from disk and expanded into arguments. */
static void feed_response_file(const blob* v) {
    static char  a0[]   = "prog";
    static char  a1[]   = "@cf_fuzz_args.txt";
    static char* argv[] = {a0, a1, NULL};
    cf_arena*    ar;
    cf_error     err = CF_ERROR_INIT;
    int          n   = 0;
    char**       out = NULL;
    FILE*        f   = fopen("cf_fuzz_args.txt", "wb");

    if (f == NULL) {
        return;
    }
    fwrite(v->b, 1, v->n, f);
    fclose(f);

    ar = cf_arena_fixed(g_arena, sizeof g_arena);
    if (ar == NULL) {
        return;
    }
    (void)cf_cli_expand(2, argv, ar, &n, &out, &err);
}

/* ---------------------------------------------------------------- */

static const char* const g_json =
    "{\"server\":{\"port\":8080,\"host\":\"0.0.0.0\",\"tags\":[\"a\",\"b\"],"
    "\"tls\":{\"enabled\":true,\"ratio\":0.5}},\"n\":null,\"big\":-9007199254740993}";

/* Each thing JSON5 adds: comments, unquoted and single-quoted names,
   trailing commas, the escapes and a continuation, every number form,
   and white space past ASCII. */
static const char* const g_json5 =
    "// a comment\n"
    "{ server: { port: 0x1F90, host: '0.0.0.0', tags: ['a', \"b\",], },\n"
    "  /* block\n comment */ $id_1: +.5e1, n: null, inf: -Infinity, nan: NaN,\n"
    "  s: 'it\\'s \\x41\\u00e9\\v\\0 \\\n"
    "line', \xC2\xA0 big: 5., \\u0061b: 1,\n"
    "}\n";

static const char* const g_ini = "# a comment\n"
                                 "top = 1\n"
                                 "[server]\n"
                                 "port = 8080\n"
                                 "host = \"0.0.0.0\"\n"
                                 "[server.tls]\n"
                                 "enabled = true\n";

/* Every construct the TOML subset reads, so a mutation lands in each
   of them: headers, arrays of tables, inline tables, escapes, every
   integer base. */
static const char* const g_toml = "title = \"a\\u00e9\" # comment\n"
                                  "[server]\n"
                                  "port = 8_080\n"
                                  "host = '0.0.0.0'\n"
                                  "masks = [0x1F, 0o17, 0b1,]\n"
                                  "tls = { on = true, r = 0.5 }\n"
                                  "a.b.c = -1e3\n"
                                  "[[up]]\n"
                                  "h = \"x\"\n"
                                  "[up.t]\n"
                                  "k = 1\n";

static const char* const g_report =
    "{\"configforge_report\": 1, \"mode\": \"full\", \"profile\": \"p\","
    "\"application\": {\"name\": \"a\", \"version\": \"1\"},"
    "\"library\": {\"version\": \"0.2.0\", \"features\": [\"json\"]},"
    "\"generated\": \"now\", \"sources\": [{\"name\": \"cli\","
    "\"kind\": \"cli\", \"rank\": 0}], \"keys\": {\"server.port\": {"
    "\"definition\": {\"type\": \"int\", \"required\": false,"
    "\"sensitive\": false, \"deprecated\": false, \"default\": 8080},"
    "\"candidates\": [{\"source\": \"cli\", \"present\": true,"
    "\"value\": 9000, \"at\": \"argv[1]\"}],"
    "\"selected\": {\"source\": \"cli\", \"reason\": \"r\"}}}}";

/* Every construct the YAML reader takes, and two it refuses, so that a
   mutation reaches the refusals too. Skipped in a build without it. */
static const char* const g_yaml = "# comment\n"
                                  "server:\n"
                                  "  port: 8080\n"
                                  "  host: \"0.0.0.0\"\n"
                                  "  tls: {on: true, cert: 'c.pem'}\n"
                                  "  tags: [a, \"b\"]\n"
                                  "up:\n"
                                  "  - h: a\n"
                                  "  - h: b\n"
                                  "nothing: ~\n"
                                  "motd: |\n"
                                  "  two\n"
                                  "base: &b 1\n"
                                  "copy: *b\n";

/* Every construct the XML reader takes - attributes, text, a list of
   values and of tables, CDATA, an entity - and refusals for a mutation
   to reach: a DOCTYPE, a list whose elements are apart, mixed content. */
static const char* const g_xml =
    "<?xml version=\"1.0\"?>\n"
    "<c a=\"1\">\n"
    "  <s port=\"8080\"><h> x </h><t>a</t><t>b &amp; c</t></s>\n"
    "  <u><h>a</h></u><u><h>b</h></u>\n"
    "  <m><![CDATA[<raw>]]></m><e/>\n"
    "  <l>1</l><n>2</n><l>3</l>\n"
    "  <x>text<y>1</y></x>\n"
    "</c>\n"
    "<!DOCTYPE c>\n";

/* Every construct the .properties reader takes: both separators and
   whitespace, both comment marks, a continuation, the escapes, a
   surrogate pair, and a bad escape for a mutation to reach. */
static const char* const g_properties = "# comment\n"
                                        "! comment\n"
                                        "server.port = 8080\n"
                                        "server.host:0.0.0.0\n"
                                        "greeting hello world\n"
                                        "path=C:\\\\x\\tb\n"
                                        "long = a, \\\n"
                                        "       b\n"
                                        "emoji=\\"
                                        "ud83d\\"
                                        "ude00\n"
                                        "bad=\\"
                                        "u12\n";

/* CBOR: one indefinite-length map holding a member of each kind the
   reader takes - nested definite maps and arrays, a text in chunks, an
   indefinite array, a tag, every width of float - so that every branch
   of the reader has a seed to start from. */
static const unsigned char g_cbor[] = {
    0xBF, /* a map of indefinite length */
    /* "server": {"port": 8080, "hosts": ["a", "b"]} */
    0x66, 0x73, 0x65, 0x72, 0x76, 0x65, 0x72, 0xA2, 0x64, 0x70, 0x6F, 0x72, 0x74, 0x19,
    0x1F, 0x90, 0x65, 0x68, 0x6F, 0x73, 0x74, 0x73, 0x82, 0x61, 0x61, 0x61, 0x62,
    /* "a": "streaming", a text in two chunks */
    0x61, 0x61, 0x7F, 0x65, 0x73, 0x74, 0x72, 0x65, 0x61, 0x64, 0x6D, 0x69, 0x6E, 0x67,
    0xFF,
    /* "b": [1, 2], an array of indefinite length */
    0x61, 0x62, 0x9F, 0x01, 0x02, 0xFF,
    /* "t": tag 0, a date and time */
    0x61, 0x74, 0xC0, 0x74, 0x32, 0x30, 0x31, 0x33, 0x2D, 0x30, 0x33, 0x2D, 0x32, 0x31,
    0x54, 0x32, 0x30, 0x3A, 0x30, 0x34, 0x3A, 0x30, 0x30, 0x5A,
    /* "h": 1.5, half precision */
    0x61, 0x68, 0xF9, 0x3E, 0x00,
    /* "i": 100000.0, single precision */
    0x61, 0x69, 0xFA, 0x47, 0xC3, 0x50, 0x00,
    /* "j": 1.1, double precision */
    0x61, 0x6A, 0xFB, 0x3F, 0xF1, 0x99, 0x99, 0x99, 0x99, 0x99, 0x9A,
    /* "k": the smallest half-precision subnormal */
    0x61, 0x6B, 0xF9, 0x00, 0x01, 0xFF /* the map's end */
};

/* TLV under fz_tlv's layout: W4-T2's device document, then BCD, hex and
   a list of two nests, then the end tag. */
static const unsigned char g_tlv[] = {
    0x01, 0x00, 0x08, 0x00, 'u',  'n',  'i',  't',  '-',  '7',  0x00, 0x00, /* name   */
    0x02, 0x00, 0x02, 0x00, 0x90, 0x01,                                     /* port   */
    0x03, 0x00, 0x01, 0x00, 0x01,                                           /* dhcp   */
    0x04, 0x00, 0x04, 0x00, 0xFF, 0xFF, 0xFF, 0xFE,                         /* offset */
    0x05, 0x00, 0x04, 0x00, 0x00, 0x00, 0xC0, 0x3F,                         /* gain   */
    0x10, 0x00, 0x0C, 0x00, 1,    2,    3,    4,    5,    6,    7,    8,
    9,    10,   11,   12,                                       /* skipped */
    0x06, 0x00, 0x03, 0x00, 0x01, 0x23, 0x45,                   /* amount */
    0x07, 0x00, 0x06, 0x00, 0x00, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E, /* mac    */
    0x08, 0x00, 0x12, 0x00, 0x01, 0x00, 0x08, 0x00, '1',  '0',  '.',  '0',
    '.',  '0',  '.',  '1',  0x02, 0x00, 0x02, 0x00, 0x90, 0x1F, /* server */
    0x08, 0x00, 0x12, 0x00, 0x01, 0x00, 0x08, 0x00, '1',  '0',  '.',  '0',
    '.',  '0',  '.',  '2',  0x02, 0x00, 0x02, 0x00, 0x91, 0x23, /* server */
    0xFF, 0xFF};                                                /* end    */

/* BER-TLV under fz_ber's layout: W4-T4's record, padding around it. */
static const unsigned char g_ber[] = {
    0x00, 0x00, 0x70, 0x81, 0x22,                               /* padding, record */
    0x5A, 0x08, 0x47, 0x61, 0x73, 0x90, 0x01, 0x01, 0x00, 0x10, /* pan             */
    0x5F, 0x24, 0x03, 0x25, 0x12, 0x31,                         /* expiry          */
    0x9F, 0x02, 0x06, 0x00, 0x00, 0x00, 0x01, 0x23, 0x45,       /* amount          */
    0xBF, 0x0C, 0x06, 0x50, 0x04, 'T',  'E',  'S',  'T',        /* issuer          */
    0xFF};                                                      /* padding         */

static const char* const g_env = "# comment\n"
                                 "export SERVER__PORT=8080\n"
                                 "SERVER__HOST=\"0.0.0.0\"\n"
                                 "EMPTY=\n";

#define ROUNDS 4000
#define FILE_ROUNDS 400 /* these touch the disk, so fewer */

static void fuzz_text(const char* name, const char* corpus, int is_ini) {
    static blob v;
    int         i;

    for (i = 0; i < ROUNDS; ++i) {
        int m;
        load(&v, corpus);
        /* One to four mutations: a single change tests the edges of a
           valid document, several test whether the parser survives
           something that stopped resembling one. */
        for (m = (int)pick(4) + 1; m > 0; --m) {
            mutate(&v);
        }
        if (is_ini < 0) {
            feed_dotenv(&v);
        } else {
            feed_codec(name, &v);
        }
    }
}

/* fuzz_text for a binary format, whose corpus is bytes, read through
   `codec` (skipped when the build has none). */
static void fuzz_bytes(const cf_codec* codec, const unsigned char* corpus, size_t n) {
    static blob v;
    int         i;

    for (i = 0; i < ROUNDS; ++i) {
        int m;
        load_bytes(&v, corpus, n);
        for (m = (int)pick(4) + 1; m > 0; --m) {
            mutate(&v);
        }
        feed_through(codec, &v);
    }
}

/* Bytes that were never a document. A mutation fuzzer starts from
   something valid and reaches this only by accident; a parser's first
   few branches are exactly where it is most likely to trust its
   input. */
static void fuzz_from_nothing(void) {
    static blob v;
    int         i;

    /* The cost of nothing, and of one byte, in every format below: what
       FUZZ_COST_EMPTY is measured from. */
    for (i = 0; i < 2; ++i) {
        v.n    = (size_t)i;
        v.b[0] = 'a';
        feed_codec("json", &v);
        feed_codec("json5", &v);
        feed_codec("ini", &v);
        feed_codec("toml", &v);
        feed_codec("yaml", &v);
        feed_codec("xml", &v);
        feed_codec("properties", &v);
        feed_codec("cbor", &v);
        feed_tlv(fz_tlv(), &v);
        feed_tlv(fz_ber(), &v);
    }
    for (i = 0; i < ROUNDS; ++i) {
        size_t j;
        v.n = pick(200) + 1;
        for (j = 0; j < v.n; ++j) {
            v.b[j] = (char)(next() & 0xFF);
        }
        feed_codec("json", &v);
        feed_codec("json5", &v);
        feed_codec("ini", &v);
        feed_codec("toml", &v); /* skipped in a build without it */
        feed_codec("yaml", &v); /* and this one too */
        feed_codec("xml", &v);  /* and this */
        feed_codec("properties", &v);
        feed_codec("cbor", &v);
        feed_tlv(fz_tlv(), &v);
        feed_tlv(fz_ber(), &v);
        feed_dotenv(&v);
        feed_argv(&v);
        feed_image(&v);
    }
}

static void fuzz_files(const char* corpus) {
    static blob v;
    int         i;

    for (i = 0; i < FILE_ROUNDS; ++i) {
        int m;
        load(&v, corpus);
        for (m = (int)pick(4) + 1; m > 0; --m) {
            mutate(&v);
        }
        feed_replay(&v);
        feed_response_file(&v);
    }
    remove("cf_fuzz_report.json");
    remove("cf_fuzz_args.txt");
}

#if CF_WITH_IMAGE
/* A valid image with provenance, to corrupt: the image's corpus is
   written by the library rather than kept as bytes here. */
static size_t image_seed(char* out, size_t cap) {
    cf_error     err = CF_ERROR_INIT;
    cf_builder*  b;
    cf_config*   c;
    size_t       n;
    static char  a0[]   = "prog";
    static char  a1[]   = "--server.port=8080";
    static char  a2[]   = "--server.host=0.0.0.0";
    static char* argv[] = {a0, a1, a2, NULL};

    b = cf_build(NULL);
    cf_builder_cli(b, "cli", CF_RANK_CLI, 3, argv);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_builder_text(b, "server.host", "0.0.0.0", 0);
    c = cf_builder_create(b, &err);
    if (c == NULL) {
        return 0;
    }
    n = cf_image_write(cf_keys(c), NULL, CF_IMAGE_PROVENANCE, out, cap);
    cf_config_free(c);
    return (n <= cap) ? n : 0;
}

static void fuzz_image(void) {
    static blob v;
    static char written[MAX_INPUT];
    size_t      n = image_seed(written, sizeof written);
    int         i;

    if (n == 0) {
        check(0, "the image corpus builds and fits");
        return;
    }

    for (i = 0; i < ROUNDS; ++i) {
        int m;
        memcpy(v.b, written, n);
        v.n = n;
        for (m = (int)pick(3) + 1; m > 0; --m) {
            mutate(&v);
        }
        feed_image(&v);
    }
}
#endif

#if defined(CF_FUZZ_SEEDS)

/* The corpora above as files, one per reader, for libFuzzer's
   -seed_inputs: the coverage-guided harnesses (tests/fuzz) start from
   the same documents as this one, so there is one corpus, not two. */
static void write_seed(const char* name, const char* p, size_t n) {
    FILE* f = fopen(name, "wb");
    if (f == NULL) {
        printf("FAIL: cannot write %s\n", name);
        ++g_fail;
        return;
    }
    fwrite(p, 1, n, f);
    fclose(f);
}

int main(void) {
    static const char argv_seed[] = "--server.port=8080 --no-tls -p 1 @none";

    write_seed("seed_json", g_json, strlen(g_json));
    write_seed("seed_json5", g_json5, strlen(g_json5));
    write_seed("seed_ini", g_ini, strlen(g_ini));
    write_seed("seed_toml", g_toml, strlen(g_toml));
    write_seed("seed_yaml", g_yaml, strlen(g_yaml));
    write_seed("seed_xml", g_xml, strlen(g_xml));
    write_seed("seed_properties", g_properties, strlen(g_properties));
    write_seed("seed_cbor", (const char*)g_cbor, sizeof g_cbor);
    write_seed("seed_tlv", (const char*)g_tlv, sizeof g_tlv);
    write_seed("seed_ber", (const char*)g_ber, sizeof g_ber);
    write_seed("seed_dotenv", g_env, strlen(g_env));
    write_seed("seed_replay", g_report, strlen(g_report));
    write_seed("seed_argv", argv_seed, sizeof argv_seed - 1);
#if CF_WITH_IMAGE
    {
        static char img[MAX_INPUT];
        size_t      n = image_seed(img, sizeof img);
        write_seed("seed_image", img, n);
    }
#else
    write_seed("seed_image", "", 0);
#endif
    return cf_test_done();
}

#elif !defined(CF_LIBFUZZER)

int main(int argc, char** argv) {
    cf_u64 s = 20260913;

    /* A failing seed is the bug report, so it can be given back. */
    if (argc > 1) {
        cf_i64 given = 0;
        if (cf_text_to_i64(argv[1], strlen(argv[1]), &given, NULL)) {
            s = (cf_u64)given;
        }
    }
    seed(s);
    printf("seed %u\n", (unsigned)s);

    fuzz_text("json", g_json, 0);
    fuzz_text("json5", g_json5, 0);
    fuzz_text("ini", g_ini, 1);
    fuzz_text("toml", g_toml, 0);
    fuzz_text("yaml", g_yaml, 0);
    fuzz_text("xml", g_xml, 0);
    fuzz_text("properties", g_properties, 0);
    fuzz_bytes(cf_codec_by_name("cbor"), g_cbor, sizeof g_cbor);
    fuzz_bytes(fz_tlv(), g_tlv, sizeof g_tlv);
    fuzz_bytes(fz_ber(), g_ber, sizeof g_ber);
    fuzz_text(NULL, g_env, -1);
#if CF_WITH_IMAGE
    fuzz_image();
#endif
    fuzz_from_nothing();
    fuzz_files(g_report);

    return cf_test_done();
}

#endif /* CF_FUZZ_SEEDS, CF_LIBFUZZER */
