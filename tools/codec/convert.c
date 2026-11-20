/*
 * convert.c - `configforge codec convert` and `configforge codec format`
 * (D-94): a document read through one codec and written through the
 * encoder of another, as the codecs' events.
 *
 * The command line, the notes, the printing of what the target cannot
 * hold and the exit codes are here; the tree, the passes and the writing
 * are convert_core.c's, which a test runs without a process (D-109). C89.
 */

#include "codec.h"
#include "convert_core.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

/* The words a message is made of. */
typedef struct cv_say {
    const char*     cmd; /* "codec convert" or "codec format" */
    const cf_codec* target;
} cv_say;

typedef struct cv_args {
    const char* in;
    const char* to;
    const char* as;
    const char* out;
    cf_bool     strict;
} cv_args;

/* The input once, `--to` once and only for `convert`, `--as` and `--out`
   once, `--strict` as a flag; anything else is the usage. */
static cf_bool cv_parse(int argc, char** argv, cf_bool same, cv_args* a) {
    int i;

    memset(a, 0, sizeof *a);
    for (i = 0; i < argc; ++i) {
        const char** slot = NULL;

        if (strcmp(argv[i], "--to") == 0 && !same) {
            slot = &a->to;
        } else if (strcmp(argv[i], "--as") == 0) {
            slot = &a->as;
        } else if (strcmp(argv[i], "--out") == 0) {
            slot = &a->out;
        } else if (strcmp(argv[i], "--strict") == 0) {
            a->strict = CF_TRUE;
        } else if (strncmp(argv[i], "--", 2) == 0 || a->in != NULL) {
            return CF_FALSE;
        } else {
            a->in = argv[i];
        }
        if (slot != NULL) {
            if (*slot != NULL || i + 1 >= argc) {
                return CF_FALSE;
            }
            *slot = argv[++i];
        }
    }
    return a->in != NULL && (same || a->to != NULL);
}

/* The formats whose files can hold comments, which no reader keeps. */
static cf_bool cv_has_comments(const char* name) {
    static const char* const formats[] = {"ini",   "toml",       "yaml",
                                          "json5", "properties", "xml"};
    size_t                   i;

    for (i = 0; i < sizeof formats / sizeof formats[0]; ++i) {
        if (strcmp(formats[i], name) == 0) {
            return CF_TRUE;
        }
    }
    return CF_FALSE;
}

/* The notes, said once each: about the formats, the same for every file,
   and so not findings. */
static void cv_notes(const cv_say* k, const cf_codec* from, const char* in) {
    if (cv_has_comments(from->name)) {
        fprintf(stderr,
                "%s: note: %s can hold comments, and no reader keeps them: any in %s "
                "are not in the output\n",
                k->cmd, from->name, in);
    }
    if ((from->caps & CF_CODEC_TYPED) == 0u &&
        (k->target->caps & CF_CODEC_TYPED) != 0u) {
        fprintf(stderr,
                "%s: note: %s carries no types, so its values are written to %s as "
                "text\n",
                k->cmd, from->name, k->target->name);
    }
}

/* What a finding says of the node: the words after its path. */
static const char* cv_what(cv_kind kind) {
    switch (kind) {
    case CV_NAME:
        return "a name it would write unquoted";
    case CV_EMPTY_NAME:
        return "an empty name";
    case CV_ARRAY:
        return "an array";
    case CV_EMPTY_ARRAY:
        return "an empty array";
    case CV_TABLE_ARRAY:
        return "an array of tables";
    case CV_NESTED_ARRAY:
        return "an array of arrays";
    case CV_EMPTY_TABLE:
        return "an empty table";
    case CV_LONG_PATH:
        return "a table path too long";
    case CV_NULL:
        return "a null";
    case CV_LINE_BREAK:
        return "text with a line break";
    case CV_NUL:
        return "text with a NUL byte";
    case CV_OTHER:
        break;
    }
    return "a value of this kind";
}

/* `<cmd>: <path>: <what>, which <format> cannot hold; left out`. */
static void cv_print(const cv_say* k, const cv_finding* f) {
    fprintf(stderr, "%s: %s: %s", k->cmd, f->path, cv_what(f->kind));
    if (f->kind == CV_LONG_PATH) {
        fprintf(stderr, " (the path above it and its name pass %u bytes)",
                (unsigned)(CF_KEY_MAX - 2));
    }
    if (f->kind == CV_NAME) {
        fprintf(stderr, " ('%s')", f->name);
    }
    fprintf(stderr, ", which %s cannot hold; left out\n", k->target->name);
}

/* `data` to `path`, or to standard output when `path` is NULL. */
static int cv_write(const char* path, const char* data, size_t n) {
    FILE*   f = stdout;
    cf_bool ok;

    if (path != NULL) {
        f = fopen(path, "wb");
        if (f == NULL) {
            fprintf(stderr, "codec: cannot write %s\n", path);
            return 2;
        }
    } else {
#ifdef _WIN32
        (void)_setmode(_fileno(stdout), _O_BINARY);
#endif
    }
    ok = (fwrite(data, 1, n, f) == n) ? CF_TRUE : CF_FALSE;
    if (path != NULL) {
        if (fclose(f) != 0) {
            ok = CF_FALSE;
        }
    } else if (fflush(f) != 0) {
        ok = CF_FALSE;
    }
    if (!ok) {
        if (path != NULL) {
            remove(path);
        }
        fprintf(stderr, "codec: cannot write %s\n",
                (path != NULL) ? path : "standard output");
        return 2;
    }
    return 0;
}

/* The target must write, and its state must fit. */
static int cv_target(const cf_codec* c) {
    if ((c->caps & CF_CODEC_WRITE) == 0u || c->encoder == NULL) {
        fprintf(stderr, "codec: format '%s' can be read but not written\n", c->name);
        return 2;
    }
    if (c->encoder_state_size > CF_ENCODER_STATE_MAX) {
        fprintf(stderr, "codec: no room for the '%s' encoder\n", c->name);
        return 2;
    }
    return 0;
}

/* The core's answer said: its notes and findings, and the exit code for
   a refusal. 0 when the bytes are to be written. */
static int cv_report(const cv_say* k, const cv_args* a, const cf_codec* from, int rc,
                     const cv_result* res, const cf_error* err) {
    size_t findings = 0;
    size_t i;

    if (rc == 1) {
        fprintf(stderr, "codec: %s\n", err->message);
        return 1;
    }
    if (rc == 2) {
        if (err->code == CF_E_MEMORY) {
            fprintf(stderr, "codec: out of memory\n");
        } else {
            fprintf(stderr, "codec: %s %s\n", a->in, err->message);
        }
        return 2;
    }
    cv_notes(k, from, a->in);
    for (i = 0; i < res->nfindings; ++i) {
        cv_print(k, &res->findings[i]);
    }
    findings = res->nfindings;
    if (rc == 3) {
        fprintf(stderr, "%s: %s\n", k->cmd, err->message);
        return 3;
    }
    if (res->as_text > 0) {
        fprintf(stderr,
                "%s: %s carries no types: %lu value(s) that were numbers, booleans or "
                "nulls are written as their text\n",
                k->cmd, k->target->name, (unsigned long)res->as_text);
        ++findings;
    }
    if (a->strict && findings > 0) {
        fprintf(stderr, "%s: %lu finding(s); nothing written (--strict)\n", k->cmd,
                (unsigned long)findings);
        return 1;
    }
    return 0;
}

int codec_convert(int argc, char** argv, cf_bool same_format) {
    cv_args         a;
    cv_say          k;
    cv_result       res;
    cf_error        err = CF_ERROR_INIT;
    cf_arena*       ar;
    cf_blob         blob;
    const cf_codec* from = NULL;
    cf_detect       how;
    int             rc;

    memset(&res, 0, sizeof res);
    k.cmd = same_format ? "codec format" : "codec convert";
    if (!cv_parse(argc, argv, same_format, &a)) {
        if (same_format) {
            fprintf(stderr,
                    "codec: format <file> [--as <format>] [--out <file>] [--strict]\n");
        } else {
            fprintf(
                stderr,
                "codec: convert <file> --to <format> [--as <format>] [--out <file>] "
                "[--strict]\n");
        }
        return 2;
    }
    k.target = NULL;
    if (!same_format) {
        rc = codec_named(a.to, &k.target);
        if (rc == 0) {
            rc = cv_target(k.target);
        }
        if (rc != 0) {
            return rc;
        }
    }
    ar = cf_arena_new(NULL, 0);
    if (ar == NULL) {
        fprintf(stderr, "codec: out of memory\n");
        return 2;
    }
    rc = codec_read(a.in, a.as, ar, &blob, &from, &how);
    if (rc == 0 && same_format) {
        k.target = from;
        rc       = cv_target(from);
    }
    if (rc == 0) {
        rc = cv_convert(ar, &blob, from, k.target, &res, &err);
        rc = cv_report(&k, &a, from, rc, &res, &err);
    }
    if (rc == 0) {
        rc = cv_write(a.out, res.out.p, res.out.n);
    }
    cf_arena_free(ar);
    return rc;
}
