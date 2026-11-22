/*
 * json.c - the built-in JSON codec. C89.
 *
 * Written here rather than vendored, for a reason worth recording.
 * The popular C JSON libraries - cJSON, parson, jansson, yyjson - all
 * build a DOM and allocate as they go. This design needs a producer of
 * document events with exact locations and no allocation, and the one
 * streaming option, jsmn, is a tokeniser that does not unescape
 * strings. None of them fits, and the grammar is small enough that not
 * fitting is the whole argument.
 *
 * JSON is also not optional in practice: the provenance report and
 * Report Replay both speak it, so this codec is a Phase 1 item even
 * though it is listed among the formats.
 *
 * JSON5 (json5.org, 1.0.0) is a mode of the same scanner, not a second
 * reader: comments, trailing commas, identifier and single-quoted names
 * and strings, more escapes and line continuations, hexadecimal,
 * signed, Infinity and NaN numbers, and Unicode white space. A JSONC
 * file - JSON with comments and trailing commas - is JSON5, so
 * `.jsonc` chooses it too. Writing is JSON's, which is valid JSON5.
 */

#include "configforge/cf_serialize.h"
#include "configforge/cf_text.h"
#include "configforge/cf_build_options.h"
#include "configforge/cf_error.h"
#include "configforge/cf_numeric.h"
#include "configforge/cf_port.h"
#include "configforge/cf_value.h"

#include <string.h>
#include <stddef.h>

#if CF_WITH_JSON

/* ================================================================== *
 * Decoding
 * ================================================================== */

typedef struct js_scan {
    const char*        p;
    const char*        end;
    const char*        file;
    cf_text_track      tr;
    const cf_doc_sink* out;
    cf_error*          err;
    char               scratch[CF_KEY_MAX];
    int                depth;
    cf_bool            json5;
} js_scan;

#define JS_MAX_DEPTH CF_MAX_DEPTH

static const cf_text_syntax js_syntax  = {CF_BLANK_INI, CF_NL_LF, 0};
static const cf_text_syntax js5_syntax = {CF_BLANK_SPTAB, CF_NL_JSON5, CF_SYN_SLASH};

static void js_loc(const js_scan* s, const char* at, cf_loc* out) {
    out->file = s->file;
    out->line = s->tr.line;
    out->col  = cf_text_track_col(&s->tr, at);
}

static cf_bool js_fail(js_scan* s, const char* at, const char* what) {
    unsigned int col = cf_text_track_col(&s->tr, at);

    return cf_text_fail(s->err, CF_E_SOURCE, s->file, s->tr.line, col, "%s", what);
}

/* White space and, in JSON5, comments. */
static cf_bool js_space(js_scan* s) {
    const char*           bad;
    const cf_text_syntax* sy = s->json5 ? &js5_syntax : &js_syntax;

    if (cf_text_skip(sy, &s->tr, &s->p, s->end, &bad) == CF_SKIP_UNCLOSED) {
        return js_fail(s, bad, "a comment is not closed");
    }
    return CF_TRUE;
}

/* k hexadecimal digits at p, which has at least k bytes. */
static cf_bool js_hex(const char* p, int k, cf_u32* out) {
    cf_u32 v = 0;
    int    i;

    for (i = 0; i < k; ++i) {
        int d = cf_text_hex_value((unsigned char)p[i]);
        if (d < 0) {
            return CF_FALSE;
        }
        v = (v << 4) | (cf_u32)d;
    }
    *out = v;
    return CF_TRUE;
}

/* k bytes onto a string being decoded, or measured with a NULL
   destination, always leaving room for its terminator. */
static cf_bool js_put(js_scan* s, char* dst, size_t cap, size_t* n, const char* b,
                      size_t k) {
    if (k >= cap - *n) {
        return js_fail(s, s->p, "string is too long");
    }
    if (dst != NULL) {
        memcpy(dst + *n, b, k);
    }
    *n += k;
    return CF_TRUE;
}

static cf_bool js_put_cp(js_scan* s, char* dst, size_t cap, size_t* n, cf_u32 cp) {
    char enc[4];
    return js_put(s, dst, cap, n, enc, cf_text_utf8_encode(cp, enc));
}

/* The escapes of JSON and of JSON5, as data (D-102): the decoder is
   cf_text_esc_decode's, and the messages and the place of each refusal are
   this reader's. JSON5 adds \v, \0 not before a digit, \xHH, an escaped
   line break, and any other character standing for itself - but a
   digit, which is an octal escape in ECMAScript and none in JSON5. */
static const cf_text_esc_simple js_simple[]  = {{'"', 34}, {'\\', 92}, {'/', 47},
                                                {'b', 8},  {'f', 12},  {'n', 10},
                                                {'r', 13}, {'t', 9},   {0, 0}};
static const cf_text_esc_simple js5_simple[] = {
    {'"', 34}, {'\\', 92}, {'/', 47}, {'b', 8}, {'f', 12}, {'n', 10},
    {'r', 13}, {'t', 9},   {'v', 11}, {'0', 0}, {0, 0}};
static const cf_text_esc_family js_family = {
    js_simple, "", "u", "", CF_ESC_PAIRS, CF_ESC_AT_LETTER, CF_NL_NONE};
static const cf_text_esc_family js5_family = {
    js5_simple,       "x",        "u", "", CF_ESC_PAIRS | CF_ESC_NO_OCTAL,
    CF_ESC_AT_LETTER, CF_NL_JSON5};

/* With s->p on the letter after the backslash. */
static cf_bool js_escape(js_scan* s, char* dst, size_t cap, size_t* n) {
    cf_text_esc r;
    char        e;

    cf_text_esc_decode(s->json5 ? &js5_family : &js_family, s->p, s->end, &r);
    switch (r.kind) {
    case CF_ESC_CP:
        s->p += r.used;
        return js_put_cp(s, dst, cap, n, r.cp);
    case CF_ESC_BREAK:
        s->p += r.used;
        cf_text_track_line(&s->tr, s->p);
        return CF_TRUE;
    case CF_ESC_E_DIGITS:
        /* Split after the backslash, as toml.c explains. */
        if (r.letter == 'u') {
            return js_fail(s, s->p + r.at,
                           "\\"
                           "u needs four hexadecimal digits");
        }
        return js_fail(s, s->p + r.at,
                       "\\"
                       "x needs two hexadecimal digits");
    case CF_ESC_E_HIGH:
        return js_fail(s, s->p + r.at, "high surrogate without a low one");
    case CF_ESC_E_LOW:
        return js_fail(s, s->p + r.at, "unpaired low surrogate");
    case CF_ESC_E_NUL_DIGIT:
        return js_fail(s, s->p + r.at, "\\0 may not be followed by a digit");
    case CF_ESC_E_DIGIT:
        return js_fail(s, s->p + r.at, "a digit may not be escaped");
    default:
        break;
    }
    if (!s->json5) {
        return js_fail(s, s->p, "unknown escape");
    }
    /* Any other byte, the first of a longer character included: the
       rest of it follows as ordinary string bytes. */
    e = *s->p;
    ++s->p;
    return js_put(s, dst, cap, n, &e, 1);
}

/* Decode, or measure with a NULL destination, using the same grammar.
   JSON5 adds single quotes, and allows a raw control character but a
   line terminator, which only a line continuation may hold. */
static size_t js_string(js_scan* s, char* dst, size_t cap) {
    size_t n = 0;
    char   quote;

    if (s->p >= s->end || !(*s->p == '"' || (s->json5 && *s->p == '\''))) {
        js_fail(s, s->p, "expected a string");
        return (size_t)-1;
    }
    quote = *s->p++;
    while (s->p < s->end) {
        unsigned char c = (unsigned char)*s->p;

        if (c == (unsigned char)quote) {
            ++s->p;
            if (dst != NULL && n < cap) {
                dst[n] = '\0';
            }
            return n;
        }
        if (c == '\n' || (s->json5 && c == '\r')) {
            js_fail(s, s->p, "a string may not contain a raw newline");
            return (size_t)-1;
        }
        if (c < 0x20u && !s->json5) {
            js_fail(s, s->p,
                    "a string may not contain a raw control "
                    "character");
            return (size_t)-1;
        }
        if (c != '\\') {
            if (!js_put(s, dst, cap, &n, s->p, 1)) {
                return (size_t)-1;
            }
            ++s->p;
            continue;
        }
        ++s->p;
        if (s->p >= s->end) {
            js_fail(s, s->p, "escape at end of input");
            return (size_t)-1;
        }
        if (!js_escape(s, dst, cap, &n)) {
            return (size_t)-1;
        }
    }
    js_fail(s, s->p, "string is not closed");
    return (size_t)-1;
}

/* JSON5's identifier characters: ECMAScript 5.1's IdentifierName. ASCII
   exactly; past it, any character that is not white space, since the
   letter and mark categories would need Unicode's tables. */
static cf_bool js5_is_id(cf_u32 cp, cf_bool first) {
    if ((cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') || cp == '$' ||
        cp == '_') {
        return CF_TRUE;
    }
    if ((cp >= '0' && cp <= '9') || cp == 0x200Cu || cp == 0x200Du) {
        return first ? CF_FALSE : CF_TRUE;
    }
    return (cp >= 0x80u && !cf_text_is_ecma_space(cp)) ? CF_TRUE : CF_FALSE;
}

/* An unquoted member name, decoded as js_string decodes a string. */
static size_t js5_name(js_scan* s, char* dst, size_t cap) {
    size_t  n     = 0;
    cf_bool first = CF_TRUE;

    for (;;) {
        cf_u32      cp = 0;
        const char* at = s->p;

        if (s->p < s->end && *s->p == '\\') {
            if (s->end - s->p < 6 || s->p[1] != 'u' || !js_hex(s->p + 2, 4, &cp)) {
                js_fail(s, at,
                        "an escape in a name is \\"
                        "u and four hexadecimal digits");
                return (size_t)-1;
            }
            s->p += 6;
            if ((cp >= 0xD800u && cp <= 0xDFFFu) || !js5_is_id(cp, first)) {
                js_fail(s, at, "the escape is not a character a name may hold");
                return (size_t)-1;
            }
        } else {
            size_t k = cf_text_utf8_decode(s->p, s->end, &cp);
            if (k == 0 || !js5_is_id(cp, first)) {
                break;
            }
            s->p += k;
        }
        if (!js_put_cp(s, dst, cap, &n, cp)) {
            return (size_t)-1;
        }
        first = CF_FALSE;
    }
    if (first) {
        js_fail(s, s->p, "expected a member name");
        return (size_t)-1;
    }
    if (dst != NULL && n < cap) {
        dst[n] = '\0';
    }
    return n;
}

/* A decimal number's text, start to s->p, as a value. JSON has one
   numeric type; ConfigForge has two. The rule is mechanical rather than
   a guess: a fractional part or an exponent makes it real, and an
   integer that does not fit becomes real with the loss recorded rather
   than silently wrapping. */
static cf_bool js_number_value(js_scan* s, const char* start, cf_bool is_real,
                               cf_value* v) {
    size_t len = (size_t)(s->p - start);

    if (!is_real) {
        cf_i64 i;
        if (cf_text_to_i64(start, len, &i, NULL)) {
            cf_value_int(v, i);
            return CF_TRUE;
        }
        /* Too large for an integer: falls through to the real path. */
    }
#if CF_WITH_REAL
    {
        double d;
        if (!cf_text_to_f64(start, len, &d, NULL)) {
            return js_fail(s, start, "malformed number");
        }
        cf_value_real(v, d);
        return CF_TRUE;
    }
#else
    return js_fail(s, start, "this build has no floating point support");
#endif
}

static cf_bool js_number(js_scan* s, cf_value* v) {
    const char* start   = s->p;
    cf_bool     is_real = CF_FALSE;

    /* RFC 8259, section 6, and nothing looser - the JSON Test Suite
       found 01, 2. and .5 accepted:
           number = [ "-" ] int [ frac ] [ exp ]
           int    = "0" / digit1-9 *DIGIT
           frac   = "." 1*DIGIT
           exp    = ("e" / "E") [ "-" / "+" ] 1*DIGIT */
    if (s->p < s->end && *s->p == '-') {
        ++s->p;
    }
    if (s->p >= s->end || !cf_is_digit((unsigned char)*s->p)) {
        return js_fail(s, start,
                       (s->p == start) ? "expected a value" : "malformed number");
    }
    if (*s->p == '0') {
        ++s->p;
        if (s->p < s->end && cf_is_digit((unsigned char)*s->p)) {
            return js_fail(s, start, "malformed number: a leading zero");
        }
    } else {
        while (s->p < s->end && cf_is_digit((unsigned char)*s->p)) {
            ++s->p;
        }
    }
    if (s->p < s->end && *s->p == '.') {
        is_real = CF_TRUE;
        ++s->p;
        if (s->p >= s->end || !cf_is_digit((unsigned char)*s->p)) {
            return js_fail(s, start, "malformed number: no digit after the point");
        }
        while (s->p < s->end && cf_is_digit((unsigned char)*s->p)) {
            ++s->p;
        }
    }
    if (s->p < s->end && (*s->p == 'e' || *s->p == 'E')) {
        is_real = CF_TRUE;
        ++s->p;
        if (s->p < s->end && (*s->p == '+' || *s->p == '-')) {
            ++s->p;
        }
        if (s->p >= s->end || !cf_is_digit((unsigned char)*s->p)) {
            return js_fail(s, start, "malformed number: no digit in the exponent");
        }
        while (s->p < s->end && cf_is_digit((unsigned char)*s->p)) {
            ++s->p;
        }
    }
    return js_number_value(s, start, is_real, v);
}

/* A hexadecimal integer, s->p on its first digit: an integer where it
   fits, else real, as a decimal one is. */
static cf_bool js5_hex(js_scan* s, const char* start, cf_bool neg, cf_value* v) {
    const char* digits = s->p;
    cf_u64      mag    = 0;
    cf_bool     over   = CF_FALSE;

    while (s->p < s->end && cf_text_hex_value((unsigned char)*s->p) >= 0) {
        if (mag > (CF_U64_MAX >> 4)) {
            over = CF_TRUE;
        }
        mag = (mag << 4) | (cf_u64)cf_text_hex_value((unsigned char)*s->p);
        ++s->p;
    }
    if (s->p == digits) {
        return js_fail(s, start, "malformed number: no hexadecimal digit");
    }
    if (!over && mag <= (cf_u64)CF_I64_MAX) {
        cf_value_int(v, neg ? -(cf_i64)mag : (cf_i64)mag);
        return CF_TRUE;
    }
    if (!over && neg && mag == (cf_u64)CF_I64_MAX + 1u) {
        cf_value_int(v, CF_I64_MIN);
        return CF_TRUE;
    }
#if CF_WITH_REAL
    {
        double      d = 0.0;
        const char* p;
        for (p = digits; p < s->p; ++p) {
            int digit = cf_text_hex_value((unsigned char)*p);
            d         = d * 16.0 + (double)digit;
        }
        cf_value_real(v, neg ? -d : d);
        return CF_TRUE;
    }
#else
    return js_fail(s, start, "this build has no floating point support");
#endif
}

/* JSON5's numbers: ECMAScript 5.1's NumericLiteral with a sign - so
   +1, .5, 5., 0x1F, Infinity and NaN - and no octal. Infinity and NaN
   are their text, as TOML's inf and nan are: the library holds no value
   that is not finite, so a key defined as real refuses one by name. */
static cf_bool js5_number(js_scan* s, cf_value* v, cf_bool* textual) {
    const char* start   = s->p;
    cf_bool     neg     = CF_FALSE;
    cf_bool     is_real = CF_FALSE;
    cf_bool     any     = CF_FALSE;
    size_t      left;

    if (*s->p == '+' || *s->p == '-') {
        neg = (*s->p == '-') ? CF_TRUE : CF_FALSE;
        ++s->p;
    }
    left = (size_t)(s->end - s->p);
    if ((left >= 8 && memcmp(s->p, "Infinity", 8) == 0) ||
        (left >= 3 && memcmp(s->p, "NaN", 3) == 0)) {
        s->p += (*s->p == 'I') ? 8 : 3;
        *textual = CF_TRUE;
        cf_value_text(v, start, (size_t)(s->p - start));
        return CF_TRUE;
    }
    if (left >= 2 && s->p[0] == '0' && (s->p[1] == 'x' || s->p[1] == 'X')) {
        s->p += 2;
        return js5_hex(s, start, neg, v);
    }
    if (s->p < s->end && *s->p == '0') {
        ++s->p;
        any = CF_TRUE;
        if (s->p < s->end && cf_is_digit((unsigned char)*s->p)) {
            return js_fail(s, start, "malformed number: a leading zero");
        }
    } else {
        while (s->p < s->end && cf_is_digit((unsigned char)*s->p)) {
            ++s->p;
            any = CF_TRUE;
        }
    }
    if (s->p < s->end && *s->p == '.') {
        is_real = CF_TRUE;
        ++s->p;
        while (s->p < s->end && cf_is_digit((unsigned char)*s->p)) {
            ++s->p;
            any = CF_TRUE;
        }
    }
    if (!any) {
        return js_fail(s, start,
                       (s->p == start) ? "expected a value" : "malformed number");
    }
    if (s->p < s->end && (*s->p == 'e' || *s->p == 'E')) {
        is_real = CF_TRUE;
        ++s->p;
        if (s->p < s->end && (*s->p == '+' || *s->p == '-')) {
            ++s->p;
        }
        if (s->p >= s->end || !cf_is_digit((unsigned char)*s->p)) {
            return js_fail(s, start, "malformed number: no digit in the exponent");
        }
        while (s->p < s->end && cf_is_digit((unsigned char)*s->p)) {
            ++s->p;
        }
    }
    return js_number_value(s, start, is_real, v);
}

static cf_bool js_value(js_scan* s, const char* name, size_t nlen);

/* Before a member or an element: past white space and the comma that
   separates them, with *done set where the container closes. JSON5
   allows a comma before the close. */
static cf_bool js_between(js_scan* s, cf_bool* first, char close, const char* unclosed,
                          const char* expected, cf_bool* done) {
    *done = CF_FALSE;
    if (!js_space(s)) {
        return CF_FALSE;
    }
    if (s->p >= s->end) {
        return js_fail(s, s->p, unclosed);
    }
    if (*s->p == close) {
        ++s->p;
        *done = CF_TRUE;
        return CF_TRUE;
    }
    if (!*first) {
        if (*s->p != ',') {
            return js_fail(s, s->p, expected);
        }
        ++s->p;
        if (!js_space(s)) {
            return CF_FALSE;
        }
        if (s->p >= s->end) {
            return js_fail(s, s->p, unclosed);
        }
        if (*s->p == close) {
            if (!s->json5) {
                return js_fail(s, s->p, "trailing comma");
            }
            ++s->p;
            *done = CF_TRUE;
            return CF_TRUE;
        }
    }
    *first = CF_FALSE;
    return CF_TRUE;
}

static cf_bool js_object(js_scan* s, const char* name, size_t nlen) {
    cf_bool first = CF_TRUE;
    cf_bool done  = CF_FALSE;

    if (++s->depth > JS_MAX_DEPTH) {
        return js_fail(s, s->p, "nesting is too deep");
    }
    if (!s->out->begin_table(s->out->ctx, name, nlen)) {
        return js_fail(s, s->p, "key is not valid");
    }
    ++s->p; /* '{' */
    for (;;) {
        size_t klen;
        if (!js_between(s, &first, '}', "object is not closed", "expected ',' or '}'",
                        &done)) {
            return CF_FALSE;
        }
        if (done) {
            break;
        }
        klen = (s->json5 && *s->p != '"' && *s->p != '\'')
                   ? js5_name(s, s->scratch, sizeof s->scratch)
                   : js_string(s, s->scratch, sizeof s->scratch);
        if (klen == (size_t)-1 || !js_space(s)) {
            return CF_FALSE;
        }
        if (s->p >= s->end || *s->p != ':') {
            return js_fail(s, s->p, "expected ':'");
        }
        ++s->p;
        {
            /* The member name has to survive the nested parse, which
               reuses the scratch buffer for the next string. */
            char keep[CF_KEY_MAX];
            if (klen + 1 > sizeof keep) {
                return js_fail(s, s->p, "member name is too long");
            }
            memcpy(keep, s->scratch, klen + 1);
            if (!js_value(s, keep, klen)) {
                return CF_FALSE;
            }
        }
    }
    --s->depth;
    return s->out->end(s->out->ctx) ? CF_TRUE : js_fail(s, s->p, "sink refused");
}

static cf_bool js_array(js_scan* s, const char* name, size_t nlen) {
    cf_bool first = CF_TRUE;
    cf_bool done  = CF_FALSE;

    if (++s->depth > JS_MAX_DEPTH) {
        return js_fail(s, s->p, "nesting is too deep");
    }
    if (!s->out->begin_array(s->out->ctx, name, nlen)) {
        return js_fail(s, s->p, "key is not valid");
    }
    ++s->p; /* '[' */
    for (;;) {
        if (!js_between(s, &first, ']', "array is not closed", "expected ',' or ']'",
                        &done)) {
            return CF_FALSE;
        }
        if (done) {
            break;
        }
        if (!js_value(s, NULL, 0)) {
            return CF_FALSE;
        }
    }
    --s->depth;
    return s->out->end(s->out->ctx) ? CF_TRUE : js_fail(s, s->p, "sink refused");
}

/* A string value: measured, then decoded where it fits. */
static cf_bool js_text(js_scan* s, const char* name, size_t nlen, const cf_loc* at) {
    const char* start = s->p;
    char        local[256];
    char*       text = local;
    cf_value    v;
    size_t      n = js_string(s, NULL, (size_t)-1);

    if (n == (size_t)-1) {
        return CF_FALSE;
    }
    if (n >= sizeof local) {
        if (s->out->size <
                offsetof(cf_doc_sink, reserve_text) + sizeof s->out->reserve_text ||
            s->out->reserve_text == NULL) {
            return js_fail(s, start, "a long string needs sink text storage");
        }
        text = s->out->reserve_text(s->out->ctx, n + 1);
        if (text == NULL) {
            return cf_error_set(s->err, CF_E_MEMORY, NULL, s->file,
                                "no room to decode a JSON string");
        }
    }
    /* Measuring counted the lines a continuation holds: count again. */
    s->p             = start;
    s->tr.line       = at->line;
    s->tr.line_start = start - (at->col - 1u);
    if (js_string(s, text, n + 1) == (size_t)-1) {
        return CF_FALSE;
    }
    cf_value_text(&v, text, n);
    /* Typed: JSON said this is a string, so a definition that declared
       an integer has a mismatch rather than something to coerce. */
    return s->out->scalar(s->out->ctx, name, nlen, &v, CF_FALSE, at)
               ? CF_TRUE
               : js_fail(s, start, "key is not valid");
}

static cf_bool js_value(js_scan* s, const char* name, size_t nlen) {
    cf_value    v;
    cf_loc      at;
    const char* start;
    cf_bool     textual = CF_FALSE;

    if (!js_space(s)) {
        return CF_FALSE;
    }
    if (s->p >= s->end) {
        return js_fail(s, s->p, "expected a value");
    }
    start = s->p;
    js_loc(s, start, &at);

    switch (*s->p) {
    case '{':
        return js_object(s, name, nlen);
    case '[':
        return js_array(s, name, nlen);
    case '"':
        return js_text(s, name, nlen, &at);

    case 't':
        if ((size_t)(s->end - s->p) < 4 || memcmp(s->p, "true", 4) != 0) {
            return js_fail(s, start, "expected a value");
        }
        s->p += 4;
        cf_value_bool(&v, CF_TRUE);
        break;

    case 'f':
        if ((size_t)(s->end - s->p) < 5 || memcmp(s->p, "false", 5) != 0) {
            return js_fail(s, start, "expected a value");
        }
        s->p += 5;
        cf_value_bool(&v, CF_FALSE);
        break;

    case 'n':
        if ((size_t)(s->end - s->p) < 4 || memcmp(s->p, "null", 4) != 0) {
            return js_fail(s, start, "expected a value");
        }
        s->p += 4;
        cf_value_null(&v);
        break;

    default:
        if (s->json5 && *s->p == '\'') {
            return js_text(s, name, nlen, &at);
        }
        if (s->json5 ? !js5_number(s, &v, &textual) : !js_number(s, &v)) {
            return CF_FALSE;
        }
        break;
    }
    return s->out->scalar(s->out->ctx, name, nlen, &v, textual, &at)
               ? CF_TRUE
               : js_fail(s, start, "key is not valid");
}

static cf_bool js_decode(const cf_blob* in, const cf_doc_sink* out, cf_error* err,
                         cf_bool json5) {
    js_scan s;

    s.p    = in->p + cf_blob_skip_bom(in);
    s.end  = in->p + in->n;
    s.file = (in->name != NULL) ? in->name : json5 ? "<json5>" : "<json>";
    cf_text_track_init(&s.tr, s.p);
    s.out   = out;
    s.err   = err;
    s.depth = 0;
    s.json5 = json5;

    if (!js_space(&s)) {
        return CF_FALSE;
    }
    if (s.p >= s.end) {
        return cf_error_set(err, CF_E_SOURCE, NULL, s.file, "%s: the document is empty",
                            s.file);
    }
    if (*s.p != '{') {
        return js_fail(&s, s.p, "a configuration document must be an object");
    }
    if (!js_object(&s, NULL, 0) || !js_space(&s)) {
        return CF_FALSE;
    }
    if (s.p != s.end) {
        return js_fail(&s, s.p, "trailing text after the document");
    }
    return CF_TRUE;
}

static cf_bool json_decode(const cf_blob* in, const cf_doc_sink* out, cf_error* err) {
    return js_decode(in, out, err, CF_FALSE);
}

static cf_bool json5_decode(const cf_blob* in, const cf_doc_sink* out, cf_error* err) {
    return js_decode(in, out, err, CF_TRUE);
}

#if CF_WITH_EMIT

/* ================================================================== *
 * Encoding
 * ================================================================== */

typedef struct js_enc {
    cf_sink* out;
    int      depth;
    cf_u8    need_comma[JS_MAX_DEPTH + 2];
    /* Which bracket opened each level. A table and an array close
       differently and the event that closes them is the same, so the
       encoder has to remember. */
    char brace[JS_MAX_DEPTH + 2];
} js_enc;

/* cf_emit keeps this on its own frame, in CF_ENCODER_STATE_MAX bytes. */
typedef char js_enc_fits[(sizeof(js_enc) <= CF_ENCODER_STATE_MAX) ? 1 : -1];

/* Members sit one level inside their container, and a closing bracket
   sits at the container's own level. Both are depth + 1 once the
   bracket has been accounted for, which is why there is one function
   and not two. */
static void js_indent(js_enc* e) {
    int i;
    cf_sink_putc(e->out, '\n');
    for (i = 0; i <= e->depth; ++i) {
        cf_sink_puts(e->out, "  ");
    }
}

static void js_sep(js_enc* e) {
    if (e->depth >= 0 && e->depth <= JS_MAX_DEPTH && e->need_comma[e->depth]) {
        cf_sink_putc(e->out, ',');
    }
    js_indent(e);
    if (e->depth >= 0 && e->depth <= JS_MAX_DEPTH) {
        e->need_comma[e->depth] = 1;
    }
}

static void js_name(js_enc* e, const char* name, size_t n) {
    if (name != NULL) {
        cf_sink_json_string(e->out, name, n);
        cf_sink_puts(e->out, ": ");
    }
}

static cf_bool js_enc_open(js_enc* e, const char* name, size_t n, char brace) {
    int level;
    /* The document and JS_MAX_DEPTH - 1 levels inside it: what the
       decoder takes, so that nothing written is refused when read.
       Compared without adding to it, so no depth overflows its type, and
       a depth below the document's own (-1) is refused, not indexed; the
       new level is fixed before anything else is written. */
    if (e->depth < -1 || e->depth >= JS_MAX_DEPTH - 1) {
        return CF_FALSE;
    }
    level = e->depth + 1;
    if (e->depth >= 0) {
        js_sep(e);
    }
    js_name(e, name, n);
    cf_sink_putc(e->out, brace);
    e->depth             = level;
    e->need_comma[level] = 0;
    e->brace[level]      = brace;
    return CF_TRUE;
}

static cf_bool js_enc_table(void* ctx, const char* name, size_t n) {
    js_enc* e = (js_enc*)ctx;
    if (e->depth < 0) { /* the document itself */
        e->depth         = 0;
        e->need_comma[0] = 0;
        e->brace[0]      = '{';
        cf_sink_putc(e->out, '{');
        return CF_TRUE;
    }
    return js_enc_open(e, name, n, '{');
}

static cf_bool js_enc_array(void* ctx, const char* name, size_t n) {
    return js_enc_open((js_enc*)ctx, name, n, '[');
}

static cf_bool js_enc_scalar(void* ctx, const char* name, size_t n, const cf_value* v,
                             cf_bool textual, const cf_loc* at) {
    js_enc* e = (js_enc*)ctx;
    char    tmp[CF_VALUE_TEXT_MAX];

    (void)textual;
    (void)at;
    js_sep(e);
    js_name(e, name, n);

    switch (v->type) {
    case CF_TYPE_TEXT:
        cf_sink_json_string(e->out, v->u.t.p, v->u.t.n);
        break;
    case CF_TYPE_NULL:
        cf_sink_puts(e->out, "null");
        break;
    case CF_TYPE_BOOL:
        cf_sink_puts(e->out, v->u.b ? "true" : "false");
        break;
    case CF_TYPE_INT:
    case CF_TYPE_REAL:
        cf_sink_write(e->out, tmp, cf_value_to_text(v, tmp, sizeof tmp));
        break;
    }
    return CF_TRUE;
}

static cf_bool js_enc_end(void* ctx) {
    js_enc* e = (js_enc*)ctx;
    cf_bool had_any;
    char    open_brace;

    if (e->depth < 0) {
        return CF_FALSE;
    }
    had_any    = e->need_comma[e->depth];
    open_brace = e->brace[e->depth];
    --e->depth;
    if (had_any) {
        js_indent(e);
    }
    cf_sink_putc(e->out, (open_brace == '[') ? ']' : '}');
    if (e->depth < 0) {
        cf_sink_putc(e->out, '\n');
    }
    return CF_TRUE;
}

static cf_bool json_encoder(cf_sink* out, void* state, cf_doc_sink* sink) {
    js_enc* e = (js_enc*)state;

    if (e == NULL || sink == NULL) {
        return CF_FALSE;
    }
    memset(e, 0, sizeof *e);
    e->out   = out;
    e->depth = -1;

    sink->size        = offsetof(cf_doc_sink, reserve_text);
    sink->begin_table = js_enc_table;
    sink->begin_array = js_enc_array;
    sink->scalar      = js_enc_scalar;
    sink->end         = js_enc_end;
    sink->ctx         = e;
    return CF_TRUE;
}

const cf_codec cf_codec_json = {sizeof(cf_codec),
                                "json",
                                ".json;.jsn",
                                CF_CODEC_READ | CF_CODEC_WRITE | CF_CODEC_TYPED |
                                    CF_CODEC_ARRAYS,
                                json_decode,
                                json_encoder,
                                sizeof(js_enc),
                                NULL,
                                NULL};

/* Written as JSON, which every JSON5 reader reads. */
const cf_codec cf_codec_json5 = {sizeof(cf_codec),
                                 "json5",
                                 ".json5;.jsonc",
                                 CF_CODEC_READ | CF_CODEC_WRITE | CF_CODEC_TYPED |
                                     CF_CODEC_ARRAYS,
                                 json5_decode,
                                 json_encoder,
                                 sizeof(js_enc),
                                 NULL,
                                 NULL};

#else /* !CF_WITH_EMIT: read only */

const cf_codec cf_codec_json = {sizeof(cf_codec),
                                "json",
                                ".json;.jsn",
                                CF_CODEC_READ | CF_CODEC_TYPED | CF_CODEC_ARRAYS,
                                json_decode,
                                NULL,
                                0,
                                NULL,
                                NULL};

const cf_codec cf_codec_json5 = {sizeof(cf_codec),
                                 "json5",
                                 ".json5;.jsonc",
                                 CF_CODEC_READ | CF_CODEC_TYPED | CF_CODEC_ARRAYS,
                                 json5_decode,
                                 NULL,
                                 0,
                                 NULL,
                                 NULL};

#endif /* CF_WITH_EMIT */

#endif /* CF_WITH_JSON */
