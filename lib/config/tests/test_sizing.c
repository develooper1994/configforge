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
    cf_error_clear(e);
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

/* ------------------------------------------------------------------ *
 * M-11b: cf_error owns its text (D-176).
 * ------------------------------------------------------------------ */

enum { SZ_LONG = 3000, SZ_LONG_KEY = 600 };

static int sz_inside(const cf_error* e, const char* p) {
    return p >= (const char*)e && p < (const char*)e + sizeof *e;
}

static void sz_error_cut(void) {
    static char big[SZ_LONG + 1];
    static char key[SZ_LONG_KEY + 1];
    cf_error    e = CF_ERROR_INIT;

    memset(big, 'm', SZ_LONG);
    memset(key, 'k', SZ_LONG_KEY);

    /* cf_error_set is what it was: cut at the old arrays' sizes, inline. */
    cf_error_set(&e, CF_E_SOURCE, key, "src", "%s", big);
    check(strlen(e.message) == (size_t)CF_MESSAGE_MAX - 1u,
          "cf_error_set cuts a 3000-byte message at CF_MESSAGE_MAX - 1");
    check(strlen(e.key) == (size_t)CF_KEY_MAX - 1u,
          "cf_error_set cuts a 600-byte key at CF_KEY_MAX - 1");
    check(sz_inside(&e, e.message) && sz_inside(&e, e.key) && sz_inside(&e, e.source),
          "and keeps all three texts in the error's own store");
    check(strcmp(e.source, "src") == 0, "a short source is whole");
    cf_error_clear(&e);
    check(e.key != NULL && e.source != NULL && e.message != NULL && e.key[0] == '\0' &&
              e.source[0] == '\0' && e.message[0] == '\0',
          "a cleared error has empty texts, never NULL");

    /* A struct of zeros has NULL pointers until its first use, and works. */
    memset(&e, 0, sizeof e);
    cf_error_set(&e, CF_E_SOURCE, "a.b", NULL, "%s", "x");
    check(strcmp(e.key, "a.b") == 0 && e.source[0] == '\0' &&
              strcmp(e.message, "x") == 0,
          "a zeroed error takes a set");
    cf_error_clear(&e);
}

#if !CF_FIXED_LIMITS
static void sz_error_long(void) {
    static char big[SZ_LONG + 1];
    static char key[SZ_LONG_KEY + 1];
    cf_error    a = CF_ERROR_INIT;
    cf_error    b = CF_ERROR_INIT;

    memset(big, 'm', SZ_LONG);
    memset(key, 'k', SZ_LONG_KEY);

    cf_error_set_long(&a, CF_E_SOURCE, key, "src", 0, "%s", big);
    check(a.code == CF_E_SOURCE, "cf_error_set_long sets the code");
    check(strlen(a.message) == (size_t)SZ_LONG && memcmp(a.message, big, SZ_LONG) == 0,
          "a 3000-byte message is whole under cap 0");
    check(strlen(a.key) == (size_t)SZ_LONG_KEY && memcmp(a.key, key, SZ_LONG_KEY) == 0,
          "a 600-byte key is whole under cap 0");
    check(!sz_inside(&a, a.message) && !sz_inside(&a, a.key),
          "a text past the store lives in a block of its own");
    check(strcmp(a.source, "src") == 0 && sz_inside(&a, a.source),
          "a short source stays in the store");

    cf_error_copy(&b, &a);
    check(strlen(b.message) == (size_t)SZ_LONG &&
              memcmp(b.message, big, SZ_LONG) == 0 &&
              strlen(b.key) == (size_t)SZ_LONG_KEY && strcmp(b.source, "src") == 0,
          "cf_error_copy keeps the long texts whole");
    check(b.message != a.message && b.key != a.key && b.source != a.source,
          "the copy's texts are its own");
    check(!sz_inside(&a, b.message) && !sz_inside(&a, b.key) &&
              !sz_inside(&a, b.source),
          "no pointer of the copy points into the source");

    cf_error_clear(&a);
    check(a.message[0] == '\0' && a.key[0] == '\0' && a.code == CF_OK &&
              a.cfi_heap[0] == NULL && a.cfi_heap[1] == NULL && a.cfi_heap[2] == NULL,
          "cf_error_clear empties the texts and drops the blocks");
    check(strlen(b.message) == (size_t)SZ_LONG && memcmp(b.message, big, SZ_LONG) == 0,
          "and the copy is not touched");
    cf_error_clear(&b);

    /* A cap holds the text to it; a short text takes no block. */
    cf_error_set_long(&a, CF_E_SOURCE, key, "src", 1000, "%s", big);
    check(strlen(a.message) == 1000u && strlen(a.key) == (size_t)SZ_LONG_KEY,
          "a cap of 1000 cuts the message and keeps the 600-byte key");
    cf_error_set_long(&a, CF_E_SOURCE, key, "src", 100, "%s", big);
    check(strlen(a.message) == 100u && strlen(a.key) == 100u,
          "a cap of 100 cuts both, over the old arrays' size");
    cf_error_set_long(&a, CF_E_SOURCE, "a.b", NULL, 0, "%s %d", "short", 7);
    check(strcmp(a.message, "short 7") == 0 && sz_inside(&a, a.message) &&
              a.cfi_heap[2] == NULL,
          "a short message is inline and a second set released the first block");
    cf_error_clear(&a);

    /* A set on top of a long text, from that text. */
    cf_error_set_long(&a, CF_E_SOURCE, NULL, NULL, 0, "%s", big);
    cf_error_set_long(&a, CF_E_SOURCE, NULL, NULL, 0, "%s%s", a.message, "!");
    check(strlen(a.message) == (size_t)SZ_LONG + 1u && a.message[SZ_LONG] == '!',
          "a long message can be built from the one it replaces");
    cf_error_clear(&a);
}
#endif

static void sz_error_fixed(void) {
#if CF_FIXED_LIMITS
    static char big[SZ_LONG + 1];
    cf_error    e = CF_ERROR_INIT;

    memset(big, 'm', SZ_LONG);
    cf_error_set_long(&e, CF_E_SOURCE, NULL, NULL, 0, "%s", big);
    check(strlen(e.message) == (size_t)CF_MESSAGE_MAX - 1u && e.cfi_heap[2] == NULL,
          "a build with fixed limits never allocates: cf_error_set_long cuts");
    cf_error_clear(&e);
#endif
}

int main(void) {
    sz_init();
    sz_default();
    sz_refused_args();
    sz_error_cut();
    sz_error_fixed();
#if CF_FIXED_LIMITS
    sz_fixed();
#else
    sz_modes();
    sz_semi_needs_cap();
    sz_derived();
    sz_readers();
    sz_error_long();
#endif
    return cf_test_done();
}
