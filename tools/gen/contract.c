/*
 * contract.c - a JSON Schema contract, checked against and written (D-64, D-67).
 *
 *     configforge gen check --schema <schema> <file>...
 *     configforge gen       --schema <schema> [--lang c|cpp|rust|python] [--name
 * <name>] [--out <file>]
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
    fputs("  configforge gen --schema <schema> [--lang c|cpp|rust|python] [--name "
          "<name>] "
          "[--out <file>]\n"
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

/* The definitions, as the calls that make them: what a function of the
   generated file holds from its declarations to its `return`. C++ compiles
   every line of it and ends it as a bool (`cxx`), the one line that
   differs. */
static int gen_c_body(FILE* out, const cf_defset* ds, cf_bool cxx) {
    size_t n = cf_defset_count(ds);
    size_t i;

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
    fputs(cxx ? "    return ok != CF_FALSE;\n" : "    return ok;\n", out);
    return 0;
}

/* Whether any key is a real, which a build without reals cannot read. */
static cf_bool gen_has_reals(const cf_defset* ds) {
    size_t i;

    for (i = 0; i < cf_defset_count(ds); ++i) {
        if (cf_defset_at(ds, i)->type == CF_TYPE_REAL) {
            return CF_TRUE;
        }
    }
    return CF_FALSE;
}

static void gen_need_reals(FILE* out) {
    fputs("#if !CF_WITH_REAL\n"
          "#error \"the contract has real keys: build with CF_WITH_REAL\"\n"
          "#endif\n\n",
          out);
}

/* ------------------------------------------------------------------ *
 * The languages (D-95)
 *
 * What `--lang` finds, and nothing else names a language: its name, the
 * extension of its file, the check of the name `--name` gives it, the
 * words it reserves (sorted, against the names the writer makes) and the
 * writer, handed the definitions, the schema's path, `--name` and its own
 * entry.
 * ------------------------------------------------------------------ */

typedef struct gen_lang gen_lang;

struct gen_lang {
    const char* name;
    /* The extension of its file: named by the table for the tools that name
       the output (W5-G4); nothing in this program reads it yet. */
    /* cppcheck-suppress unusedStructMember */
    const char* extension;
    /* What `--name` is when it is not given. */
    const char* default_name;
    cf_bool (*identifier)(const char* name);
    const char* const* reserved;
    size_t             reserved_count;
    int (*write)(FILE* out, const cf_defset* ds, const char* schema, const char* name,
                 const gen_lang* self);
};

/* The contract as C: one function. */
static int gen_c_file(FILE* out, const cf_defset* ds, const char* schema,
                      const char* name, const gen_lang* self) {
    int rc;

    (void)self;
    fputs("/* The contract of ", out);
    c_comment_name(out, schema);
    fputs(", as calls: written by `configforge gen --schema`.\n"
          "   Regenerate it from the schema rather than editing it. */\n\n"
          "#include <configforge/configforge.h>\n\n",
          out);
    c_lines(out, gen_checks);
    if (gen_has_reals(ds)) {
        gen_need_reals(out);
    }
    fprintf(out, "cf_bool %s(cf_builder* b);\n\ncf_bool %s(cf_builder* b) {\n", name,
            name);
    rc = gen_c_body(out, ds, CF_FALSE);
    if (rc != 0) {
        return rc;
    }
    fputs("}\n", out);
    c_lines(out, gen_undefs);
    return 0;
}

/* ------------------------------------------------------------------ *
 * C++: one header
 *
 * The C body, whole, in a function of the namespace; a Key for each key,
 * to read; and a struct filled through a cf::Binding. The names are made
 * here, from the keys (docs/Tools.md).
 * ------------------------------------------------------------------ */

/* C++'s reserved words, and the names the header makes itself; sorted. */
static const char* const gen_cxx_reserved[] = {"Contract",
                                               "alignas",
                                               "alignof",
                                               "and",
                                               "and_eq",
                                               "asm",
                                               "auto",
                                               "bind",
                                               "bitand",
                                               "bitor",
                                               "bool",
                                               "break",
                                               "case",
                                               "catch",
                                               "cf",
                                               "char",
                                               "char16_t",
                                               "char32_t",
                                               "char8_t",
                                               "class",
                                               "co_await",
                                               "co_return",
                                               "co_yield",
                                               "compl",
                                               "concept",
                                               "const",
                                               "const_cast",
                                               "consteval",
                                               "constexpr",
                                               "constinit",
                                               "continue",
                                               "decltype",
                                               "default",
                                               "define",
                                               "delete",
                                               "do",
                                               "double",
                                               "dynamic_cast",
                                               "else",
                                               "enum",
                                               "explicit",
                                               "export",
                                               "extern",
                                               "false",
                                               "float",
                                               "for",
                                               "friend",
                                               "goto",
                                               "if",
                                               "inline",
                                               "int",
                                               "keys",
                                               "long",
                                               "mutable",
                                               "namespace",
                                               "new",
                                               "noexcept",
                                               "not",
                                               "not_eq",
                                               "nullptr",
                                               "operator",
                                               "or",
                                               "or_eq",
                                               "private",
                                               "protected",
                                               "public",
                                               "register",
                                               "reinterpret_cast",
                                               "requires",
                                               "return",
                                               "short",
                                               "signed",
                                               "sizeof",
                                               "static",
                                               "static_assert",
                                               "static_cast",
                                               "struct",
                                               "switch",
                                               "template",
                                               "this",
                                               "thread_local",
                                               "throw",
                                               "true",
                                               "try",
                                               "typedef",
                                               "typeid",
                                               "typename",
                                               "union",
                                               "unsigned",
                                               "using",
                                               "virtual",
                                               "void",
                                               "volatile",
                                               "wchar_t",
                                               "while",
                                               "xor",
                                               "xor_eq"};

/* The room a name made from a key takes: the key, a `k_` or a `K` before
   it, `Table` or `_` after. */
#define GEN_NAME_MAX (CF_KEY_MAX + 8)

static int gen_word_cmp(const void* word, const void* entry) {
    return strcmp((const char*)word, *(const char* const*)entry);
}

static cf_bool gen_reserved(const gen_lang* lang, const char* word) {
    return lang->reserved_count > 0 &&
           bsearch(word, (const void*)lang->reserved, lang->reserved_count,
                   sizeof *lang->reserved, gen_word_cmp) != NULL;
}

/* A key's identifier: its text with '.' and '-' as '_'; `k_` before a
   digit; `_` after a word the language reserves. */
static void gen_ident(char* dst, const char* s, size_t n, const gen_lang* lang) {
    size_t i;
    size_t o = 0;

    if (n > 0 && cf_is_digit((unsigned char)s[0])) {
        dst[o++] = 'k';
        dst[o++] = '_';
    }
    for (i = 0; i < n; ++i) {
        dst[o] = s[i];
        if (s[i] == '.' || s[i] == '-') {
            dst[o] = '_';
        }
        ++o;
    }
    dst[o] = '\0';
    if (gen_reserved(lang, dst)) {
        dst[o++] = '_';
        dst[o]   = '\0';
    }
}

static void gen_append(char* dst, const char* tail) {
    memcpy(dst + strlen(dst), tail, strlen(tail) + 1);
}

/* A struct's name: a key's top segment in CamelCase, '-' and '_' taken as
   breaks; `K` before what is empty or starts with a digit, `Table` after
   one equal to the top struct's name (`top`), `_` after a reserved word. */
static void gen_struct_name(char* dst, const char* s, size_t n, const char* top,
                            const gen_lang* lang) {
    size_t  i;
    size_t  o  = 0;
    cf_bool up = CF_TRUE;

    for (i = 0; i < n; ++i) {
        if (s[i] == '-' || s[i] == '_') {
            up = CF_TRUE;
        } else {
            dst[o] = s[i];
            if (up && s[i] >= 'a' && s[i] <= 'z') {
                dst[o] = (char)(s[i] - 32);
            }
            ++o;
            up = CF_FALSE;
        }
    }
    dst[o] = '\0';
    if (o == 0 || cf_is_digit((unsigned char)dst[0])) {
        memmove(dst + 1, dst, o + 1);
        dst[0] = 'K';
    }
    if (strcmp(dst, top) == 0) {
        gen_append(dst, "Table");
    } else if (gen_reserved(lang, dst)) {
        gen_append(dst, "_");
    }
}

/* What the writer knows of each struct below the top, and of each key. */
typedef struct cx_table {
    const char* top; /* the segment, in the first key's text */
    size_t      top_n;
    const char* first_key; /* the first key under it, for what is said */
    size_t      first;     /* and its place among the keys */
    char        member[GEN_NAME_MAX];
    char        type[GEN_NAME_MAX];
} cx_table;

#define CX_TOP ((size_t)-1)

typedef struct cx_key {
    const cf_def* def;
    const char*   rest;  /* the key past its top segment: the field's text */
    size_t        table; /* the struct it is in, or CX_TOP */
    int           skip;  /* 0 a field, 1 a list, 2 a type C++ does not read */
    char          full[GEN_NAME_MAX];
    char          field[GEN_NAME_MAX];
} cx_key;

typedef struct cx_model {
    const gen_lang* lang;
    const char*     top; /* the top struct's name, which no other struct takes */
    cx_key*         keys;
    cx_table*       tables;
    size_t          n;
    size_t          tables_n;
} cx_model;

static const char* cx_type(cf_type t) {
    switch (t) {
    case CF_TYPE_BOOL:
        return "bool";
    case CF_TYPE_INT:
        return "cf_i64";
    case CF_TYPE_REAL:
        return "double";
    case CF_TYPE_TEXT:
        return "std::string";
    case CF_TYPE_NULL:
    default:
        return NULL;
    }
}

/* The struct under a key's top segment, made at its first key. */
static size_t cx_table_of(cx_model* m, const char* key, size_t top_n, size_t index) {
    size_t    t;
    cx_table* tb;

    for (t = 0; t < m->tables_n; ++t) {
        if (m->tables[t].top_n == top_n && memcmp(m->tables[t].top, key, top_n) == 0) {
            return t;
        }
    }
    t             = m->tables_n++;
    tb            = &m->tables[t];
    tb->top       = key;
    tb->top_n     = top_n;
    tb->first_key = key;
    tb->first     = index;
    gen_ident(tb->member, key, top_n, m->lang);
    gen_struct_name(tb->type, key, top_n, m->top, m->lang);
    return t;
}

/* The shape: a key of one segment is a field of the top struct, of two
   or more a field of its top segment's struct (two levels, as far as a
   Binding reaches), in the order the keys come. */
static void cx_shape(cx_model* m, const cf_defset* ds) {
    size_t i;

    for (i = 0; i < m->n; ++i) {
        cx_key*     k = &m->keys[i];
        const char* key;
        const char* dot;

        k->def   = cf_defset_at(ds, i);
        key      = k->def->key;
        dot      = strchr(key, '.');
        k->table = (dot != NULL) ? cx_table_of(m, key, (size_t)(dot - key), i) : CX_TOP;
        k->rest  = (dot != NULL) ? dot + 1 : key;
        gen_ident(k->full, key, strlen(key), m->lang);
        gen_ident(k->field, k->rest, strlen(k->rest), m->lang);
        k->skip = (k->def->list_mode != CF_LIST_NONE) ? 1 : 0;
        if (k->skip == 0 && cx_type(k->def->type) == NULL) {
            k->skip = 2;
        }
    }
}

static int cx_clash(const cx_model* m, const char* a, const char* b, const char* name) {
    fprintf(stderr, "gen: '%s' and '%s' are both '%s' in %s; rename one\n", a, b, name,
            m->lang->name);
    return 2;
}

/* The structs' names and members, against each other and against the
   top struct's fields. */
static int cx_distinct_tables(const cx_model* m) {
    size_t i;
    size_t j;

    for (i = 0; i < m->tables_n; ++i) {
        const cx_table* a = &m->tables[i];
        for (j = i + 1; j < m->tables_n; ++j) {
            const cx_table* b = &m->tables[j];
            if (strcmp(a->member, b->member) == 0) {
                return cx_clash(m, a->first_key, b->first_key, a->member);
            }
            if (strcmp(a->type, b->type) == 0) {
                return cx_clash(m, a->first_key, b->first_key, a->type);
            }
        }
        for (j = 0; j < m->n; ++j) {
            const cx_key* k = &m->keys[j];
            if (k->table == CX_TOP && k->skip == 0 &&
                strcmp(k->field, a->member) == 0) {
                return cx_clash(m, k->def->key, a->first_key, a->member);
            }
        }
    }
    return 0;
}

/* Two keys of one identifier are refused, whole or as fields of one
   struct. */
static int cx_distinct(const cx_model* m) {
    size_t i;
    size_t j;

    for (i = 0; i < m->n; ++i) {
        const cx_key* a = &m->keys[i];
        for (j = i + 1; j < m->n && a->skip == 0; ++j) {
            const cx_key* b = &m->keys[j];
            if (b->skip != 0) {
                continue;
            }
            if (strcmp(a->full, b->full) == 0) {
                return cx_clash(m, a->def->key, b->def->key, a->full);
            }
            if (a->table == b->table && strcmp(a->field, b->field) == 0) {
                return cx_clash(m, a->def->key, b->def->key, a->field);
            }
        }
    }
    return cx_distinct_tables(m);
}

/* A key's default, or its type's zero, as a C++ expression. */
static void cx_default(FILE* out, const cf_def* d) {
    const cf_value* v = &d->default_value;

    switch (d->type) {
    case CF_TYPE_BOOL:
        fputs((d->has_default && v->u.b) ? "true" : "false", out);
        break;
    case CF_TYPE_INT:
        if (d->has_default) {
            c_i64(out, v->u.i);
        } else {
            fputs("(cf_i64)0", out);
        }
        break;
    case CF_TYPE_TEXT:
        if (d->has_default) {
            c_string(out, v->u.t.p, v->u.t.n);
        } else {
            fputs("std::string()", out);
        }
        break;
    case CF_TYPE_REAL:
#if CF_WITH_REAL
        if (d->has_default) {
            fprintf(out, "%.17g", v->u.r);
            break;
        }
#endif
        fputs("0.0", out);
        break;
    case CF_TYPE_NULL:
    default:
        break;
    }
}

static void cx_upper(FILE* out, const char* name) {
    for (; *name != '\0'; ++name) {
        fputc((*name >= 'a' && *name <= 'z') ? (*name - 'a' + 'A') : *name, out);
    }
}

/* A key's line in its struct, or what it says where its field would be. */
static void cx_member(FILE* out, const cx_key* k) {
    if (k->skip == 1) {
        const char* key = k->def->key;
        fprintf(out,
                "    /* %s: a list; its elements are %s.0, %s.1, ... and %s.count */\n",
                k->rest, key, key, key);
    } else if (k->skip == 2) {
        fprintf(out, "    /* %s: a type C++ does not read */\n", k->rest);
    } else {
        fprintf(out, "    %s %s;\n", cx_type(k->def->type), k->field);
    }
}

static void cx_keys(FILE* out, const cx_model* m) {
    size_t i;

    fputs("/* Each key, to read with cf::Config::get; its default is the one a\n"
          "   configuration without the key reads as. */\n"
          "namespace keys {\n",
          out);
    for (i = 0; i < m->n; ++i) {
        const cx_key* k = &m->keys[i];
        if (k->skip == 0) {
            fprintf(out, "const cf::Key<%s> %s(", cx_type(k->def->type), k->full);
            c_text(out, k->def->key);
            fputs(", ", out);
            cx_default(out, k->def);
            fputs(");\n", out);
        }
    }
    fputs("} /* namespace keys */\n\n", out);
}

static void cx_structs(FILE* out, const cx_model* m) {
    size_t t;
    size_t i;

    for (t = 0; t < m->tables_n; ++t) {
        fprintf(out, "struct %s {\n", m->tables[t].type);
        for (i = 0; i < m->n; ++i) {
            if (m->keys[i].table == t) {
                cx_member(out, &m->keys[i]);
            }
        }
        fputs("};\n\n", out);
    }
    fputs("struct Settings {\n", out);
    for (i = 0; i < m->n; ++i) {
        const cx_key* k = &m->keys[i];
        if (k->table == CX_TOP) {
            cx_member(out, k);
        } else if (m->tables[k->table].first == i) {
            fprintf(out, "    %s %s;\n", m->tables[k->table].type,
                    m->tables[k->table].member);
        }
    }
    fputs("};\n\n", out);
}

static void cx_bind(FILE* out, const cx_model* m) {
    size_t  i;
    cf_bool any = CF_FALSE;

    fputs("/* The fields of Settings, each from its key:\n"
          "       cf::Binding<N::Settings> b;\n"
          "       N::bind(b);\n"
          "       N::Settings s = b.read(cfg); */\n"
          "inline void bind(cf::Binding<Settings>& b) {\n",
          out);
    for (i = 0; i < m->n; ++i) {
        const cx_key* k = &m->keys[i];
        if (k->skip != 0) {
            continue;
        }
        any = CF_TRUE;
        fputs("    b.field(", out);
        c_text(out, k->def->key);
        if (k->table == CX_TOP) {
            fprintf(out, ", &Settings::%s, ", k->field);
        } else {
            fprintf(out, ", &Settings::%s, &%s::%s, ", m->tables[k->table].member,
                    m->tables[k->table].type, k->field);
        }
        cx_default(out, k->def);
        fputs(");\n", out);
    }
    if (!any) {
        fputs("    (void)b;\n", out);
    }
    fputs("}\n\n", out);
}

static int cx_emit(FILE* out, const cx_model* m, const cf_defset* ds,
                   const char* schema, const char* name) {
    int rc;

    fputs("/* The contract of ", out);
    c_comment_name(out, schema);
    fputs(", in C++: written by `configforge\n"
          "   gen --schema --lang cpp`. Regenerate it from the schema rather than\n"
          "   editing it. */\n\n#ifndef CF_GEN_",
          out);
    cx_upper(out, name);
    fputs("_HPP\n#define CF_GEN_", out);
    cx_upper(out, name);
    fputs("_HPP\n\n#include <configforge/configforge.hpp>\n\n#include <string>\n\n",
          out);
    c_lines(out, gen_checks);
    if (gen_has_reals(ds)) {
        gen_need_reals(out);
    }
    fprintf(out,
            "namespace %s {\n\n"
            "/* Every definition the schema makes, descriptions, enumerations and\n"
            "   lengths with the rest, through the library's own calls. */\n"
            "inline bool define(cf_builder* b) {\n",
            name);
    rc = gen_c_body(out, ds, CF_TRUE);
    if (rc != 0) {
        return rc;
    }
    fputs("}\n\n"
          "/* cf::Builder().define(N::Contract()): a refusal is the builder's,\n"
          "   and create() reports it. */\n"
          "struct Contract {\n"
          "    void defineInto(cf_builder* b) const {\n"
          "        (void)define(b);\n"
          "    }\n"
          "};\n\n",
          out);
    cx_keys(out, m);
    cx_structs(out, m);
    cx_bind(out, m);
    fprintf(out, "} /* namespace %s */\n\n", name);
    c_lines(out, gen_undefs);
    fputs("\n#endif\n", out);
    return 0;
}

/* The model of a contract's keys and the structs they make, `top` being the
   top struct's name; 2 when it cannot be made or two keys make one name.
   cx_finish frees it either way. */
static int cx_start(cx_model* m, const cf_defset* ds, const gen_lang* lang,
                    const char* top) {
    size_t room;

    m->lang     = lang;
    m->top      = top;
    m->n        = cf_defset_count(ds);
    m->tables_n = 0;
    room        = (m->n > 0) ? m->n : 1;
    m->keys     = (cx_key*)calloc(room, sizeof(cx_key));
    m->tables   = (cx_table*)calloc(room, sizeof(cx_table));
    if (m->keys == NULL || m->tables == NULL) {
        fputs("gen: cannot start\n", stderr);
        return 2;
    }
    cx_shape(m, ds);
    return cx_distinct(m);
}

static void cx_finish(cx_model* m) {
    free(m->keys);
    free(m->tables);
}

/* The contract as a C++ header. */
static int gen_cxx_file(FILE* out, const cf_defset* ds, const char* schema,
                        const char* name, const gen_lang* self) {
    cx_model m;
    int      rc = cx_start(&m, ds, self, "Settings");

    if (rc == 0) {
        rc = cx_emit(out, &m, ds, schema, name);
    }
    cx_finish(&m);
    return rc;
}

/* ------------------------------------------------------------------ *
 * Rust: one module
 *
 * The schema itself, embedded: the library reads it as it would read the
 * file, so every rule the schema holds is the library's (D-95). What the
 * file adds is read only: a Key for each key, and a settings! struct.
 * ------------------------------------------------------------------ */

/* Rust's reserved words, and the names the module makes itself; sorted. */
static const char* const gen_rs_reserved[] = {
    "SCHEMA", "Self",    "abstract", "as",       "async",    "await",    "become",
    "box",    "break",   "const",    "continue", "contract", "crate",    "do",
    "dyn",    "else",    "enum",     "extern",   "false",    "final",    "fn",
    "for",    "gen",     "if",       "impl",     "in",       "let",      "loop",
    "macro",  "match",   "mod",      "move",     "mut",      "override", "priv",
    "pub",    "ref",     "return",   "self",     "static",   "struct",   "super",
    "trait",  "true",    "try",      "type",     "typeof",   "unsafe",   "unsized",
    "use",    "virtual", "where",    "while",    "yield"};

/* A Rust type name, as `--name` must be: an ASCII letter first, then
   letters, digits and '_', and not a word the language reserves or the
   module makes. */
static cf_bool rs_identifier(const char* s) {
    size_t i;

    if (!cf_is_alpha((unsigned char)s[0])) {
        return CF_FALSE;
    }
    for (i = 0; s[i] != '\0'; ++i) {
        if (!cf_is_alnum((unsigned char)s[i]) && s[i] != '_') {
            return CF_FALSE;
        }
    }
    return bsearch(s, (const void*)gen_rs_reserved,
                   sizeof gen_rs_reserved / sizeof gen_rs_reserved[0],
                   sizeof gen_rs_reserved[0], gen_word_cmp) == NULL;
}

/* Text as a Rust string literal: its bytes as they are, UTF-8 included,
   but '\\' and '"' escaped and any other control byte as \u{..}. `lines`
   keeps a schema's lines as lines (CR LF is LF; a lone CR is \r), which a
   literal may span; without it a line break is \n, for a value that stays
   on its line. */
static void rs_string(FILE* out, const char* p, size_t n, cf_bool lines) {
    size_t i;

    fputc('"', out);
    for (i = 0; i < n; ++i) {
        unsigned char ch = (unsigned char)p[i];
        if (ch == '\\' || ch == '"') {
            fputc('\\', out);
            fputc((int)ch, out);
        } else if (lines && ch == '\r' && i + 1 < n && p[i + 1] == '\n') {
            continue;
        } else if (ch == '\r') {
            fputs("\\r", out);
        } else if (ch == '\n') {
            fputs(lines ? "\n" : "\\n", out);
        } else if (ch < 0x20u && ch != '\t') {
            fputc('\\', out); /* "\\u{" read as a character name by old compilers */
            fprintf(out, "u{%X}", (unsigned)ch);
        } else {
            fputc((int)ch, out);
        }
    }
    fputc('"', out);
}

/* A cf_i64 in decimal, for a language whose integers are wide enough
   (Rust's `i64` writes its extremes as names, in rs_i64). */
static void gen_decimal(FILE* out, cf_i64 v) {
    char   digits[24];
    size_t n = 0;
    cf_u64 mag;

    mag = (v < 0) ? (cf_u64)0 - (cf_u64)v : (cf_u64)v;
    do {
        digits[n] = (char)('0' + (int)(mag % 10u));
        ++n;
        mag /= 10u;
    } while (mag > 0u && n < sizeof digits);
    if (v < 0) {
        fputc('-', out);
    }
    for (; n > 0u; --n) {
        fputc(digits[n - 1u], out);
    }
}

/* A cf_i64 as a Rust i64 expression. */
static void rs_i64(FILE* out, cf_i64 v) {
    if (v == CF_I64_MIN) {
        fputs("i64::MIN", out);
    } else if (v == CF_I64_MAX) {
        fputs("i64::MAX", out);
    } else {
        gen_decimal(out, v);
    }
}

static const char* rs_type(cf_type t) {
    switch (t) {
    case CF_TYPE_BOOL:
        return "bool";
    case CF_TYPE_INT:
        return "i64";
    case CF_TYPE_TEXT:
        return "String";
    case CF_TYPE_REAL:
    case CF_TYPE_NULL:
    default:
        return NULL;
    }
}

/* A key's default, or its type's zero, as a Rust expression. */
static void rs_default(FILE* out, const cf_def* d) {
    const cf_value* v = &d->default_value;

    switch (d->type) {
    case CF_TYPE_BOOL:
        fputs((d->has_default && v->u.b) ? "true" : "false", out);
        break;
    case CF_TYPE_INT:
        if (d->has_default) {
            rs_i64(out, v->u.i);
        } else {
            fputc('0', out);
        }
        break;
    case CF_TYPE_TEXT:
        if (d->has_default) {
            rs_string(out, v->u.t.p, v->u.t.n, CF_FALSE);
        } else {
            fputs("\"\"", out);
        }
        break;
    case CF_TYPE_REAL:
    case CF_TYPE_NULL:
    default:
        break;
    }
}

/* What the writer knows of each key: a field of the struct and a constant
   (skip 0), a list (1), or a type Rust does not read (2). */
typedef struct rs_key {
    const cf_def* def;
    int           skip;
    char          field[GEN_NAME_MAX];
    char          constant[GEN_NAME_MAX];
} rs_key;

static void rs_shape(rs_key* keys, size_t n, const cf_defset* ds,
                     const gen_lang* lang) {
    size_t i;
    size_t j;

    for (i = 0; i < n; ++i) {
        rs_key* k = &keys[i];

        k->def = cf_defset_at(ds, i);
        if (k->def->list_mode != CF_LIST_NONE) {
            k->skip = 1;
        } else {
            k->skip = (rs_type(k->def->type) == NULL) ? 2 : 0;
        }
        gen_ident(k->field, k->def->key, strlen(k->def->key), lang);
        for (j = 0; k->field[j] != '\0'; ++j) {
            char ch = k->field[j];
            /* Not a conditional expression: its char operands become int. */
            k->constant[j] = ch;
            if (ch >= 'a' && ch <= 'z') {
                k->constant[j] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"[ch - 'a'];
            }
        }
        k->constant[j] = '\0';
        if (gen_reserved(lang, k->constant)) {
            gen_append(k->constant, "_");
        }
    }
}

/* Two keys of one identifier are refused: a field and a constant are each
   one thing. */
static int rs_distinct(const rs_key* keys, size_t n, const gen_lang* lang) {
    size_t i;
    size_t j;

    for (i = 0; i < n; ++i) {
        for (j = i + 1; j < n && keys[i].skip == 0; ++j) {
            if (keys[j].skip == 0 &&
                (strcmp(keys[i].field, keys[j].field) == 0 ||
                 strcmp(keys[i].constant, keys[j].constant) == 0)) {
                fprintf(stderr, "gen: '%s' and '%s' are both '%s' in %s; rename one\n",
                        keys[i].def->key, keys[j].def->key, keys[i].field, lang->name);
                return 2;
            }
        }
    }
    return 0;
}

/* The schema's bytes, in memory the caller frees; NULL when they cannot
   be read. */
static char* gen_slurp(const char* path, size_t* n) {
    FILE*  f = fopen(path, "rb");
    char*  buf;
    size_t cap = 4096;
    size_t len = 0;

    if (f == NULL) {
        return NULL;
    }
    buf = (char*)malloc(cap);
    while (buf != NULL) {
        char* more;

        len += fread(buf + len, 1, cap - len, f);
        if (len < cap) {
            break;
        }
        cap *= 2;
        more = (char*)realloc(buf, cap);
        if (more == NULL) {
            free(buf);
        }
        buf = more;
    }
    (void)fclose(f);
    *n = len;
    return buf;
}

/* The schema's file name, without the directories, which are the
   machine's. */
static const char* gen_base(const char* path) {
    const char* base = path;
    const char* p;

    for (p = path; *p != '\0'; ++p) {
        if (*p == '/' || *p == '\\') {
            base = p + 1;
        }
    }
    return base;
}

static void rs_keys(FILE* out, const rs_key* keys, size_t n) {
    size_t i;

    for (i = 0; i < n; ++i) {
        const rs_key* k = &keys[i];
        if (k->skip == 1) {
            fprintf(out,
                    "// %s: a list; its elements are %s.0, %s.1, ... and %s.count\n",
                    k->def->key, k->def->key, k->def->key, k->def->key);
        } else if (k->skip == 2) {
            fprintf(out, "// %s: %s\n", k->def->key,
                    (k->def->type == CF_TYPE_REAL)
                        ? "a real, which the Rust binding does not read yet"
                        : "a type Rust does not read");
        } else {
            fprintf(out, "pub const %s: Key<%s> = Key::new(", k->constant,
                    rs_type(k->def->type));
            c_text(out, k->def->key);
            fputs(", ", out);
            rs_default(out, k->def);
            fputs(");\n", out);
        }
    }
}

static void rs_struct(FILE* out, const rs_key* keys, size_t n, const char* name) {
    size_t i;

    fprintf(out, "configforge::settings! {\n    pub struct %s {\n", name);
    for (i = 0; i < n; ++i) {
        const rs_key* k = &keys[i];
        if (k->skip == 0) {
            fprintf(out, "        pub %s: %s = ", k->field, rs_type(k->def->type));
            c_text(out, k->def->key);
            fputs(" => ", out);
            rs_default(out, k->def);
            fputs(",\n", out);
        }
    }
    fputs("    }\n}\n", out);
}

static void rs_emit(FILE* out, const rs_key* keys, size_t n, const char* schema,
                    const char* name, const char* text, size_t text_n) {
    /* Plain comments: `//!` is an inner attribute, which include! refuses. */
    fputs("// The contract of ", out);
    c_comment_name(out, schema);
    fputs(", in Rust: written by `configforge\n"
          "// gen --schema --lang rust`. Regenerate it from the schema rather\n"
          "// than editing it.\n\n"
          "use configforge::{Builder, Key};\n\n"
          "/// The schema, the contract's one source: [`contract`] gives it to a\n"
          "/// builder, which reads it as it would read the file.\n"
          "pub const SCHEMA: &str = ",
          out);
    rs_string(out, text, text_n, CF_TRUE);
    fputs(";\n\n/// Every definition the schema makes, on `b`.\n"
          "pub fn contract(b: Builder) -> Builder {\n    b.schema(SCHEMA, ",
          out);
    rs_string(out, gen_base(schema), strlen(gen_base(schema)), CF_FALSE);
    fputs(")\n}\n\n", out);
    rs_keys(out, keys, n);
    fputc('\n', out);
    rs_struct(out, keys, n, name);
}

/* The contract as a Rust module. */
static int gen_rs_file(FILE* out, const cf_defset* ds, const char* schema,
                       const char* name, const gen_lang* self) {
    size_t  n      = cf_defset_count(ds);
    size_t  text_n = 0;
    rs_key* keys   = (rs_key*)calloc((n > 0) ? n : 1, sizeof(rs_key));
    char*   text   = gen_slurp(schema, &text_n);
    int     rc     = 2;

    if (keys == NULL || text == NULL) {
        fprintf(stderr, "gen: %s\n",
                (keys == NULL) ? "cannot start" : "cannot read the schema");
    } else {
        rs_shape(keys, n, ds, self);
        rc = rs_distinct(keys, n, self);
        if (rc == 0) {
            rs_emit(out, keys, n, schema, name, text, text_n);
        }
    }
    free(keys);
    free(text);
    return rc;
}

/* ------------------------------------------------------------------ *
 * Python: one module
 *
 * The schema itself, embedded, as Rust's: the library reads it. The module
 * adds dataclasses of cf.setting fields, which only read; the shape is the
 * C++ header's (a class for each top segment, deeper keys flattened).
 * ------------------------------------------------------------------ */

/* Python's reserved words, the names the module makes itself and the type
   names its annotations read (a field of one of those names would be that
   name in the class's own scope); sorted. */
static const char* const gen_py_reserved[] = {
    "False",    "None",      "SCHEMA", "True",   "and",      "as",    "assert",
    "async",    "await",     "bool",   "break",  "cf",       "class", "continue",
    "contract", "dataclass", "def",    "del",    "elif",     "else",  "except",
    "finally",  "float",     "for",    "from",   "global",   "if",    "import",
    "in",       "int",       "is",     "lambda", "nonlocal", "not",   "or",
    "pass",     "raise",     "return", "str",    "try",      "while", "with",
    "yield"};

/* A Python identifier, as `--name` must be: a letter or '_' first, then
   letters, digits and '_', ASCII, and not a word the language reserves or
   the module makes. */
static cf_bool py_identifier(const char* s) {
    size_t i;

    if (!cf_is_alpha((unsigned char)s[0]) && s[0] != '_') {
        return CF_FALSE;
    }
    for (i = 0; s[i] != '\0'; ++i) {
        if (!cf_is_alnum((unsigned char)s[i]) && s[i] != '_') {
            return CF_FALSE;
        }
    }
    return bsearch(s, (const void*)gen_py_reserved,
                   sizeof gen_py_reserved / sizeof gen_py_reserved[0],
                   sizeof gen_py_reserved[0], gen_word_cmp) == NULL;
}

/* Text as the inside of a Python string literal: its bytes as they are, UTF-8
   included (a module's source is UTF-8), but '\\' and '"' escaped, a line
   break, a tab and a carriage return as \n, \t and \r and any other control
   byte as \xNN. */
static void py_escape(FILE* out, const char* p, size_t n) {
    size_t i;

    for (i = 0; i < n; ++i) {
        unsigned char ch = (unsigned char)p[i];
        if (ch == '\\' || ch == '"') {
            fputc('\\', out);
            fputc((int)ch, out);
        } else if (ch == '\n') {
            fputs("\\n", out);
        } else if (ch == '\r') {
            fputs("\\r", out);
        } else if (ch == '\t') {
            fputs("\\t", out);
        } else if (ch < 0x20u || ch == 0x7Fu) {
            fputc('\\', out);
            fprintf(out, "x%02X", (unsigned)ch);
        } else {
            fputc((int)ch, out);
        }
    }
}

static void py_string(FILE* out, const char* p, size_t n) {
    fputc('"', out);
    py_escape(out, p, n);
    fputc('"', out);
}

/* The schema as one literal a line, joined by the parentheses; its line
   feeds stay in the literals, so the text is the file's bytes. */
static void py_schema(FILE* out, const char* text, size_t n) {
    size_t start = 0;
    size_t i;

    if (n == 0u) {
        fputs("SCHEMA = \"\"\n", out);
        return;
    }
    fputs("SCHEMA = (\n", out);
    for (i = 0; i < n; ++i) {
        if (text[i] == '\n' || i + 1u == n) {
            fputs("    ", out);
            py_string(out, text + start, i + 1u - start);
            fputc('\n', out);
            start = i + 1u;
        }
    }
    fputs(")\n", out);
}

static const char* py_type(cf_type t) {
    switch (t) {
    case CF_TYPE_BOOL:
        return "bool";
    case CF_TYPE_INT:
        return "int";
    case CF_TYPE_REAL:
        return "float";
    case CF_TYPE_TEXT:
        return "str";
    case CF_TYPE_NULL:
    default:
        return "object"; /* never written: such a key is a comment */
    }
}

#if CF_WITH_REAL
/* A real as a Python float literal: always with a '.' or an exponent. */
static void py_real(FILE* out, double v) {
    char buf[48];

    sprintf(buf, "%.17g", v);
    fputs(buf, out);
    if (strpbrk(buf, ".en") == NULL) {
        fputs(".0", out);
    }
}
#endif

/* A key's default, or its type's zero, as a Python expression. */
static void py_default(FILE* out, const cf_def* d) {
    const cf_value* v = &d->default_value;

    switch (d->type) {
    case CF_TYPE_BOOL:
        fputs((d->has_default && v->u.b) ? "True" : "False", out);
        break;
    case CF_TYPE_INT:
        if (d->has_default) {
            gen_decimal(out, v->u.i);
        } else {
            fputc('0', out);
        }
        break;
    case CF_TYPE_TEXT:
        if (d->has_default) {
            py_string(out, v->u.t.p, v->u.t.n);
        } else {
            fputs("\"\"", out);
        }
        break;
    case CF_TYPE_REAL:
#if CF_WITH_REAL
        if (d->has_default) {
            py_real(out, v->u.r);
            break;
        }
#endif
        fputs("0.0", out);
        break;
    case CF_TYPE_NULL:
    default:
        break;
    }
}

/* The `range=(lo, hi)` argument of a number that has one. */
static void py_range(FILE* out, const cf_def* d) {
    fputs("range=(", out);
    if (d->type == CF_TYPE_INT) {
        gen_decimal(out, d->imin);
        fputs(", ", out);
        gen_decimal(out, d->imax);
#if CF_WITH_REAL
    } else {
        fprintf(out, "%.17g, %.17g", d->rmin, d->rmax);
#endif
    }
    fputc(')', out);
}

/* The `flags=` argument: the definition's flags, joined by ` | `. */
static void py_flags(FILE* out, unsigned flags) {
    static const struct {
        unsigned    bit;
        const char* name;
    } names[] = {{CF_REQUIRED, "REQUIRED"},
                 {CF_SENSITIVE, "SENSITIVE"},
                 {CF_DEPRECATED, "DEPRECATED"},
                 {CF_TRANSIENT, "TRANSIENT"},
                 {CF_RESTART, "RESTART"}};
    size_t   i;
    unsigned rest = flags;
    cf_bool  any  = CF_FALSE;

    fputs("flags=", out);
    for (i = 0; i < sizeof names / sizeof names[0]; ++i) {
        if ((flags & names[i].bit) != 0) {
            fprintf(out, "%scf.flag.%s", any ? " | " : "", names[i].name);
            rest &= ~names[i].bit;
            any = CF_TRUE;
        }
    }
    if (rest != 0) {
        fprintf(out, "%s%u", any ? " | " : "", rest);
    }
}

/* The next argument of a cf.setting call: the first follows the default on
   its line, the others each get a line, under the first argument. */
static void py_arg(FILE* out, cf_bool* first, size_t col) {
    size_t i;

    if (*first) {
        fputs(", ", out);
        *first = CF_FALSE;
        return;
    }
    fputs(",\n", out);
    for (i = 0; i < col; ++i) {
        fputc(' ', out);
    }
}

/* A key's line in its class, or what it says where its field would be. */
static void py_member(FILE* out, const cx_key* k) {
    const cf_def* d     = k->def;
    cf_bool       first = CF_TRUE;
    size_t        col;

    if (k->skip == 1) {
        fprintf(out,
                "    # %s: a list; its elements are %s.0, %s.1, ... and %s.count\n",
                k->rest, d->key, d->key, d->key);
        return;
    }
    if (k->skip == 2) {
        fprintf(out, "    # %s: a type Python does not read\n", k->rest);
        return;
    }
    fprintf(out, "    %s: %s = cf.setting(", k->field, py_type(d->type));
    col = 4u + strlen(k->field) + 2u + strlen(py_type(d->type)) + 3u + 11u;
    py_string(out, d->key, strlen(d->key));
    fputs(", ", out);
    py_default(out, d);
    if (d->has_range && (d->type == CF_TYPE_INT || d->type == CF_TYPE_REAL)) {
        py_arg(out, &first, col);
        py_range(out, d);
    }
    if (d->flags != 0) {
        py_arg(out, &first, col);
        py_flags(out, d->flags);
    }
    if (d->description != NULL) {
        py_arg(out, &first, col);
        fputs("doc=", out);
        py_string(out, d->description, strlen(d->description));
    }
    fputs(")\n", out);
}

/* One dataclass: the table `t`, or the top class (CX_TOP), whose sections
   come where their first key does. */
static void py_class(FILE* out, const cx_model* m, size_t t, const char* name) {
    size_t  i;
    cf_bool any = CF_FALSE;

    fprintf(out, "\n\n@dataclass\nclass %s:\n", name);
    for (i = 0; i < m->n; ++i) {
        const cx_key* k = &m->keys[i];
        if (k->table == t) {
            py_member(out, k);
            if (k->skip == 0) {
                any = CF_TRUE;
            }
        } else if (t == CX_TOP && m->tables[k->table].first == i) {
            fprintf(out, "    %s: %s = cf.section(%s)\n", m->tables[k->table].member,
                    m->tables[k->table].type, m->tables[k->table].type);
            any = CF_TRUE;
        }
    }
    if (!any) {
        fputs("    pass\n", out);
    }
}

static void py_emit(FILE* out, const cx_model* m, const char* schema, const char* name,
                    const char* text, size_t text_n) {
    const char* base = gen_base(schema);
    size_t      t;

    fputs("\"\"\"The contract of ", out);
    py_escape(out, base, strlen(base));
    fprintf(out,
            ", in Python: written by `configforge\n"
            "gen --schema --lang python`. Regenerate it from the schema rather than\n"
            "editing it. Read it with ``cf.read(cfg, %s)``: ``contract`` defines the\n"
            "keys already, so the program must not also call ``cf.define``.\"\"\"\n\n"
            "from __future__ import annotations\n\n"
            "from dataclasses import dataclass\n\n"
            "import configforge as cf\n\n",
            name);
    py_schema(out, text, text_n);
    fputs("\n\ndef contract(b: cf.Builder) -> cf.Builder:\n"
          "    \"\"\"Every definition the schema makes, on ``b``.\"\"\"\n"
          "    return b.schema(SCHEMA, ",
          out);
    py_string(out, base, strlen(base));
    fputs(")\n", out);
    for (t = 0; t < m->tables_n; ++t) {
        py_class(out, m, t, m->tables[t].type);
    }
    py_class(out, m, CX_TOP, name);
}

/* The contract as a Python module. */
static int gen_py_file(FILE* out, const cf_defset* ds, const char* schema,
                       const char* name, const gen_lang* self) {
    cx_model m;
    size_t   text_n = 0;
    char*    text   = gen_slurp(schema, &text_n);
    int      rc     = (text == NULL) ? 2 : cx_start(&m, ds, self, name);

    if (text == NULL) {
        fputs("gen: cannot read the schema\n", stderr);
        return 2;
    }
    if (rc == 0) {
        py_emit(out, &m, schema, name, text, text_n);
    }
    cx_finish(&m);
    free(text);
    return rc;
}

static const gen_lang gen_langs[] = {
    {"c", ".c", "cf_contract", c_identifier, NULL, 0, gen_c_file},
    {"cpp", ".hpp", "cf_contract", c_identifier, gen_cxx_reserved,
     sizeof gen_cxx_reserved / sizeof gen_cxx_reserved[0], gen_cxx_file},
    {"rust", ".rs", "Settings", rs_identifier, gen_rs_reserved,
     sizeof gen_rs_reserved / sizeof gen_rs_reserved[0], gen_rs_file},
    {"python", ".py", "Settings", py_identifier, gen_py_reserved,
     sizeof gen_py_reserved / sizeof gen_py_reserved[0], gen_py_file},
};

#define GEN_LANGS (sizeof gen_langs / sizeof gen_langs[0])

/* The language `--lang` names; NULL, and what there is said, when none. */
static const gen_lang* gen_find(const char* name) {
    size_t i;

    for (i = 0; i < GEN_LANGS; ++i) {
        if (strcmp(gen_langs[i].name, name) == 0) {
            return &gen_langs[i];
        }
    }
    fprintf(stderr, "gen: no language '%s'; there are: ", name);
    for (i = 0; i < GEN_LANGS; ++i) {
        fprintf(stderr, "%s%s", (i > 0) ? ", " : "", gen_langs[i].name);
    }
    fputc('\n', stderr);
    return NULL;
}

int gen_code(int argc, char** argv) {
    const char*     schema = NULL;
    const char*     name   = NULL;
    const char*     path   = NULL;
    const char*     lang   = "c";
    const gen_lang* gl;
    cf_arena*       ar;
    cf_defset       ds;
    cf_error        err = CF_ERROR_INIT;
    FILE*           out = stdout;
    int             rc;
    int             i;

    for (i = 0; i + 1 < argc; i += 2) {
        if (strcmp(argv[i], "--schema") == 0) {
            schema = argv[i + 1];
        } else if (strcmp(argv[i], "--lang") == 0) {
            lang = argv[i + 1];
        } else if (strcmp(argv[i], "--name") == 0) {
            name = argv[i + 1];
        } else if (strcmp(argv[i], "--out") == 0) {
            path = argv[i + 1];
        } else {
            return contract_usage();
        }
    }
    if (i != argc || schema == NULL) {
        return contract_usage();
    }
    gl = gen_find(lang);
    if (gl == NULL) {
        return 2;
    }
    if (name == NULL) {
        name = gl->default_name;
    }
    if (!gl->identifier(name)) {
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
        out = fopen(path, "wb");
        if (out == NULL) {
            fprintf(stderr, "gen: cannot write %s\n", path);
            cf_arena_free(ar);
            return 2;
        }
    }
    rc = gl->write(out, &ds, schema, name, gl);
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
