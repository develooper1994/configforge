/*
 * test_sizing.c - the sizing modes of a builder (D-175, M-10).
 *
 * cf_builder_sizing records how a configuration's buffers may grow:
 * manual (the build's limits, the default), semi-dynamic (up to the
 * program's cap) or dynamic. The mode is read back by cf_config_sizing
 * and kept by a derived configuration. A build with fixed limits (the
 * minimal profile) accepts the manual mode only.
 */

#include <configforge/configforge.h>

#include <stddef.h>
#include <string.h>

#include "cf_test.h"

static cf_config* sz_create(cf_builder* b, cf_error* err) {
    cf_builder_text(b, "app.name", "x", 0);
    return cf_builder_create(b, err);
}

/* A call, not a constant compared with a constant (MSVC's C6326). */
static int sz_value(cf_sizing m) {
    return (int)m;
}

static void sz_init(void) {
    cf_sizing_options o;

    memset(&o, 0xAB, sizeof o);
    cf_sizing_options_init(&o);
    check(o.size == sizeof(cf_sizing_options), "init sets the size");
    check(o.mode == CF_SIZING_MANUAL, "the default mode is manual");
    check(o.cap == 0, "the default cap is 0, none");
    cf_sizing_options_init(NULL);
    check(sz_value(CF_SIZING_MANUAL) == 0 && sz_value(CF_SIZING_SEMI) == 1 &&
              sz_value(CF_SIZING_DYNAMIC) == 2,
          "the modes have the documented values");
}

static void sz_default(void) {
    cf_error          err = CF_ERROR_INIT;
    cf_sizing_options o;
    cf_config*        c;
    cf_builder*       b = cf_build(NULL);

    c = sz_create(b, &err);
    check(c != NULL, "a configuration builds with no sizing call");
    cf_sizing_options_init(&o);
    o.mode = CF_SIZING_DYNAMIC;
    o.cap  = 7;
    check(cf_config_sizing(c, &o), "its sizing can be read");
    check(o.mode == CF_SIZING_MANUAL && o.cap == 0, "and is manual with no cap");
    check(o.size == sizeof(cf_sizing_options), "the caller's size is kept");
    cf_config_free(c);

    b = cf_build(NULL);
    check(cf_builder_sizing(b, NULL), "NULL options are accepted");
    c = sz_create(b, &err);
    check(c != NULL, "and the configuration builds");
    cf_sizing_options_init(&o);
    check(cf_config_sizing(c, &o) && o.mode == CF_SIZING_MANUAL,
          "NULL options are manual");
    cf_config_free(c);
}

static void sz_refused_args(void) {
    cf_error          err = CF_ERROR_INIT;
    cf_sizing_options o;
    cf_config*        c;
    cf_builder*       b;

    cf_sizing_options_init(&o);
    check(!cf_builder_sizing(NULL, &o), "a NULL builder is refused");
    check(!cf_config_sizing(NULL, &o), "a NULL configuration is refused");
    b = cf_build(NULL);
    c = sz_create(b, &err);
    check(!cf_config_sizing(c, NULL), "NULL options to read into are refused");
    cf_config_free(c);

    b      = cf_build(NULL);
    o.size = offsetof(cf_sizing_options, cap);
    check(!cf_builder_sizing(b, &o), "options too small for a cap are refused");
    c = sz_create(b, &err);
    check(c == NULL, "and the builder fails");
    check(err.code == CF_E_POLICY, "with the policy code");
    cf_config_free(c);

    b = cf_build(NULL);
    cf_sizing_options_init(&o);
    o.mode = (cf_sizing)7;
    check(!cf_builder_sizing(b, &o), "an unknown mode is refused");
    c = sz_create(b, &err);
    check(c == NULL && err.code == CF_E_POLICY, "with the policy code");
    cf_config_free(c);
}

#if CF_FIXED_LIMITS

static void sz_fixed(void) {
    cf_error          err = CF_ERROR_INIT;
    cf_sizing_options o;
    cf_config*        c;
    cf_builder*       b;

    cf_sizing_options_init(&o);
    b = cf_build(NULL);
    check(cf_builder_sizing(b, &o), "manual is accepted by a build with fixed limits");
    c = sz_create(b, &err);
    check(c != NULL, "and builds");
    cf_config_free(c);

    b      = cf_build(NULL);
    o.mode = CF_SIZING_SEMI;
    o.cap  = 4096;
    check(!cf_builder_sizing(b, &o), "semi-dynamic is refused with fixed limits");
    c = sz_create(b, &err);
    check(c == NULL, "and the builder fails");
    check(err.code == CF_E_CAPABILITY, "with the capability code");
    check_text(err.message, "this build has fixed limits (profile minimal)",
               "and says why");
    cf_config_free(c);

    b      = cf_build(NULL);
    o.mode = CF_SIZING_DYNAMIC;
    o.cap  = 0;
    check(!cf_builder_sizing(b, &o), "dynamic is refused with fixed limits");
    c = sz_create(b, &err);
    check(c == NULL && err.code == CF_E_CAPABILITY, "with the capability code");
    cf_config_free(c);
}

#else

static void sz_modes(void) {
    cf_error          err = CF_ERROR_INIT;
    cf_sizing_options o;
    cf_sizing_options got;
    cf_config*        c;
    cf_builder*       b;

    cf_sizing_options_init(&o);
    cf_sizing_options_init(&got);
    o.mode = CF_SIZING_SEMI;
    o.cap  = 65536;
    b      = cf_build(NULL);
    check(cf_builder_sizing(b, &o), "semi-dynamic with a cap is accepted");
    c = sz_create(b, &err);
    check(c != NULL, "and builds");
    check(cf_config_sizing(c, &got), "it is read back");
    check(got.mode == CF_SIZING_SEMI && got.cap == 65536,
          "as semi-dynamic with its cap");
    cf_config_free(c);

    o.mode = CF_SIZING_DYNAMIC;
    o.cap  = 0;
    b      = cf_build(NULL);
    check(cf_builder_sizing(b, &o), "dynamic with no cap is accepted");
    c = sz_create(b, &err);
    check(c != NULL, "and builds");
    check(cf_config_sizing(c, &got) && got.mode == CF_SIZING_DYNAMIC && got.cap == 0,
          "it is read back as dynamic, no cap");
    cf_config_free(c);

    o.cap = 1048576;
    b     = cf_build(NULL);
    check(cf_builder_sizing(b, &o), "dynamic with a cap is accepted");
    c = sz_create(b, &err);
    check(c != NULL && cf_config_sizing(c, &got) && got.cap == 1048576,
          "and the cap is read back");
    cf_config_free(c);

    o.mode = CF_SIZING_MANUAL;
    o.cap  = 0;
    b      = cf_build(NULL);
    check(cf_builder_sizing(b, &o), "manual is accepted");
    c = sz_create(b, &err);
    check(c != NULL && cf_config_sizing(c, &got) && got.mode == CF_SIZING_MANUAL,
          "and read back");
    cf_config_free(c);

    o.mode = CF_SIZING_DYNAMIC;
    o.cap  = 1024;
    b      = cf_build(NULL);
    check(cf_builder_sizing(b, &o), "the first call is accepted");
    o.mode = CF_SIZING_MANUAL;
    o.cap  = 0;
    check(cf_builder_sizing(b, &o), "and so is a second");
    c = sz_create(b, &err);
    check(c != NULL && cf_config_sizing(c, &got) && got.mode == CF_SIZING_MANUAL &&
              got.cap == 0,
          "the last call wins");
    cf_config_free(c);
}

static void sz_semi_needs_cap(void) {
    cf_error          err = CF_ERROR_INIT;
    cf_sizing_options o;
    cf_config*        c;
    cf_builder*       b = cf_build(NULL);

    cf_sizing_options_init(&o);
    o.mode = CF_SIZING_SEMI;
    check(!cf_builder_sizing(b, &o), "semi-dynamic with cap 0 is refused");
    c = sz_create(b, &err);
    check(c == NULL, "and the builder fails");
    check(err.code == CF_E_POLICY, "with the policy code");
    check_text(err.message, "semi-dynamic sizing needs a cap",
               "and says what is missing");
    cf_config_free(c);
}

static void sz_derived(void) {
    static const cf_pair pairs[] = {{"app.name", "y"}, {NULL, NULL}};
    cf_error             err     = CF_ERROR_INIT;
    cf_sizing_options    o;
    cf_sizing_options    got;
    cf_config*           c;
    cf_config*           d;
    cf_builder*          b = cf_build(NULL);

    cf_sizing_options_init(&o);
    cf_sizing_options_init(&got);
    o.mode = CF_SIZING_SEMI;
    o.cap  = 9000;
    check(cf_builder_sizing(b, &o), "semi-dynamic is set");
    c = sz_create(b, &err);
    d = (c != NULL) ? cf_derive(c, "derived", pairs, 1, &err) : NULL;
    check(d != NULL, "a configuration is derived");
    check(d != NULL && cf_config_sizing(d, &got), "its sizing can be read");
    check(got.mode == CF_SIZING_SEMI && got.cap == 9000, "and is the original's");
    cf_config_free(d);
    cf_config_free(c);
}

#endif

int main(void) {
    sz_init();
    sz_default();
    sz_refused_args();
#if CF_FIXED_LIMITS
    sz_fixed();
#else
    sz_modes();
    sz_semi_needs_cap();
    sz_derived();
#endif
    return cf_test_done();
}
