/*
 * test_port.c - portability layer conformance tests.
 *
 * Compiled as C89. Run under several locales; output must be identical.
 */

#include "configforge/cf_port.h"

#include <stdio.h>
#include <string.h>
#include <locale.h>

static int g_fail = 0;
static int g_run  = 0;

static void check(int cond, const char* what) {
    ++g_run;
    if (!cond) {
        ++g_fail;
        printf("FAIL: %s\n", what);
    }
}

static void check_text(const char* got, const char* want, const char* what) {
    ++g_run;
    if (strcmp(got, want) != 0) {
        ++g_fail;
        printf("FAIL: %s  got=\"%s\" want=\"%s\"\n", what, got, want);
    }
}

/* ---------------------------------------------------------------- */

static void test_ascii_case(void) {
    /* The Turkish-I hazard. With <ctype.h> in a tr_TR locale,
       toupper('i') is U+0130 and tolower('I') is U+0131. These must
       not depend on the locale at all. */
    check(cf_to_upper('i') == 'I', "ascii upper i");
    check(cf_to_lower('I') == 'i', "ascii lower I");
    check(cf_to_upper('z') == 'Z', "ascii upper z");
    check(cf_to_lower(0xE7) == 0xE7, "non-ascii byte untouched");
    check(cf_is_digit('7'), "is_digit 7");
    check(!cf_is_digit(0xB2), "superscript two is not a digit");
}

static void test_compare(void) {
    check(cf_cmp_bytes("a", 1, "b", 1) < 0, "a < b");
    check(cf_cmp_bytes("ab", 2, "a", 1) > 0, "ab > a");
    check(cf_cmp_bytes("a", 1, "a", 1) == 0, "a == a");
    check(cf_cmp_ascii_ci("Server.Port", 11, "server.port", 11) == 0,
          "case-insensitive compare");
}

static void test_fmt(void) {
    char   buf[64];
    size_t n;

    n = cf_fmt(buf, sizeof buf, "port=%d host=%s", 8080, "localhost");
    check_text(buf, "port=8080 host=localhost", "fmt basic");
    check(n == 24, "fmt returns length");

    cf_fmt(buf, sizeof buf, "%I", CF_I64_MIN);
    check_text(buf, "-9223372036854775808", "fmt i64 min");

    cf_fmt(buf, sizeof buf, "100%%");
    check_text(buf, "100%", "fmt percent");

    cf_fmt(buf, sizeof buf, "%s", (const char*)0);
    check_text(buf, "(null)", "fmt null string");

    /* Truncation must terminate and still report the full length. */
    n = cf_fmt(buf, 6, "abcdefghij");
    check(n == 10, "fmt reports untruncated length");
    check_text(buf, "abcde", "fmt truncates and terminates");

    n = cf_fmt_append(buf, sizeof buf, 0, "a=%d", 1);
    n = cf_fmt_append(buf, sizeof buf, n, " b=%d", 2);
    check_text(buf, "a=1 b=2", "fmt append");
}

/* ---------------------------------------------------------------- */

int main(int argc, char** argv) {
    const char* want = (argc > 1) ? argv[1] : "C";
    const char* got;

    got = setlocale(LC_ALL, want);
    printf("locale requested=%s active=%s\n", want, got ? got : "(unavailable)");
    printf("build: %s\n", cf_port_description());

    test_ascii_case();
    test_compare();
    test_fmt();

    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
