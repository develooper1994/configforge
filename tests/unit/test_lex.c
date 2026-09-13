/*
 * test_lex.c - the flat lexers, without a key space.
 *
 * The point of separating lexing from recording: these tests need no
 * arena, no key space and no source object. The sink is an array.
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

/* ---------------------------------------------------------------- *
 * A sink that records into an array
 * ---------------------------------------------------------------- */

#define CAP 32

typedef struct rec {
    char          key[CAP][CF_KEY_MAX];
    char          val[CAP][128];
    char          at[CAP][128];
    size_t        n;
    cf_entry_sink sink;
} rec;

static void copy(char* dst, size_t cap, const char* p, size_t n) {
    if (n + 1 > cap) {
        n = cap - 1;
    }
    memcpy(dst, p, n);
    dst[n] = '\0';
}

static cf_bool rec_text(void* ctx, const char* key, size_t klen, const char* value,
                        size_t vlen, const char* at) {
    rec* r = (rec*)ctx;
    if (r->n >= CAP) {
        return CF_FALSE;
    }
    copy(r->key[r->n], CF_KEY_MAX, key, klen);
    copy(r->val[r->n], 128, value, vlen);
    copy(r->at[r->n], 128, at ? at : "", at ? strlen(at) : 0);
    ++r->n;
    return CF_TRUE;
}

static cf_bool rec_typed(void* ctx, const char* key, size_t klen, const cf_value* v,
                         const char* at) {
    char   tmp[CF_VALUE_TEXT_MAX];
    size_t n = cf_value_to_text(v, tmp, sizeof tmp);
    return rec_text(ctx, key, klen, tmp, n, at);
}

static void rec_init(rec* r) {
    memset(r, 0, sizeof *r);
    r->sink.text  = rec_text;
    r->sink.typed = rec_typed;
    r->sink.ctx   = r;
}

static int find(const rec* r, const char* key) {
    size_t i;
    for (i = 0; i < r->n; ++i) {
        if (strcmp(r->key[i], key) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static void entry_is(const rec* r, const char* key, const char* val, const char* at,
                     const char* what) {
    int i = find(r, key);
    ++g_run;
    if (i < 0) {
        ++g_fail;
        printf("FAIL: %s  '%s' was not produced\n", what, key);
        return;
    }
    if (strcmp(r->val[i], val) != 0 || (at != NULL && strcmp(r->at[i], at) != 0)) {
        ++g_fail;
        printf("FAIL: %s  '%s' = \"%s\" at \"%s\"\n", what, key, r->val[i], r->at[i]);
    }
}

/* ---------------------------------------------------------------- *
 * Key mapping
 * ---------------------------------------------------------------- */

static void to_key_is(const cf_keymap* m, const char* raw, const char* want,
                      const char* what) {
    char   out[CF_KEY_MAX];
    size_t n = cf_keymap_to_key(m, raw, strlen(raw), out, sizeof out);
    ++g_run;
    if (want == NULL) {
        if (n != 0) {
            ++g_fail;
            printf("FAIL: %s  expected a skip, got \"%s\"\n", what, out);
        }
        return;
    }
    if (n == 0 || strcmp(out, want) != 0) {
        ++g_fail;
        printf("FAIL: %s  got \"%s\" want \"%s\"\n", what, n ? out : "<skip>", want);
    }
}

static void from_key_is(const cf_keymap* m, const char* key, cf_bool esc,
                        const char* want, const char* what) {
    char   out[CF_KEY_MAX];
    size_t n = cf_keymap_from_key(m, key, strlen(key), esc, out, sizeof out);
    ++g_run;
    if (n == 0 || strcmp(out, want) != 0) {
        ++g_fail;
        printf("FAIL: %s  got \"%s\" want \"%s\"\n", what, out, want);
    }
}

static void test_keymap(void) {
    cf_keymap env, direct;

    cf_keymap_init(&env, CF_KEYMAP_ENV, "MYAPP_");
    cf_keymap_init(&direct, CF_KEYMAP_DIRECT, NULL);

    to_key_is(&env, "MYAPP_SERVER__PORT", "server.port", "double is a dot");
    to_key_is(&env, "MYAPP_TLS_MIN_VERSION", "tls_min_version", "single is literal");
    to_key_is(&env, "MYAPP_A__B__C", "a.b.c", "three segments");
    to_key_is(&env, "OTHER_PORT", NULL, "wrong prefix skipped");
    to_key_is(&env, "MYAPP_", NULL, "prefix alone skipped");
    to_key_is(&env, "MYAPP_A B", NULL, "space skipped");
    to_key_is(&env, "MYAPP___X", NULL, "leading dot skipped");

    to_key_is(&direct, "server.port", "server.port", "direct passes through");
    to_key_is(&direct, "Server.Port", NULL, "direct still validates");

    /* The direction that only matters once a contract exists: given a
       key, what would this source call it? */
    from_key_is(&env, "server.port", CF_TRUE, "MYAPP_SERVER__PORT",
                "escaped round trips");
    from_key_is(&env, "server.port", CF_FALSE, "MYAPP_SERVER_PORT", "natural spelling");
    from_key_is(&env, "tls_min_version", CF_TRUE, "MYAPP_TLS_MIN_VERSION",
                "an underscore in a key is untouched");

    /* Escaped is what enumeration needs; a round trip proves it. */
    {
        char   out[CF_KEY_MAX];
        char   back[CF_KEY_MAX];
        size_t a = cf_keymap_from_key(&env, "a.b_c.d", 7, CF_TRUE, out, sizeof out);
        size_t b = cf_keymap_to_key(&env, out, a, back, sizeof back);
        check(b == 7 && strcmp(back, "a.b_c.d") == 0,
              "escaped spelling survives a round trip");
    }
}

/* ---------------------------------------------------------------- *
 * Lexers
 * ---------------------------------------------------------------- */

static void test_lex_env(void) {
    static char  e0[]  = "MYAPP_SERVER__PORT=9090";
    static char  e1[]  = "MYAPP_TLS__ENABLED=true";
    static char  e2[]  = "PATH=/usr/bin";
    static char  e3[]  = "MYAPP_BAD KEY=1";
    static char  e4[]  = "MYAPP_NOEQUALS";
    static char  e5[]  = "MYAPP_EMPTY=";
    static char* env[] = {e0, e1, e2, e3, e4, e5, NULL};

    static rec r;
    cf_keymap  map;
    cf_error   err = CF_ERROR_INIT;

    rec_init(&r);
    cf_keymap_init(&map, CF_KEYMAP_ENV, "MYAPP_");
    check(cf_lex_env(env, &map, &r.sink, &err), "env lexes");

    check(r.n == 3, "three usable variables of six");
    entry_is(&r, "server.port", "9090", "MYAPP_SERVER__PORT", "value and origin");
    entry_is(&r, "tls.enabled", "true", "MYAPP_TLS__ENABLED", "bool as written");
    /* Empty is a value, not absence: the two are different states and
       conflating them is a recurring defect elsewhere. */
    entry_is(&r, "empty", "", "MYAPP_EMPTY", "an empty value is a value");

    /* An environment is shared with every other program on the
       machine, so refusing one would be refusing the machine. */
    check(find(&r, "path") < 0, "another program's variable is skipped");

    rec_init(&r);
    check(cf_lex_env(NULL, &map, &r.sink, &err) && r.n == 0,
          "no environment is not an error");
}

static void test_lex_cli(void) {
    static char  a0[]   = "prog";
    static char  a1[]   = "--server.port=8080";
    static char  a2[]   = "--tls.enabled";
    static char  a3[]   = "positional";
    static char  a4[]   = "--";
    static char  a5[]   = "--not.parsed=1";
    static char* argv[] = {a0, a1, a2, a3, a4, a5, NULL};

    static rec r;
    cf_keymap  map;
    cf_error   err = CF_ERROR_INIT;

    rec_init(&r);
    cf_keymap_init(&map, CF_KEYMAP_DIRECT, NULL);
    check(cf_lex_cli(6, argv, &map, NULL, &r.sink, &err), "cli lexes");

    entry_is(&r, "server.port", "8080", "argv[1]", "equals form");
    /* A bare option is a true boolean. The separate "--key value" form
       is deliberately absent: it cannot be parsed correctly without
       knowing which options take a value. */
    entry_is(&r, "tls.enabled", "true", "argv[2]", "bare flag is true");
    check(find(&r, "not.parsed") < 0, "-- ends parsing");
    check(r.n == 2, "positional arguments are not configuration");

    /* A command line belongs to this program alone, so a malformed
       option is the user's typo rather than someone else's setting. */
    rec_init(&r);
    {
        static char  b0[]  = "prog";
        static char  b1[]  = "--Server.Port=1";
        static char* bad[] = {b0, b1, NULL};
        cf_error     e2    = CF_ERROR_INIT;
        check(!cf_lex_cli(2, bad, &map, NULL, &r.sink, &e2),
              "an invalid option is rejected");
        check(e2.code == CF_E_KEY, "reported as a key error");
        check(strstr(e2.message, "argv[1]") != NULL, "naming the argument");
        check(strstr(e2.message, "--Server.Port") != NULL, "and the option");
    }

    /* An empty value after '=' is still a value. */
    rec_init(&r);
    {
        static char  c0[] = "prog";
        static char  c1[] = "--a.b=";
        static char* ce[] = {c0, c1, NULL};
        cf_error     e3   = CF_ERROR_INIT;
        check(cf_lex_cli(2, ce, &map, NULL, &r.sink, &e3), "empty value lexes");
        entry_is(&r, "a.b", "", "argv[1]", "and is empty, not absent");
    }
}

static void test_sink_refusal_is_reported(void) {
    static char  a0[] = "prog";
    static char* argv[CAP + 4];
    static rec   r;
    cf_keymap    map;
    cf_error     err = CF_ERROR_INIT;
    static char  opt[CAP + 4][32];
    int          i;

    /* A sink that runs out of room must stop the lexer rather than
       have it carry on producing entries nobody keeps. */
    argv[0] = a0;
    for (i = 1; i < CAP + 3; ++i) {
        cf_fmt(opt[i], sizeof opt[i], "--k.%d=1", i);
        argv[i] = opt[i];
    }
    argv[CAP + 3] = NULL;

    rec_init(&r);
    cf_keymap_init(&map, CF_KEYMAP_DIRECT, NULL);
    check(!cf_lex_cli(CAP + 3, argv, &map, NULL, &r.sink, &err),
          "a full sink stops the lexer");
    check(err.code == CF_E_MEMORY, "and says why");
    check(r.n == CAP, "with nothing written past the end");
}

/* ---------------------------------------------------------------- *
 * A spec, written by hand. The lexer asks questions and never guesses,
 * so a test can answer them without a definition set existing.
 * ---------------------------------------------------------------- */

static cf_cli_kind fake_classify(const void* ctx, const char* key, size_t klen) {
    (void)ctx;
    if (klen == 11 && memcmp(key, "server.port", 11) == 0) {
        return CF_CLI_VALUE;
    }
    if (klen == 11 && memcmp(key, "tls.enabled", 11) == 0) {
        return CF_CLI_FLAG;
    }
    if (klen == 8 && memcmp(key, "no-proxy", 8) == 0) {
        return CF_CLI_VALUE;
    }
    return CF_CLI_UNKNOWN;
}

static size_t fake_alias(const void* ctx, const char* raw, size_t rawlen, char* out,
                         size_t cap) {
    (void)ctx;
    (void)cap;
    if (rawlen == 1 && raw[0] == 'p') {
        memcpy(out, "server.port", 12);
        return 11;
    }
    return 0;
}

static void spec_init(cf_cli_spec* sp) {
    sp->classify = fake_classify;
    sp->alias    = fake_alias;
    sp->ctx      = NULL;
}

static void test_cli_with_a_contract(void) {
    static char  a0[]   = "prog";
    static char  a1[]   = "--server.port";
    static char  a2[]   = "8080";
    static char  a3[]   = "--no-tls.enabled";
    static char  a4[]   = "-p";
    static char  a5[]   = "9090";
    static char* argv[] = {a0, a1, a2, a3, a4, a5, NULL};
    static rec   r;
    cf_keymap    map;
    cf_cli_spec  sp;
    cf_error     err = CF_ERROR_INIT;

    rec_init(&r);
    cf_keymap_init(&map, CF_KEYMAP_DIRECT, NULL);
    spec_init(&sp);
    check(cf_lex_cli(6, argv, &map, &sp, &r.sink, &err), "cli lexes");

    /* Both occurrences are recorded. The lexer does not resolve; that is
       the key space's job, and hiding one here would take the second
       occurrence out of the chain that explain() prints. */
    check(r.n == 3, "three entries, and no positional eaten");
    check(strcmp(r.key[0], "server.port") == 0 && strcmp(r.val[0], "8080") == 0 &&
              strcmp(r.at[0], "argv[2]") == 0,
          "the separate form, safe only because the spec allowed it");
    check(strcmp(r.key[2], "server.port") == 0 && strcmp(r.val[2], "9090") == 0 &&
              strcmp(r.at[2], "argv[5]") == 0,
          "and the alias, recorded as its canonical key");
    /* --no- negates a declared flag and nothing else. */
    entry_is(&r, "tls.enabled", "false", "argv[3]", "--no- negates a flag");

    /* A value-taking option with nothing after it is an error rather
       than a true boolean: guessing here is what eats an argument. */
    {
        static char  b0[]  = "prog";
        static char  b1[]  = "--server.port";
        static char* bad[] = {b0, b1, NULL};
        cf_error     e2    = CF_ERROR_INIT;
        rec_init(&r);
        check(!cf_lex_cli(2, bad, &map, &sp, &r.sink, &e2),
              "a value option with no value is refused");
        check(strstr(e2.message, "needs a value") != NULL, "and says so");
    }

    /* A key that genuinely begins with "no-" is read literally, because
       only a declared flag can be negated and this one is not. */
    {
        static char  c0[] = "prog";
        static char  c1[] = "--no-proxy";
        static char  c2[] = "1.2.3.4";
        static char* lit  = NULL;
        static char* ce[] = {c0, c1, c2, NULL};
        cf_error     e3   = CF_ERROR_INIT;
        (void)lit;
        rec_init(&r);
        check(cf_lex_cli(3, ce, &map, &sp, &r.sink, &e3), "no-proxy lexes");
        entry_is(&r, "no-proxy", "1.2.3.4", "argv[2]",
                 "a key that starts with no- is not a negation");
    }
}

static void test_cli_without_a_contract(void) {
    static char  a0[]   = "prog";
    static char  a1[]   = "--server.port";
    static char  a2[]   = "8080";
    static char* argv[] = {a0, a1, a2, NULL};
    static rec   r;
    cf_keymap    map;
    cf_error     err = CF_ERROR_INIT;

    rec_init(&r);
    cf_keymap_init(&map, CF_KEYMAP_DIRECT, NULL);
    /* No spec, so the separate form does not exist and the positional
       argument survives. Predictability rather than guesswork. */
    check(cf_lex_cli(3, argv, &map, NULL, &r.sink, &err), "tier 0 lexes");
    entry_is(&r, "server.port", "true", "argv[1]", "bare option is true");
    check(r.n == 1, "the next argument is left alone");
}

/* ---------------------------------------------------------------- */

static void test_dotenv(void) {
    static const char* const doc = "# a comment\n"
                                   "SERVER__PORT=9090\n"
                                   "export SERVER__HOST=0.0.0.0\n"
                                   "GREETING=\"hello world\"\n"
                                   "\n"
                                   "EMPTY=\n";
    static rec               r;
    cf_keymap                map;
    cf_error                 err = CF_ERROR_INIT;

    rec_init(&r);
    cf_keymap_init(&map, CF_KEYMAP_ENV, NULL);
    check(cf_lex_dotenv(doc, strlen(doc), "app.env", &map, &r.sink, &err),
          "dotenv lexes");
    if (err.message[0] != 0) {
        printf("  %s\n", err.message);
    }

    entry_is(&r, "server.port", "9090", "app.env:2", "value and line");
    /* "export NAME=value" is what a shell-sourced file looks like. */
    entry_is(&r, "server.host", "0.0.0.0", "app.env:3", "export is ignored");
    entry_is(&r, "greeting", "hello world", "app.env:4", "quotes stripped");
    entry_is(&r, "empty", "", "app.env:6", "empty is a value");
    check(r.n == 4, "comments and blank lines produce nothing");

    /* Unlike the environment, this file belongs to the program, so a
       name that derives no key is a mistake rather than another
       program's variable. */
    {
        cf_error e2 = CF_ERROR_INIT;
        rec_init(&r);
        check(!cf_lex_dotenv("BAD KEY=1\n", 10, "app.env", &map, &r.sink, &e2),
              "an unusable name is refused");
        check(strstr(e2.message, "app.env:1") != NULL, "with the line");
        rec_init(&r);
        check(!cf_lex_dotenv("novalue\n", 8, "app.env", &map, &r.sink, &e2),
              "a line with no = is refused");
    }
}

static cf_bool spill(const char* path, const char* text) {
    FILE* f = fopen(path, "wb");
    if (f == NULL) {
        return CF_FALSE;
    }
    fputs(text, f);
    fclose(f);
    return CF_TRUE;
}

static void test_response_file(void) {
    static char  a0[]   = "prog";
    static char  a1[]   = "@cf_args.txt";
    static char  a2[]   = "--after=1";
    static char* argv[] = {a0, a1, a2, NULL};
    cf_arena*    ar     = cf_arena_new(NULL, 0);
    cf_error     err    = CF_ERROR_INIT;
    int          n      = 0;
    char**       out    = NULL;

    check(spill("cf_args.txt", "# a comment\n"
                               "--server.port=8080\n"
                               "\n"
                               "   --server.host=1.2.3.4   \n"),
          "wrote a response file");

    check(cf_cli_expand(3, argv, ar, &n, &out, &err), "expansion succeeds");
    check(n == 4, "two lines in, one comment and one blank out");
    check(strcmp(out[0], "prog") == 0, "argv[0] is never a response file");
    check(strcmp(out[1], "--server.port=8080") == 0, "first line");
    check(strcmp(out[2], "--server.host=1.2.3.4") == 0, "trimmed");
    check(strcmp(out[3], "--after=1") == 0, "arguments after it survive");

    /* A response file that includes another is a command line nobody
       can follow from the one they typed. */
    {
        static char  b1[]     = "@cf_nest.txt";
        static char* nested[] = {a0, b1, NULL};
        cf_error     e2       = CF_ERROR_INIT;
        check(spill("cf_nest.txt", "@cf_args.txt\n"), "wrote a nested file");
        check(!cf_cli_expand(2, nested, ar, &n, &out, &e2), "nesting is refused");
        check(strstr(e2.message, "may not include") != NULL, "and says why");
        remove("cf_nest.txt");
    }

    /* Everything after "--" belongs to the program, including an
       argument that happens to start with @. */
    {
        static char  c1[]   = "--";
        static char  c2[]   = "@notafile";
        static char* rest[] = {a0, c1, c2, NULL};
        cf_error     e3     = CF_ERROR_INIT;
        check(cf_cli_expand(3, rest, ar, &n, &out, &e3), "-- stops expansion");
        check(n == 3 && strcmp(out[2], "@notafile") == 0,
              "and the argument is left alone");
    }

    {
        static char  d1[]   = "@cf_absent.txt";
        static char* miss[] = {a0, d1, NULL};
        cf_error     e4     = CF_ERROR_INIT;
        check(!cf_cli_expand(2, miss, ar, &n, &out, &e4),
              "a missing response file is an error");
    }

    /* Identity on this platform, and the caller writes the same two
       lines everywhere. */
    {
        int          m = 0;
        char* const* u = cf_cli_args(3, argv, ar, &m);
        check(m == 3 && u != NULL, "cf_cli_args returns the arguments");
    }

    cf_arena_free(ar);
    remove("cf_args.txt");
}

int main(void) {
    test_keymap();
    test_lex_env();
    test_lex_cli();
    test_cli_with_a_contract();
    test_cli_without_a_contract();
    test_dotenv();
    test_response_file();
    test_sink_refusal_is_reported();

    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
