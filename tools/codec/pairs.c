/*
 * pairs.c - `configforge codec pairs`: what each format that has a writer
 * can hold, and what each ordered pair of them loses (D-109). It reads
 * cf_format_holds only, so its output is the same in every build; the
 * markdown form is the section of docs/Formats.md between its markers,
 * and a test holds the page to it. C89.
 */

#include "codec.h"

#include "configforge/cf_serialize.h"

#include <stdio.h>
#include <string.h>

static const char* const pr_formats[] = {"ini",  "json", "json5",
                                         "toml", "yaml", "cbor"};

#define PR_N (sizeof pr_formats / sizeof pr_formats[0])

typedef struct pr_row {
    unsigned    bit;
    const char* label;
} pr_row;

/* In bit order. */
static const pr_row pr_rows[] = {
    {CF_HOLDS_INT, "integers"},
    {CF_HOLDS_REAL, "reals"},
    {CF_HOLDS_BOOL, "booleans"},
    {CF_HOLDS_NULL, "null"},
    {CF_HOLDS_ARRAYS, "arrays of values"},
    {CF_HOLDS_EMPTY_ARRAY, "empty arrays"},
    {CF_HOLDS_TABLE_ARRAYS, "arrays of tables"},
    {CF_HOLDS_NESTED_ARRAYS, "arrays of arrays"},
    {CF_HOLDS_EMPTY_TABLE, "empty tables"},
    {CF_HOLDS_ANY_NAME, "names beyond letters, digits, _ and -"},
    {CF_HOLDS_EMPTY_NAME, "an empty name"},
    {CF_HOLDS_LINE_BREAK, "line breaks in text"},
    {CF_HOLDS_NUL, "NUL bytes in text"}};

#define PR_ROWS (sizeof pr_rows / sizeof pr_rows[0])

/* What a format that lacks `bit` does with it: a type is written as its
   text, anything else is left out. */
static const char* pr_missing(unsigned bit, unsigned holds) {
    if (bit == CF_HOLDS_INT || bit == CF_HOLDS_REAL || bit == CF_HOLDS_BOOL) {
        return "as text";
    }
    if (bit == CF_HOLDS_NULL && (holds & CF_REFUSES_NULL) == 0u) {
        return "as text";
    }
    return "left out";
}

static const char* pr_cell(unsigned bit, unsigned holds) {
    return ((holds & bit) != 0u) ? "yes" : pr_missing(bit, holds);
}

/* The bits of every format, or false (and the name) when the table has
   no row for one. */
static cf_bool pr_load(unsigned* holds) {
    size_t i;

    for (i = 0; i < PR_N; ++i) {
        if (!cf_format_holds(pr_formats[i], &holds[i])) {
            fprintf(stderr, "codec: no table for '%s'\n", pr_formats[i]);
            return CF_FALSE;
        }
    }
    return CF_TRUE;
}

/* What `from` has and `to` lacks, as `<label> (<cell>)` joined by ", ". */
static void pr_lost(unsigned from, unsigned to) {
    size_t  r;
    cf_bool any = CF_FALSE;

    for (r = 0; r < PR_ROWS; ++r) {
        if ((from & pr_rows[r].bit) != 0u && (to & pr_rows[r].bit) == 0u) {
            printf("%s%s (%s)", any ? ", " : "", pr_rows[r].label,
                   pr_missing(pr_rows[r].bit, to));
            any = CF_TRUE;
        }
    }
    if (!any) {
        fputs("nothing", stdout);
    }
}

int codec_pairs(int argc, char** argv) {
    unsigned holds[PR_N];
    cf_bool  markdown = CF_FALSE;
    size_t   r;
    size_t   a;
    size_t   b;
    int      i;

    for (i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "--format") == 0 && i + 1 < argc) {
            ++i;
            if (strcmp(argv[i], "markdown") == 0) {
                markdown = CF_TRUE;
            } else if (strcmp(argv[i], "text") != 0) {
                fprintf(stderr, "codec: pairs prints text or markdown, not '%s'\n",
                        argv[i]);
                return 2;
            }
        } else {
            fprintf(stderr, "codec: pairs [--format text|markdown]\n");
            return 2;
        }
    }
    if (!pr_load(holds)) {
        return 2;
    }
    if (markdown) {
        fputs("| What |", stdout);
        for (a = 0; a < PR_N; ++a) {
            printf(" %s |", pr_formats[a]);
        }
        fputs("\n| ---- |", stdout);
        for (a = 0; a < PR_N; ++a) {
            fputs(" ", stdout);
            for (b = 0; b < strlen(pr_formats[a]); ++b) {
                fputc('-', stdout);
            }
            fputs(" |", stdout);
        }
        fputc('\n', stdout);
    }
    for (r = 0; r < PR_ROWS; ++r) {
        fputs(markdown ? "| " : "", stdout);
        fputs(pr_rows[r].label, stdout);
        for (a = 0; a < PR_N; ++a) {
            fputs(markdown ? " | " : "\t", stdout);
            fputs(pr_cell(pr_rows[r].bit, holds[a]), stdout);
        }
        fputs(markdown ? " |\n" : "\n", stdout);
    }
    fputc('\n', stdout);
    if (markdown) {
        fputs("| From | To | Lost |\n| ---- | -- | ---- |\n", stdout);
    }
    for (a = 0; a < PR_N; ++a) {
        for (b = 0; b < PR_N; ++b) {
            if (a == b) {
                continue;
            }
            printf(markdown ? "| %s | %s | " : "%s\t%s\t", pr_formats[a],
                   pr_formats[b]);
            pr_lost(holds[a], holds[b]);
            fputs(markdown ? " |\n" : "\n", stdout);
        }
    }
    return 0;
}
