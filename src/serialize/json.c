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
 */

#include "configforge/cf_serialize.h"

#include <string.h>

#if CF_WITH_JSON

/* ================================================================== *
 * Decoding
 * ================================================================== */

typedef struct js_scan {
    const char*        p;
    const char*        end;
    const char*        file;
    const char*        line_start;
    unsigned int       line;
    const cf_doc_sink* out;
    cf_error*          err;
    cf_arena*          arena; /* for unescaped strings; may be NULL */
    char               scratch[CF_MESSAGE_MAX];
    int                depth;
} js_scan;

#define JS_MAX_DEPTH CF_MAX_DEPTH

static void js_loc(const js_scan* s, const char* at, cf_loc* out) {
    out->file = s->file;
    out->line = s->line;
    out->col  = (unsigned int)(at - s->line_start) + 1u;
}

static cf_bool js_fail(js_scan* s, const char* at, const char* what) {
    cf_loc l;
    js_loc(s, at, &l);
    return cf_error_set(s->err, CF_E_SOURCE, NULL, s->file, "%s:%u:%u: %s", l.file,
                        l.line, l.col, what);
}

static void js_space(js_scan* s) {
    while (s->p < s->end) {
        char c = *s->p;
        if (c == '\n') {
            ++s->p;
            ++s->line;
            s->line_start = s->p;
        } else if (c == ' ' || c == '\t' || c == '\r') {
            ++s->p;
        } else {
            break;
        }
    }
}

/* UTF-8 encoding of one code point. JSON's \u escapes are UTF-16, so
   surrogate pairs have to be recombined; a lone surrogate is invalid
   rather than something to pass through. */
static size_t js_utf8(cf_u32 cp, char* out) {
    if (cp < 0x80u) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800u) {
        out[0] = (char)(0xC0u | (cp >> 6));
        out[1] = (char)(0x80u | (cp & 0x3Fu));
        return 2;
    }
    if (cp < 0x10000u) {
        out[0] = (char)(0xE0u | (cp >> 12));
        out[1] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        out[2] = (char)(0x80u | (cp & 0x3Fu));
        return 3;
    }
    out[0] = (char)(0xF0u | (cp >> 18));
    out[1] = (char)(0x80u | ((cp >> 12) & 0x3Fu));
    out[2] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
    out[3] = (char)(0x80u | (cp & 0x3Fu));
    return 4;
}

static cf_bool js_hex4(const char* p, cf_u32* out) {
    cf_u32 v = 0;
    int    i;

    for (i = 0; i < 4; ++i) {
        int c = (unsigned char)p[i];
        v <<= 4;
        if (c >= '0' && c <= '9') {
            v |= (cf_u32)(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            v |= (cf_u32)(c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            v |= (cf_u32)(c - 'A' + 10);
        } else {
            return CF_FALSE;
        }
    }
    *out = v;
    return CF_TRUE;
}

/* Reads a string into dst. Returns the length, or (size_t)-1 on
   error. When the string has no escapes the caller can use the slice
   in place; this always copies, because the arena is cheap and the
   alternative is two code paths that have to agree. */
static size_t js_string(js_scan* s, char* dst, size_t cap) {
    size_t n = 0;

    if (s->p >= s->end || *s->p != '"') {
        js_fail(s, s->p, "expected a string");
        return (size_t)-1;
    }
    ++s->p;
    while (s->p < s->end) {
        unsigned char c = (unsigned char)*s->p;

        if (c == '"') {
            ++s->p;
            if (n < cap) {
                dst[n] = '\0';
            }
            return n;
        }
        if (c == '\n') {
            js_fail(s, s->p, "a string may not contain a raw newline");
            return (size_t)-1;
        }
        if (c < 0x20u) {
            js_fail(s, s->p,
                    "a string may not contain a raw control "
                    "character");
            return (size_t)-1;
        }
        if (c != '\\') {
            if (n + 1 >= cap) {
                js_fail(s, s->p, "string is too long");
                return (size_t)-1;
            }
            dst[n++] = (char)c;
            ++s->p;
            continue;
        }

        ++s->p;
        if (s->p >= s->end) {
            js_fail(s, s->p, "escape at end of input");
            return (size_t)-1;
        }
        switch (*s->p) {
        case '"':
            if (n + 1 < cap) {
                dst[n] = '"';
            }
            ++n;
            ++s->p;
            break;
        case '\\':
            if (n + 1 < cap) {
                dst[n] = '\\';
            }
            ++n;
            ++s->p;
            break;
        case '/':
            if (n + 1 < cap) {
                dst[n] = '/';
            }
            ++n;
            ++s->p;
            break;
        case 'b':
            if (n + 1 < cap) {
                dst[n] = '\b';
            }
            ++n;
            ++s->p;
            break;
        case 'f':
            if (n + 1 < cap) {
                dst[n] = '\f';
            }
            ++n;
            ++s->p;
            break;
        case 'n':
            if (n + 1 < cap) {
                dst[n] = '\n';
            }
            ++n;
            ++s->p;
            break;
        case 'r':
            if (n + 1 < cap) {
                dst[n] = '\r';
            }
            ++n;
            ++s->p;
            break;
        case 't':
            if (n + 1 < cap) {
                dst[n] = '\t';
            }
            ++n;
            ++s->p;
            break;
        case 'u': {
            cf_u32 cp = 0;
            char   enc[4];
            size_t k;

            if (s->end - s->p < 5 || !js_hex4(s->p + 1, &cp)) {
                /* Split after the backslash, as toml.c explains. */
                js_fail(s, s->p,
                        "\\"
                        "u needs four hexadecimal digits");
                return (size_t)-1;
            }
            s->p += 5;
            if (cp >= 0xD800u && cp <= 0xDBFFu) {
                cf_u32 lo = 0;
                if (s->end - s->p < 6 || s->p[0] != '\\' || s->p[1] != 'u' ||
                    !js_hex4(s->p + 2, &lo) || lo < 0xDC00u || lo > 0xDFFFu) {
                    js_fail(s, s->p, "high surrogate without a low one");
                    return (size_t)-1;
                }
                s->p += 6;
                cp = 0x10000u + ((cp - 0xD800u) << 10) + (lo - 0xDC00u);
            } else if (cp >= 0xDC00u && cp <= 0xDFFFu) {
                js_fail(s, s->p, "unpaired low surrogate");
                return (size_t)-1;
            }
            k = js_utf8(cp, enc);
            if (n + k + 1 >= cap) {
                js_fail(s, s->p, "string is too long");
                return (size_t)-1;
            }
            memcpy(dst + n, enc, k);
            n += k;
            break;
        }
        default:
            js_fail(s, s->p, "unknown escape");
            return (size_t)-1;
        }
        if (n + 1 >= cap) {
            js_fail(s, s->p, "string is too long");
            return (size_t)-1;
        }
    }
    js_fail(s, s->p, "string is not closed");
    return (size_t)-1;
}

static cf_bool js_number(js_scan* s, cf_value* v) {
    const char* start   = s->p;
    cf_bool     is_real = CF_FALSE;
    size_t      len;

    if (s->p < s->end && *s->p == '-') {
        ++s->p;
    }
    while (s->p < s->end && cf_is_digit((unsigned char)*s->p)) {
        ++s->p;
    }
    if (s->p < s->end && *s->p == '.') {
        is_real = CF_TRUE;
        ++s->p;
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
        while (s->p < s->end && cf_is_digit((unsigned char)*s->p)) {
            ++s->p;
        }
    }
    len = (size_t)(s->p - start);
    if (len == 0) {
        return js_fail(s, start, "expected a value");
    }

    /* JSON has one numeric type; ConfigForge has two. The rule is
       mechanical rather than a guess: a fractional part or an exponent
       makes it real, and an integer that does not fit becomes real
       with the loss recorded rather than silently wrapping. */
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

static cf_bool js_value(js_scan* s, const char* name, size_t nlen);

static cf_bool js_object(js_scan* s, const char* name, size_t nlen) {
    cf_bool first = CF_TRUE;

    if (++s->depth > JS_MAX_DEPTH) {
        return js_fail(s, s->p, "nesting is too deep");
    }
    if (!s->out->begin_table(s->out->ctx, name, nlen)) {
        return js_fail(s, s->p, "key is not valid");
    }
    ++s->p; /* '{' */
    for (;;) {
        size_t klen;
        js_space(s);
        if (s->p >= s->end) {
            return js_fail(s, s->p, "object is not closed");
        }
        if (*s->p == '}') {
            ++s->p;
            break;
        }
        if (!first) {
            if (*s->p != ',') {
                return js_fail(s, s->p, "expected ',' or '}'");
            }
            ++s->p;
            js_space(s);
            if (s->p < s->end && *s->p == '}') {
                return js_fail(s, s->p, "trailing comma");
            }
        }
        first = CF_FALSE;

        klen = js_string(s, s->scratch, sizeof s->scratch);
        if (klen == (size_t)-1) {
            return CF_FALSE;
        }
        js_space(s);
        if (s->p >= s->end || *s->p != ':') {
            return js_fail(s, s->p, "expected ':'");
        }
        ++s->p;
        js_space(s);
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

    if (++s->depth > JS_MAX_DEPTH) {
        return js_fail(s, s->p, "nesting is too deep");
    }
    if (!s->out->begin_array(s->out->ctx, name, nlen)) {
        return js_fail(s, s->p, "key is not valid");
    }
    ++s->p; /* '[' */
    for (;;) {
        js_space(s);
        if (s->p >= s->end) {
            return js_fail(s, s->p, "array is not closed");
        }
        if (*s->p == ']') {
            ++s->p;
            break;
        }
        if (!first) {
            if (*s->p != ',') {
                return js_fail(s, s->p, "expected ',' or ']'");
            }
            ++s->p;
            js_space(s);
            if (s->p < s->end && *s->p == ']') {
                return js_fail(s, s->p, "trailing comma");
            }
        }
        first = CF_FALSE;
        if (!js_value(s, NULL, 0)) {
            return CF_FALSE;
        }
    }
    --s->depth;
    return s->out->end(s->out->ctx) ? CF_TRUE : js_fail(s, s->p, "sink refused");
}

static cf_bool js_value(js_scan* s, const char* name, size_t nlen) {
    cf_value    v;
    cf_loc      at;
    const char* start;

    js_space(s);
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

    case '"': {
        char   text[CF_MESSAGE_MAX];
        size_t n = js_string(s, text, sizeof text);
        if (n == (size_t)-1) {
            return CF_FALSE;
        }
        cf_value_text(&v, text, n);
        /* Typed: JSON said this is a string, so a definition that
           declared an integer has a mismatch rather than something to
           coerce. */
        return s->out->scalar(s->out->ctx, name, nlen, &v, CF_FALSE, &at)
                   ? CF_TRUE
                   : js_fail(s, start, "key is not valid");
    }

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
        if (!js_number(s, &v)) {
            return CF_FALSE;
        }
        break;
    }
    return s->out->scalar(s->out->ctx, name, nlen, &v, CF_FALSE, &at)
               ? CF_TRUE
               : js_fail(s, start, "key is not valid");
}

static cf_bool json_decode(const cf_blob* in, const cf_doc_sink* out, cf_error* err) {
    js_scan s;

    s.p          = in->p;
    s.end        = in->p + in->n;
    s.file       = (in->name != NULL) ? in->name : "<json>";
    s.line_start = in->p;
    s.line       = 1;
    s.out        = out;
    s.err        = err;
    s.arena      = NULL;
    s.depth      = 0;

    js_space(&s);
    if (s.p >= s.end) {
        return cf_error_set(err, CF_E_SOURCE, NULL, s.file, "%s: the document is empty",
                            s.file);
    }
    if (*s.p != '{') {
        return js_fail(&s, s.p, "a configuration document must be an object");
    }
    if (!js_object(&s, NULL, 0)) {
        return CF_FALSE;
    }
    js_space(&s);
    if (s.p != s.end) {
        return js_fail(&s, s.p, "trailing text after the document");
    }
    return CF_TRUE;
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

static void js_quote(cf_sink* out, const char* p, size_t n) {
    size_t i;

    cf_sink_putc(out, '"');
    for (i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)p[i];
        switch (c) {
        case '"':
            cf_sink_puts(out, "\\\"");
            break;
        case '\\':
            cf_sink_puts(out, "\\\\");
            break;
        case '\b':
            cf_sink_puts(out, "\\b");
            break;
        case '\f':
            cf_sink_puts(out, "\\f");
            break;
        case '\n':
            cf_sink_puts(out, "\\n");
            break;
        case '\r':
            cf_sink_puts(out, "\\r");
            break;
        case '\t':
            cf_sink_puts(out, "\\t");
            break;
        default:
            if (c < 0x20u) {
                /* Control characters must be escaped; everything above
                   is passed through, because the value is already
                   UTF-8 and re-encoding it would only lose that. */
                cf_sink_fmt(out, "\\u00%c%c", "0123456789abcdef"[(c >> 4) & 0xF],
                            "0123456789abcdef"[c & 0xF]);
            } else {
                cf_sink_putc(out, (char)c);
            }
            break;
        }
    }
    cf_sink_putc(out, '"');
}

static void js_name(js_enc* e, const char* name, size_t n) {
    if (name != NULL) {
        js_quote(e->out, name, n);
        cf_sink_puts(e->out, ": ");
    }
}

static cf_bool js_enc_open(js_enc* e, const char* name, size_t n, char brace) {
    if (e->depth >= JS_MAX_DEPTH) {
        return CF_FALSE;
    }
    if (e->depth >= 0) {
        js_sep(e);
    }
    js_name(e, name, n);
    cf_sink_putc(e->out, brace);
    ++e->depth;
    e->need_comma[e->depth] = 0;
    e->brace[e->depth]      = brace;
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
        js_quote(e->out, v->u.t.p, v->u.t.n);
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

    sink->begin_table = js_enc_table;
    sink->begin_array = js_enc_array;
    sink->scalar      = js_enc_scalar;
    sink->end         = js_enc_end;
    sink->ctx         = e;
    return CF_TRUE;
}

const cf_codec cf_codec_json = {"json",
                                ".json;.jsn",
                                CF_CODEC_READ | CF_CODEC_WRITE | CF_CODEC_TYPED |
                                    CF_CODEC_ARRAYS,
                                json_decode,
                                json_encoder,
                                sizeof(js_enc)};

#else /* !CF_WITH_EMIT: read only */

const cf_codec cf_codec_json = {
    "json",      ".json;.jsn", CF_CODEC_READ | CF_CODEC_TYPED | CF_CODEC_ARRAYS,
    json_decode, NULL,         0};

#endif /* CF_WITH_EMIT */

#endif /* CF_WITH_JSON */
