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

/* ------------------------------------------------------------------ *
 * M-11a: the readers and the key space follow the mode.
 * ------------------------------------------------------------------ */

enum { SZ_DOC_MAX = 6000 };

/* A document of one key of `klen` letters holding a string of `vlen`
   letters, in the named format; its length, or 0 for an unknown one. */
static size_t sz_doc(char* out, const char* format, size_t klen, size_t vlen) {
    size_t n = 0;
    size_t i;
    int    json = strcmp(format, "json") == 0;
    int    toml = strcmp(format, "toml") == 0;
    int    ini  = strcmp(format, "ini") == 0;

    if (json) {
        out[n++] = '{';
        out[n++] = '"';
    } else if (toml || ini) {
        /* a bare key */
    }
    for (i = 0; i < klen; ++i) {
        out[n++] = 'k';
    }
    if (json) {
        out[n++] = '"';
        out[n++] = ':';
        out[n++] = '"';
    } else if (toml) {
        memcpy(out + n, " = \"", 4);
        n += 4;
    } else if (ini) {
        memcpy(out + n, " = ", 3);
        n += 3;
    } else {
        memcpy(out + n, ": ", 2);
        n += 2;
    }
    for (i = 0; i < vlen; ++i) {
        out[n++] = 'v';
    }
    if (json) {
        out[n++] = '"';
        out[n++] = '}';
    } else if (toml) {
        out[n++] = '"';
    }
    out[n++] = '\n';
    out[n]   = '\0';
    return n;
}

static void sz_fresh(cf_error* e) {
    cf_error fresh = CF_ERROR_INIT;
    *e             = fresh;
}

/* "<format>: <text>" in `out`, which holds 96 bytes. */
static void sz_msg(char* out, const char* format, const char* text) {
    size_t a = strlen(format);
    size_t b = strlen(text);
    memcpy(out, format, a);
    out[a]     = ':';
    out[a + 1] = ' ';
    memcpy(out + a + 2, text, b + 1);
}

static cf_config* sz_read(cf_sizing mode, size_t cap, const char* format, size_t klen,
                          size_t vlen, cf_error* err) {
    static char       doc[SZ_DOC_MAX];
    cf_sizing_options o;
    cf_builder*       b = cf_build(NULL);
    size_t            n = sz_doc(doc, format, klen, vlen);

    cf_sizing_options_init(&o);
    o.mode = mode;
    o.cap  = cap;
    if (!cf_builder_sizing(b, &o)) {
        return cf_builder_create(b, err);
    }
    cf_builder_document(b, "doc", CF_RANK_USER_FILE, doc, n, format, 0);
    return cf_builder_create(b, err);
}

static void sz_key_name(char* out, size_t klen) {
    memset(out, 'k', klen);
    out[klen] = '\0';
}

static void sz_readers_one(const char* format) {
    static char key[SZ_DOC_MAX];
    cf_error    err = CF_ERROR_INIT;
    cf_config*  c;
    size_t      n = 0;
    char        what[96];

    sz_key_name(key, 600);

    /* MANUAL: refused, as before. */
    c = sz_read(CF_SIZING_MANUAL, 0, format, 600, 1, &err);
    sz_msg(what, format, "a 600-byte key is refused under manual sizing");
    check(c == NULL, what);
    cf_config_free(c);
    sz_fresh(&err);
    c = sz_read(CF_SIZING_MANUAL, 0, format, 4, 4000, &err);
    sz_msg(what, format, "a 4000-byte string under manual sizing");
    check(c != NULL || err.code != CF_OK, what);
    cf_config_free(c);

    /* DYNAMIC: accepted. */
    sz_fresh(&err);
    c = sz_read(CF_SIZING_DYNAMIC, 0, format, 600, 1, &err);
    sz_msg(what, format, "a 600-byte key is accepted by dynamic sizing");
    check(c != NULL, what);
    check(c != NULL && cf_has(c, key), "and its key is there");
    cf_config_free(c);
    sz_fresh(&err);
    c = sz_read(CF_SIZING_DYNAMIC, 0, format, 4, 4000, &err);
    sz_msg(what, format, "a 4000-byte string is accepted by dynamic sizing");
    check(c != NULL, what);
    check(c != NULL && cf_get_text_n(c, "kkkk", &n, NULL) != NULL && n == 4000,
          "and its value is whole");
    cf_config_free(c);

    /* SEMI with a cap of 1024: 600 in, 2000 out. */
    sz_fresh(&err);
    c = sz_read(CF_SIZING_SEMI, 1024, format, 600, 1, &err);
    sz_msg(what, format, "a 600-byte key is accepted under a cap of 1024");
    check(c != NULL, what);
    cf_config_free(c);
    sz_fresh(&err);
    c = sz_read(CF_SIZING_SEMI, 1024, format, 2000, 1, &err);
    sz_msg(what, format, "a 2000-byte key is refused under a cap of 1024");
    check(c == NULL, what);
    cf_config_free(c);
}

static void sz_readers(void) {
    sz_readers_one("json");
    sz_readers_one("toml");
    sz_readers_one("ini");
    sz_readers_one("yaml");
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
    sz_readers();
#endif
    return cf_test_done();
}
