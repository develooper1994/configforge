/*
 * contract.c - Doctor and a JSON Schema contract (D-64, D-67).
 *
 *     doctor check --schema <schema> <file>...
 *     doctor gen   --schema <schema> [--name <function>] [--out <file>]
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
 * the schema it came from (tests/doctor/test_gen.c).
 */

#include "adapters.h"

#include <configforge/configforge.h>

#include <stdio.h>
#include <string.h>

static int contract_usage(void) {
    fputs("  doctor check --schema <schema> <file>...\n"
          "  doctor gen   --schema <schema> [--name <function>] [--out <file>]\n",
          stderr);
    return 2;
}

/* ------------------------------------------------------------------ *
 * check
 * ------------------------------------------------------------------ */

int doctor_check_schema(int argc, char** argv) {
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
        fprintf(stderr, "doctor: cannot start\n");
        return 2;
    }
    /* A schema that cannot be read is unusable input, not a finding
       about the files. */
    if (!cf_builder_schema_file(b, schema)) {
        (void)cf_builder_create(b, &err);
        fprintf(stderr, "doctor: %s\n", err.message);
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

/* What one definition says, past cf_def_init's zeros. */
static cf_bool c_define(FILE* out, size_t i, const cf_def* d) {
    char   target[64];
    size_t k;

    fprintf(out, "\n    cf_def_init(&d, %s, ", c_type(d->type));
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
    fputs(", as calls: written by `doctor gen --schema`.\n"
          "   Regenerate it from the schema rather than editing it. */\n\n"
          "#include <configforge/configforge.h>\n\n",
          out);
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
            fprintf(stderr, "doctor: '%s' has a value this generator cannot write\n",
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
    return 0;
}

int doctor_gen_schema(int argc, char** argv) {
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
        fprintf(stderr, "doctor: %s\n", (ar == NULL) ? "cannot start" : err.message);
        cf_arena_free(ar);
        return 2;
    }
    if (path != NULL) {
        out = fopen(path, "w");
        if (out == NULL) {
            fprintf(stderr, "doctor: cannot write %s\n", path);
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
