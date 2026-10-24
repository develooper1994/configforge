/*
 * test_features.c - every part of the library that can be compiled out,
 * in both directions.
 *
 * Built with a feature, the feature works. Built without it, every
 * function is still there - a program links either way - and using one
 * fails at construction with the option it needs in the message, never
 * by doing nothing: a drop-in directory that a build cannot read must
 * not look like an empty one. And cf_has_feature agrees with the build,
 * because a provenance report records that list and Doctor reads it.
 *
 * The same file is the test for both, so the default preset proves the
 * features and the minimal preset, which turns them all off, proves the
 * refusals.
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

static cf_bool spill(const char* path, const char* text) {
    FILE* f = fopen(path, "wb");
    if (f == NULL) {
        return CF_FALSE;
    }
    fputs(text, f);
    fclose(f);
    return CF_TRUE;
}

/* A configuration to write out, from memory: no feature needed. */
static cf_config* small(cf_error* err) {
    static const cf_pair pairs[] = {{"server.port", "9443"},
                                    {"server.host", "example"}};
    cf_builder*          b       = cf_build(NULL);

    cf_builder_memory(b, "memory", CF_RANK_USER_FILE, pairs, 2);
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    cf_builder_text(b, "server.host", "0.0.0.0", 0);
    return cf_builder_create(b, err);
}

/* Whether a builder with one source in it builds, and if not, why. */
static cf_config* built(cf_builder* b, cf_error* err) {
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);
    return cf_builder_create(b, err);
}

static void test_listed(void) {
    check(cf_has_feature("properties") == (CF_WITH_PROPERTIES ? CF_TRUE : CF_FALSE),
          "the .properties reader, as the build says");
    check(cf_has_feature("xml") == (CF_WITH_XML ? CF_TRUE : CF_FALSE),
          "XML, which once said yes with nothing behind it");
    check(cf_has_feature("sqlite") == (CF_WITH_SQLITE ? CF_TRUE : CF_FALSE),
          "the store");
    check(cf_has_feature("dir") == (CF_WITH_DIR ? CF_TRUE : CF_FALSE),
          "the feature list says whether there is a directory source");
    check(cf_has_feature("dotenv") == (CF_WITH_DOTENV ? CF_TRUE : CF_FALSE),
          "a .env source");
    check(cf_has_feature("emit") == (CF_WITH_EMIT ? CF_TRUE : CF_FALSE), "an emitter");
    check(cf_has_feature("image") == (CF_WITH_IMAGE ? CF_TRUE : CF_FALSE), "an image");
    check(cf_has_feature("report") == (CF_WITH_REPORT ? CF_TRUE : CF_FALSE),
          "a report");
}

static void test_dir(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  c;

    /* Optional and absent: with the source, that builds; without it,
       the build still fails, because "optional" is about the directory,
       not about whether this build can read one. */
    cf_builder_dir(b, "conf.d", CF_RANK_SYS_FILE, "cf_feat_absent.d", NULL,
                   CF_FILE_OPTIONAL);
    c = built(b, &err);
#if CF_WITH_DIR
    check(c != NULL, "an optional, absent directory builds");
#else
    check(c == NULL, "a directory source refuses to build without CF_WITH_DIR");
    has(err.message, "CF_WITH_DIR", "and names the option");
#endif
    cf_config_free(c);
}

static void test_dotenv(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  c;

    check(spill("cf_feat.env", "MYAPP_SERVER__PORT=7000\n"), "wrote a .env file");
    cf_builder_dotenv(b, "dotenv", CF_RANK_USER_FILE, "cf_feat.env", "MYAPP_", 0);
    c = built(b, &err);
#if CF_WITH_DOTENV
    check(c != NULL && cf_get_int(c, "server.port", 0) == 7000, "a .env file is read");
#else
    check(c == NULL, "a .env source refuses to build without CF_WITH_DOTENV");
    has(err.message, "CF_WITH_DOTENV", "and names the option");
#endif
    cf_config_free(c);
    remove("cf_feat.env");
}

static void test_emit(void) {
    cf_error        err = CF_ERROR_INIT;
    cf_config*      c   = small(&err);
    static char     out[1024];
    size_t          n;
    const cf_codec* json = cf_codec_by_name("json");

    check(c != NULL, "a configuration from memory");
    if (c == NULL) {
        return;
    }
    n = cf_emit(c, "json", CF_EMIT_ALL, 0, out, sizeof out, &err);
#if CF_WITH_EMIT
    check(n > 0, "a configuration is written out");
    has(out, "9443", "with its values");
    check(json == NULL || (json->caps & CF_CODEC_WRITE) != 0, "and codecs can write");
#else
    check(n == 0 && out[0] == '\0', "nothing is written without CF_WITH_EMIT");
    has(err.message, "CF_WITH_EMIT", "and the error names the option");
    check(json == NULL || ((json->caps & CF_CODEC_WRITE) == 0 && json->encoder == NULL),
          "and the codecs read without claiming to write");
#endif
    cf_config_free(c);
}

static void test_image(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_config*  c   = small(&err);
    static char img[4096];
    size_t      n;
    cf_image    opened;

    check(c != NULL, "a configuration from memory");
    if (c == NULL) {
        return;
    }
    n = cf_image_write(cf_keys(c), NULL, 0, img, sizeof img);
#if CF_WITH_IMAGE
    check(n > 0 && n <= sizeof img, "an image is written");
    check(cf_image_open(&opened, img, n, &err) && cf_image_count(&opened) == 2,
          "and read back");
#else
    check(n == 0, "no image is written without CF_WITH_IMAGE");
    check(!cf_image_open(&opened, img, sizeof img, &err), "and none is opened");
    has(err.message, "CF_WITH_IMAGE", "and the error names the option");
#endif
    cf_config_free(c);
}

static void test_report(void) {
    cf_error       err = CF_ERROR_INIT;
    cf_config*     c   = small(&err);
    static char    rep[8192];
    cf_report_info info;
    size_t         n;
    cf_builder*    b;
    cf_config*     back;

    check(c != NULL, "a configuration from memory");
    if (c == NULL) {
        return;
    }
    memset(&info, 0, sizeof info);
    info.mode = CF_REPORT_FULL;
    n         = cf_report(c, &info, rep, sizeof rep);
#if CF_WITH_REPORT
    check(n > 0 && n < sizeof rep && rep[0] == '{', "a report is written");
    check(spill("cf_feat_report.json", rep), "and saved");
#else
    check(n == 0, "no report is written without CF_WITH_REPORT");
    check(spill("cf_feat_report.json", "{}\n"), "a file to replay all the same");
#endif

    b = cf_build(NULL);
    cf_builder_replay(b, "replay", CF_RANK_ENV, "cf_feat_report.json");
    back = built(b, &err);
#if CF_WITH_REPORT && CF_WITH_JSON
    check(back != NULL && cf_get_int(back, "server.port", 0) == 9443,
          "the report replays to the same value");
#elif !CF_WITH_REPORT
    check(back == NULL, "a replay refuses to build without CF_WITH_REPORT");
    has(err.message, "CF_WITH_REPORT", "and names the option");
#endif
    cf_config_free(back);
    cf_config_free(c);
    remove("cf_feat_report.json");
}

/* The version is what cf_version.h says, and the build id is either
   nothing or what git abbreviates a commit to, "-dirty" allowed. */
static void test_version(void) {
    const char* id = cf_build_id();
    size_t      i  = 0;

    check(strcmp(cf_version(), CF_VERSION_STRING) == 0, "the version is the header's");
    check(cf_version_number() == CF_VERSION_NUMBER, "and so is its number");
    check(id != NULL, "a build id is never NULL");
    if (id == NULL) {
        return;
    }
    while ((id[i] >= '0' && id[i] <= '9') || (id[i] >= 'a' && id[i] <= 'f')) {
        ++i;
    }
    check(id[0] == '\0' || (i >= 4 && (id[i] == '\0' || strcmp(id + i, "-dirty") == 0)),
          "a build id is empty or a commit hash");
    printf("  version %s, build %s\n", cf_version(),
           (id[0] != '\0') ? id : "(unknown)");
}

int main(void) {
    test_version();
    test_listed();
    test_dir();
    test_dotenv();
    test_emit();
    test_image();
    test_report();
    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
