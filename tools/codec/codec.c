/*
 * codec.c - configforge-codec: the serialization library's own program.
 *
 *     configforge codec formats [--format text|markdown]
 *     configforge codec pairs [--format text|markdown]
 *     configforge codec detect <file>
 *     configforge codec check <file>... [--as <format>]
 *     configforge codec dump <file> [--as <format>]
 *     configforge codec convert <file> --to <format> [--as F] [--out F] [--strict]
 *     configforge codec format <file> [--as <format>] [--out F] [--strict]
 *     configforge codec recode <file> --from <encoding> [--out F]
 *     configforge codec recode <file> --check
 *     configforge codec --version
 *
 * It reads documents, in every format the library reads, and never a
 * configuration: no contract, no key space, no layers. It is linked to
 * configforge_serialize alone, and is the proof that the serialization
 * library is enough. C89.
 */

#include "codec.h"

#include <stdio.h>
#include <string.h>

static int usage(void) {
    static const char* const text[] = {
        "ConfigForge codec: documents, in every format the library reads\n",
        "\n",
        "  configforge codec formats [--format text|markdown]\n",
        "                                 the built-in codecs and their kind\n",
        "  configforge codec pairs [--format text|markdown]\n",
        "                                 what each format can hold, and what each\n",
        "                                 pair of formats loses\n",
        "  configforge codec detect <file>\n",
        "                                 the format of a file, and how it was told\n",
        "  configforge codec check <file>... [--as <format>]\n",
        "                                 whether each file is well formed\n",
        "  configforge codec convert <file> --to <format> [--as <format>]\n",
        "                                 [--out <file>] [--strict]\n",
        "                                 the document in another format, and\n",
        "                                 what it cannot hold\n",
        "  configforge codec format <file> [--as <format>] [--out <file>] [--strict]\n",
        "                                 the document rewritten in its own format\n",
        "  configforge codec recode <file> --from <encoding> [--out <file>]\n",
        "                                 the file's bytes in UTF-8, nothing else\n",
        "                                 changed\n",
        "  configforge codec recode <file> --check\n",
        "                                 whether the file is UTF-8, and if not what\n",
        "                                 it may be\n",
        "  configforge codec --version\n",
        "\n",
        "Exit codes: 0 done (check: every file is well formed), 1 a file is\n",
        "not, or its format cannot be told; 2 unusable input.\n"};
    size_t i;

    for (i = 0; i < sizeof text / sizeof text[0]; ++i) {
        fputs(text[i], stderr);
    }
    return 2;
}

int main(int argc, char** argv) {
    if (argc >= 2 && strcmp(argv[1], "formats") == 0) {
        return codec_formats(argc - 2, argv + 2);
    }
    if (argc >= 2 && strcmp(argv[1], "pairs") == 0) {
        return codec_pairs(argc - 2, argv + 2);
    }
    if (argc >= 2 && strcmp(argv[1], "detect") == 0) {
        return codec_detect(argc - 2, argv + 2);
    }
    if (argc >= 2 && strcmp(argv[1], "check") == 0) {
        return codec_check(argc - 2, argv + 2);
    }
    if (argc >= 2 && strcmp(argv[1], "dump") == 0) {
        return codec_dump(argc - 2, argv + 2);
    }
    if (argc >= 2 && strcmp(argv[1], "convert") == 0) {
        return codec_convert(argc - 2, argv + 2, CF_FALSE);
    }
    if (argc >= 2 && strcmp(argv[1], "format") == 0) {
        return codec_convert(argc - 2, argv + 2, CF_TRUE);
    }
    if (argc >= 2 && strcmp(argv[1], "recode") == 0) {
        return codec_recode(argc - 2, argv + 2);
    }
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        printf("ConfigForge codec %s\n", cf_serialize_version());
        return 0;
    }
    return usage();
}
