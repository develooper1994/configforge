/*
 * test_lex.c - the flat lexers, without a key space.
 *
 * The point of separating lexing from recording: these tests need no
 * arena, no key space and no source object. The sink is an array.
 */

#include <configforge/configforge.h>

#include <stdio.h>
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
    check(cf_lex_cli(6, argv, &map, &r.sink, &err), "cli lexes");

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
        check(!cf_lex_cli(2, bad, &map, &r.sink, &e2), "an invalid option is rejected");
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
        check(cf_lex_cli(2, ce, &map, &r.sink, &e3), "empty value lexes");
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
    check(!cf_lex_cli(CAP + 3, argv, &map, &r.sink, &err),
          "a full sink stops the lexer");
    check(err.code == CF_E_MEMORY, "and says why");
    check(r.n == CAP, "with nothing written past the end");
}

int main(void) {
    test_keymap();
    test_lex_env();
    test_lex_cli();
    test_sink_refusal_is_reported();

    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
