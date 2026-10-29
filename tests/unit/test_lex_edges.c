/*
 * test_lex_edges.c - the number conversions and the flat lexers at their
 * edges: buffers too short, a sign with nothing after it, values past
 * every range, non-finite reals; and for the command line, the
 * arguments that are not options, a response file with trailing blanks
 * and quotes, and the mappings asked about keys they cannot spell.
 */

#include <configforge/configforge.h>

#include <float.h>
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

static void test_integers(void) {
    char   buf[32];
    cf_i64 i   = 0;
    cf_u64 u   = 0;
    size_t end = 0;

    check(cf_i64_to_text(5, NULL, 4) == 0 && cf_i64_to_text(5, buf, 0) == 0,
          "nowhere to write an int");
    check(cf_i64_to_text(-123, buf, 2) == 0, "an int that does not fit");
    check(cf_u64_to_text(7, NULL, 4) == 0 && cf_u64_to_text(77, buf, 1) == 0,
          "nowhere, or no room, for an unsigned");
    check(cf_u64_to_text(0, buf, sizeof buf) == 1 && buf[0] == '0',
          "zero is one digit");
    check(!cf_text_to_i64(NULL, 1, &i, NULL) && !cf_text_to_i64("1", 0, &i, NULL) &&
              !cf_text_to_i64("1", 1, NULL, NULL),
          "no text, no length, nowhere to put it");
    check(!cf_text_to_i64("-", 1, &i, NULL) && !cf_text_to_i64("+", 1, &i, NULL),
          "a sign alone");
    check(cf_text_to_i64("+42", 3, &i, NULL) && i == 42, "a plus sign");
    check(!cf_text_to_i64("99999999999999999999", 20, &i, NULL), "past the range");
    check(cf_text_to_i64("12abc", 5, &i, &end) && i == 12 && end == 2,
          "stops where the digits do, and says where");
    check(!cf_text_to_i64("12abc", 5, &i, NULL),
          "and without an end, refuses the rest");
    check(!cf_text_to_u64(NULL, 1, &u, NULL) && !cf_text_to_u64("-1", 2, &u, NULL),
          "an unsigned is not negative");
    check(!cf_text_to_u64("99999999999999999999", 20, &u, NULL), "nor past its range");
    check(cf_text_to_u64("18446744073709551615", 20, &u, NULL), "and reaches its top");
    check(cf_text_to_u64("7x", 2, &u, &end) && end == 1, "and says where it stopped");
}

#if CF_WITH_REAL
static void test_reals(void) {
    char   buf[64];
    double r    = 0.0;
    size_t end  = 0;
    double zero = 0.0;

    check(cf_f64_to_text(1.5, NULL, 8) == 0 && cf_f64_to_text(1.5, buf, 0) == 0,
          "nowhere to write a real");
    check(cf_f64_to_text(1.0 / zero, buf, 2) == 0, "infinity does not fit in two");
    (void)cf_f64_to_text(1.0 / zero, buf, sizeof buf);
    (void)cf_f64_to_text(-1.0 / zero, buf, sizeof buf);
    (void)cf_f64_to_text(zero / zero, buf, sizeof buf);
    check(1, "the non-finite values answer");
    check(cf_f64_to_text(DBL_MAX, buf, sizeof buf) > 0, "the largest finite");
    check(cf_f64_to_text(DBL_MIN, buf, sizeof buf) > 0, "the smallest normal");
    check(cf_f64_to_text(4.9e-324, buf, sizeof buf) > 0, "the smallest subnormal");
    check(cf_f64_to_text(0.1, buf, 3) == 0, "a real that does not fit");
    check(cf_f64_to_text(123456789012345678.0, buf, sizeof buf) > 0,
          "a large integral");
    check(cf_f64_to_text(1e300, buf, sizeof buf) > 0 &&
              cf_f64_to_text(1e-300, buf, sizeof buf) > 0,
          "the far ends");

    check(!cf_text_to_f64(NULL, 1, &r, NULL) && !cf_text_to_f64("1", 0, &r, NULL) &&
              !cf_text_to_f64("1", 1, NULL, NULL),
          "no text, no length, nowhere to put it");
    check(!cf_text_to_f64("-", 1, &r, NULL) && !cf_text_to_f64("+.", 2, &r, NULL),
          "a sign alone");
    check(cf_text_to_f64("+1.5", 4, &r, NULL) && r == 1.5, "a plus sign");
    (void)cf_text_to_f64("1e400", 5, &r, NULL);
    (void)cf_text_to_f64("1e-400", 6, &r, NULL);
    check(cf_text_to_f64("1e+5", 4, &r, NULL) && r == 100000.0,
          "an exponent with a sign");
    (void)cf_text_to_f64("1e999999999", 11, &r, NULL);
    check(cf_text_to_f64("12345678901234567890123", 23, &r, NULL),
          "more digits than fit");
    check(cf_text_to_f64("0.000000000000000000000001", 26, &r, NULL),
          "many zeros first");
    check(cf_text_to_f64("2.5x", 4, &r, &end) && end == 3,
          "stops where the number does");
    check(!cf_text_to_f64("1e", 2, &r, NULL) && !cf_text_to_f64("1e+", 3, &r, NULL),
          "an exponent with nothing after it");
    check(cf_infer_scalar("1.5e3", 5) != cf_infer_scalar("15", 2),
          "inference tells them apart");
    (void)cf_infer_scalar("", 0);
    (void)cf_infer_scalar("+", 1);
    (void)cf_infer_scalar("1.", 2);
    (void)cf_infer_scalar("1e5", 3);
    (void)cf_infer_scalar("-1E-5", 5);
    check(1, "inference of the odd forms answers");
}
#endif

static void test_keymaps(void) {
    cf_keymap m;
    char      out[16];

    cf_keymap_init(&m, CF_KEYMAP_ENV, "APP_");
    check(cf_keymap_to_key(NULL, "APP_A", 5, out, sizeof out) == 0 &&
              cf_keymap_to_key(&m, NULL, 0, out, sizeof out) == 0 &&
              cf_keymap_to_key(&m, "APP_A", 5, NULL, 4) == 0,
          "a mapping of nothing");
    check(cf_keymap_to_key(&m, "APP_VERY__LONG__NAME__HERE", 26, out, 6) == 0,
          "a key too long for the buffer");
    check(cf_keymap_from_key(NULL, "a", 1, CF_FALSE, out, sizeof out) == 0 &&
              cf_keymap_from_key(&m, NULL, 0, CF_FALSE, out, sizeof out) == 0,
          "the reverse of nothing");
    check(cf_keymap_from_key(&m, "a.b.c", 5, CF_FALSE, out, 4) == 0,
          "a name that does not fit");
    cf_keymap_init(&m, CF_KEYMAP_DIRECT, NULL);
    check(cf_keymap_from_key(&m, "a.b", 3, CF_FALSE, out, sizeof out) == 3,
          "a direct map is the key");
    check(cf_keymap_from_key(&m, "a.b.c.d", 7, CF_FALSE, out, 4) == 0,
          "and refuses no room");
}

static void test_cli(void) {
    static char  a0[]    = "prog";
    static char  a1[]    = "-";
    static char  a2[]    = "--";
    static char  a3[]    = "--after.dashes=1";
    static char  a4[]    = "positional";
    static char* argv1[] = {a0, a1, NULL};
    static char* argv2[] = {a0, a2, a3, NULL};
    static char* argv3[] = {a0, a4, NULL};
    static char  r1[]    = "@cf_lexedges.rsp";
    static char* argv4[] = {a0, r1, NULL};
    cf_error     err     = CF_ERROR_INIT;
    cf_builder*  b;
    cf_config*   c;
    cf_arena*    ar;
    int          n        = 0;
    char**       expanded = NULL;
    FILE*        f;

    b = cf_build(NULL);
    cf_builder_cli(b, "cli", CF_RANK_CLI, 2, argv1);
    c = cf_builder_create(b, &err);
    (void)c; /* a lone dash answers, one way or the other */
    cf_config_free(c);

    b = cf_build(NULL);
    cf_builder_cli(b, "cli", CF_RANK_CLI, 3, argv2);
    c = cf_builder_create(b, &err);
    check(c == NULL || !cf_has(c, "after.dashes"), "nothing after -- is an option");
    cf_config_free(c);

    b = cf_build(NULL);
    cf_builder_cli(b, "cli", CF_RANK_CLI, 2, argv3);
    c = cf_builder_create(b, &err);
    check(c == NULL || cf_count(c) == 0, "a positional argument configures nothing");
    cf_config_free(c);

    f = fopen("cf_lexedges.rsp", "wb");
    if (f != NULL) {
        fputs("# comment\n\n   --rsp.indented=1   \r\n--rsp.tabbed=2\t\n", f);
        fclose(f);
    }
    /* Expansion is its own pass (cf_cli_expand), as cf_app runs it; the
       arguments it makes live in `ar` until the configuration is gone. */
    ar = cf_arena_new(NULL, 0);
    check(cf_cli_expand(2, argv4, ar, &n, &expanded, &err), "a response file expands");
    b = cf_build(NULL);
    cf_builder_cli(b, "cli", CF_RANK_CLI, n, expanded);
    c = cf_builder_create(b, &err);
    check(c != NULL && cf_get_int(c, "rsp.indented", 0) == 1 &&
              cf_get_int(c, "rsp.tabbed", 0) == 2,
          "a response file's blanks and line endings are not part of an argument");
    cf_config_free(c);
    cf_arena_free(ar);
    remove("cf_lexedges.rsp");

    check(!cf_cli_expand(1, NULL, NULL, NULL, NULL, &err), "expanding nothing");
}

static void test_dotenv(void) {
    static const char text[] = "export A=1\nB='single'\nC=\"double\"\n# c\n\nD=\n";
    cf_error          err    = CF_ERROR_INIT;
    cf_keymap         m;

    cf_keymap_init(&m, CF_KEYMAP_ENV, NULL);
    check(!cf_lex_dotenv(NULL, 0, "x", &m, NULL, &err), "a .env of nothing");
    check(!cf_lex_dotenv(text, sizeof text - 1, NULL, &m, NULL, &err),
          "or into nothing");
}

int main(void) {
    test_integers();
#if CF_WITH_REAL
    test_reals();
#endif
    test_keymaps();
    test_cli();
    test_dotenv();
    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
