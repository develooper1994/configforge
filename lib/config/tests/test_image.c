/*
 * test_image.c - the binary snapshot image.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <string.h>

#include "cf_test.h"

#if CF_WITH_IMAGE

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

    /* And the chain is read, not only carried: loaded as a source, a
       value says where it was set before it was baked. */
    {
        static union {
            cf_i64 a;
            char   b[1024];
        } state;
        static char why[2048];
        cf_source   src;
        cf_builder* back_b;
        cf_config*  back;

        back_b = cf_build(NULL);
        cf_source_image_mem(&src, "firmware", CF_RANK_SYS_FILE, g_img2.b, n2, state.b,
                            sizeof state.b);
        cf_builder_source(back_b, &src);
        cf_builder_int(back_b, "server.port", 8080, 1, 65535, 0);
        back = cf_builder_create(back_b, &err);
        check(back != NULL, "the image with its chain loads as a source");
        if (back != NULL) {
            cf_explain(back, "server.port", why, sizeof why);
            has(why, "argv[1]", "and explain() names where the value was set");
            cf_config_free(back);
        }

        back_b = cf_build(NULL);
        cf_source_image_mem(&src, "firmware", CF_RANK_SYS_FILE, g_img.b, n1, state.b,
                            sizeof state.b);
        cf_builder_source(back_b, &src);
        cf_builder_int(back_b, "server.port", 8080, 1, 65535, 0);
        back = cf_builder_create(back_b, &err);
        if (back != NULL) {
            cf_explain(back, "server.port", why, sizeof why);
            has(why, "image", "a resolved-only image can only say it was the image");
            cf_config_free(back);
        }
    }

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
       pointed anywhere it liked. All bits set is spelled (char)-1:
       (char)-1 truncates a constant where char is signed, and MSVC
       says so at /W4. */
    {
        size_t idx_off = (size_t)(unsigned char)g_img.b[16]; /* from the header */
        g_img2.b[idx_off + 8]  = (char)-1;
        g_img2.b[idx_off + 9]  = (char)-1;
        g_img2.b[idx_off + 10] = (char)-1;
        g_img2.b[idx_off + 11] = (char)0x7F;
    }
    check(cf_image_open(&img, g_img2.b, n, &err),
          "the header still describes a plausible image");
    check(!cf_image_at(&img, 0, NULL, NULL, &v),
          "and the entry with the impossible value index is refused");

    /* A chain count with nowhere to put it. */
    memcpy(g_img2.b, g_img.b, n);
    g_img2.b[36] = (char)-1;
    g_img2.b[37] = (char)-1;
    check(!cf_image_open(&img, g_img2.b, n, &err),
          "a chain that does not fit is refused at the door");

    /* A count whose records times their 16 bytes is 2^32: where size_t
       is 32 bits the product wrapped to 0, the regions "fit", and every
       record past the real ones was read out of bounds - wasm32 found it
       under the fuzzer. Refused before it is multiplied, everywhere. */
    memcpy(g_img2.b, g_img.b, n);
    g_img2.b[12] = 0;
    g_img2.b[13] = 0;
    g_img2.b[14] = 0;
    g_img2.b[15] = 0x10;
    check(!cf_image_open(&img, g_img2.b, n, &err) &&
              strstr(err.message, "outside the image") != NULL,
          "a count that wraps where size_t is 32 bits is refused at the door");
}

/* ---------------------------------------------------------------- */
/* M-12: major 2 carries the build's limits and id in a 128-byte header;
   major 1 images still load. */

static unsigned rd16(const char* p) {
    return (unsigned)(unsigned char)p[0] | ((unsigned)(unsigned char)p[1] << 8);
}

static cf_u32 rd32(const char* p) {
    return (cf_u32)rd16(p) | ((cf_u32)rd16(p + 2) << 16);
}

static void test_major_2(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_config*  cfg = build(&err, 1);
    cf_image    img;
    size_t      n, m, i;
    cf_value    v;
    const char* id = cf_build_id();

    if (cfg == NULL) {
        check(0, "builds");
        return;
    }
    n = cf_image_write(cf_keys(cfg), NULL, 0, g_img.b, sizeof g_img.b);
    check(n > 0 && rd16(g_img.b + 4) == 2 && rd16(g_img.b + 6) == 0,
          "major 2, minor 0");
    check(rd32(g_img.b + 16) >= 128, "the index starts at or after 128");
    check(rd16(g_img.b + 64) == CF_KEY_MAX && rd16(g_img.b + 66) == CF_SOURCE_MAX &&
              rd16(g_img.b + 68) == CF_MESSAGE_MAX &&
              rd16(g_img.b + 70) == CF_MAX_DEPTH,
          "the limits are at 64");
    check(rd32(g_img.b + 104) == 0 && rd32(g_img.b + 108) == 0,
          "the manifest fields are written zero");
    check(cf_image_open(&img, g_img.b, n, &err), "a major 2 image opens");
    check(img.key_max == CF_KEY_MAX && img.source_max == CF_SOURCE_MAX &&
              img.message_max == CF_MESSAGE_MAX && img.max_depth == CF_MAX_DEPTH,
          "the limits read back");
    check(img.build_id != NULL && strncmp(img.build_id, id, 31) == 0 &&
              strlen(img.build_id) <= 31,
          "the build id reads back (31 bytes at most)");
    check(cf_image_get(&img, "server.port", 11, &v) && v.u.i == 7000, "and it reads");
    check(img.digest == cf_digest(cfg), "the digest is of the key space");

    /* CF_IMAGE_V1 writes major 1, 64-byte header, same digest. */
    m = cf_image_write(cf_keys(cfg), NULL, CF_IMAGE_V1, g_img2.b, sizeof g_img2.b);
    check(m > 0 && rd16(g_img2.b + 4) == 1, "CF_IMAGE_V1 writes major 1");
    check(rd32(g_img2.b + 16) == 64, "with the 64-byte header");
    check(rd32(g_img2.b + 8) == 0, "the request is not stored in the image's flags");
    check(cf_image_open(&img, g_img2.b, m, &err), "a major 1 image opens");
    check(img.build_id == NULL && img.key_max == 0 && img.max_depth == 0,
          "with no limits and no id");
    check(img.digest == cf_digest(cfg), "the same digest in both");
    check(n == m + 64, "the sizes differ by the header");
    check(cf_image_write(cf_keys(cfg), NULL, CF_IMAGE_V1, NULL, 0) == m,
          "a major 1 image can be measured");
    cf_config_free(cfg);

    /* A key limit above this build's is refused, naming both. */
    cfg = build(&err, 1);
    n   = cf_image_write(cf_keys(cfg), NULL, 0, g_img.b, sizeof g_img.b);
    cf_config_free(cfg);
    g_img.b[64] = (char)((CF_KEY_MAX + 1) & 0xFF);
    g_img.b[65] = (char)(((CF_KEY_MAX + 1) >> 8) & 0xFF);
    check(!cf_image_open(&img, g_img.b, n, &err),
          "longer keys than this build's refused");
    has(err.message, "image was made with longer keys", "and says so");
    has(err.message, "its limit", "naming the image's limit");

    /* Major 3 is refused with the old message. */
    cfg = build(&err, 1);
    n   = cf_image_write(cf_keys(cfg), NULL, 0, g_img.b, sizeof g_img.b);
    cf_config_free(cfg);
    g_img.b[4] = 3;
    check(!cf_image_open(&img, g_img.b, n, &err), "major 3 refused");
    has(err.message, "this build reads", "naming what it can read");

    /* A later minor with the manifest fields filled and unknown bytes
       in the reserved ranges opens: a reader ignores what it does not
       know. */
    cfg = build(&err, 1);
    n   = cf_image_write(cf_keys(cfg), NULL, 0, g_img.b, sizeof g_img.b);
    cf_config_free(cfg);
    g_img.b[6] = 1;
    for (i = 104; i < 112; ++i) {
        g_img.b[i] = (char)0x5A;
    }
    g_img.b[52]  = 7;
    g_img.b[120] = 9;
    check(cf_image_open(&img, g_img.b, n, &err), "a later minor with a manifest opens");
    check(cf_image_get(&img, "server.host", 11, &v), "and reads");
}

static void test_major_1_fixture(void) {
    cf_error err = CF_ERROR_INIT;
    cf_image img;
    cf_value v;
    FILE*    f = fopen(CF_IMAGE_DATA "/image-v1.cfgb", "rb");
    size_t   n;

    check(f != NULL, "the major 1 fixture is there");
    if (f == NULL) {
        return;
    }
    n = fread(g_img2.b, 1, sizeof g_img2.b, f);
    fclose(f);
    check(n > 64 && rd16(g_img2.b + 4) == 1, "it is major 1");
    check(cf_image_open(&img, g_img2.b, n, &err), "and loads");
    check(img.build_id == NULL, "with no build id");
    check(cf_image_get(&img, "server.port", 11, &v) && v.type == CF_TYPE_INT &&
              v.u.i == 7000,
          "and its values read");
}

int main(void) {
    test_round_trip();
    test_as_a_source();
    test_refusals();
    test_corrupt_index_is_refused();
    test_provenance_travels();
    test_major_2();
    test_major_1_fixture();

    return cf_test_done();
}

#else /* !CF_WITH_IMAGE */

int main(void) {
    printf("skipped: this build has no snapshot image (CF_WITH_IMAGE=OFF)\n");
    return cf_test_done();
}

#endif /* CF_WITH_IMAGE */
