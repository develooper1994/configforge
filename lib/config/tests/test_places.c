/*
 * test_places.c - refusal messages recorded before the A-8 reader changes.
 * C89.
 */

#include <configforge/cf_entry.h>
#include <configforge/cf_yamlparse.h>
#include <configforge/configforge.h>

#include <stdio.h>
#include <string.h>

#include "cf_test.h"

typedef struct place_case {
    const char* name;
    const char* input;
    const char* message;
} place_case;

static const place_case json_cases[] = {
    {"JSON LF", "{\"a\":\n1 x}", "t.json:2:3: expected ',' or '}'"},
    {"JSON CR", "{\"a\":\r1 x}", "t.json:1:9: expected ',' or '}'"},
    {"JSON CRLF", "{\r\n\"a\":]", "t.json:2:5: expected a value"},
    {"JSON blank lines", "\n \t\n[",
     "t.json:3:1: a configuration document must be an object"},
    {"JSON raw LF", "{\"a\":\"x\ny\"}",
     "t.json:1:8: a string may not contain a raw newline"},
    {"JSON trailing line", "{}\n\t x", "t.json:2:3: trailing text after the document"}};

static const place_case json5_cases[] = {
    {"JSON5 CR", "{\"a\":\r1 x}", "t.json5:2:3: expected ',' or '}'"},
    {"JSON5 block comment", "/* \n */ x",
     "t.json5:2:5: a configuration document must be an object"},
    {"JSON5 continuation", "{a:'one\\\r\ntwo' x}", "t.json5:2:6: expected ',' or '}'"},
    {"JSON5 line separator",
     "// c\xE2\x80\xA8"
     "x",
     "t.json5:2:1: a configuration document must be an object"},
    {"JSON5 unclosed comment", "/* a\r\nb\r", "t.json5:3:1: a comment is not closed"},
    {"JSON5 VT", "{a:\v1 x}", "t.json5:1:7: expected ',' or '}'"},
    {"JSON5 FF", "{a:\f1 x}", "t.json5:1:7: expected ',' or '}'"},
    {"JSON5 NBSP",
     "{a:\xC2\xA0"
     "1 x}",
     "t.json5:1:8: expected ',' or '}'"},
    {"JSON5 BOM as space",
     "{a:\xEF\xBB\xBF"
     "1 x}",
     "t.json5:1:9: expected ',' or '}'"}};

static const place_case toml_cases[] = {
    {"TOML CRLF line 3", "# first\r\na = 1\r\nb = ]", "t.toml:3:5: expected a value"},
    {"TOML lone CR", "a = 1\rb = 2", "t.toml:1:6: expected the end of the line"},
    {"TOML multiline string", "a = \"\"\"\r\nx\r\ny\"\"\" x",
     "t.toml:3:6: expected the end of the line"},
    {"TOML lone CR in comment", "# c\rx",
     "t.toml:1:4: a comment may not contain a control character"},
    {"TOML control in trailing comment", "a = 1 # c\001",
     "t.toml:1:10: a comment may not contain a control character"},
    {"TOML multiline array", "a = [1,\r\n # c\r\n ] x",
     "t.toml:3:4: expected the end of the line"}};

static const place_case yaml_cases[] = {
    {"YAML CRLF", "a: 1\r\nb: ]",
     "t.yaml:2:4: a value cannot start with this character"},
    {"YAML quoted fold", "a: \"one\r\n  two\" x",
     "t.yaml:2:8: unexpected text after the value"},
    {"YAML CR", "# c\ra: [1,\r  }", "t.yaml:3:3: expected ',' or ']'"},
    {"YAML separated comment", "a: [x #c\n ] x",
     "t.yaml:2:4: unexpected text after the value"},
    {"YAML adjacent hash", "a: [x#c\n ] x",
     "t.yaml:2:4: unexpected text after the value"},
    {"YAML blank lines", "\n\t# c\n a: \"\\q\"", "t.yaml:3:7: an unknown escape"}};

static const place_case ini_cases[] = {
    {"INI CRLF", "# c\r\n[section\r\n", "t.ini:2: section header is not closed"},
    {"INI CR only", "\r\r[section", "t.ini:1: section header is not closed"},
    {"INI empty key", "; c\n = value", "t.ini:2: empty key"},
    {"INI after header", "[section] x",
     "t.ini:1: trailing text after a section header"},
    {"INI missing separator", "\nkey", "t.ini:2: expected 'key = value'"},
    {"INI invalid key", "a b = value\r\n", "t.ini:1: 'a b' is not a valid key"}};

static const place_case properties_cases[] = {
    {"properties continuation",
     "a=one\\\n  \\"
     "u1",
     "t.properties:2: \\u needs four hexadecimal digits"},
    {"properties form feed", "\fBad=1", "t.properties:1: 'Bad' is not a valid key"},
    {"properties CRLF",
     "# c\r\na=\\"
     "u1",
     "t.properties:2: \\u needs four hexadecimal digits"},
    {"properties lone CR",
     "! c\ra=\\"
     "u1",
     "t.properties:2: \\u needs four hexadecimal digits"},
    {"properties invalid key", "A=one", "t.properties:1: 'A' is not a valid key"},
    {"properties surrogate",
     "a=\\"
     "uD800",
     "t.properties:1: a \\u escape is half a surrogate pair"}};

#if CF_WITH_DOTENV
static const place_case dotenv_cases[] = {
    {"dotenv line 3", "# c\nA=1\nbad", "t.dotenv:3: expected NAME=value"},
    {"dotenv CRLF", "A=1\r\n bad\r\n", "t.dotenv:2: expected NAME=value"},
    {"dotenv CR only", "\r\r bad", "t.dotenv:1: expected NAME=value"},
    {"dotenv blank lines", "\n\t\n\n bad", "t.dotenv:4: expected NAME=value"},
    {"dotenv empty name", "# c\n=value", "t.dotenv:2: '' is not a usable name"},
    {"dotenv invalid name", "A=1\n.=value", "t.dotenv:2: '.' is not a usable name"}};

static cf_bool entry_text(void* ctx, const char* key, size_t klen, const char* value,
                          size_t vlen, const char* at) {
    (void)ctx;
    (void)key;
    (void)klen;
    (void)value;
    (void)vlen;
    (void)at;
    return CF_TRUE;
}

static cf_bool decode_dotenv(const char* input, const char* file, cf_error* err) {
    cf_keymap     map;
    cf_entry_sink sink;

    memset(&sink, 0, sizeof sink);
    sink.text = entry_text;
    cf_keymap_init(&map, CF_KEYMAP_ENV, NULL);
    return cf_lex_dotenv(input, strlen(input), file, &map, &sink, err);
}
#endif

static cf_bool begin(void* ctx, const char* name, size_t n) {
    (void)ctx;
    (void)name;
    (void)n;
    return CF_TRUE;
}

static cf_bool scalar(void* ctx, const char* name, size_t n, const cf_value* value,
                      cf_bool textual, const cf_loc* at) {
    (void)ctx;
    (void)value;
    (void)textual;
    (void)at;
    return name == NULL ? CF_TRUE : cf_key_valid(name, n);
}

static cf_bool end(void* ctx) {
    (void)ctx;
    return CF_TRUE;
}

static char* reserve_text(void* ctx, size_t n) {
    static char text[1024];
    (void)ctx;
    return n <= sizeof text ? text : NULL;
}

static void* reserve_work(void* ctx, size_t n) {
    static union {
        void* align;
        char  bytes[1 << 16];
    } work;
    size_t* used = (size_t*)ctx;
    void*   p;

    n = (n + 15u) & ~(size_t)15u;
    if (n > sizeof work.bytes - *used) {
        return NULL;
    }
    p = work.bytes + *used;
    *used += n;
    return p;
}

static cf_bool decode_codec(const cf_codec* codec, const char* input, const char* file,
                            cf_error* err) {
    cf_blob     blob;
    cf_doc_sink sink;
    size_t      used = 0;

    cf_blob_mem(&blob, input, strlen(input), file);
    memset(&sink, 0, sizeof sink);
    sink.size         = sizeof sink;
    sink.ctx          = &used;
    sink.begin_table  = begin;
    sink.begin_array  = begin;
    sink.scalar       = scalar;
    sink.end          = end;
    sink.reserve_text = reserve_text;
    sink.reserve_work = reserve_work;
    return cf_codec_decode(codec, &blob, &sink, err);
}

static void cases(const char* format, const place_case* table, size_t count) {
    const cf_codec* codec = cf_codec_by_name(format);
    char            file[32];
    size_t          i;

    if (codec == NULL && strcmp(format, "dotenv") != 0) {
        return;
    }
    cf_fmt(file, sizeof file, "t.%s", format);
    for (i = 0; i < count; ++i) {
        cf_error err = CF_ERROR_INIT;
        cf_bool  ok;
        int      matches;

#if CF_WITH_DOTENV
        if (codec == NULL) {
            ok = decode_dotenv(table[i].input, file, &err);
        } else
#endif
        {
            ok = decode_codec(codec, table[i].input, file, &err);
        }
        matches = !ok && strcmp(err.message, table[i].message) == 0;
        if (!matches) {
            printf("%s: expected [%s], got [%s]%s\n", table[i].name, table[i].message,
                   err.message, ok ? " (accepted)" : "");
        }
        check(matches, table[i].name);
    }
}

#define CASES(format, table)                                                           \
    cases((format), (table), sizeof(table) / sizeof((table)[0]))

int main(void) {
    CASES("json", json_cases);
    CASES("json5", json5_cases);
    CASES("toml", toml_cases);
    if (!CF_YAML_LIBYAML && !CF_YAML_LIBFYAML) {
        CASES("yaml", yaml_cases);
    }
    CASES("ini", ini_cases);
    CASES("properties", properties_cases);
#if CF_WITH_DOTENV
    CASES("dotenv", dotenv_cases);
#endif
    return cf_test_done();
}
