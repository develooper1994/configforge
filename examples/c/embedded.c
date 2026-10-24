/*
 * embedded.c - a configuration on a device with no filesystem and no heap.
 *
 * Two halves of one workflow, in one program so that it runs anywhere:
 *
 *   on the build machine   resolve the configuration as usual and write
 *                          it as a binary snapshot image, provenance
 *                          included - in a real project this is a build
 *                          step, and the image is compiled into the
 *                          firmware as a const array;
 *
 *   on the device          read the image in place from that array, in
 *                          a fixed buffer handed to cf_build_in: nothing
 *                          parsed, nothing allocated, and explain() still
 *                          names the file each value came from.
 *
 * The build that suits such a device turns off what it does not need
 * (README.md, "Leaving parts out") and sets its stack budget; this
 * example works in the default build too.
 *
 *     ./embedded
 */

#include <configforge/configforge.h>

#include <stdio.h>

/* The build machine's side: an ordinary configuration, from the
   release's own file, written out. */
static size_t bake(char* image, size_t cap) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  c;
    size_t      n;
    FILE*       f = fopen("release.ini", "wb");

    if (f == NULL) {
        return 0;
    }
    fputs("[radio]\nchannel = 11\npower = 14\n", f);
    fclose(f);

    cf_builder_file(b, "release.ini", CF_RANK_SYS_FILE, "release.ini", NULL, 0);
    cf_builder_int(b, "radio.channel", 1, 1, 26, 0);
    cf_builder_int(b, "radio.power", 0, 0, 20, 0);
    cf_builder_int(b, "sensor.period_ms", 1000, 10, 60000, 0);
    c = cf_builder_create(b, &err);
    if (c == NULL) {
        fprintf(stderr, "bake: %s\n", err.message);
        return 0;
    }
    n = cf_image_write(cf_keys(c), cf_sources(c), CF_IMAGE_PROVENANCE, image, cap);
    cf_config_free(c);
    return (n <= cap) ? n : 0;
}

int main(void) {
    /* On the device these are static: the image in flash, the arena and
       the source's state in RAM, every size known at link time. */
    static union {
        cf_i64 align;
        char   b[4096];
    } image;
    static char arena_buf[16384];
    static union {
        cf_i64 align;
        char   b[1024];
    } state;
    static char why[1024];
    cf_error    err = CF_ERROR_INIT;
    cf_arena*   arena;
    cf_builder* b;
    cf_config*  cfg;
    cf_source   src;
    size_t      n = bake(image.b, sizeof image.b);

    if (n == 0) {
        return 1;
    }
    printf("image: %u bytes\n", (unsigned)n);

    /* From here on, the device: one fixed buffer, and no allocator. */
    arena = cf_arena_fixed(arena_buf, sizeof arena_buf);
    b     = cf_build_in(arena);
    if (cf_source_image_state_size() > sizeof state.b ||
        !cf_source_image_mem(&src, "firmware", CF_RANK_SYS_FILE, image.b, n, state.b,
                             sizeof state.b)) {
        fprintf(stderr, "embedded: the image source does not fit\n");
        return 1;
    }
    cf_builder_source(b, &src);
    cf_builder_int(b, "radio.channel", 1, 1, 26, 0);
    cf_builder_int(b, "radio.power", 0, 0, 20, 0);
    cf_builder_int(b, "sensor.period_ms", 1000, 10, 60000, 0);
    cfg = cf_builder_create(b, &err);
    if (cfg == NULL) {
        fprintf(stderr, "embedded: %s\n", err.message);
        return 1;
    }
    printf("channel %d, power %d dBm, period %d ms\n",
           (int)cf_get_int(cfg, "radio.channel", 0),
           (int)cf_get_int(cfg, "radio.power", 0),
           (int)cf_get_int(cfg, "sensor.period_ms", 0));
    printf("arena: %u of %u bytes used\n", (unsigned)cf_arena_used(arena),
           (unsigned)sizeof arena_buf);

    /* The provenance travelled in the image: the device says which line
       of which file on the build machine the value came from. */
    cf_explain(cfg, "radio.channel", why, sizeof why);
    fputs(why, stdout);

    cf_config_free(cfg);
    /* The buffer is the program's: cf_config_free leaves it, and this
       clears what lived in it. */
    cf_arena_wipe(arena);
    remove("release.ini");
    return 0;
}
