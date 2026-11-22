/*
 * test_utf8_refused.c - bytes that are not UTF-8 are refused, with their place
 * and a hint (D-108, rules 1, 2 and 5).
 *
 * Part one is cf_encoding_check alone: the message, the place, the three
 * hints and the case with none. Part two goes through the builders: every
 * text format, a file whose format the content decides, XML by its own
 * declaration, the environment, the arguments, a response file, the memory
 * and defaults sources, and a value of each kind written and read back.
 */

#include <configforge/configforge.h>
#include "configforge/cf_encoding.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cf_test.h"

/* ================================================================== *
 * Part one: cf_encoding_check
 * ================================================================== */

#define HINT_1254                                                                      \
    "; it may be windows-1254 (Turkish): read it with CF_ENCODING_WINDOWS_1254, or "   \
    "the encoding windows-1254 in the tools"
#define HINT_1252                                                                      \
    "; it may be windows-1252: read it with CF_ENCODING_WINDOWS_1252, or the "         \
    "encoding windows-1252 in the tools"
#define HINT_8859_1                                                                    \
    "; it may be iso-8859-1: read it with CF_ENCODING_ISO_8859_1, or the encoding "    \
    "iso-8859-1 in the tools"

/* A message holds CF_MESSAGE_MAX bytes, and a build with short limits (the
   minimal preset's 128) cuts the hint off: it is looked for only where it
   fits. */
/* A function, not a constant condition: MSVC's analyzer refuses a
   comparison of two constants (C6326). */
static int hints_fit(void) {
    volatile size_t room = CF_MESSAGE_MAX;
    return room >= 256u;
}

static void hinted(const char* m, const char* prefix, const char* hint,
                   const char* what) {
    has(m, prefix, what);
    if (hints_fit()) {
        has(m, hint, what);
    } else {
        printf("skipped: %s: the message holds %d bytes\n", what, (int)CF_MESSAGE_MAX);
    }
}

/* What cf_encoding_check says of these bytes: "" when it passes. */
static const char* said(const char* bytes, int* ok) {
    static char msg[CF_MESSAGE_MAX];
    cf_error    err = CF_ERROR_INIT;

    msg[0] = '\0';
    *ok    = cf_encoding_check("a.ini", bytes, strlen(bytes), &err) ? 1 : 0;
    if (!*ok) {
        check(err.code == CF_E_SOURCE, "the refusal is a CF_E_SOURCE");
        size_t n = strlen(err.message);

        if (n > 500u) {
            n = 500u;
        }
        if (n >= sizeof msg) {
            n = sizeof msg - 1u;
        }
        memcpy(msg, err.message, n);
        msg[n] = '\0';
    }
    return msg;
}

static void test_check(void) {
    int         ok;
    const char* m;

    m = said("", &ok);
    check(ok, "nothing is UTF-8");
    m = said("k=\xC5\x9E\xC3\xBC\xE2\x82\xAC\xF0\x9F\x98\x80\n", &ok);
    check(ok, "valid multi-byte sequences pass");
    m = said("k=\xDE\n", &ok);
    check(!ok, "a stray byte is refused");
    hinted(m, "a.ini:1:3: byte 0xDE is not valid UTF-8", HINT_1254,
           "with its line, its column, its byte and the hint");

    /* The place, counted as every reader counts: lines by LF, columns in
       bytes from 1. */
    m = said("a\nb\r\n  \xFF", &ok);
    check(!ok, "0xFF is never UTF-8");
    has(m, "a.ini:3:3: byte 0xFF is not valid UTF-8", "the third line, its third byte");
    m = said("ab\xC3", &ok);
    has(m, "a.ini:1:3: byte 0xC3 is not valid UTF-8",
        "a sequence cut short at the end");
    m = said("\xC0\xAF", &ok);
    has(m, "a.ini:1:1: byte 0xC0", "an overlong form is refused at its first byte");
    m = said("\xED\xA0\x80", &ok);
    has(m, "a.ini:1:1: byte 0xED", "a surrogate is refused at its first byte");
    m = said("\xF4\x90\x80\x80", &ok);
    has(m, "a.ini:1:1: byte 0xF4", "a code point past U+10FFFF is refused");

    /* The hint: from the bytes of the whole input that are not part of a
       valid sequence. */
    m = said("a\x93z\n", &ok);
    hinted(m, "a.ini:1:2: byte 0x93 is not valid UTF-8", HINT_1252,
           "a byte from 0x80 to 0x9F: windows-1252");
    m = said("caf\xE9\n", &ok);
    hinted(m, "a.ini:1:4: byte 0xE9 is not valid UTF-8", HINT_8859_1,
           "bytes from 0xA0 to 0xFF only: iso-8859-1");
    m = said("\xE9\x93\xFD", &ok);
    if (hints_fit()) {
        has(m, "it may be windows-1254 (Turkish)",
            "a byte of the six Turkish letters wins over the others");
    }
    m = said("\xFC\xFE", &ok);
    if (hints_fit()) {
        has(m, "windows-1254", "0xFE is one of the six");
    }
    m = said("\xE9\x93", &ok);
    if (hints_fit()) {
        has(m, "windows-1252", "0x80 to 0x9F wins over 0xA0 to 0xFF");
    }
    m = said("\xC5\x9E k=\xDE", &ok);
    check(!ok, "a document that mixes encodings is refused");
    lacks(m, "; it may be", "and gets no hint");
    has(m, "a.ini:1:6: byte 0xDE is not valid UTF-8", "at the byte that is not UTF-8");

    {
        cf_error err = CF_ERROR_INIT;

        check(cf_encoding_check(NULL, "x\xDE", 2, &err) == CF_FALSE,
              "a missing origin is allowed");
        check(strstr(err.message, ":1:2: byte 0xDE") != NULL, "and the place is shown");
        check(cf_encoding_check("a", "x\xDE", 2, NULL) == CF_FALSE,
              "a missing error is allowed");
    }
}

/* ================================================================== *
 * Part two: through the builders
 * ================================================================== */

typedef struct got {
    int  ok;
    char value[64];
    char error[512];
} got;

static void finish(got* g, cf_builder* b, const char* key) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* c;

    c = cf_builder_create(b, &err);
    if (c == NULL) {
        sprintf(g->error, "%.500s", err.message);
        return;
    }
    g->ok = 1;
    if (key != NULL) {
        sprintf(g->value, "%.60s", cf_get_text(c, key, "?"));
    }
    cf_config_free(c);
}

/* `bytes` as a file at `path`, read with `format`. */
static void from_file(got* g, const char* format, const char* path, const char* bytes,
                      const char* key) {
    cf_builder* b = cf_build(NULL);

    memset(g, 0, sizeof *g);
    check(spill(path, bytes), "wrote the file");
    cf_builder_file(b, "file", CF_RANK_USER_FILE, path, format, 0);
    finish(g, b, key);
    remove(path);
}

/* One document, valid but for one byte, in one format: refused at that
   byte's line and column. */
static void refused_in(const char* path, const char* doc, const char* place,
                       const char* what) {
    got  g;
    char want[160];

    from_file(&g, NULL, path, doc, NULL);
    check(!g.ok, what);
    sprintf(want, "%s:%s: byte 0xDE is not valid UTF-8", path, place);
    has(g.error, want, what);
    if (hints_fit()) {
        has(g.error, "windows-1254", "with the hint");
    }
}

static void test_formats(void) {
#if CF_WITH_JSON
    refused_in("cf_u8_a.json", "{\"a\": 1,\n \"x\xDE\": 2}\n", "2:4", "JSON");
    refused_in("cf_u8_b.json5", "{a: 1,\n // c\n b: 'x\xDE'}\n", "3:7", "JSON5");
#endif
#if CF_WITH_YAML
    refused_in("cf_u8_c.yaml", "a: 1\nb: x\xDE\n", "2:5", "YAML");
#endif
#if CF_WITH_TOML
    refused_in("cf_u8_d.toml", "a = 1\nb = \"x\xDE\"\n", "2:7", "TOML");
#endif
#if CF_WITH_INI
    refused_in("cf_u8_e.ini", "[s]\nk=x\xDE\n", "2:4", "INI");
#endif
#if CF_WITH_PROPERTIES
    refused_in("cf_u8_f.properties", "k=x\xDE\n", "1:4", ".properties");
#endif
#if CF_WITH_DOTENV
    {
        got         g;
        cf_builder* b = cf_build(NULL);

        memset(&g, 0, sizeof g);
        check(spill("cf_u8_g.env", "A=1\nB=x\xDE\n"), "wrote the .env file");
        cf_builder_dotenv(b, "dotenv", CF_RANK_USER_FILE, "cf_u8_g.env", NULL, 0);
        finish(&g, b, NULL);
        check(!g.ok, "dotenv is refused");
        has(g.error, "cf_u8_g.env:2:4: byte 0xDE is not valid UTF-8", "at its place");
        remove("cf_u8_g.env");
    }
#endif
#if CF_WITH_JSON
    {
        /* A file with no extension: the content would decide the format, and
           the refusal comes first, with its hint. */
        got g;

        from_file(&g, NULL, "cf_u8_noext", "{\"a\": \"x\xDE\"}\n", NULL);
        check(!g.ok, "a file with no extension is refused");
        hinted(g.error, "cf_u8_noext:1:9: byte 0xDE is not valid UTF-8", HINT_1254,
               "with its place and its hint");
        lacks(g.error, "cannot tell", "and not as a format nobody could tell");
    }
#endif
#if CF_WITH_JSON
    {
        /* A document held in memory, its format named. */
        cf_builder* b = cf_build(NULL);
        got         g;

        memset(&g, 0, sizeof g);
        cf_builder_document(b, "doc", CF_RANK_USER_FILE, "{\"a\": \"\xDE\"}", 10,
                            "json", 0);
        finish(&g, b, NULL);
        check(!g.ok, "a document with a stray byte is refused");
        has(g.error, "doc:1:8: byte 0xDE is not valid UTF-8", "named by the source");
    }
#endif
    {
        /* Valid UTF-8 in every format still reads. */
        got g;

#if CF_WITH_JSON
        from_file(&g, NULL, "cf_u8_h.json", "{\"a\": \"\xC5\x9E\xC3\xBCkr\xC3\xBC\"}\n",
                  "a");
        check(g.ok && strcmp(g.value, "\xC5\x9E\xC3\xBCkr\xC3\xBC") == 0,
              "valid UTF-8 is read as it was");
#else
        (void)g;
#endif
    }
}

#if CF_WITH_XML
static void test_xml(void) {
    got g;

    /* XML reads by its own declaration: ISO-8859-1 bytes are not refused. */
    from_file(&g, NULL, "cf_u8_i.xml",
              "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n"
              "<config><ad>S\xFC"
              "kr\xFC</ad></config>\n",
              "ad");
    check(g.ok, "XML in ISO-8859-1 is read by its declaration");
    check_text(g.value, "S\xC3\xBCkr\xC3\xBC", "and its text is UTF-8");
}
#endif

static void test_env_cli_memory(void) {
    got g;

    /* A mapped variable with an invalid byte is refused by its name; the
       value is not in the message. An unmapped one is ignored. */
    {
        static char        e1[]    = "T_A=x\xDE";
        static char        e2[]    = "UNRELATED=\xDE\xDE";
        static char        e3[]    = "T_=\xDE";
        static char* const bad[]   = {e1, NULL};
        static char* const other[] = {e2, e3, NULL};
        cf_builder*        b       = cf_build(NULL);

        memset(&g, 0, sizeof g);
        cf_builder_env(b, "env", CF_RANK_ENV, "T_", bad);
        finish(&g, b, NULL);
        check(!g.ok, "a mapped variable with an invalid byte is refused");
        has(g.error, "env T_A:1:2: byte 0xDE is not valid UTF-8", "by its name");
        lacks(g.error, "T_A=x", "and its value is not in the message");

        b = cf_build(NULL);
        memset(&g, 0, sizeof g);
        cf_builder_env(b, "env", CF_RANK_ENV, "T_", other);
        finish(&g, b, NULL);
        check(g.ok, "a variable that maps to no key never stops a start");
    }

    /* An option's value is refused by its argument number, a positional
       argument is not looked at. */
    {
        static char        p0[]  = "prog";
        static char        p1[]  = "--c=x\xDE";
        static char        p2[]  = "\xDE\xDE";
        static char        p3[]  = "--c=1";
        static char* const bad[] = {p0, p1, NULL};
        static char* const pos[] = {p0, p2, p3, NULL};
        cf_builder*        b     = cf_build(NULL);

        memset(&g, 0, sizeof g);
        cf_builder_cli(b, "cli", CF_RANK_CLI, 2, bad);
        finish(&g, b, NULL);
        check(!g.ok, "an option's value with an invalid byte is refused");
        has(g.error, "argument 1:1:2: byte 0xDE is not valid UTF-8",
            "by its argument number");

        b = cf_build(NULL);
        memset(&g, 0, sizeof g);
        cf_builder_cli(b, "cli", CF_RANK_CLI, 3, pos);
        finish(&g, b, NULL);
        check(g.ok, "a positional argument is not checked");
    }

    /* A memory value, by its key. */
    {
        static const cf_pair bad[] = {{"m", "a\xDE"}};
        cf_builder*          b     = cf_build(NULL);

        memset(&g, 0, sizeof g);
        cf_builder_memory(b, "mem", CF_RANK_OVERRIDE, bad, 1);
        finish(&g, b, NULL);
        check(!g.ok, "a memory value with an invalid byte is refused");
        has(g.error, "m:1:2: byte 0xDE is not valid UTF-8", "by its key");
    }

    /* A default the program wrote, by its key. */
    {
        cf_builder* b = cf_build(NULL);

        memset(&g, 0, sizeof g);
        cf_builder_text(b, "d", "x\xDE", 0);
        finish(&g, b, NULL);
        check(!g.ok, "a default with an invalid byte is refused");
        has(g.error, "d:1:2: byte 0xDE is not valid UTF-8", "by its key");
    }
}

/* A response file, one argument a line: cf_cli_expand, which the builder
   calls on the real command line. */
static void test_response_file(void) {
    static char  a0[]   = "prog";
    static char  a1[]   = "@cf_u8_resp.rsp";
    static char* argv[] = {a0, a1, NULL};
    cf_arena*    ar     = cf_arena_new(NULL, 0);
    cf_error     err    = CF_ERROR_INIT;
    int          n      = 0;
    char**       out    = NULL;

    check(spill("cf_u8_resp.rsp", "--c=x\n--d=y\xDE\n"), "wrote the response file");
    check(!cf_cli_expand(2, argv, ar, &n, &out, &err),
          "a response file with an invalid byte is refused");
    has(err.message, "cf_u8_resp.rsp:2:6: byte 0xDE is not valid UTF-8",
        "at its place");
    remove("cf_u8_resp.rsp");
    cf_arena_free(ar);
}

/* A value of each source kind, written by cf_emit and read back. */
static void test_round_trip(void) {
#define SUKRU "\xC5\x9E\xC3\xBCkr\xC3\xBC"
#if CF_WITH_JSON && CF_WITH_DOTENV
    static char          e1[]  = "T_E=" SUKRU;
    static char          c0[]  = "prog";
    static char          c1[]  = "--c=" SUKRU;
    static char* const   env[] = {e1, NULL};
    static char* const   cli[] = {c0, c1, NULL};
    static const cf_pair mem[] = {{"m", SUKRU}};
    static char          buf[4096];
    cf_error             err = CF_ERROR_INIT;
    cf_builder*          b   = cf_build(NULL);
    cf_config*           c;
    cf_config*           back;
    size_t               n;
    const char*          keys[] = {"m", "e", "c", "n", "d", "f"};
    size_t               i;

    check(spill("cf_u8_rt.env", "T_N=" SUKRU "\n"), "wrote the .env file");
    check(spill("cf_u8_rt.json", "{\"f\": \"" SUKRU "\"}\n"), "wrote the JSON file");
    cf_builder_memory(b, "mem", CF_RANK_OVERRIDE, mem, 1);
    cf_builder_env(b, "env", CF_RANK_ENV, "T_", env);
    cf_builder_cli(b, "cli", CF_RANK_CLI, 2, cli);
    cf_builder_dotenv(b, "dotenv", CF_RANK_USER_FILE, "cf_u8_rt.env", "T_", 0);
    cf_builder_file(b, "file", CF_RANK_USER_FILE, "cf_u8_rt.json", NULL, 0);
    cf_builder_text(b, "d", SUKRU, 0);
    c = cf_builder_create(b, &err);
    check(c != NULL, "a configuration of every source kind builds");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    n = cf_emit(c, "json", CF_EMIT_ALL, 0, buf, sizeof buf, &err);
    check(n > 0, "it is written");
    back = NULL;
    if (n > 0) {
        cf_builder* r = cf_build(NULL);

        cf_builder_document(r, "back", CF_RANK_USER_FILE, buf, n, "json", 0);
        back = cf_builder_create(r, &err);
    }
    check(back != NULL, "and what was written reads back");
    if (back != NULL) {
        for (i = 0; i < sizeof keys / sizeof keys[0]; ++i) {
            char msg[64];

            sprintf(msg, "the value of '%s' is the same", keys[i]);
            check_text(cf_get_text(back, keys[i], "?"), SUKRU, msg);
        }
        cf_config_free(back);
    }
    cf_config_free(c);
    remove("cf_u8_rt.env");
    remove("cf_u8_rt.json");
#endif
#undef SUKRU
}

int main(void) {
    test_check();
    test_formats();
#if CF_WITH_XML
    test_xml();
#else
    printf("skipped: XML is not compiled in\n");
#endif
    test_env_cli_memory();
    test_response_file();
    test_round_trip();
    return cf_test_done();
}
