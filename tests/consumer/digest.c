/*
 * digest.c - the same configuration, built two ways.
 *
 * Printed by a program compiled against the library and by one compiled
 * against the amalgamation. If the two numbers differ, the amalgamation
 * is not the library - a source file left out of the generator's list,
 * an X-macro list inlined at the wrong point, a static function that
 * merged with another of the same name. All three have happened.
 *
 * The digest is over the resolved configuration, so it moves if any
 * value, type or key does.
 */

#include <configforge/configforge.h>

#include <stdio.h>

static char  a0[]     = "prog";
static char  a1[]     = "--server.port=9000";
static char  a2[]     = "--server.host=10.0.0.1";
static char  a3[]     = "--tags.0=alpha";
static char  a4[]     = "--tags.1=beta";
static char  a5[]     = "--tags.count=2";
static char  a6[]     = "--ratio=0.125";
static char* g_argv[] = {a0, a1, a2, a3, a4, a5, a6, NULL};

static char  e0[]    = "MYAPP_DB__PASSWORD=secret";
static char* g_env[] = {e0, NULL};

int main(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  c;
    char        buf[4096];

    cf_builder_cli(b, "cli", CF_RANK_CLI, 7, g_argv);
    cf_builder_env(b, "env", CF_RANK_ENV, "MYAPP_", g_env);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_builder_text(b, "server.host", "0.0.0.0", 0);
    cf_builder_text(b, "db.password", NULL, CF_SENSITIVE);
#if CF_WITH_REAL
#if CF_WITH_REAL
    /* Guarded: the minimal profile compiles floating point out, and a
       differential test that only builds in some profiles is a
       differential test that stops running in the others. */
    cf_builder_real(b, "ratio", 1.0, 0.0, 2.0, 0);
#endif
#endif

    c = cf_builder_create(b, &err);
    if (c == NULL) {
        fprintf(stderr, "digest: %s\n", err.message);
        return 1;
    }

    /* The digest, and then the things built on top of it: a
       difference in any of them is a difference in the library. */
    printf("digest %08lx%08lx\n", (unsigned long)(cf_digest(c) >> 32),
           (unsigned long)(cf_digest(c) & 0xFFFFFFFFUL));
    printf("keys %u\n", (unsigned)cf_count(c));
    cf_emit(c, "json", CF_EMIT_ALL, 0, buf, sizeof buf, &err);
    printf("emit %u\n", (unsigned)cf_fmt(NULL, 0, "%s", buf));
    cf_inspect(c, buf, sizeof buf);
    printf("inspect %u\n", (unsigned)cf_fmt(NULL, 0, "%s", buf));

    /* One call into every layer, so a difference anywhere shows as a
       difference here. The file list check in the generator is the real
       guarantee that nothing is missing; this is the guarantee that
       what is there behaves the same. */
    {
        cf_iter it;
        size_t  n = 0;
        cf_iter_keys(&it, c, NULL);
        while (cf_iter_next(&it)) {
            ++n;
        }
        printf("walked %u\n", (unsigned)n);
        cf_iter_children(&it, c, "server");
        n = 0;
        while (cf_iter_next(&it)) {
            ++n;
        }
        printf("children %u\n", (unsigned)n);
        printf("array %u\n", (unsigned)cf_array_count(c, "tags"));
        printf("explain %u\n", (unsigned)cf_explain(c, "server.port", NULL, 0));
        printf("image %u\n", (unsigned)cf_image_write(cf_keys(c), NULL,
                                                      CF_IMAGE_PROVENANCE, NULL, 0));
        printf("version %s platform %s\n", cf_version(), cf_platform_name());
    }

    cf_config_free(c);
    return 0;
}
