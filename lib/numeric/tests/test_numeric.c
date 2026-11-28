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

#include <float.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>

#include "cf_test.h"

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

static void check_f64_roundtrip(double value, size_t element) {
    char   buf[CF_F64_DIGITS];
    double back = 0.0;
    size_t n    = cf_f64_to_text(value, buf, sizeof buf);

    ++g_run;
    if (n == 0) {
        ++g_fail;
        printf("FAIL: format failed for element %u\n", (unsigned)element);
    } else if (!cf_text_to_f64(buf, n, &back, NULL) ||
               memcmp(&back, &value, sizeof back) != 0) {
        ++g_fail;
        printf("FAIL: round-trip element %u text=\"%s\"\n", (unsigned)element, buf);
    }
}

static void test_f64_roundtrip(void) {
    static const double vals[] = {
        0.1, 0.2, 0.3, 1.0 / 3.0, 2.0 / 3.0, 1.0, 2.0, 3.0, 10.0, 1e10, 1e-10, 1e100,
        1e-100, 3.141592653589793, 2.718281828459045, 1234.5678, 0.0001,
        123456789.123456, 1e300, 1e-300, 1.7976931348623157e308,
        /* The smallest two subnormals, followed by
           values around the chain-fuzzer cases. */
        4.9406564584124654e-324, 9.8813129168249309e-324, 2.781347213671473e-309,
        2.78134721367146e-309, 2.0191894549312623e-308, 2.019189454931258e-308,
        2.1758355328459157e-308,
        /* Neighbours across the normal boundary. */
        2.2250738585072004e-308, 2.2250738585072009e-308, DBL_MIN,
        2.2250738585072019e-308};
    /* Bit patterns for the short decimal spellings in the fuzzer report.
       The first is the binary64 payload from the saved CBOR input. */
    static const cf_u64 cbor[] = {((cf_u64)0x000e8500u << 32) | 0x0e00003du,
                                  ((cf_u64)0x000fa55cu << 32) | 0x380f000fu,
                                  ((cf_u64)0x00020000u << 32) | 0x3b00003bu};
    size_t              i;

    for (i = 0; i < sizeof vals / sizeof vals[0]; ++i) {
        check_f64_roundtrip(vals[i], i);
    }
    for (i = 0; i < sizeof cbor / sizeof cbor[0]; ++i) {
        double value;
        memcpy(&value, &cbor[i], sizeof value);
        check_f64_roundtrip(value, sizeof vals / sizeof vals[0] + i);
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

/* ---------------------------------------------------------------- *
 * Durations and sizes (D-113, D-222)
 * ---------------------------------------------------------------- */

/* A text that must read as `want`; the text is what a failure shows. */
static void unit_ok(const cf_unit* u, const char* s, cf_i64 want) {
    cf_i64      v   = -12345;
    const char* why = NULL;
    cf_bool     ok  = cf_unit_parse(u, s, strlen(s), &v, &why);

    check(ok && v == want, s);
}

/* A text that must be refused, with `reason` when one is given. */
static void unit_bad(const cf_unit* u, const char* s, const char* reason) {
    cf_i64      v   = 7;
    const char* why = "unset";
    cf_bool     ok  = cf_unit_parse(u, s, strlen(s), &v, &why);

    check(!ok, s);
    if (reason != NULL) {
        check_text(why, reason, s);
    }
}

static void test_duration(void) {
    const cf_unit* d = cf_unit_duration_get();
    cf_unit        cal;
    cf_unit        bare;
    cf_unit        secs;
    cf_unit        micros;
    cf_i64         v = 0;
    char           buf[64];

    check(d != NULL && d->parse != NULL && d->format != NULL, "the duration unit");
    if (d == NULL) {
        return;
    }
    check_text(d->name, "duration", "the duration unit's name");
    check(d->options.allow_bare && !d->options.calendar && !d->options.bare_si &&
              d->options.base == CF_UNIT_BASE_DEFAULT,
          "the duration unit's own defaults");

    unit_ok(d, "30", 30);
    unit_ok(d, "-1", -1);
    unit_ok(d, "+30", 30);
    unit_ok(d, "30s", 30000);
    unit_ok(d, "1h30m", 5400000);
    unit_ok(d, "2d12h", 216000000);
    unit_ok(d, "250ms", 250);
    unit_ok(d, "1m", 60000);
    unit_ok(d, "1min", 60000);
    unit_ok(d, "1s500ms", 1500);
    unit_ok(d, "1.5h", 5400000);
    unit_ok(d, "PT1H30M", 5400000);
    unit_ok(d, "P1DT2H", 93600000);
    unit_ok(d, "PT1.5S", 1500);
    unit_ok(d, "P1W", 604800000);
    unit_ok(d, "1w", 604800000);
    unit_ok(d, "1d", 86400000);
    unit_ok(d, "0s", 0);
    unit_ok(d, "2000us", 2);
    unit_bad(d, "1500us", "is not a whole number of ms");
    unit_ok(d, "2000000ns", 2);
    unit_ok(d, "3000\xC2\xB5s", 3);
    unit_ok(d, "3000\xCE\xBCs", 3);

    unit_bad(d, "", NULL);
    unit_bad(d, "+1s", NULL);
    unit_bad(d, "1 h", NULL);
    unit_bad(d, "1H", NULL);
    unit_bad(d, "30m1h", NULL);
    unit_bad(d, "1h1h", NULL);
    unit_bad(d, "1m1min", NULL);
    unit_bad(d, "10x", NULL);
    unit_bad(d, "s", NULL);
    unit_bad(d, "1h30", NULL);
    unit_bad(d, "1.5", NULL);
    unit_bad(d, "P", NULL);
    unit_bad(d, "PT", NULL);
    unit_bad(d, "P1DT", NULL);
    unit_bad(d, "PT1M1H", NULL);
    unit_bad(d, "pt1h", NULL);
    unit_bad(d, "9223372036854775807s", "overflows");
    unit_bad(d, "9223372036854775808", "overflows");
    unit_bad(d, "-5s", NULL);
    unit_bad(d, "1.0005s", "is not a whole number of ms");
    unit_bad(d, "1y", "is ambiguous: use days");
    unit_bad(d, "P1M", "is ambiguous: use days");
    unit_bad(d, "P1Y", "is ambiguous: use days");
    unit_bad(d, "1mo", "is ambiguous: use days");

    /* A month is 30 days and a year 365 when the unit is told so. */
    cal                  = *d;
    cal.options.calendar = CF_TRUE;
    unit_ok(&cal, "1y", (cf_i64)31536000 * 1000);
    unit_ok(&cal, "1mo", (cf_i64)2592000 * 1000);
    unit_ok(&cal, "P1M", (cf_i64)2592000 * 1000);
    unit_ok(&cal, "P1Y2M", (cf_i64)31536000 * 1000 + (cf_i64)2 * 2592000 * 1000);
    unit_ok(&cal, "1y1mo1w",
            (cf_i64)31536000 * 1000 + (cf_i64)2592000 * 1000 + 604800000);

    /* A bare number is refused when the unit is told to. */
    bare                    = *d;
    bare.options.allow_bare = CF_FALSE;
    unit_bad(&bare, "30", NULL);
    unit_ok(&bare, "30s", 30000);

    /* The base: seconds or microseconds instead of milliseconds. */
    secs              = *d;
    secs.options.base = CF_UNIT_BASE_S;
    unit_ok(&secs, "1h30m", 5400);
    unit_ok(&secs, "30", 30);
    unit_bad(&secs, "1500ms", "is not a whole number of s");
    micros              = *d;
    micros.options.base = CF_UNIT_BASE_US;
    unit_ok(&micros, "1s", 1000000);
    unit_ok(&micros, "250ms", 250000);
    unit_ok(&micros, "5us", 5);
    unit_bad(&micros, "1500ns", "is not a whole number of us");

    check(cf_unit_format(d, 5400000, buf, sizeof buf) == 5, "1h30m is five bytes");
    check_text(buf, "1h30m", "5400000 ms written");
    check(cf_unit_format(d, 0, buf, sizeof buf) == 3, "zero ms is three bytes");
    check_text(buf, "0ms", "zero is written with its unit");
    cf_unit_format(d, 216000000, buf, sizeof buf);
    check_text(buf, "2d12h", "216000000 ms written");
    cf_unit_format(d, 1500, buf, sizeof buf);
    check_text(buf, "1s500ms", "1500 ms written");
    cf_unit_format(d, -7, buf, sizeof buf);
    check_text(buf, "-7", "a negative duration is the bare number");
    cf_unit_format(d, CF_I64_MIN, buf, sizeof buf);
    check_text(buf, "-9223372036854775808", "the smallest duration is the bare number");
    cf_unit_format(&secs, 5400, buf, sizeof buf);
    check_text(buf, "1h30m", "5400 s written");
    cf_unit_format(&micros, 1500, buf, sizeof buf);
    check_text(buf, "1ms500us", "1500 us written");
    check(cf_unit_format(d, 5400000, buf, 5) == 0 && buf[0] == '\0',
          "too small a buffer writes nothing");
    check(cf_unit_format(d, 5400000, NULL, 0) == 0, "no buffer, no text");
    cf_unit_format(d, 93784123, buf, sizeof buf);
    check(cf_unit_parse(d, buf, strlen(buf), &v, NULL) && v == 93784123,
          "what is written reads back");
    cf_unit_format(d, CF_I64_MAX, buf, sizeof buf);
    check(cf_unit_parse(d, buf, strlen(buf), &v, NULL) && v == CF_I64_MAX,
          "the largest duration reads back");
}

static void test_size(void) {
    const cf_unit* z = cf_unit_size_get();
    cf_unit        si;
    cf_unit        bare;
    cf_i64         v = 0;
    char           buf[64];

    check(z != NULL && z->parse != NULL && z->format != NULL, "the size unit");
    if (z == NULL) {
        return;
    }
    check_text(z->name, "size", "the size unit's name");

    unit_ok(z, "512", 512);
    unit_ok(z, "512B", 512);
    unit_ok(z, "1kB", 1000);
    unit_ok(z, "1KB", 1000);
    unit_ok(z, "1KiB", 1024);
    unit_ok(z, "512MiB", 536870912);
    unit_ok(z, "2GB", 2000000000);
    unit_ok(z, "8TiB", (cf_i64)8 << 40);
    unit_ok(z, "1PiB", (cf_i64)1 << 50);
    unit_ok(z, "1PB", (cf_i64)1000000 * 1000000000);
    unit_ok(z, "0B", 0);
    unit_ok(z, "1.5KiB", 1536);
    unit_ok(z, "0.5MB", 500000);
    unit_ok(z, "1K", 1024);
    unit_ok(z, "1M", 1048576);
    unit_ok(z, "1G", 1073741824);
    unit_ok(z, "1T", (cf_i64)1 << 40);
    unit_ok(z, "-1", -1);

    unit_bad(z, "", NULL);
    unit_bad(z, "1 KB", NULL);
    unit_bad(z, "1kb", NULL);
    unit_bad(z, "1KiB2B", NULL);
    unit_bad(z, "9223372036854775807KiB", "overflows");
    unit_bad(z, "-1KB", NULL);
    unit_bad(z, "+1KB", NULL);
    unit_bad(z, "KiB", NULL);
    unit_bad(z, "1.5B", "is not a whole number of B");
    unit_bad(z, "1.0001KB", "is not a whole number of B");

    /* A bare K is SI when the unit is told so, and bare numbers can go. */
    si                 = *z;
    si.options.bare_si = CF_TRUE;
    unit_ok(&si, "1K", 1000);
    unit_ok(&si, "2M", 2000000);
    unit_ok(&si, "1KiB", 1024);
    bare                    = *z;
    bare.options.allow_bare = CF_FALSE;
    unit_bad(&bare, "512", NULL);
    unit_ok(&bare, "512B", 512);

    cf_unit_format(z, 536870912, buf, sizeof buf);
    check_text(buf, "512MiB", "536870912 B written");
    cf_unit_format(z, 2000000000, buf, sizeof buf);
    check_text(buf, "2GB", "2000000000 B written");
    cf_unit_format(z, 1536, buf, sizeof buf);
    check_text(buf, "1536B", "1536 B has no exact larger suffix");
    cf_unit_format(z, 1024, buf, sizeof buf);
    check_text(buf, "1KiB", "1024 B written");
    cf_unit_format(z, 0, buf, sizeof buf);
    check_text(buf, "0B", "zero bytes written");
    cf_unit_format(z, CF_I64_MIN, buf, sizeof buf);
    check_text(buf, "-9223372036854775808", "the smallest size is the bare number");
    cf_unit_format(z, 7, buf, sizeof buf);
    check_text(buf, "7B", "seven bytes written");
    cf_unit_format(z, CF_I64_MAX, buf, sizeof buf);
    check(cf_unit_parse(z, buf, strlen(buf), &v, NULL) && v == CF_I64_MAX,
          "the largest size reads back");
}

/* A unit of a program's own: a table of two abbreviations. */
static cf_bool tick_parse(const cf_unit* u, const char* s, size_t n, cf_i64* out,
                          const char** reason) {
    static const cf_unit_term terms[] = {{"tk", 1}, {"ktk", 1000}};
    cf_unit_table             t;

    t.terms      = terms;
    t.count      = 2;
    t.per_base   = 1;
    t.sum        = CF_TRUE;
    t.allow_bare = u->options.allow_bare;
    t.not_whole  = "is not a whole number of ticks";
    t.refused    = NULL;
    return cf_unit_table_parse(&t, s, n, out, reason);
}

static size_t tick_format(const cf_unit* u, cf_i64 v, char* buf, size_t cap) {
    (void)u;
    (void)v;
    (void)buf;
    (void)cap;
    return 7;
}

static void test_own_unit(void) {
    cf_unit tick;

    memset(&tick, 0, sizeof tick);
    cf_unit_options_init(&tick.options);
    tick.size   = sizeof tick;
    tick.name   = "tick";
    tick.parse  = tick_parse;
    tick.format = tick_format;

    unit_ok(&tick, "5", 5);
    unit_ok(&tick, "5tk", 5);
    unit_ok(&tick, "2ktk", 2000);
    unit_ok(&tick, "2ktk5tk", 2005);
    unit_bad(&tick, "5tk2ktk", NULL);
    unit_bad(&tick, "5s", NULL);
    unit_bad(&tick, "0.5tk", "is not a whole number of ticks");
    unit_ok(&tick, "0.5ktk", 500);
    check(cf_unit_format(&tick, 5, NULL, 0) == 7, "its own formatter is called");
}

static void test_unit_misc(void) {
    cf_unit_options o;
    cf_i64          v = 0;

    memset(&o, 0xAB, sizeof o);
    cf_unit_options_init(&o);
    check(o.size == sizeof o && o.base == CF_UNIT_BASE_DEFAULT && o.allow_bare &&
              !o.calendar && !o.bare_si,
          "unit options start as the defaults");
    cf_unit_options_init(NULL);

    check(!cf_unit_parse(NULL, "1s", 2, &v, NULL), "no unit, no parse");
    check(!cf_unit_parse(cf_unit_duration_get(), NULL, 0, &v, NULL), "no text");
    check(!cf_unit_parse(cf_unit_duration_get(), "1s", 2, NULL, NULL), "no result");
    check(cf_unit_parse(cf_unit_duration_get(), "1s", 2, &v, NULL) && v == 1000,
          "a refusal reason is optional");
    check(cf_unit_format(NULL, 1, NULL, 0) == 0, "no unit, no text");
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
    test_duration();
    test_size();
    test_own_unit();
    test_unit_misc();
#if CF_WITH_REAL
    test_f64_format();
    test_f64_parse();
    test_f64_carry();
    test_f64_roundtrip();
    test_f64_rejects_nonfinite();
#endif

    return cf_test_done();
}
