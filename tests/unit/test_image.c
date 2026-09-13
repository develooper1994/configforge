/*
 * test_image.c - the binary snapshot image.
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
static void has(const char* hay, const char* needle, const char* w) {
    ++g_run;
    if (hay == NULL || strstr(hay, needle) == NULL) {
        ++g_fail;
        printf("FAIL: %s  missing \"%s\" in \"%s\"\n", w, needle, hay ? hay : "(null)");
    }
}

/* The image is dereferenced in place, so the buffer has to be aligned
   the way a linker would align a const array. A char array on the
   stack is not, which is the mistake the loader refuses. */
static union {
    cf_i64 align;
    char   b[8192];
} g_img;
static union {
    cf_i64 align;
    char   b[8192];
} g_img2;

static cf_config* build(cf_error* err, int with_real) {
    static char  a0[]   = "prog";
    static char  a1[]   = "--server.port=7000";
    static char* argv[] = {a0, a1, NULL};
    cf_builder*  b      = cf_build(NULL);

    cf_builder_cli(b, "cli", CF_RANK_CLI, 2, argv);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_builder_text(b, "server.host", "0.0.0.0", 0);
    cf_builder_text(b, "log.level", "info", 0);
    cf_builder_bool(b, "tls.enabled", CF_FALSE, 0);
#if CF_WITH_REAL
    if (with_real) {
        cf_builder_real(b, "sample.ratio", 0.125, 0.0, 1.0, 0);
    }
#else
    (void)with_real;
#endif
    return cf_builder_create(b, err);
}

/* ---------------------------------------------------------------- */

static void test_round_trip(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* cfg = build(&err, 1);
    cf_image   img;
    size_t     n, m;
    cf_value   v;

    check(cfg != NULL, "builds");
    if (cfg == NULL) {
        printf("  %s\n", err.message);
        return;
    }

    m = cf_image_write(cf_keys(cfg), NULL, 0, NULL, 0);
    n = cf_image_write(cf_keys(cfg), NULL, 0, g_img.b, sizeof g_img.b);
    check(n > 0 && n < sizeof g_img.b, "image fits");
    check(m == n, "an image can be measured before it is written");

    check(cf_image_open(&img, g_img.b, n, &err), "image opens");
    if (err.message[0] != '\0') {
        printf("  %s\n", err.message);
    }

    /* The digest travels with the image and is the same number the key
       space computed, so a boot path can check it did not get a
       half-written flash page. */
    check(img.digest == cf_digest(cfg), "digest survives");
    check(cf_image_count(&img) == cf_count(cfg), "every key survives");

    check(cf_image_get(&img, "server.port", 11, &v) && v.type == CF_TYPE_INT &&
              v.u.i == 7000,
          "int by binary search");
    check(cf_image_get(&img, "server.host", 11, &v) && v.type == CF_TYPE_TEXT &&
              strcmp(v.u.t.p, "0.0.0.0") == 0,
          "text points into the image");
    check(cf_image_get(&img, "tls.enabled", 11, &v) && v.type == CF_TYPE_BOOL && !v.u.b,
          "bool");
#if CF_WITH_REAL
    check(cf_image_get(&img, "sample.ratio", 12, &v) && v.type == CF_TYPE_REAL &&
              v.u.r == 0.125,
          "a double survives as its bit pattern");
#endif
    check(!cf_image_get(&img, "no.such.key", 11, &v), "absent is absent");

    /* Keys come out sorted, because the image is a picture of a sorted
       key space rather than a dump of one. */
    {
        const char* prev    = NULL;
        size_t      plen    = 0, i;
        int         ordered = 1;
        for (i = 0; i < cf_image_count(&img); ++i) {
            const char* k;
            size_t      kl;
            cf_image_at(&img, i, &k, &kl, NULL);
            if (prev != NULL && cf_cmp_bytes(prev, plen, k, kl) >= 0) {
                ordered = 0;
            }
            prev = k;
            plen = kl;
        }
        check(ordered, "the index is sorted");
    }
    cf_config_free(cfg);
}

static void test_as_a_source(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_config*  cfg = build(&err, 0);
    cf_builder* b;
    cf_config*  back;
    /* Sized from the library rather than guessed. The first version
       used a 256-byte array, cf_source_image_mem refused it, and the
       uninitialised cf_source that followed crashed - which is what a
       state size the caller has to know would do to every caller. */
    static union {
        cf_i64 a;
        char   b[1024];
    } state;
    size_t       n;
    static char  a0[]     = "prog";
    static char* noargs[] = {a0, NULL};
    cf_source    src;

    if (cfg == NULL) {
        check(0, "builds");
        return;
    }
    n = cf_image_write(cf_keys(cfg), NULL, 0, g_img.b, sizeof g_img.b);

    /* No file, no parser, no allocation: the freestanding path, with
       the image standing in for a const array in flash. */
    b = cf_build(NULL);
    cf_builder_cli(b, "cli", CF_RANK_CLI, 1, noargs);
    check(cf_source_image_state_size() <= sizeof state.b,
          "the state size the library asks for fits");
    check(cf_source_image_mem(&src, "baked", CF_RANK_SYS_FILE, g_img.b, n, state.b,
                              sizeof state.b),
          "image source built");
    check(cf_builder_source(b, &src), "and registered");
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    back = cf_builder_create(b, &err);

    check(back != NULL, "an image loads as a source");
    if (back == NULL) {
        printf("  %s\n", err.message);
        cf_config_free(cfg);
        return;
    }
    check(cf_get_int(back, "server.port", 0) == 7000, "the value the image carried");

    {
        static char buf[2048];
        cf_explain(back, "server.port", buf, sizeof buf);
        has(buf, "baked", "the image is a named source");
        has(buf, "8080", "and the default is still under it");
    }
    cf_config_free(back);
    cf_config_free(cfg);
}

static void test_refusals(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* cfg = build(&err, 0);
    cf_image   img;
    size_t     n;

    if (cfg == NULL) {
        check(0, "builds");
        return;
    }
    n = cf_image_write(cf_keys(cfg), NULL, 0, g_img.b, sizeof g_img.b);
    cf_config_free(cfg);

    /* A truncated image and a corrupt one look identical from inside,
       and neither may produce a configuration. */
    check(!cf_image_open(&img, g_img.b, n - 1, &err), "truncation refused");
    has(err.message, "bytes", "and says the size does not match");

    check(!cf_image_open(&img, g_img.b, 4, &err), "a stub is refused");

    memcpy(g_img2.b, g_img.b, n);
    g_img2.b[0] = 'X';
    check(!cf_image_open(&img, g_img2.b, n, &err), "a wrong magic is refused");
    has(err.message, "not a ConfigForge image", "by name");

    memcpy(g_img2.b, g_img.b, n);
    g_img2.b[4] = (char)(CF_IMAGE_MAJOR + 1);
    check(!cf_image_open(&img, g_img2.b, n, &err), "a future format is refused");
    has(err.message, "this build reads", "naming what it can read");

    /* Refusing an unaligned buffer is the honest answer: dereferencing
       one is undefined on every target that cares and silent on the
       ones that do not. */
    check(!cf_image_open(&img, g_img.b + 1, n - 1, &err),
          "an unaligned buffer is refused");
    has(err.message, "aligned", "and says so");
}

static void test_provenance_travels(void) {
    static char  e0[]   = "MYAPP_SERVER__PORT=9090";
    static char* env[]  = {e0, NULL};
    static char  a0[]   = "prog";
    static char  a1[]   = "--server.port=7000";
    static char* argv[] = {a0, a1, NULL};
    cf_error     err    = CF_ERROR_INIT;
    cf_builder*  b      = cf_build(NULL);
    cf_config*   cfg;
    cf_image     plain, full;
    size_t       n1, n2;

    cf_builder_cli(b, "cli", CF_RANK_CLI, 2, argv);
    cf_builder_env(b, "env", CF_RANK_ENV, "MYAPP_", env);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cfg = cf_builder_create(b, &err);
    if (cfg == NULL) {
        check(0, "builds");
        return;
    }

    n1 = cf_image_write(cf_keys(cfg), NULL, 0, g_img.b, sizeof g_img.b);
    n2 = cf_image_write(cf_keys(cfg), NULL, CF_IMAGE_PROVENANCE, g_img2.b,
                        sizeof g_img2.b);

    check(cf_image_open(&plain, g_img.b, n1, &err), "resolved-only opens");
    check(cf_image_open(&full, g_img2.b, n2, &err), "with-chain opens");

    /* Carrying the chain is what makes the image a ConfigForge artifact
       rather than a blob: a device with no filesystem can still say why
       a value is what it is. It costs bytes, and the flag says so. */
    check(n2 > n1, "the chain costs space");
    check((plain.flags & CF_IMAGE_PROVENANCE) == 0, "plain says so");
    check((full.flags & CF_IMAGE_PROVENANCE) != 0, "the other says so");
    check(plain.digest == full.digest, "and both describe the same configuration");

    cf_config_free(cfg);
}

/* Every number in an image is a claim, not a fact. The value index in
   particular comes straight out of the file and was dereferenced
   without a bound, which the mutation fuzzer found on its first run. */
static void test_corrupt_index_is_refused(void) {
    cf_error   err = CF_ERROR_INIT;
    cf_config* cfg = build(&err, 0);
    cf_image   img;
    size_t     n;
    cf_value   v;

    if (cfg == NULL) {
        check(0, "builds");
        return;
    }
    n = cf_image_write(cf_keys(cfg), NULL, 0, g_img.b, sizeof g_img.b);
    cf_config_free(cfg);

    memcpy(g_img2.b, g_img.b, n);
    /* The first index record's value_index, at offset 8 of the record,
       pointed anywhere it liked. */
    {
        size_t idx_off         = 64; /* the header is 64 bytes */
        g_img2.b[idx_off + 8]  = (char)0xFF;
        g_img2.b[idx_off + 9]  = (char)0xFF;
        g_img2.b[idx_off + 10] = (char)0xFF;
        g_img2.b[idx_off + 11] = (char)0x7F;
    }
    check(cf_image_open(&img, g_img2.b, n, &err),
          "the header still describes a plausible image");
    check(!cf_image_at(&img, 0, NULL, NULL, &v),
          "and the entry with the impossible value index is refused");

    /* A chain count with nowhere to put it. */
    memcpy(g_img2.b, g_img.b, n);
    g_img2.b[36] = (char)0xFF;
    g_img2.b[37] = (char)0xFF;
    check(!cf_image_open(&img, g_img2.b, n, &err),
          "a chain that does not fit is refused at the door");
}

int main(void) {
    test_round_trip();
    test_as_a_source();
    test_refusals();
    test_corrupt_index_is_refused();
    test_provenance_travels();

    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
