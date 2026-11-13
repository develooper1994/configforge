/*
 * formats.c - `doctor formats`: every built-in codec, with its
 * extensions and what kind of format it is (cf_codec_kind). The
 * markdown form is the table in docs/Formats.md between its markers,
 * and a test holds the page to it. C89.
 */

#include "adapters.h"

#include "configforge/configforge.h"

#include <stdio.h>
#include <string.h>

/* The extensions of a codec as a person lists them: ".json, .jsn". */
static void ext_list(const char* ext, char* out, size_t cap) {
    size_t n = 0;

    for (; ext != NULL && *ext != '\0' && n + 3 < cap; ++ext) {
        if (*ext == ';') {
            out[n++] = ',';
            out[n++] = ' ';
        } else {
            out[n++] = *ext;
        }
    }
    out[n] = '\0';
}

int doctor_formats(int argc, char** argv) {
    cf_bool markdown = CF_FALSE;
    size_t  i;
    int     a;

    for (a = 0; a < argc; ++a) {
        if (strcmp(argv[a], "--format") == 0 && a + 1 < argc) {
            ++a;
            if (strcmp(argv[a], "markdown") == 0) {
                markdown = CF_TRUE;
            } else if (strcmp(argv[a], "text") != 0) {
                fprintf(stderr, "doctor: formats prints text or markdown, not '%s'\n",
                        argv[a]);
                return 2;
            }
        } else {
            fprintf(stderr, "doctor: formats [--format text|markdown]\n");
            return 2;
        }
    }
    if (markdown) {
        fputs("| Format | Extensions | Kind |\n| ------ | ---------- | ---- |\n",
              stdout);
    }
    for (i = 0; i < cf_codec_count(); ++i) {
        const cf_codec* c = cf_codec_at(i);
        char            ext[64];
        char            kind[160];

        ext_list(c->extensions, ext, sizeof ext);
        cf_codec_kind(c, kind, sizeof kind);
        if (markdown) {
            printf("| %s | %s | %s |\n", c->name, ext, kind);
        } else {
            printf("%-11s %-22s %s\n", c->name, ext, kind);
        }
    }
    return 0;
}
