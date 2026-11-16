/*
 * gen.c - configforge-gen, the contract's program: a JSON Schema
 * contract written as C calls, and files held to it.
 *
 *     configforge gen --schema <schema> [--name <function>] [--out <file>]
 *     configforge gen check --schema <schema> <file>...
 *     configforge gen --version
 *
 * The work is contract.c's; this file reads the first argument.
 */

#include "gen.h"

#include <configforge/configforge.h>

#include <stdio.h>
#include <string.h>

static int usage(void) {
    fputs("ConfigForge gen: the contract, and code from it\n\n", stderr);
    fputs("  configforge gen --schema <schema> [--name <function>] [--out <file>]\n",
          stderr);
    fputs("                                 the contract as C calls\n", stderr);
    fputs("  configforge gen check --schema <schema> <file>...\n", stderr);
    fputs("                                 files against the contract\n", stderr);
    fputs("  configforge gen --version\n\n", stderr);
    fputs("Exit codes: 0 done, and for check every file holds to the contract;\n",
          stderr);
    fputs("1 a file does not; 2 unusable input.\n", stderr);
    return 2;
}

int main(int argc, char** argv) {
    if (argc >= 2 && strcmp(argv[1], "check") == 0) {
        return gen_check(argc - 2, argv + 2);
    }
    /* The version, and the commit it was built from when the build knew
       it, as Doctor prints them. */
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        const char* id = cf_build_id();
        printf("ConfigForge gen %s%s%s%s\n", cf_version(),
               (id[0] != '\0') ? " (" : "", id, (id[0] != '\0') ? ")" : "");
        return 0;
    }
    if (argc >= 2 && strncmp(argv[1], "--", 2) == 0) {
        return gen_code(argc - 1, argv + 1);
    }
    return usage();
}
