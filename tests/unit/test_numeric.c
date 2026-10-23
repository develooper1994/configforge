/*
 * test_numeric.c - numeric conversion conformance.
 *
 * Run with a locale name as argv[1]. The suite must produce identical
 * results under "C", a comma-decimal locale such as de_DE.UTF-8, and a
 * Turkish locale. The Turkish case is the standard test for the
 * tolower hazard; the comma-decimal case is the standard test for the
 * decimal-separator hazard.
 */

#include "configforge/cf_numeric.h"

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

static void test_i64(void) {
    char   buf[CF_I64_DIGITS];
    cf_i64 v;

    cf_i64_to_text(0, buf, sizeof buf);
    check_text(buf, "0", "i64 zero");
    cf_i64_to_text(-1, buf, sizeof buf);
    check_text(buf, "-1", "i64 minus one");
    cf_i64_to_text(CF_I64_MAX, buf, sizeof buf);
    check_text(buf, "9223372036854775807", "i64 max");
    cf_i64_to_text(CF_I64_MIN, buf, sizeof buf);
    check_text(buf, "-9223372036854775808", "i64 min");

    check(cf_text_to_i64("9223372036854775807", 19, &v, NULL) && v == CF_I64_MAX,
          "parse i64 max");
    check(cf_text_to_i64("-9223372036854775808", 20, &v, NULL) && v == CF_I64_MIN,
          "parse i64 min");
    check(!cf_text_to_i64("9223372036854775808", 19, &v, NULL), "reject max+1");
    check(!cf_text_to_i64("-9223372036854775809", 20, &v, NULL), "reject min-1");
    check(!cf_text_to_i64("", 0, &v, NULL), "reject empty");
    check(!cf_text_to_i64("12a", 3, &v, NULL), "reject trailing");
    check(!cf_text_to_i64(" 12", 3, &v, NULL), "reject leading space");
    check(!cf_text_to_i64("-", 1, &v, NULL), "reject lone sign");
    check(cf_text_to_i64("+42", 3, &v, NULL) && v == 42, "accept plus");
}

static void test_u64(void) {
    char   buf[CF_I64_DIGITS];
    cf_u64 v;

    cf_u64_to_text(CF_U64_MAX, buf, sizeof buf);
    check_text(buf, "18446744073709551615", "u64 max");
    check(cf_text_to_u64("18446744073709551615", 20, &v, NULL) && v == CF_U64_MAX,
          "parse u64 max");
    check(!cf_text_to_u64("18446744073709551616", 20, &v, NULL), "reject u64 max+1");
}

static void test_small_buffer(void) {
    char buf[3];
    check(cf_i64_to_text(12345, buf, sizeof buf) == 0, "too-small returns 0");
    check(buf[0] == '\0', "too-small terminates");
}

#if CF_WITH_REAL

static void fmt_is(double v, const char* want, const char* what) {
    char buf[CF_F64_DIGITS];
    cf_f64_to_text(v, buf, sizeof buf);
    check_text(buf, want, what);
}

static void test_f64_format(void) {
    fmt_is(0.0, "0.0", "zero");
    fmt_is(1.0, "1.0", "one");
    fmt_is(-1.0, "-1.0", "minus one");
    fmt_is(0.5, "0.5", "half");
    fmt_is(1.5, "1.5", "one and a half");
    fmt_is(100.0, "100.0", "hundred");

    /* The whole reason the conversion is ours: 0.1 is not 0.1, and a
       17-digit formatter would print 0.10000000000000001. Shortest
       round-trip prints what the author wrote. */
    fmt_is(0.1, "0.1", "one tenth");
    fmt_is(0.2, "0.2", "two tenths");
    fmt_is(0.3, "0.3", "three tenths");
    fmt_is(1.0 / 3.0, "0.3333333333333333", "one third");

    fmt_is(1e-5, "1.0e-05", "small exponential");
    fmt_is(1e20, "1.0e+20", "large exponential");
    fmt_is(1e15, "1000000000000000.0", "last fixed value");
    fmt_is(1e16, "1.0e+16", "first exponential value");
    fmt_is(1234.5678, "1234.5678", "mixed");
    fmt_is(-0.0625, "-0.0625", "negative fraction");
}

static void test_f64_parse(void) {
    double d;

    check(cf_text_to_f64("0.1", 3, &d, NULL) && d == 0.1, "parse 0.1");
    check(cf_text_to_f64("1.0", 3, &d, NULL) && d == 1.0, "parse 1.0");
    check(cf_text_to_f64("-2.5", 4, &d, NULL) && d == -2.5, "parse -2.5");
    check(cf_text_to_f64("1e3", 3, &d, NULL) && d == 1000.0, "parse 1e3");
    check(cf_text_to_f64("1E-3", 4, &d, NULL) && d == 0.001, "parse 1E-3");
    check(cf_text_to_f64(".5", 2, &d, NULL) && d == 0.5, "parse .5");
    check(cf_text_to_f64("5.", 2, &d, NULL) && d == 5.0, "parse 5.");
    check(!cf_text_to_f64("", 0, &d, NULL), "reject empty");
    check(!cf_text_to_f64("abc", 3, &d, NULL), "reject text");
    check(!cf_text_to_f64("1.2.3", 5, &d, NULL), "reject two points");

    /* A comma must never be accepted as a decimal separator, whatever
       the active locale says. */
    check(!cf_text_to_f64("1,5", 3, &d, NULL), "reject comma separator");
}

/* The rounding carry has to travel past the last digit, not just be
   handled at the first. The original code only fixed index zero, which
   is correct for a one-digit result and silently wrong for "1:" - found
   by clang-tidy noticing the dead store that was the symptom. */
static void test_f64_carry(void) {
    static const double vals[] = {
        0.9999999999999999, 1.9999999999999998, 9.999999999999998, 0.19999999999999998,
        99.99999999999999,  2.0000000000000004, 4.9999999999999996};
    char   buf[CF_F64_DIGITS];
    size_t i;

    for (i = 0; i < sizeof vals / sizeof vals[0]; ++i) {
        double back = 0.0;
        size_t n    = cf_f64_to_text(vals[i], buf, sizeof buf);
        ++g_run;
        if (n == 0 || strchr(buf, ':') != NULL) {
            ++g_fail;
            printf("FAIL: carry leaked a digit past nine: \"%s\"\n", buf);
            continue;
        }
        ++g_run;
        if (!cf_text_to_f64(buf, n, &back, NULL) || back != vals[i]) {
            ++g_fail;
            printf("FAIL: carry broke a round trip: \"%s\"\n", buf);
        }
    }
    /* 0.99..9 rounding up becomes one digit and a larger exponent
       rather than a string of zeros. */
    fmt_is(4.9999999999999996, "5.0", "rounds up to a single digit");
}

static void test_f64_roundtrip(void) {
    static const double vals[] = {0.1,
                                  0.2,
                                  0.3,
                                  1.0 / 3.0,
                                  2.0 / 3.0,
                                  1.0,
                                  2.0,
                                  3.0,
                                  10.0,
                                  1e10,
                                  1e-10,
                                  1e100,
                                  1e-100,
                                  3.141592653589793,
                                  2.718281828459045,
                                  1234.5678,
                                  0.0001,
                                  123456789.123456,
                                  1e300,
                                  1e-300,
                                  5e-324,
                                  1.7976931348623157e308,
                                  2.2250738585072014e-308};
    char                buf[CF_F64_DIGITS];
    size_t              i;

    for (i = 0; i < sizeof vals / sizeof vals[0]; ++i) {
        double back = 0.0;
        size_t n    = cf_f64_to_text(vals[i], buf, sizeof buf);
        if (n == 0) {
            ++g_run;
            ++g_fail;
            printf("FAIL: format failed for element %u\n", (unsigned)i);
            continue;
        }
        ++g_run;
        if (!cf_text_to_f64(buf, n, &back, NULL) || back != vals[i]) {
            ++g_fail;
            printf("FAIL: round-trip element %u text=\"%s\"\n", (unsigned)i, buf);
        }
    }
}

static void test_f64_rejects_nonfinite(void) {
    char   buf[CF_F64_DIGITS];
    double inf, nan;
    /* volatile so that no optimiser sees the division coming: MSVC's
       Release build calls a constant one "potential divide by 0"
       (C4723), which /WX makes an error. */
    volatile double zero = 0.0;
    double          one  = 1.0;

    /* Built without relying on INFINITY or NAN, which are C99. */
    inf = one / zero;
    nan = zero / zero;

    check(cf_f64_to_text(inf, buf, sizeof buf) == 0, "reject infinity");
    check(cf_f64_to_text(nan, buf, sizeof buf) == 0, "reject nan");
}

#endif /* CF_WITH_REAL */

static void test_inference(void) {
    check(cf_infer_scalar("true", 4) == CF_SCALAR_BOOL, "true is bool");
    check(cf_infer_scalar("false", 5) == CF_SCALAR_BOOL, "false is bool");

    /* The Norway problem: YAML 1.1 reads NO as boolean false. */
    check(cf_infer_scalar("NO", 2) == CF_SCALAR_TEXT, "NO stays text");
    check(cf_infer_scalar("no", 2) == CF_SCALAR_TEXT, "no stays text");
    check(cf_infer_scalar("yes", 3) == CF_SCALAR_TEXT, "yes stays text");
    check(cf_infer_scalar("on", 2) == CF_SCALAR_TEXT, "on stays text");
    check(cf_infer_scalar("off", 3) == CF_SCALAR_TEXT, "off stays text");
    check(cf_infer_scalar("True", 4) == CF_SCALAR_TEXT, "True stays text");

    check(cf_infer_scalar("8080", 4) == CF_SCALAR_INT, "8080 is int");
    check(cf_infer_scalar("-1", 2) == CF_SCALAR_INT, "-1 is int");
    check(cf_infer_scalar("0", 1) == CF_SCALAR_INT, "0 is int");

    /* Leading zeros stay text: 08 is not an octal accident. */
    check(cf_infer_scalar("08", 2) == CF_SCALAR_TEXT, "08 stays text");
    check(cf_infer_scalar("007", 3) == CF_SCALAR_TEXT, "007 stays text");

    /* Version strings must not become numbers. */
    check(cf_infer_scalar("1.10", 4) == CF_SCALAR_REAL, "1.10 is real");
    check(cf_infer_scalar("1.2.3", 5) == CF_SCALAR_TEXT, "1.2.3 stays text");

    check(cf_infer_scalar("1e5", 3) == CF_SCALAR_REAL, "1e5 is real");
    check(cf_infer_scalar("1e", 2) == CF_SCALAR_TEXT, "1e stays text");
    check(cf_infer_scalar("", 0) == CF_SCALAR_TEXT, "empty is text");
    check(cf_infer_scalar("hello", 5) == CF_SCALAR_TEXT, "hello is text");
}

/* ---------------------------------------------------------------- */

int main(int argc, char** argv) {
    const char* want = (argc > 1) ? argv[1] : "C";
    const char* got  = setlocale(LC_ALL, want);

    printf("locale requested=%s active=%s\n", want, got ? got : "(unavailable)");
    if (got == NULL) {
        printf("NOTE: locale not present on this system; this run does not\n"
               "      exercise the decimal-separator or tolower hazards.\n");
    }

    test_i64();
    test_u64();
    test_small_buffer();
    test_inference();
#if CF_WITH_REAL
    test_f64_format();
    test_f64_parse();
    test_f64_carry();
    test_f64_roundtrip();
    test_f64_rejects_nonfinite();
#endif

    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
