/*
 * test_text_space.c - the shared blanks, newlines and refusal places. C89.
 */

#include <configforge/cf_text.h>

#include <string.h>

#include "cf_test.h"

#define LS "\xE2\x80\xA8"
#define PS "\xE2\x80\xA9"
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

static void breaks(void) {
    static const struct {
        const char* bytes;
        size_t      len;
        size_t      want[5];
    } cases[]                     = {{"\n", 1, {1, 1, 1, 1, 0}},
                                     {"\r\n", 2, {0, 2, 2, 2, 0}},
                                     {"\r", 1, {0, 1, 0, 1, 0}},
                                     {"\ra", 2, {0, 1, 0, 1, 0}},
                                     {LS, 3, {0, 3, 0, 0, 0}},
                                     {PS, 3, {0, 3, 0, 0, 0}},
                                     {"\xE2\x80\xAA", 3, {0, 0, 0, 0, 0}},
                                     {LS, 2, {0, 0, 0, 0, 0}},
                                     {"", 0, {0, 0, 0, 0, 0}}};
    static const char   line[]    = "a\r " LS "\r\n";
    static const size_t columns[] = {7, 1, 6, 1, 8};
    unsigned            rule;
    size_t              i;

    for (rule = CF_NL_LF; rule <= CF_NL_NONE; ++rule) {
        for (i = 0; i < COUNT(cases); ++i) {
            const char* p = cases[i].bytes;
            check(cf_text_break(rule, p, p + cases[i].len) == cases[i].want[rule],
                  "break length for the rule");
        }
        check(cf_text_break(rule, line + 1, line + 1) == 0, "no break at end");
        check(cf_text_eol(rule, line, line + sizeof line - 1) == line + columns[rule],
              "first break for the rule");
    }
}

static void blanks(void) {
    static const char   line[]       = " \t\r\f\r\n";
    static const size_t offsets[][4] = {
        {2, 3, 2, 2}, {2, 3, 2, 2}, {3, 3, 4, 3}, {4, 5, 4, 5}};
    static const struct {
        const char* bytes;
        size_t      start;
        size_t      length;
    } trim[] = {{"  a b \r", 2, 3}, {"", 0, 0}, {" \t\r", 3, 0}};
    unsigned set;
    size_t   i;

    for (set = CF_BLANK_SPTAB; set <= CF_BLANK_TOML; ++set) {
        for (i = 0; i < COUNT(offsets); ++i) {
            static const size_t start[] = {0, 2, 3, 4};
            check(cf_text_blanks(set, line + start[i], line + sizeof line - 1) ==
                      line + offsets[i][set],
                  "blank set ends at the right byte");
        }
    }
    for (i = 0; i < COUNT(trim); ++i) {
        const char* out;
        size_t      n = cf_text_trim(trim[i].bytes, strlen(trim[i].bytes), &out);
        check(n == trim[i].length && out == trim[i].bytes + trim[i].start,
              "trimmed range");
    }
}

static void track_breaks(void) {
    static const char   line[]  = "a\nb\r\nc\rd" LS "e" PS "f";
    static const size_t after[] = {2, 5, 7, 11, 15};
    const char*         end     = line + sizeof line - 1;
    const char*         p       = line;
    cf_text_track       tr;
    size_t              i;

    cf_text_track_init(&tr, p);
    check(tr.line == 1 && tr.line_start == p && cf_text_track_col(&tr, p) == 1,
          "initial place");
    for (i = 0; i < COUNT(after); ++i) {
        p = cf_text_eol(CF_NL_JSON5, p, end);
        check(cf_text_track_col(&tr, p) == 2, "column at a break");
        p += cf_text_break(CF_NL_JSON5, p, end);
        cf_text_track_line(&tr, p);
        check(p == line + after[i] && tr.line == i + 2 &&
                  cf_text_track_col(&tr, p) == 1 && cf_text_track_col(&tr, p + 1) == 2,
              "place after a break");
    }
}

/* The alphabets and maximum lengths of test_reader_corpus.c (A-7). */
static void track_corpus(void) {
    static const struct {
        unsigned    length;
        const char* symbols[20];
    } runs[] = {
        {4,
         {"{", "}", "\"", ":", ",", "a", "0", " ", "\t", "\r", "\n", "/", "*", "\\",
          "u", LS}},
        {4,
         {"'", "x", "1", "v", "\\", "u", "0", "D", "8", "\r", "\n", "\"", "{", "}", ":",
          "a"}},
        {4,
         {"[", "]", "=", "a", "1", "\"", "'", "#", " ", "\t", "\r", "\n", "\\", ",",
          "{", "}", "."}},
        {4,
         {"\"", "\\", "u", "x", "U", "0", "D", "8", "e", "b", "\n", "\r", "=", "a", " ",
          "'"}},
        {4,
         {"a", ":", " ", "\t", "-", "#", "\n", "\r", "\"", "'", "\\", "|", "[", "]",
          "{", "}", ",", "!", "%"}},
        {4,
         {"\"", "\\", "x", "u", "U", "0", "D", "8", "N", "L", "\n", "\r", "a", ":", " ",
          "/"}},
        {5, {"[", "]", "=", "a", " ", "\t", "\r", "\n", "#", ";", "\""}},
        {4, {"a", "=", ":", " ", "\t", "\f", "\r", "\n", "#", "!", "\\", "u", "0"}},
        {4, {"\\", "u", "D", "8", "0", "4", "1", "t", "=", "a", "\n", "\r"}},
        {5, {"A", "=", " ", "\t", "\r", "\n", "#", "\"", "'"}}};
    size_t r;

    for (r = 0; r < COUNT(runs); ++r) {
        size_t   base = 0, count = 1;
        unsigned len;

        while (runs[r].symbols[base] != NULL) {
            ++base;
        }
        for (len = 0; len <= runs[r].length; ++len, count *= base) {
            size_t i;
            for (i = 0; i < count; ++i) {
                char          doc[16];
                size_t        n = 0, rest = i;
                unsigned      j;
                cf_text_track tr;

                for (j = 0; j < len; ++j) {
                    const char* symbol = runs[r].symbols[rest % base];
                    size_t      bytes  = strlen(symbol);
                    memcpy(doc + n, symbol, bytes);
                    n += bytes;
                    rest /= base;
                }
                cf_text_track_init(&tr, doc);
                for (j = 0; j <= n; ++j) {
                    cf_u32 line, col;

                    cf_text_place(doc, j, 1u, &line, &col);
                    if (tr.line != line || cf_text_track_col(&tr, doc + j) != col) {
                        printf("LF tracker disagrees with cf_text_place at offset %u "
                               "(run %u, document %u, length %u)\n",
                               j, (unsigned)r, (unsigned)i, len);
                        check(0, "corpus location agrees");
                        return;
                    }
                    if (cf_text_break(CF_NL_LF, doc + j, doc + n) != 0) {
                        cf_text_track_line(&tr, doc + j + 1);
                    }
                }
            }
        }
    }
    check(1, "corpus locations agree");
}

static void skips(void) {
    static const cf_text_syntax syntaxes[] = {
        {CF_BLANK_INI, CF_NL_LF, 0},
        {CF_BLANK_SPTAB, CF_NL_JSON5, CF_SYN_SLASH},
        {CF_BLANK_TOML, CF_NL_TOML, CF_SYN_HASH | CF_SYN_TOML_COMMENT},
        {CF_BLANK_SPTAB, CF_NL_ANY, CF_SYN_HASH | CF_SYN_HASH_BLANK}};
    static const struct {
        unsigned    syntax;
        const char* bytes;
        size_t      start, stop;
        unsigned    line, col;
        int         result;
    } cases[] = {{0, " \t\r\n //", 0, 5, 2, 2, CF_SKIP_OK},
                 {1, "/* a \n b */ x", 0, 12, 2, 7, CF_SKIP_OK},
                 {1, "/* a \r b", 0, 8, 2, 3, CF_SKIP_UNCLOSED},
                 {1, "// a " LS "b", 0, 8, 2, 1, CF_SKIP_OK},
                 {1, " \v\f\xC2\xA0\xEF\xBB\xBF" LS "x", 0, 11, 2, 1, CF_SKIP_OK},
                 {2, "# c\001", 0, 3, 1, 4, CF_SKIP_CONTROL},
                 {2, "# c\xE2\x80", 0, 3, 1, 4, CF_SKIP_UTF8},
                 {2, "# c\r\nx", 0, 5, 2, 1, CF_SKIP_OK},
                 {3, "# c\n", 0, 4, 2, 1, CF_SKIP_OK},
                 {3, "a #c", 1, 4, 1, 5, CF_SKIP_OK},
                 {3, "a#c", 1, 1, 1, 2, CF_SKIP_OK}};
    static const char comment[] = "# c\r\nx";
    size_t            i;
    const char*       p   = comment;
    const char*       bad = NULL;

    for (i = 0; i < COUNT(cases); ++i) {
        const char*   bytes   = cases[i].bytes;
        const char*   cursor  = bytes + cases[i].start;
        const char*   refused = NULL;
        cf_text_track tr;
        int           result;

        cf_text_track_init(&tr, bytes);
        result = cf_text_skip(&syntaxes[cases[i].syntax], &tr, &cursor,
                              bytes + strlen(bytes), &refused);
        check(result == cases[i].result && cursor == bytes + cases[i].stop &&
                  tr.line == cases[i].line &&
                  cf_text_track_col(&tr, cursor) == cases[i].col,
              "skip result and place");
        check(refused == (result == CF_SKIP_OK ? NULL : cursor), "refused byte");
    }
    check(cf_text_comment(&syntaxes[2], &p, comment + sizeof comment - 1, &bad) ==
                  CF_SKIP_OK &&
              p == comment + 4,
          "TOML comment leaves LF");
    p = comment;
    check(cf_text_comment(&syntaxes[3], &p, comment + sizeof comment - 1, &bad) ==
                  CF_SKIP_OK &&
              p == comment + 3,
          "YAML comment leaves CRLF");
}

static void failures(void) {
    cf_error got = CF_ERROR_INIT;
    cf_error old = CF_ERROR_INIT;
    char     text[CF_MESSAGE_MAX + 12];

    check(!cf_text_fail(&got, CF_E_SOURCE, "t.json", 2, 3, "%s", "body"),
          "failure returns false");
    check(got.code == CF_E_SOURCE && strcmp(got.source, "t.json") == 0 &&
              got.key[0] == '\0' && strcmp(got.message, "t.json:2:3: body") == 0,
          "file, line and column");
    cf_text_fail(&got, CF_E_KEY, "t.ini", 4, 0, "'%S': %u", "abcd", (size_t)2, 9u);
    cf_error_set(&old, CF_E_KEY, NULL, "t.ini", "%s:%u: '%S': %u", "t.ini", 4u, "abcd",
                 (size_t)2, 9u);
    check(got.code == CF_E_KEY && strcmp(got.message, old.message) == 0,
          "file and line with a length-delimited string");
    memset(text, 'a', sizeof text);
    text[sizeof text - 1] = '\0';
    cf_text_fail(&got, CF_E_SOURCE, "t.toml", 5, 6, "%s!", text);
    cf_error_set(&old, CF_E_SOURCE, NULL, "t.toml", "%s:%u:%u: %s!", "t.toml", 5u, 6u,
                 text);
    check(strcmp(got.message, old.message) == 0, "message is cut at the same byte");
    check(!cf_text_fail(NULL, CF_E_SOURCE, "t.toml", 1, 1, "body"),
          "failure without error storage");
}

int main(void) {
    breaks();
    blanks();
    track_breaks();
    track_corpus();
    skips();
    failures();
    return cf_test_done();
}
