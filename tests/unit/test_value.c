/* test_value.c - the five types and their canonical text form. */
#include "configforge/cf_value.h"
#include <stdio.h>
#include <string.h>
#include <locale.h>

static int  g_fail = 0, g_run = 0;
static void check(int c, const char* w) {
    ++g_run;
    if (!c) {
        ++g_fail;
        printf("FAIL: %s\n", w);
    }
}

static void text_is(const cf_value* v, const char* want, const char* what) {
    char   buf[128];
    size_t n = cf_value_to_text(v, buf, sizeof buf);
    ++g_run;
    if (n != strlen(want) || strcmp(buf, want) != 0) {
        ++g_fail;
        printf("FAIL: %s got=\"%s\" want=\"%s\"\n", what, buf, want);
    }
}

static void test_roundtrip(void) {
    cf_value v;

    check(cf_value_from_text(CF_TYPE_BOOL, "true", 4, &v), "bool true parses");
    text_is(&v, "true", "bool true prints");
    check(cf_value_from_text(CF_TYPE_BOOL, "false", 5, &v), "bool false parses");
    text_is(&v, "false", "bool false prints");

    /* Not booleans here. See decision D-24. */
    check(!cf_value_from_text(CF_TYPE_BOOL, "yes", 3, &v), "yes is not bool");
    check(!cf_value_from_text(CF_TYPE_BOOL, "TRUE", 4, &v), "TRUE is not bool");
    check(!cf_value_from_text(CF_TYPE_BOOL, "1", 1, &v), "1 is not bool");

    check(cf_value_from_text(CF_TYPE_INT, "-42", 3, &v) && v.u.i == -42, "int parses");
    text_is(&v, "-42", "int prints");
    check(!cf_value_from_text(CF_TYPE_INT, "4.2", 3, &v), "int rejects fraction");

    check(cf_value_from_text(CF_TYPE_NULL, "null", 4, &v), "null parses");
    text_is(&v, "null", "null prints");

    check(cf_value_from_text(CF_TYPE_TEXT, "a b", 3, &v), "text parses");
    text_is(&v, "a b", "text prints");
}

#if CF_WITH_REAL
static void test_real(void) {
    cf_value v;
    check(cf_value_from_text(CF_TYPE_REAL, "0.1", 3, &v), "real parses");
    text_is(&v, "0.1", "real prints shortest");
    check(!cf_value_from_text(CF_TYPE_REAL, "1,5", 3, &v), "comma rejected");
}
#endif

static void test_infer(void) {
    cf_value v;
    check(cf_value_infer("8080", 4, &v) && v.type == CF_TYPE_INT, "infer int");
    check(cf_value_infer("true", 4, &v) && v.type == CF_TYPE_BOOL, "infer bool");
    check(cf_value_infer("no", 2, &v) && v.type == CF_TYPE_TEXT, "no stays text");
    check(cf_value_infer("08", 2, &v) && v.type == CF_TYPE_TEXT, "08 stays text");
}

static void test_compare(void) {
    cf_value a, b;
    cf_value_int(&a, 1);
    cf_value_int(&b, 2);
    check(cf_value_cmp(&a, &b) < 0, "1 < 2");
    cf_value_int(&b, 1);
    check(cf_value_equal(&a, &b), "1 == 1");
    cf_value_text(&b, "1", 1);
    check(!cf_value_equal(&a, &b), "int 1 != text 1");
}

static void test_small_buffer(void) {
    cf_value v;
    char     buf[2];
    cf_value_text(&v, "hello", 5);
    check(cf_value_to_text(&v, buf, sizeof buf) == 0, "too-small returns 0");
    check(buf[0] == '\0', "too-small terminates");
}

int main(int argc, char** argv) {
    if (argc > 1) {
        setlocale(LC_ALL, argv[1]);
    }
    test_roundtrip();
#if CF_WITH_REAL
    test_real();
#endif
    test_infer();
    test_compare();
    test_small_buffer();
    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
