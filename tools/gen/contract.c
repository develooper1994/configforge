/*
 * contract.c - a JSON Schema contract, checked against and written (D-64, D-67).
 *
 *     configforge gen check --schema <schema> <file>...
 *     configforge gen       --schema <schema> [--name <function>] [--out <file>]
 *
 * `check` reads configuration files against the contract as a program
 * built from it would - every key defined, every value of its type and
 * in its range, nothing unknown outside an opened prefix - without
 * writing or running that program: a CI job or a pre-commit hook. The
 * files are sources in the order given, each overriding the ones before.
 *
 * `gen` writes the contract as C: one function that defines every key
 * on a builder with cf_def and cf_builder_define, and opens what the
 * schema opens. A build without the JSON reader - an embedded one - then
 * has the same contract from the same source, and the schema stays the
 * one place it is written. The function reads back through cf_schema as
 * the schema it came from (tests/gen/test_gen.c).
 */

#include "gen.h"

#include <configforge/configforge.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static int contract_usage(void) {
    fputs("  configforge gen --schema <schema> [--name <function>] [--out <file>]\n"
          "  configforge gen check --schema <schema> <file>...\n",
          stderr);
    return 2;
}

/* ------------------------------------------------------------------ *
 * check
 * ------------------------------------------------------------------ */

int gen_check(int argc, char** argv) {
    const char* schema = NULL;
    cf_builder* b;
    cf_config*  c;
    cf_error    err    = CF_ERROR_INIT;
    int         nfiles = 0;
    int         i;

    for (i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "--schema") == 0 && i + 1 < argc && schema == NULL) {
            schema = argv[++i];
        } else if (strncmp(argv[i], "--", 2) == 0) {
            return contract_usage();
        } else {
            ++nfiles;
        }
    }
    if (schema == NULL || nfiles == 0) {
        return contract_usage();
    }
    b = cf_build(NULL);
    if (b == NULL) {
        fprintf(stderr, "gen: cannot start\n");
        return 2;
    }
    /* A schema that cannot be read is unusable input, not a finding
       about the files. */
    if (!cf_builder_schema_file(b, schema)) {
        (void)cf_builder_create(b, &err);
        fprintf(stderr, "gen: %s\n", err.message);
        return 2;
    }
    cf_builder_unknown(b, CF_UNKNOWN_REJECT);
    for (i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "--schema") == 0) {
            ++i;
        } else {
            cf_builder_file(b, argv[i], CF_RANK_USER_FILE, argv[i], NULL, 0);
        }
    }
    c = cf_builder_create(b, &err);
    if (c == NULL) {
        printf("%s\n", err.message);
        return 1;
    }
    printf("ok: %d file(s) hold to %s\n", nfiles, schema);
    cf_config_free(c);
    return 0;
}

/* ------------------------------------------------------------------ *
 * gen
 * ------------------------------------------------------------------ */

/* Text as a C string literal: every byte that is not printable ASCII,
   and '"', '\\' and '?' (a trigraph's start), as a three-digit octal
   escape, which no following digit can extend. */
static void c_string(FILE* out, const char* p, size_t n) {
    size_t i;

    fputc('"', out);
    for (i = 0; i < n; ++i) {
        unsigned char ch = (unsigned char)p[i];
        if (ch < 0x20u || ch >= 0x7Fu || ch == '"' || ch == '\\' || ch == '?') {
            fprintf(out, "\\%03o", (unsigned)ch);
        } else {
            fputc((int)ch, out);
        }
    }
    fputc('"', out);
}

static void c_text(FILE* out, const char* s) {
    c_string(out, s, strlen(s));
}

/* A cf_i64 as an expression C89 compiles everywhere: no long long
   literal, and no literal that does not fit a 32-bit long. */
static void c_i64(FILE* out, cf_i64 v) {
    cf_u64 mag;

    if (v == CF_I64_MIN) {
        fputs("CF_I64_MIN", out);
        return;
    }
    if (v == CF_I64_MAX) {
        fputs("CF_I64_MAX", out);
        return;
    }
    if (v >= -2147483647 && v <= 2147483647) {
        fprintf(out, "(cf_i64)%ld", (long)v);
        return;
    }
    mag = (v < 0) ? (cf_u64)(-v) : (cf_u64)v;
    fprintf(out, "%s(((cf_i64)%luUL << 31) | (cf_i64)%luUL)", (v < 0) ? "-" : "",
            (unsigned long)(mag >> 31), (unsigned long)(mag & 0x7FFFFFFFu));
}

static const char* c_type(cf_type t) {
    switch (t) {
    case CF_TYPE_BOOL:
        return "CF_TYPE_BOOL";
    case CF_TYPE_INT:
        return "CF_TYPE_INT";
    case CF_TYPE_REAL:
        return "CF_TYPE_REAL";
    case CF_TYPE_TEXT:
        return "CF_TYPE_TEXT";
    case CF_TYPE_NULL:
    default:
        return NULL;
    }
}

/* One value assigned through the cf_value constructors. */
static cf_bool c_value(FILE* out, const char* target, const cf_value* v) {
    switch (v->type) {
    case CF_TYPE_BOOL:
        fprintf(out, "    cf_value_bool(%s, %s);\n", target,
                v->u.b ? "CF_TRUE" : "CF_FALSE");
        return CF_TRUE;
    case CF_TYPE_INT:
        fprintf(out, "    cf_value_int(%s, ", target);
        c_i64(out, v->u.i);
        fputs(");\n", out);
        return CF_TRUE;
#if CF_WITH_REAL
    case CF_TYPE_REAL:
        fprintf(out, "    cf_value_real(%s, %.17g);\n", target, v->u.r);
        return CF_TRUE;
#endif
    case CF_TYPE_TEXT:
        fprintf(out, "    cf_value_text(%s, ", target);
        c_string(out, v->u.t.p, v->u.t.n);
        fprintf(out, ", (size_t)%luu);\n", (unsigned long)v->u.t.n);
        return CF_TRUE;
    case CF_TYPE_NULL:
#if !CF_WITH_REAL
    case CF_TYPE_REAL:
#endif
    default:
        return CF_FALSE;
    }
}

static void c_flags(FILE* out, unsigned flags) {
    static const struct {
        unsigned    bit;
        const char* name;
    } names[] = {{CF_REQUIRED, "CF_REQUIRED"},
                 {CF_SENSITIVE, "CF_SENSITIVE"},
                 {CF_DEPRECATED, "CF_DEPRECATED"},
                 {CF_TRANSIENT, "CF_TRANSIENT"},
                 {CF_RESTART, "CF_RESTART"}};
    size_t   i;
    unsigned rest = flags;
    cf_bool  any  = CF_FALSE;

    fputs("    d.flags = ", out);
    for (i = 0; i < sizeof names / sizeof names[0]; ++i) {
        if ((flags & names[i].bit) != 0) {
            fprintf(out, "%s%s", any ? " | " : "", names[i].name);
            rest &= ~names[i].bit;
            any = CF_TRUE;
        }
    }
    if (rest != 0) {
        fprintf(out, "%s%uu", any ? " | " : "", rest);
    }
    fputs(";\n", out);
}

/* A NULL-terminated list of text, declared at the function's top. */
static void c_list_decl(FILE* out, const char* what, size_t i,
                        const char* const* list) {
    if (list == NULL) {
        return;
    }
    fprintf(out, "    static const char* const %s_%lu[] = {", what, (unsigned long)i);
    for (; *list != NULL; ++list) {
        c_text(out, *list);
        fputs(", ", out);
    }
    fputs("NULL};\n", out);
}

/* What the generated file defines to check each definition at compile
   time, by the language it is compiled as (D-75), one line each. */
static const char* const gen_checks[] = {
    "/* Each definition is checked by the compiler where the language can",
    "   (D-75): its key by the library's rule from C++11; from C11 its",
    "   key's length, an int default against its range and a text default",
    "   against its length. cf_builder_create checks all of it, in every",
    "   language. */",
    "#ifdef __cplusplus",
    "#include <configforge/configforge.hpp>",
    "#endif",
    "#if defined(__cplusplus) && CF_CPP11",
    "#define CF_GEN_BAD_ \"configforge: not a canonical key\"",
    "#define CF_GEN_OUT_ \"configforge: a default outside its own range\"",
    "#define CF_GEN_TXT1_ \"configforge: a text default longer than its max_len, \"",
    "#define CF_GEN_TXT_ CF_GEN_TXT1_ \"or shorter than its min_len\"",
    "#define CF_GEN_IN_(d, lo, hi) ((lo) <= (d) && (d) <= (hi))",
    "#define CF_GEN_MAX_(t, hi) ((hi) == 0 || sizeof(t) <= (hi) + 1)",
    "#define CF_GEN_FIT_(t, lo, hi) (sizeof(t) > (lo) && CF_GEN_MAX_(t, hi))",
    "#define CF_GEN_KEY(k) static_assert(cf::key_is_valid(k), CF_GEN_BAD_)",
    "#define CF_GEN_INT(d, lo, hi) static_assert(CF_GEN_IN_(d, lo, hi), CF_GEN_OUT_)",
    "#define CF_GEN_REAL(d, lo, hi) static_assert(CF_GEN_IN_(d, lo, hi), CF_GEN_OUT_)",
    "#define CF_GEN_LEN(t, l, h) static_assert(CF_GEN_FIT_(t, l, h), CF_GEN_TXT_)",
    "#elif CF_HAVE_C11 && !defined(__cplusplus)",
    "#define CF_GEN_KEY(k) CF_C11_KEY_(k)",
    "#define CF_GEN_INT(d, lo, hi) CF_C11_RANGE_(d, lo, hi)",
    "#define CF_GEN_REAL(d, lo, hi) ((void)0)",
    "#define CF_GEN_LEN(t, lo, hi) CF_C11_LEN_(t, lo, hi)",
    "#else",
    "#define CF_GEN_KEY(k) ((void)0)",
    "#define CF_GEN_INT(d, lo, hi) ((void)0)",
    "#define CF_GEN_REAL(d, lo, hi) ((void)0)",
    "#define CF_GEN_LEN(t, lo, hi) ((void)0)",
    "#endif",
    "",
    NULL};

static const char* const gen_undefs[] = {
    "#undef CF_GEN_KEY",  "#undef CF_GEN_INT",   "#undef CF_GEN_REAL",
    "#undef CF_GEN_LEN",  "#undef CF_GEN_BAD_",  "#undef CF_GEN_OUT_",
    "#undef CF_GEN_TXT_", "#undef CF_GEN_TXT1_", "#undef CF_GEN_IN_",
    "#undef CF_GEN_MAX_", "#undef CF_GEN_FIT_",  NULL};

static void c_lines(FILE* out, const char* const* lines) {
    for (; *lines != NULL; ++lines) {
        fputs(*lines, out);
        fputc('\n', out);
    }
}

/* The checks the compiler can make of one definition (D-75): an int or
   real default against its range, a text default against its length
   bounds. A list's defaults are checked by cf_define, as each is made. */
static void c_checks(FILE* out, const cf_def* d) {
    const cf_value* v = &d->default_value;

    if (!d->has_default || d->list_mode != CF_LIST_NONE) {
        return;
    }
    if (d->type == CF_TYPE_INT && d->has_range) {
        fputs("    CF_GEN_INT(", out);
        c_i64(out, v->u.i);
        fputs(", ", out);
        c_i64(out, d->imin);
        fputs(", ", out);
        c_i64(out, d->imax);
        fputs(");\n", out);
    }
#if CF_WITH_REAL
    if (d->type == CF_TYPE_REAL && d->has_range) {
        fprintf(out, "    CF_GEN_REAL(%.17g, %.17g, %.17g);\n", v->u.r, d->rmin,
                d->rmax);
    }
#endif
    if (d->type == CF_TYPE_TEXT && (d->min_len != 0 || d->max_len != 0)) {
        fputs("    CF_GEN_LEN(", out);
        c_string(out, v->u.t.p, v->u.t.n);
        fprintf(out, ", (size_t)%luu, (size_t)%luu);\n", (unsigned long)d->min_len,
                (unsigned long)d->max_len);
    }
}

/* What one definition says, past cf_def_init's zeros. */
static cf_bool c_define(FILE* out, size_t i, const cf_def* d) {
    char   target[64];
    size_t k;

    fprintf(out, "\n    cf_def_init(&d, %s, ", c_type(d->type));
    c_text(out, d->key);
    fputs(");\n    CF_GEN_KEY(", out);
    c_text(out, d->key);
    fputs(");\n", out);
    if (d->flags != 0) {
        c_flags(out, d->flags);
    }
    if (d->on_invalid == CF_INVALID_FALLBACK) {
        fputs("    d.on_invalid = CF_INVALID_FALLBACK;\n", out);
    }
    if (d->description != NULL) {
        fputs("    d.description = ", out);
        c_text(out, d->description);
        fputs(";\n", out);
    }
    if (d->replaced_by != NULL) {
        fputs("    d.replaced_by = ", out);
        c_text(out, d->replaced_by);
        fputs(";\n", out);
    }
    if (d->has_range && d->type == CF_TYPE_INT) {
        fputs("    d.has_range = CF_TRUE;\n    d.imin = ", out);
        c_i64(out, d->imin);
        fputs(";\n    d.imax = ", out);
        c_i64(out, d->imax);
        fputs(";\n", out);
    }
#if CF_WITH_REAL
    if (d->has_range && d->type == CF_TYPE_REAL) {
        fprintf(
            out,
            "    d.has_range = CF_TRUE;\n    d.rmin = %.17g;\n    d.rmax = %.17g;\n",
            d->rmin, d->rmax);
    }
#endif
    if (d->min_len != 0) {
        fprintf(out, "    d.min_len = (size_t)%luu;\n", (unsigned long)d->min_len);
    }
    if (d->max_len != 0) {
        fprintf(out, "    d.max_len = (size_t)%luu;\n", (unsigned long)d->max_len);
    }
    if (d->allowed != NULL) {
        fprintf(out, "    d.allowed = allowed_%lu;\n", (unsigned long)i);
    }
    if (d->aliases != NULL) {
        fprintf(out, "    d.aliases = aliases_%lu;\n", (unsigned long)i);
    }
    if (d->policy != NULL) {
        fprintf(out, "    d.policy = policy_%lu;\n", (unsigned long)i);
    }
    if (d->list_mode != CF_LIST_NONE) {
        fprintf(out, "    d.list_mode = %s;\n",
                (d->list_mode == CF_LIST_APPEND) ? "CF_LIST_APPEND"
                                                 : "CF_LIST_REPLACE");
    }
    if (d->list_separator != '\0') {
        fprintf(out, "    d.list_separator = (char)%d;\n", (int)d->list_separator);
    }
    if (d->has_default) {
        fputs("    d.has_default = CF_TRUE;\n", out);
        if (d->list_mode == CF_LIST_NONE) {
            if (!c_value(out, "&d.default_value", &d->default_value)) {
                return CF_FALSE;
            }
        } else {
            for (k = 0; k < d->list_default_count; ++k) {
                sprintf(target, "&list_%lu[%lu]", (unsigned long)i, (unsigned long)k);
                if (!c_value(out, target, &d->list_defaults[k])) {
                    return CF_FALSE;
                }
            }
            if (d->list_default_count > 0) {
                fprintf(out, "    d.list_defaults = list_%lu;\n", (unsigned long)i);
            }
            fprintf(out, "    d.list_default_count = (size_t)%luu;\n",
                    (unsigned long)d->list_default_count);
        }
    }
    c_checks(out, d);
    fputs("    ok = ok && cf_builder_define(b, &d);\n", out);
    return CF_TRUE;
}

/* A C identifier, as the function's name must be. */
static cf_bool c_identifier(const char* s) {
    size_t i;
    if (s[0] == '\0' || cf_is_digit((unsigned char)s[0])) {
        return CF_FALSE;
    }
    for (i = 0; s[i] != '\0'; ++i) {
        if (!cf_is_alnum((unsigned char)s[i]) && s[i] != '_') {
            return CF_FALSE;
        }
    }
    return CF_TRUE;
}

/* The schema's file name, for the generated file's comment: no
   directories, which are the machine's, and nothing that would end the
   comment. */
static void c_comment_name(FILE* out, const char* path) {
    const char* base = path;
    const char* p;

    for (p = path; *p != '\0'; ++p) {
        if (*p == '/' || *p == '\\') {
            base = p + 1;
        }
    }
    for (p = base; *p != '\0'; ++p) {
        fputc((p[0] == '*' && p[1] == '/') ? '_' : (int)(unsigned char)*p, out);
    }
}

static int gen_write(FILE* out, const cf_defset* ds, const char* schema,
                     const char* name) {
    size_t  n = cf_defset_count(ds);
    size_t  i;
    cf_bool reals = CF_FALSE;

    for (i = 0; i < n; ++i) {
        if (cf_defset_at(ds, i)->type == CF_TYPE_REAL) {
            reals = CF_TRUE;
        }
    }
    fputs("/* The contract of ", out);
    c_comment_name(out, schema);
    fputs(", as calls: written by `configforge gen --schema`.\n"
          "   Regenerate it from the schema rather than editing it. */\n\n"
          "#include <configforge/configforge.h>\n\n",
          out);
    c_lines(out, gen_checks);
    if (reals) {
        fputs("#if !CF_WITH_REAL\n"
              "#error \"the contract has real keys: build with CF_WITH_REAL\"\n"
              "#endif\n\n",
              out);
    }
    fprintf(out, "cf_bool %s(cf_builder* b);\n\ncf_bool %s(cf_builder* b) {\n", name,
            name);
    for (i = 0; i < n; ++i) {
        const cf_def* d = cf_defset_at(ds, i);
        c_list_decl(out, "allowed", i, d->allowed);
        c_list_decl(out, "aliases", i, d->aliases);
        c_list_decl(out, "policy", i, d->policy);
        if (d->has_default && d->list_mode != CF_LIST_NONE &&
            d->list_default_count > 0) {
            fprintf(out, "    cf_value list_%lu[%lu];\n", (unsigned long)i,
                    (unsigned long)d->list_default_count);
        }
    }
    if (n > 0) {
        fputs("    cf_def  d;\n", out);
    }
    fputs("    cf_bool ok = CF_TRUE;\n", out);
    for (i = 0; i < n; ++i) {
        const cf_def* d = cf_defset_at(ds, i);
        if (c_type(d->type) == NULL || !c_define(out, i, d)) {
            fprintf(stderr, "gen: '%s' has a value this generator cannot write\n",
                    d->key);
            return 2;
        }
    }
    if (ds->open_count > 0) {
        fputc('\n', out);
    }
    for (i = 0; i < ds->open_count; ++i) {
        fputs("    ok = ok && cf_builder_open(b, ", out);
        c_text(out, ds->open_prefixes[i]);
        fputs(");\n", out);
    }
    if (n == 0 && ds->open_count == 0) {
        fputs("    (void)b;\n", out);
    }
    fputs("    return ok;\n}\n", out);
    c_lines(out, gen_undefs);
    return 0;
}

int gen_code(int argc, char** argv) {
    const char* schema = NULL;
    const char* name   = "cf_contract";
    const char* path   = NULL;
    cf_arena*   ar;
    cf_defset   ds;
    cf_error    err = CF_ERROR_INIT;
    FILE*       out = stdout;
    int         rc;
    int         i;

    for (i = 0; i + 1 < argc; i += 2) {
        if (strcmp(argv[i], "--schema") == 0) {
            schema = argv[i + 1];
        } else if (strcmp(argv[i], "--name") == 0) {
            name = argv[i + 1];
        } else if (strcmp(argv[i], "--out") == 0) {
            path = argv[i + 1];
        } else {
            return contract_usage();
        }
    }
    if (i != argc || schema == NULL || !c_identifier(name)) {
        return contract_usage();
    }
    ar = cf_arena_new(NULL, 0);
    if (ar == NULL || !cf_defset_init(&ds, ar, 0) ||
        !cf_defset_schema_file(&ds, schema, &err)) {
        fprintf(stderr, "gen: %s\n", (ar == NULL) ? "cannot start" : err.message);
        cf_arena_free(ar);
        return 2;
    }
    if (path != NULL) {
        out = fopen(path, "w");
        if (out == NULL) {
            fprintf(stderr, "gen: cannot write %s\n", path);
            cf_arena_free(ar);
            return 2;
        }
    }
    rc = gen_write(out, &ds, schema, name);
    if (path != NULL && (fclose(out) != 0 || rc != 0)) {
        (void)remove(path);
        rc = (rc != 0) ? rc : 2;
    }
    cf_arena_free(ar);
    return rc;
}

/* ------------------------------------------------------------------ *
 * schema --from
 * ------------------------------------------------------------------ */

/* The bytes of standard input, in memory the caller frees; NULL when
   there is no room. On Windows a text stream would turn CR LF into LF,
   so the stream is made binary. */
static char* sd_stdin(size_t* n) {
    size_t cap = 4096;
    size_t len = 0;
    char*  buf = (char*)malloc(cap);

#ifdef _WIN32
    (void)_setmode(_fileno(stdin), _O_BINARY);
#endif
    while (buf != NULL) {
        size_t got;

        if (len == cap) {
            char* grown;

            cap *= 2u;
            grown = (char*)realloc(buf, cap);
            if (grown == NULL) {
                free(buf);
                return NULL;
            }
            buf = grown;
        }
        got = fread(buf + len, 1, cap - len, stdin);
        if (got == 0) {
            break;
        }
        len += got;
    }
    *n = len;
    return buf;
}

static void sd_not_compiled(const char* format) {
    fprintf(stderr, "gen: format '%s' is not compiled into this build; turn on %s\n",
            format, cf_format_option(format));
}

/* The codec `as` names, or the one the path's name and then the bytes
   say (D-73); NULL with the reason on stderr. */
static const cf_codec* sd_codec(const char* path, const char* as, const cf_blob* blob,
                                cf_arena* trial, cf_bool is_stdin) {
    const cf_codec* c;
    cf_detect       d;

    if (as != NULL) {
        c = cf_codec_by_name(as);
        if (c == NULL && cf_format_option(as) != NULL) {
            sd_not_compiled(as);
        } else if (c == NULL) {
            fprintf(stderr, "gen: no format called '%s'\n", as);
        }
        return c;
    }
    c = cf_codec_detect(NULL, 0, blob, is_stdin ? NULL : path, trial, &d);
    if (c != NULL) {
        return c;
    }
    if (d.first != NULL) {
        fprintf(stderr,
                "gen: %s reads as %s and as %s, differently; name its format with "
                "--as\n",
                path, d.first->name, d.second->name);
    } else if (!is_stdin && cf_format_of_path(path) != NULL) {
        sd_not_compiled(cf_format_of_path(path));
    } else {
        fprintf(stderr, "gen: cannot tell the format of %s; name it with --as\n", path);
    }
    return NULL;
}

int gen_read_keys(const char* path, const char* as, cf_arena* ar, cf_keyspace* ks,
                  const cf_codec** used) {
    cf_blob         blob;
    cf_error        err      = CF_ERROR_INIT;
    cf_bool         is_stdin = (strcmp(path, "-") == 0) ? CF_TRUE : CF_FALSE;
    cf_arena*       bytes    = cf_blob_arena(ar);
    char*           held     = NULL;
    const cf_codec* codec;
    cf_flatten      fl;
    int             rc = 2;

    if (bytes == NULL) {
        fprintf(stderr, "gen: cannot start\n");
        return 2;
    }
    if (is_stdin) {
        size_t n = 0;

        held = sd_stdin(&n);
        if (held == NULL) {
            fprintf(stderr, "gen: cannot read %s\n", path);
            cf_blob_arena_done(bytes, ar);
            return 2;
        }
        cf_blob_mem(&blob, held, n, "stdin");
    } else if (!cf_blob_file(&blob, bytes, path, &err)) {
        fprintf(stderr, "gen: cannot read %s\n", path);
        cf_blob_arena_done(bytes, ar);
        return 2;
    }
    codec = sd_codec(path, as, &blob, bytes, is_stdin);
    if (codec != NULL && (codec->caps & CF_CODEC_READ) == 0) {
        fprintf(stderr, "gen: format '%s' can be written but not read\n", codec->name);
        codec = NULL;
    }
    if (codec != NULL) {
        if (used != NULL) {
            *used = codec;
        }
        if (!cf_ks_init(ks, ar, 64) || !cf_flatten_init(&fl, ks, 0)) {
            fprintf(stderr, "gen: cannot start\n");
        } else {
            cf_bool decoded = cf_codec_decode(codec, &blob, cf_flatten_sink(&fl), &err);
            /* A key the flattener refused reads to the codec as a sink
               that refused it: the flattener says which, whether the
               decoding failed for it or went on. */
            if (fl.failed || fl.overflow) {
                fprintf(stderr, "gen: '%s' holds a key this build cannot hold\n", path);
            } else if (!decoded) {
                fprintf(stderr, "gen: %s\n", err.message);
            } else if (!cf_ks_resolve(ks)) {
                fprintf(stderr, "gen: cannot start\n");
            } else {
                rc = 0;
            }
        }
    }
    free(held);
    cf_blob_arena_done(bytes, ar);
    return rc;
}

/* The walk: the document's events, with the path of each, into the
   definitions. A value is a definition at its path; an array of one
   scalar type is a list; what a contract has no type for is named on
   stderr and left out. */
enum { SD_TABLE, SD_ARRAY, SD_SKIPPED };

typedef struct sd_frame {
    size_t saved; /* the path's length outside this container */
    int    kind;  /* SD_SKIPPED: inside an array, which it has left out */
} sd_frame;

typedef struct sd_walk {
    const cf_keyspace* ks;
    cf_defset*         ds;
    cf_error           err;
    char               path[CF_KEY_MAX];
    size_t             plen;
    sd_frame           stack[CF_MAX_DEPTH + 4];
    int                depth; /* containers open; the root is the first */

    /* The array being read, when one is: how many scalars it has held,
       their type, and what else. */
    size_t  index;
    cf_type etype;
    cf_bool etable;
    cf_bool earray;
    cf_bool emixed;
} sd_walk;

/* The key of the next member, joined by a dot below the root. */
static cf_bool sd_name(sd_walk* w, const char* name, size_t n) {
    size_t need = w->plen + ((w->plen > 0) ? 1u : 0u) + n;

    if (need >= sizeof w->path) {
        return CF_FALSE;
    }
    if (w->plen > 0) {
        w->path[w->plen++] = '.';
    }
    memcpy(w->path + w->plen, name, n);
    w->plen          = need;
    w->path[w->plen] = '\0';
    return CF_TRUE;
}

/* The type a scalar's definition has: the value's, except that text a
   format has no types for is what Tier 0 inference says (D-24). A null
   is CF_TYPE_NULL. */
static cf_type sd_type(const sd_walk* w, const cf_value* v) {
    const cf_entry* e;

    if (v->type != CF_TYPE_TEXT) {
        return v->type;
    }
    e = cf_ks_find(w->ks, w->path, w->plen);
    if (e != NULL && e->selected != NULL &&
        (e->selected->flags & CF_CAND_TEXTUAL) != 0) {
        switch (cf_infer_scalar(v->u.t.p, v->u.t.n)) {
        case CF_SCALAR_BOOL:
            return CF_TYPE_BOOL;
        case CF_SCALAR_INT:
            return CF_TYPE_INT;
        case CF_SCALAR_REAL:
            return CF_TYPE_REAL;
        case CF_SCALAR_TEXT:
        default:
            break;
        }
    }
    return CF_TYPE_TEXT;
}

static void sd_left_out(const sd_walk* w, const char* why) {
    fprintf(stderr, "gen schema: %s: %s; left out\n", w->path, why);
}

/* The key at the path, defined as `type`, a list when `list`: no
   default, no range, an empty description to fill in. */
static cf_bool sd_define(sd_walk* w, cf_type type, cf_bool list) {
    cf_def d;

    cf_def_init(&d, type, w->path);
    d.description = "";
    if (list) {
        d.list_mode = CF_LIST_REPLACE;
    }
    return cf_define(w->ds, &d, &w->err);
}

static cf_bool sd_begin(void* ctx, const char* name, size_t n, int kind) {
    sd_walk* w     = (sd_walk*)ctx;
    int      under = (w->depth > 0) ? w->stack[w->depth - 1].kind : SD_TABLE;
    size_t   saved = w->plen;

    if (w->depth >= (int)(sizeof w->stack / sizeof w->stack[0])) {
        return CF_FALSE;
    }
    if (under == SD_ARRAY || under == SD_SKIPPED) {
        if (under == SD_ARRAY) {
            if (kind == SD_ARRAY) {
                w->earray = CF_TRUE;
            } else {
                w->etable = CF_TRUE;
            }
        }
        kind = SD_SKIPPED;
    } else {
        if (name != NULL && !sd_name(w, name, n)) {
            return CF_FALSE;
        }
        if (kind == SD_ARRAY) {
            w->index  = 0;
            w->etype  = CF_TYPE_NULL;
            w->etable = CF_FALSE;
            w->earray = CF_FALSE;
            w->emixed = CF_FALSE;
        }
    }
    w->stack[w->depth].saved = saved;
    w->stack[w->depth].kind  = kind;
    ++w->depth;
    return CF_TRUE;
}

static cf_bool sd_begin_table(void* ctx, const char* name, size_t n) {
    return sd_begin(ctx, name, n, SD_TABLE);
}

static cf_bool sd_begin_array(void* ctx, const char* name, size_t n) {
    return sd_begin(ctx, name, n, SD_ARRAY);
}

static cf_bool sd_scalar(void* ctx, const char* name, size_t n, const cf_value* v,
                         cf_bool textual, const cf_loc* at) {
    sd_walk* w     = (sd_walk*)ctx;
    int      under = (w->depth > 0) ? w->stack[w->depth - 1].kind : SD_TABLE;
    size_t   saved = w->plen;
    cf_type  type;
    cf_bool  ok;

    (void)textual; /* cf_doc_emit says none: the key space does (sd_type) */
    (void)at;
    if (under == SD_SKIPPED) {
        return CF_TRUE;
    }
    if (under == SD_ARRAY) {
        char num[32];

        sprintf(num, "%lu", (unsigned long)w->index);
        ok = sd_name(w, num, strlen(num));
    } else {
        ok = sd_name(w, name, n);
    }
    if (!ok) {
        return CF_FALSE;
    }
    type = sd_type(w, v);
    if (under == SD_ARRAY) {
        if (w->index == 0) {
            w->etype = type;
        } else if (w->etype != type) {
            w->emixed = CF_TRUE;
        }
        ++w->index;
        ok = CF_TRUE;
    } else if (type == CF_TYPE_NULL) {
        sd_left_out(w, "a null, whose type the sample does not show");
        ok = CF_TRUE;
    } else {
        ok = sd_define(w, type, CF_FALSE);
    }
    w->plen          = saved;
    w->path[w->plen] = '\0';
    return ok;
}

static cf_bool sd_end(void* ctx) {
    sd_walk* w = (sd_walk*)ctx;
    sd_frame f;
    cf_bool  ok = CF_TRUE;

    if (w->depth == 0) {
        return CF_FALSE;
    }
    f = w->stack[--w->depth];
    if (f.kind == SD_ARRAY) {
        if (w->etable) {
            sd_left_out(w, "an array of tables, which a contract has no type for");
        } else if (w->earray) {
            sd_left_out(w, "an array of arrays, which a contract has no type for");
        } else if (w->emixed) {
            sd_left_out(w, "an array of mixed types, which a contract has no type for");
        } else if (w->index == 0) {
            sd_left_out(w,
                        "an empty array, whose element type the sample does not show");
        } else if (w->etype == CF_TYPE_NULL) {
            sd_left_out(w, "an array of nulls, whose type the sample does not show");
        } else {
            ok = sd_define(w, w->etype, CF_TRUE);
        }
    }
    w->plen          = f.saved;
    w->path[w->plen] = '\0';
    return ok;
}

/* A file's base name without its extension: the contract's title. */
static const char* sd_title(cf_arena* ar, const char* path) {
    const char* base = path;
    const char* p;
    size_t      n;
    char*       t;

    if (strcmp(path, "-") == 0) {
        return "stdin";
    }
    for (p = path; *p != '\0'; ++p) {
        if (*p == '/' || *p == '\\') {
            base = p + 1;
        }
    }
    n = strlen(base);
    for (p = base + n; p > base + 1; --p) {
        if (p[-1] == '.') {
            n = (size_t)(p - 1 - base);
            break;
        }
    }
    t = (char*)cf_arena_alloc(ar, n + 1u);
    if (t != NULL) {
        memcpy(t, base, n);
        t[n] = '\0';
    }
    return t;
}

static int schema_usage(void) {
    fputs("  configforge gen schema --from <file> [--as <format>] [--title <t>] "
          "[--out <file>]\n",
          stderr);
    return 2;
}

/* The draft, measured and then written, to `path` or standard output. */
static int sd_write(const cf_defset* ds, const char* title, const char* path,
                    cf_arena* ar, cf_error* err) {
    size_t need = cf_schema_write(ds, title, CF_TRUE, NULL, 0, err);
    char*  buf;
    FILE*  out = stdout;
    int    rc  = 0;

    if (need == 0) {
        fprintf(stderr, "gen: %s\n", err->message);
        return 2;
    }
    buf = (char*)cf_arena_alloc(ar, need + 1u);
    if (buf == NULL ||
        cf_schema_write(ds, title, CF_TRUE, buf, need + 1u, err) != need) {
        fprintf(stderr, "gen: %s\n", (buf == NULL) ? "cannot start" : err->message);
        return 2;
    }
    if (path != NULL) {
        out = fopen(path, "w");
        if (out == NULL) {
            fprintf(stderr, "gen: cannot write %s\n", path);
            return 2;
        }
    }
    if (fwrite(buf, 1, need, out) != need) {
        rc = 2;
    }
    if (path != NULL && (fclose(out) != 0 || rc != 0)) {
        (void)remove(path);
        rc = 2;
    }
    if (rc != 0) {
        fprintf(stderr, "gen: cannot write %s\n",
                (path != NULL) ? path : "standard output");
    }
    return rc;
}

int gen_schema(int argc, char** argv) {
    const char* from  = NULL;
    const char* as    = NULL;
    const char* title = NULL;
    const char* out   = NULL;
    cf_arena*   ar;
    cf_keyspace ks;
    cf_defset   ds;
    sd_walk*    w;
    cf_doc_sink sink;
    cf_error    err = CF_ERROR_INIT;
    int         rc;
    int         i;

    for (i = 0; i + 1 < argc; i += 2) {
        if (strcmp(argv[i], "--from") == 0) {
            from = argv[i + 1];
        } else if (strcmp(argv[i], "--as") == 0) {
            as = argv[i + 1];
        } else if (strcmp(argv[i], "--title") == 0) {
            title = argv[i + 1];
        } else if (strcmp(argv[i], "--out") == 0) {
            out = argv[i + 1];
        } else {
            return schema_usage();
        }
    }
    if (i != argc || from == NULL) {
        return schema_usage();
    }
    ar = cf_arena_new(NULL, 0);
    if (ar == NULL) {
        fprintf(stderr, "gen: cannot start\n");
        return 2;
    }
    rc = gen_read_keys(from, as, ar, &ks, NULL);
    if (rc != 0) {
        cf_arena_free(ar);
        return rc;
    }
    if (!cf_defset_init(&ds, ar, 0)) {
        fprintf(stderr, "gen: cannot start\n");
        cf_arena_free(ar);
        return 2;
    }
    w = (sd_walk*)cf_arena_calloc(ar, sizeof *w);
    if (w == NULL) {
        fprintf(stderr, "gen: cannot start\n");
        cf_arena_free(ar);
        return 2;
    }
    w->ks  = &ks;
    w->ds  = &ds;
    w->err = err;
    memset(&sink, 0, sizeof sink);
    sink.size        = sizeof sink;
    sink.begin_table = sd_begin_table;
    sink.begin_array = sd_begin_array;
    sink.scalar      = sd_scalar;
    sink.end         = sd_end;
    sink.ctx         = w;
    if (!cf_doc_emit(&ks, &sink, &err)) {
        /* The definition's own refusal says why, when it was one. */
        fprintf(stderr, "gen: %s\n",
                (w->err.message[0] != '\0') ? w->err.message : err.message);
        cf_arena_free(ar);
        return 2;
    }
    cf_defset_seal(&ds);
    if (title == NULL) {
        title = sd_title(ar, from);
    }
    rc = sd_write(&ds, title, out, ar, &err);
    if (rc == 0) {
        fprintf(stderr,
                "gen schema: a draft from %s: %lu key(s), every one optional, with no "
                "defaults, ranges or descriptions; review it before it is the "
                "contract\n",
                from, (unsigned long)cf_defset_count(&ds));
    }
    cf_arena_free(ar);
    return rc;
}
