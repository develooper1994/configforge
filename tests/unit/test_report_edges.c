/*
 * test_report_edges.c - the report and the image at their edges.
 *
 * The report written for every definition attribute there is - a
 * description, ranges, length limits, a replacement, a secret read from
 * a file - and for text that needs escaping, then replayed; the reports
 * Replay must refuse, each damaged a different way; and the image,
 * written with and without provenance, opened, read and refused.
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

static char g_rep[1 << 16];
static char g_img[1 << 16];

static cf_bool spill(const char* path, const char* text) {
    FILE* f = fopen(path, "wb");
    if (f == NULL) {
        return CF_FALSE;
    }
    fputs(text, f);
    fclose(f);
    return CF_TRUE;
}

static cf_config* rich(cf_error* err) {
    static const cf_pair mem[] = {
        {"text.ctrl", "bell\x07 back\x08 feed\x0c nl\n tab\t quote\" slash\\"},
        {"flag.on", "true"},
        {"flag.off", "false"}};
    cf_builder* b = cf_build(NULL);
    cf_def      d;

    cf_builder_memory(b, "mem", CF_RANK_SYS_FILE, mem, 3);
    cf_builder_file(b, "secret.ini", CF_RANK_USER_FILE, "cf_repedge.ini", NULL, 0);
    cf_def_init(&d, CF_TYPE_INT, "num.port");
    d.description = "the port";
    d.has_range   = CF_TRUE;
    d.imin        = 1;
    d.imax        = 65535;
    d.has_default = CF_TRUE;
    cf_value_int(&d.default_value, 80);
    cf_builder_define(b, &d);
#if CF_WITH_REAL
    cf_def_init(&d, CF_TYPE_REAL, "num.ratio");
    d.has_range   = CF_TRUE;
    d.rmin        = 0.0;
    d.rmax        = 1.0;
    d.has_default = CF_TRUE;
    cf_value_real(&d.default_value, 0.5);
    cf_builder_define(b, &d);
#endif
    cf_def_init(&d, CF_TYPE_TEXT, "text.name");
    d.min_len     = 1;
    d.max_len     = 32;
    d.has_default = CF_TRUE;
    cf_value_text(&d.default_value, "app", 3);
    cf_builder_define(b, &d);
    cf_def_init(&d, CF_TYPE_TEXT, "text.old");
    d.flags       = CF_DEPRECATED;
    d.replaced_by = "text.name";
    cf_builder_define(b, &d);
    cf_def_init(&d, CF_TYPE_TEXT, "db.password");
    d.flags = CF_SENSITIVE;
    cf_builder_define(b, &d);
    cf_builder_text(b, "text.ctrl", NULL, 0);
    cf_builder_bool(b, "flag.on", CF_FALSE, 0);
    cf_builder_bool(b, "flag.off", CF_TRUE, 0);
    return cf_builder_create(b, err);
}

static void test_report_round_trip(void) {
    cf_error       err = CF_ERROR_INIT;
    cf_config*     c;
    cf_config*     back;
    cf_builder*    b;
    cf_report_info info;
    size_t         n;

    /* The report, its reader and the file it reads, or nothing to test. */
    if (!cf_has_feature("report") || !cf_has_feature("json") ||
        !cf_has_feature("ini")) {
        return;
    }
    check(spill("cf_repedge.ini", "[db]\npassword = hunter2\n[text]\nold = legacy\n"),
          "wrote the file");
    c = rich(&err);
    check(c != NULL, "every attribute builds");
    if (c == NULL) {
        printf("  %s\n", err.message);
        return;
    }
    memset(&info, 0, sizeof info);
    info.mode        = CF_REPORT_FULL;
    info.profile     = "staging";
    info.application = "edges";
    info.app_version = "1.2.3";
    info.generated   = "2026-10-29T00:00:00Z";
    n                = cf_report(c, &info, g_rep, sizeof g_rep);
    check(n > 0 && n < sizeof g_rep, "a full report");
    check(strstr(g_rep, "\"description\"") != NULL &&
              strstr(g_rep, "\"replaced_by\"") != NULL,
          "with the description and the replacement");
    check(strstr(g_rep, "\\"
                        "u0007") != NULL,
          "a control character escaped");
    check(spill("cf_repedge.json", g_rep), "written");

    b = cf_build(NULL);
    cf_builder_replay(b, "replay", CF_RANK_USER_FILE, "cf_repedge.json");
    back = cf_builder_create(b, &err);
    check(back != NULL && strcmp(cf_get_text(back, "text.ctrl", ""),
                                 cf_get_text(c, "text.ctrl", "x")) == 0,
          "and replayed, control characters and all");
    cf_config_free(back);

    info.mode = CF_REPORT_BLIND;
    (void)cf_report(c, &info, g_rep, sizeof g_rep);
    check(strstr(g_rep, "hunter2") == NULL, "a blind report holds no secret");
    (void)cf_report(c, NULL, g_rep, sizeof g_rep);
    check(cf_report(NULL, &info, g_rep, sizeof g_rep) == 0, "a report of nothing");
    (void)cf_schema(c, "Edges", g_rep, sizeof g_rep);
    check(strstr(g_rep, "\"maxLength\"") != NULL || strstr(g_rep, "maxLength") != NULL,
          "the schema has the length limits");
    check(cf_schema(NULL, "x", g_rep, sizeof g_rep) == 0, "a schema of nothing");
    cf_config_free(c);
    remove("cf_repedge.json");
}

/* Each damaged differently; Replay refuses, saying why, or reads what is
   readable - never a crash. */
static void test_damaged(void) {
    static const char* const docs[] = {
        "{\"keys\": {}}",
        "{\"configforge_report\": \"one\", \"mode\": \"full\", \"keys\": {}}",
        "{\"configforge_report\": 99, \"mode\": \"full\", \"keys\": {}}",
        "{\"configforge_report\": 1, \"keys\": {}}",
        "{\"configforge_report\": 1, \"mode\": 7, \"keys\": {}}",
        "{\"configforge_report\": 1, \"mode\": \"sideways\", \"keys\": {}}",
        "{\"configforge_report\": 1, \"mode\": \"blind\", \"keys\": {}}",
        "{\"configforge_report\": 1, \"mode\": \"full\", \"keys\": {\"a\": "
        "{\"candidates\": "
        "[{\"source\": \"x\", \"value\": 1}]}}}",
        "{\"configforge_report\": 1, \"mode\": \"full\", \"keys\": {\"a\": "
        "{\"candidates\": "
        "{\"x\": {\"value\": 1}}}}}",
        "{\"configforge_report\": 1, \"mode\": \"full\", \"keys\": {\"a\": {\"value\": "
        "1}}}",
        "{\"configforge_report\": 1, \"mode\": \"full\", \"keys\": {\"a\": "
        "{\"selected\": "
        "{\"value\": 1}}}}",
        "not json at all",
        NULL};
    int i;

    if (!cf_has_feature("report") || !cf_has_feature("json")) {
        return;
    }
    for (i = 0; docs[i] != NULL; ++i) {
        cf_error    err = CF_ERROR_INIT;
        cf_builder* b   = cf_build(NULL);
        cf_config*  c;
        check(spill("cf_repedge_bad.json", docs[i]), "wrote a damaged report");
        cf_builder_replay(b, "replay", CF_RANK_USER_FILE, "cf_repedge_bad.json");
        c = cf_builder_create(b, &err);
        check(c != NULL || err.message[0] != '\0',
              "a damaged report is refused with a reason");
        cf_config_free(c);
    }
    {
        cf_error    err = CF_ERROR_INIT;
        cf_builder* b   = cf_build(NULL);
        cf_config*  c;
        cf_builder_replay(b, "replay", CF_RANK_USER_FILE, "cf_no_such_report.json");
        c = cf_builder_create(b, &err);
        check(c == NULL && err.message[0] != '\0', "a missing report is refused");
        cf_config_free(c);
    }
    remove("cf_repedge_bad.json");
}

static void test_image(void) {
    static const cf_pair mem[] = {{"i", "3"}, {"t", "x"}, {"b", "true"}, {"r", "1.5"}};
    cf_error             err   = CF_ERROR_INIT;
    cf_builder*          b     = cf_build(NULL);
    cf_config*           c;
    cf_image             img;
    const char*          key;
    size_t               klen, n = 0, flags;
    cf_value             v;

    if (!cf_has_feature("image")) {
        cf_builder_abort(b);
        return;
    }
    cf_builder_memory(b, "mem", 0, mem, 4);
    c = cf_builder_create(b, &err);
    check(c != NULL, "a configuration to image");
    if (c == NULL) {
        return;
    }
    for (flags = 0; flags <= CF_IMAGE_PROVENANCE; flags += CF_IMAGE_PROVENANCE) {
        n = cf_image_write(cf_keys(c), cf_sources(c), (unsigned)flags, g_img,
                           sizeof g_img);
        check(n > 0, "an image is written");
        check(cf_image_write(cf_keys(c), cf_sources(c), (unsigned)flags, g_img, 8) == n,
              "and measured past a short buffer");
        check(cf_image_open(&img, g_img, n, &err), "and opened");
        check(cf_image_count(&img) == 4, "with every key");
        check(cf_image_at(&img, 0, &key, &klen, &v) &&
                  !cf_image_at(&img, 99, &key, &klen, &v),
              "read by index, and refused past the end");
        check(cf_image_get(&img, "t", 1, &v) && !cf_image_get(&img, "nope", 4, &v),
              "and by key");
        check(!cf_image_open(&img, g_img, 8, &err) && err.message[0] != '\0',
              "a truncated image is refused");
    }
    check(!cf_image_open(NULL, g_img, n, &err) && !cf_image_open(&img, NULL, 0, &err),
          "opening nothing");
    check(cf_image_count(NULL) == 0 && !cf_image_at(NULL, 0, &key, &klen, &v) &&
              !cf_image_get(NULL, "t", 1, &v) && !cf_image_get(&img, NULL, 0, &v),
          "reading nothing");
    check(cf_image_write(NULL, NULL, 0, g_img, sizeof g_img) == 0,
          "an image of nothing");
    check(!cf_image_load(NULL, NULL, 0, &err), "loading nothing");
    memset(g_img, 'x', 64);
    check(!cf_image_open(&img, g_img, 64, &err), "and bytes that are no image");
    cf_config_free(c);
}

int main(void) {
    test_report_round_trip();
    test_damaged();
    test_image();
    remove("cf_repedge.ini");
    printf("%d checks, %d failures\n", g_run, g_fail);
    return g_fail == 0 ? 0 : 1;
}
