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

static int  g_fail = 0, g_run = 0;
static void check(int c, const char* w) {
    ++g_run;
    if (!c) {
        ++g_fail;
        printf("FAIL: %s\n", w);
    }
}

/* xorshift64*, written out rather than taken from the C library: the
   sequence has to be the same on every platform for a failing seed to
   mean anything to the person who receives it. */
static cf_u64 g_state;

static void seed(cf_u64 s) {
    g_state = (s != 0) ? s : (cf_u64)88172645463325252U;
}

static cf_u64 next(void) {
    g_state ^= g_state >> 12;
    g_state ^= g_state << 25;
    g_state ^= g_state >> 27;
    return g_state * (cf_u64)2685821657736338717U;
}

static size_t pick(size_t n) {
    return (n == 0) ? 0 : (size_t)(next() % (cf_u64)n);
}

/* ---------------------------------------------------------------- *
 * Mutation
 * ---------------------------------------------------------------- */

#define MAX_INPUT 8192

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

/* ---------------------------------------------------------------- *
 * Targets
 *
 * Every run uses a fresh fixed arena, so a parser that leaks or
 * over-allocates runs out rather than growing until the machine
 * notices.
 * ---------------------------------------------------------------- */

static char g_arena[1 << 16];

static void feed_codec(const char* format, const blob* v) {
    const cf_codec* codec = cf_codec_by_name(format);
    cf_arena*       ar;
    cf_keyspace     ks;
    cf_flatten      fl;
    cf_blob         b;
    cf_error        err = CF_ERROR_INIT;

    if (codec == NULL) {
        return;
    }
    ar = cf_arena_fixed(g_arena, sizeof g_arena);
    if (ar == NULL || !cf_ks_init(&ks, ar, 32)) {
        return;
    }
    cf_blob_mem(&b, v->b, v->n, "fuzz");
    cf_flatten_init(&fl, &ks, 0);

    /* The only contract: it returns. Success and failure are both fine;
       a parser that refuses a corrupted document is doing its job, and
       one that accepts it has produced a configuration from garbage,
       which is also legal - the garbage was a valid document. */
    if (codec->decode(&b, cf_flatten_sink(&fl), &err)) {
        cf_ks_resolve(&ks);
        cf_ks_infer_untyped(&ks);
        /* Reading what it produced is part of the target: a key space
           built from nonsense must still be searchable. */
        (void)cf_ks_get(&ks, "a", 1);
        (void)cf_ks_digest(&ks);
    } else {
        check(err.message[0] != '\0', "a refusal always says why");
    }
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
    cf_builder_replay(b, "replay", CF_RANK_ENV, "cf_fuzz_report.json");
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

/* Bytes that were never a document. A mutation fuzzer starts from
   something valid and reaches this only by accident; a parser's first
   few branches are exactly where it is most likely to trust its
   input. */
static void fuzz_from_nothing(void) {
    static blob v;
    int         i;

    for (i = 0; i < ROUNDS; ++i) {
        size_t j;
        v.n = pick(200) + 1;
        for (j = 0; j < v.n; ++j) {
            v.b[j] = (char)(next() & 0xFF);
        }
        feed_codec("json", &v);
        feed_codec("ini", &v);
        feed_codec("toml", &v); /* skipped in a build without it */
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
static void fuzz_image(void) {
    static blob  v;
    static char  written[MAX_INPUT];
    cf_error     err = CF_ERROR_INIT;
    cf_builder*  b;
    cf_config*   c;
    size_t       n;
    int          i;
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
        check(0, "the image corpus builds");
        return;
    }
    n = cf_image_write(cf_keys(c), NULL, CF_IMAGE_PROVENANCE, written, sizeof written);
    cf_config_free(c);
    if (n == 0 || n > MAX_INPUT) {
        check(0, "the image corpus fits");
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
    fuzz_text("ini", g_ini, 1);
    fuzz_text("toml", g_toml, 0);
    fuzz_text(NULL, g_env, -1);
#if CF_WITH_IMAGE
    fuzz_image();
#endif
    fuzz_from_nothing();
    fuzz_files(g_report);

    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
