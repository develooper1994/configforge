/*
 * freestanding.c - what a build with no operating system and no C library
 * gives and refuses (K-11b, D-220).
 *
 * Built with CF_PLATFORM=none, CF_NO_STDIO and CF_NO_STDLIB: no files, no
 * default allocator. The configuration is built in a fixed arena over a
 * static buffer, from a JSON text held in memory; a file source answers
 * CF_E_CAPABILITY, never a plain "cannot open". The program uses the
 * host's puts for its report only.
 */

#include "configforge/configforge.h"
#include "configforge/cf_alloc.h"
#include "configforge/cf_error.h"
#include "configforge/cf_platform_file.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

static void check(int ok, const char* what) {
    if (!ok) {
        ++failures;
        puts("FAIL:");
        puts(what);
    }
}

static const char text[] = "{\"server\": {\"port\": 8443, \"host\": \"edge\"}}";

static char arena_buffer[16384];

static void from_memory(void) {
    cf_arena*   ar = cf_arena_fixed(arena_buffer, sizeof arena_buffer);
    cf_builder* b;
    cf_config*  c;
    cf_error    err = CF_ERROR_INIT;
    char        why[256];

    check(ar != NULL, "a fixed arena over a static buffer");
    if (ar == NULL) {
        return;
    }
    b = cf_build_in(ar);
    check(b != NULL, "a builder in the arena");
    if (b == NULL) {
        return;
    }
    cf_builder_int(b, "server.port", 80, 0, 0, 0);
    cf_builder_text(b, "server.host", "localhost", 0);
    check(cf_builder_document(b, "inline.json", CF_RANK_USER_FILE, text, strlen(text),
                              "json", 0u),
          "a JSON text in memory is a source");
    c = cf_builder_create(b, &err);
    check(c != NULL, "the configuration is created in the arena");
    if (c == NULL) {
        puts(err.message);
        return;
    }
    check(cf_get_int(c, "server.port", 0) == 8443, "the int comes from the text");
    check(strcmp(cf_get_text(c, "server.host", ""), "edge") == 0,
          "the text comes from the text");
    why[0] = '\0';
    check(cf_explain(c, "server.port", why, sizeof why) > 0 && why[0] != '\0',
          "cf_explain says where the port came from");
    cf_config_free(c);
}

static void no_files(void) {
    cf_arena*   ar = cf_arena_fixed(arena_buffer, sizeof arena_buffer);
    cf_builder* b;
    cf_config*  c;
    cf_error    err = CF_ERROR_INIT;

    check(cf_default_allocator() == NULL, "no default allocator without stdlib");
    check(cf_platform_has_files() == CF_FALSE, "no files without stdio");
    check(ar != NULL, "a fixed arena again");
    if (ar == NULL) {
        return;
    }
    b = cf_build_in(ar);
    check(b != NULL, "a second builder");
    if (b == NULL) {
        return;
    }
    (void)cf_builder_file(b, "app.json", CF_RANK_USER_FILE, "app.json", "json", 0u);
    c = cf_builder_create(b, &err);
    check(c == NULL, "a file source of a path fails");
    if (c != NULL) {
        cf_config_free(c);
        return;
    }
    check(err.code == CF_E_CAPABILITY, "it fails as CF_E_CAPABILITY, not CF_E_SOURCE");
    check(strstr(err.message, "this build has no files to read") != NULL,
          "and says what this build lacks");
    if (failures != 0) {
        puts(err.message);
    }
}

int main(void) {
    from_memory();
    no_files();
    if (failures != 0) {
        puts("platform.freestanding: failed");
        return 1;
    }
    puts("platform.freestanding: ok");
    return 0;
}
