/*
 * fake_gen.c - configforge-gen-fake, the reference for a generator
 * outside the tree (W5-G4, D-110): a program `configforge gen --lang fake`
 * finds and runs, handed the contract as JSON on standard input and the
 * command line's other options on its own.
 *
 * It prints its arguments to standard error, then acts on the value of
 * `--name`:
 *
 *     exit7   exits 7 after saying so
 *     noread  exits 3 without reading its input
 *     ran     creates ran.marker beside the --out file, reads nothing, exits 0
 *     else    reads standard input whole and writes it to the --out file
 *             or to standard output, exit 0
 *
 * A generator written in another language does the same: read the input
 * whole, write the output where --out says.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

/* ran.marker in the directory of `out`, which is "" for a bare name. */
static int make_marker(const char* out) {
    static const char marker[] = "ran.marker";
    size_t            dir      = 0;
    size_t            i;
    char*             path;
    FILE*             f;

    for (i = 0; out[i] != '\0'; ++i) {
        if (out[i] == '/' || out[i] == '\\') {
            dir = i + 1;
        }
    }
    path = (char*)malloc(dir + sizeof marker);
    if (path == NULL) {
        return 2;
    }
    memcpy(path, out, dir);
    memcpy(path + dir, marker, sizeof marker);
    f = fopen(path, "wb");
    free(path);
    if (f == NULL) {
        return 2;
    }
    return (fclose(f) == 0) ? 0 : 2;
}

/* Standard input to `f`, whole. */
static int copy_input(FILE* f) {
    char buf[512];

    for (;;) {
        size_t got = fread(buf, 1, sizeof buf, stdin);

        if (got == 0) {
            break;
        }
        if (fwrite(buf, 1, got, f) != got) {
            return 2;
        }
    }
    return 0;
}

int main(int argc, char** argv) {
    const char* name = NULL;
    const char* out  = NULL;
    FILE*       f    = stdout;
    int         i;
    int         rc;

#ifdef _WIN32
    (void)_setmode(_fileno(stdin), _O_BINARY);
    (void)_setmode(_fileno(stdout), _O_BINARY);
#endif
    fputs("fake: args=", stderr);
    for (i = 1; i < argc; ++i) {
        fprintf(stderr, "%s%s", (i > 1) ? "|" : "", argv[i]);
        if (i + 1 < argc && strcmp(argv[i], "--name") == 0) {
            name = argv[i + 1];
        }
        if (i + 1 < argc && strcmp(argv[i], "--out") == 0) {
            out = argv[i + 1];
        }
    }
    fputc('\n', stderr);
    if (name != NULL && strcmp(name, "exit7") == 0) {
        fputs("fake: exit 7 asked for\n", stderr);
        return 7;
    }
    if (name != NULL && strcmp(name, "noread") == 0) {
        return 3;
    }
    if (name != NULL && strcmp(name, "ran") == 0) {
        return (out != NULL) ? make_marker(out) : 2;
    }
    if (out != NULL) {
        f = fopen(out, "wb");
        if (f == NULL) {
            fputs("fake: cannot write the output\n", stderr);
            return 2;
        }
    }
    rc = copy_input(f);
    if (out != NULL && fclose(f) != 0) {
        rc = 2;
    }
    return rc;
}
