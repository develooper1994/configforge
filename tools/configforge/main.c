/*
 * main.c - `configforge <command> ...`, the front door of the tools.
 *
 * Runs `configforge-<command>` with the rest of the arguments, found
 * beside this program first and then on the PATH, as git runs
 * `git-<command>`: the tools are one family, each beside the library it
 * serves, and one name to remember. It decides nothing about a command,
 * and a program a user writes as configforge-<name> is run like the
 * ones that ship (D-99).
 *
 * Exit codes: the program's own when it ran; 2 when this program
 * refused the command line; 2 when the program is not there or could
 * not be started. A name is lower case letters, digits and hyphens,
 * so that no path and no option is ever run.
 */

#include "front.h"

#include "configforge/cf_platform.h"
#include "configforge/cf_version.h"

#include <stdio.h>
#include <string.h>

#define FRONT_NAME_MAX 64
/* "configforge-", a name of FRONT_NAME_MAX, ".exe" and a NUL. */
#define FRONT_PROGRAM_MAX (11 + 1 + FRONT_NAME_MAX + 4 + 1)

/* In two calls: C90 requires support for string literals only up to
   509 characters. */
static void help(FILE* to) {
    fputs("ConfigForge\n"
          "\n"
          "  configforge doctor <command> ...   reports: explain, dump, lint, diff,\n"
          "                                     history, blame, import-*\n"
          "  configforge gen ...                the contract: check files, write code\n"
          "  configforge codec <command> ...    documents: formats, detect, check,\n"
          "                                     dump, convert, format\n"
          "  configforge help\n"
          "  configforge --version\n"
          "\n",
          to);
    fputs("`configforge <name> ...` runs configforge-<name>, found beside this\n"
          "program or on the PATH, and exits with its exit code.\n",
          to);
}

/* 1 to FRONT_NAME_MAX bytes of a-z, 0-9 and -, not starting with -. */
static int is_name(const char* s) {
    size_t n = strlen(s);
    size_t i;
    if (n == 0 || n > FRONT_NAME_MAX || s[0] == '-') {
        return 0;
    }
    for (i = 0; i < n; i++) {
        char c = s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) {
            return 0;
        }
    }
    return 1;
}

int main(int argc, char** argv) {
    char   program[FRONT_PROGRAM_MAX];
    size_t n;

    if (argc < 2) {
        help(stderr);
        return 2;
    }
    if (argc == 2 && (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0 ||
                      strcmp(argv[1], "-h") == 0)) {
        help(stdout);
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        printf("ConfigForge %s\n", CF_VERSION_STRING);
        return 0;
    }
    if (!is_name(argv[1])) {
        fprintf(stderr, "configforge: '%s' is not a command name\n", argv[1]);
        return 2;
    }
    /* The name is at most FRONT_NAME_MAX bytes, so this fits. */
    n = strlen(argv[1]);
    memcpy(program, "configforge-", 12);
    memcpy(program + 12, argv[1], n);
    n += 12;
#if CF_PLATFORM_WIN32
    memcpy(program + n, ".exe", 4);
    n += 4;
#endif
    program[n] = '\0';
    return front_run(program, argc, argv);
}
