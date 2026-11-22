/*
 * A repair before the document events, a filter after them, and the
 * name of the composed codec in the places the reader gives (D-103).
 */

#include <configforge/cf_encoding.h>
#include <configforge/cf_serialize.h>
#include <configforge/configforge.h>

#include <string.h>

#include "cf_test.h"

#if !CF_WITH_COMPOSE || CF_WITH_JSON
static cf_error* fresh_error(void) {
    static cf_error err = CF_ERROR_INIT;

    cf_error_clear(&err);
    return &err;
}
#endif

#if CF_WITH_COMPOSE && CF_WITH_JSON

static cf_bool blank(char ch) {
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\v' ||
           ch == '\f';
}

static cf_bool letter(char ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

static cf_bool digit(char ch) {
    return ch >= '0' && ch <= '9';
}

typedef struct braces_reader {
    const cf_blob* in;
    size_t         pos;
    cf_error*      err;
} braces_reader;

static char peek(braces_reader* r) {
    while (r->pos < r->in->n && blank(r->in->p[r->pos])) {
        ++r->pos;
    }
    return r->pos < r->in->n ? r->in->p[r->pos] : '\0';
}

static cf_bool expected(const braces_reader* r, const char* what) {
    return cf_error_set(r->err, CF_E_SOURCE, NULL, r->in->name,
                        "braces:%zu: expected %s", r->pos, what);
}

static cf_bool take(braces_reader* r, char ch) {
    if (peek(r) != ch) {
        char token[2];

        token[0] = ch;
        token[1] = '\0';
        return expected(r, token);
    }
    ++r->pos;
    return CF_TRUE;
}

static cf_bool braces_member(braces_reader* r, const cf_doc_sink* out) {
    size_t start;

    (void)peek(r);
    start = r->pos;
    while (r->pos < r->in->n && letter(r->in->p[r->pos])) {
        ++r->pos;
    }
    if (start == r->pos) {
        return expected(r, "a name");
    }
    if (!out->begin_table(out->ctx, r->in->p + start, r->pos - start) ||
        !take(r, ':') || !take(r, '{')) {
        return CF_FALSE;
    }
    if (peek(r) != '}') {
        cf_i64   value = 0;
        cf_value v;
        cf_loc   loc;

        if (r->pos == r->in->n || !digit(r->in->p[r->pos])) {
            return expected(r, "digits or }");
        }
        loc.file = r->in->name;
        loc.line = 1;
        loc.col  = (unsigned)(r->pos + 1);
        do {
            int next = r->in->p[r->pos] - '0';

            if (value > (CF_I64_MAX - next) / 10) {
                return expected(r, "an integer that fits");
            }
            value = value * 10 + next;
            ++r->pos;
        } while (r->pos < r->in->n && digit(r->in->p[r->pos]));
        cf_value_int(&v, value);
        if (!out->scalar(out->ctx, NULL, 0, &v, CF_FALSE, &loc)) {
            return CF_FALSE;
        }
    }
    return take(r, '}') && out->end(out->ctx);
}

static cf_bool braces_decode(const cf_blob* in, const cf_doc_sink* out, cf_error* err) {
    braces_reader r;

    r.in  = in;
    r.pos = 0;
    r.err = err;
    if (!take(&r, '{') || !out->begin_table(out->ctx, NULL, 0)) {
        return CF_FALSE;
    }
    if (peek(&r) != '}') {
        for (;;) {
            if (!braces_member(&r, out)) {
                return CF_FALSE;
            }
            if (peek(&r) != ',') {
                break;
            }
            ++r.pos;
        }
    }
    if (!take(&r, '}')) {
        return CF_FALSE;
    }
    (void)peek(&r);
    if (r.pos != in->n) {
        return expected(&r, "the end of the document");
    }
    return out->end(out->ctx);
}

static const cf_codec braces = {sizeof(cf_codec),
                                "braces",
                                NULL,
                                CF_CODEC_READ | CF_CODEC_TYPED,
                                braces_decode,
                                NULL,
                                0,
                                NULL,
                                NULL};

static cf_bool braces_repair(void* ctx, const cf_blob* in, cf_blob* out,
                             cf_error* err) {
    size_t start = 0;
    size_t end   = in->n;

    (void)ctx;
    (void)err;
    while (start < end && blank(in->p[start])) {
        ++start;
    }
    while (start < end && blank(in->p[end - 1])) {
        --end;
    }
    out->p = in->p;
    out->n = in->n;
    if (end - start >= 4 && memcmp(in->p + start, "{{", 2) == 0 &&
        memcmp(in->p + end - 2, "}}", 2) == 0) {
        out->p = in->p + start + 1;
        out->n = end - start - 2;
    }
    return CF_TRUE;
}

static cf_bool latin1_repair(void* ctx, const cf_blob* in, cf_blob* out,
                             cf_error* err) {
    static char buf[256];
    size_t      i;
    size_t      n = 0;

    (void)ctx;
    for (i = 0; i < in->n; ++i) {
        unsigned ch    = (unsigned char)in->p[i];
        size_t   width = ch < 0x80 ? 1 : 2;

        if (width > sizeof buf - n) {
            return cf_error_set(err, CF_E_SOURCE, NULL, in->name, "latin1: too long");
        }
        if (width == 1) {
            buf[n++] = (char)ch;
        } else {
            buf[n++] = (char)(0xC0u | (ch >> 6));
            buf[n++] = (char)(0x80u | (ch & 0x3Fu));
        }
    }
    out->p = buf;
    out->n = n;
    return CF_TRUE;
}

static cf_bool broken_repair(void* ctx, const cf_blob* in, cf_blob* out,
                             cf_error* err) {
    (void)ctx;
    (void)in;
    (void)out;
    (void)err;
    return CF_FALSE;
}

static cf_bool missing_memory(void* ctx, const cf_blob* in, cf_blob* out,
                              cf_error* err) {
    (void)ctx;
    (void)in;
    (void)err;
    out->n = 3;
    return CF_TRUE;
}

typedef struct unwrap_state {
    const cf_doc_sink* down;
    unsigned           depth;
    int                pending;
    char               name[32];
} unwrap_state;

static cf_bool unwrap_flush(unwrap_state* s) {
    if (s->pending != 1) {
        return CF_TRUE;
    }
    s->pending = 0;
    return s->down->begin_table(s->down->ctx, s->name, strlen(s->name));
}

static cf_bool unwrap_table(void* ctx, const char* name, size_t n) {
    unwrap_state* s = (unwrap_state*)ctx;

    if (s->depth++ == 0) {
        return s->down->begin_table(s->down->ctx, name, n);
    }
    if (!unwrap_flush(s) || name == NULL || n >= sizeof s->name) {
        return CF_FALSE;
    }
    memcpy(s->name, name, n);
    s->name[n] = '\0';
    s->pending = 1;
    return CF_TRUE;
}

static cf_bool unwrap_array(void* ctx, const char* name, size_t n) {
    unwrap_state* s = (unwrap_state*)ctx;

    ++s->depth;
    return unwrap_flush(s) && s->down->begin_array(s->down->ctx, name, n);
}

static cf_bool unwrap_scalar(void* ctx, const char* name, size_t n, const cf_value* v,
                             cf_bool textual, const cf_loc* at) {
    unwrap_state* s = (unwrap_state*)ctx;

    if (s->pending == 1 && name == NULL) {
        s->pending = 2;
        return s->down->scalar(s->down->ctx, s->name, strlen(s->name), v, textual, at);
    }
    return unwrap_flush(s) && s->down->scalar(s->down->ctx, name, n, v, textual, at);
}

static cf_bool unwrap_end(void* ctx) {
    unwrap_state* s = (unwrap_state*)ctx;

    --s->depth;
    if (s->pending == 2) {
        s->pending = 0;
        return CF_TRUE;
    }
    return unwrap_flush(s) && s->down->end(s->down->ctx);
}

static cf_bool unwrap(void* ctx, const cf_doc_sink* down, cf_doc_sink* up,
                      cf_error* err) {
    unwrap_state* s = (unwrap_state*)ctx;

    (void)err;
    s->down         = down;
    s->depth        = 0;
    s->pending      = 0;
    up->ctx         = s;
    up->begin_table = unwrap_table;
    up->begin_array = unwrap_array;
    up->scalar      = unwrap_scalar;
    up->end         = unwrap_end;
    return CF_TRUE;
}

typedef struct pass_state {
    const cf_doc_sink* down;
    cf_bool            suffix;
} pass_state;

static const char* suffixed(const pass_state* s, const char* name, size_t* n, char* key,
                            size_t cap) {
    if (name == NULL || !s->suffix || *n > cap - 2) {
        return name;
    }
    memcpy(key, name, *n);
    memcpy(key + *n, "-x", 2);
    *n += 2;
    return key;
}

static cf_bool pass_table(void* ctx, const char* name, size_t n) {
    const pass_state* s = (const pass_state*)ctx;
    char              key[CF_KEY_MAX];

    name = suffixed(s, name, &n, key, sizeof key);
    return s->down->begin_table(s->down->ctx, name, n);
}

static cf_bool pass_array(void* ctx, const char* name, size_t n) {
    const pass_state* s = (const pass_state*)ctx;
    char              key[CF_KEY_MAX];

    name = suffixed(s, name, &n, key, sizeof key);
    return s->down->begin_array(s->down->ctx, name, n);
}

static cf_bool pass_scalar(void* ctx, const char* name, size_t n, const cf_value* v,
                           cf_bool textual, const cf_loc* at) {
    const pass_state* s = (const pass_state*)ctx;
    char              key[CF_KEY_MAX];

    name = suffixed(s, name, &n, key, sizeof key);
    return s->down->scalar(s->down->ctx, name, n, v, textual, at);
}

static cf_bool pass_end(void* ctx) {
    const pass_state* s = (const pass_state*)ctx;

    return s->down->end(s->down->ctx);
}

static cf_bool pass(void* ctx, const cf_doc_sink* down, cf_doc_sink* up,
                    cf_error* err) {
    pass_state* s = (pass_state*)ctx;

    (void)err;
    s->down         = down;
    s->suffix       = CF_FALSE;
    up->ctx         = s;
    up->begin_table = pass_table;
    up->begin_array = pass_array;
    up->scalar      = pass_scalar;
    up->end         = pass_end;
    return CF_TRUE;
}

static cf_bool rename_keys(void* ctx, const cf_doc_sink* down, cf_doc_sink* up,
                           cf_error* err) {
    pass(ctx, down, up, err);
    ((pass_state*)ctx)->suffix = CF_TRUE;
    return CF_TRUE;
}

static cf_bool lazy(void* ctx, const cf_doc_sink* down, cf_doc_sink* up,
                    cf_error* err) {
    pass(ctx, down, up, err);
    up->end = NULL;
    return CF_TRUE;
}

static cf_compose json5_repair(void) {
    cf_compose spec = CF_COMPOSE_INIT;

    spec.name   = "json5+braces";
    spec.base   = cf_codec_by_name("json5");
    spec.repair = braces_repair;
    return spec;
}

static cf_config* read_doc(const cf_codec* composed, const char* name, const char* doc,
                           const char* format, unsigned flags, cf_error* err) {
    cf_builder* b = cf_build(NULL);

    check(b != NULL, "the builder opens");
    if (b == NULL) {
        return NULL;
    }
    if (composed != NULL) {
        check(cf_builder_codec(b, composed), "the builder takes the composition");
    }
    check(cf_builder_document(b, name, CF_RANK_USER_FILE, doc, strlen(doc), format,
                              flags),
          "the builder takes the document");
    return cf_builder_create(b, err);
}

static void test_repair(void) {
    static char ex[512];
    cf_compose  spec = json5_repair();
    cf_codec    composed;
    cf_error*   err = fresh_error();
    cf_config*  c;

    check(cf_codec_compose(&composed, &spec, err), "make json5+braces");
    c = read_doc(&composed, "x.json", "{{a:1, b:2}}", spec.name, 0, err);
    check(c != NULL && cf_get_int(c, "a", 0) == 1 && cf_get_int(c, "b", 0) == 2,
          "the double-braced JSON5 reads");
    if (c != NULL) {
        const cf_source_view* sources = cf_sources(c);
        cf_detect_how         how     = CF_DETECT_NONE;

        cf_explain(c, "a", ex, sizeof ex);
        has(ex, "x.json (json5+braces):1:", "explain places the value under its codec");
        check(sources != NULL && sources->count == 1, "one document source");
        if (sources != NULL && sources->count == 1) {
            check_text(cf_source_file_format(sources->items[0], &how), spec.name,
                       "the source names the composition");
            check(how == CF_DETECT_NAMED, "by name");
        }
#if CF_WITH_REPORT
        {
            static char    report[4096];
            cf_report_info info = CF_REPORT_INFO_INIT;

            check(cf_report(c, &info, report, sizeof report) > 0, "make the report");
            has(report, "\"format\": \"json5+braces\", \"format_how\": \"named\"",
                "the report names the composition");
        }
#endif
    }
    cf_config_free(c);
    c = read_doc(NULL, "x.json", "{{a:1, b:2}}", "json5", 0, fresh_error());
    check(c == NULL, "built-in JSON5 refuses double braces");
    cf_config_free(c);
    c = read_doc(&composed, "x.json", "{a:1}", spec.name, 0, fresh_error());
    check(c != NULL && cf_get_int(c, "a", 0) == 1, "plain JSON5 still reads");
    cf_config_free(c);
}

typedef struct observed_sink {
    unsigned tables;
    unsigned ends;
    size_t   text;
} observed_sink;

static cf_bool observe_table(void* ctx, const char* name, size_t n) {
    observed_sink* s = (observed_sink*)ctx;

    if (name != NULL) {
        check(n == 1 && name[0] == 'a', "the empty table keeps its name");
    }
    ++s->tables;
    return CF_TRUE;
}

static cf_bool observe_scalar(void* ctx, const char* name, size_t n, const cf_value* v,
                              cf_bool textual, const cf_loc* at) {
    observed_sink* s = (observed_sink*)ctx;

    (void)name;
    (void)n;
    (void)textual;
    (void)at;
    if (v->type == CF_TYPE_TEXT) {
        s->text = v->u.t.n;
    }
    return CF_TRUE;
}

static cf_bool observe_end(void* ctx) {
    ++((observed_sink*)ctx)->ends;
    return CF_TRUE;
}

static cf_doc_sink observing(observed_sink* state) {
    cf_doc_sink sink;

    memset(&sink, 0, sizeof sink);
    sink.size        = sizeof sink;
    sink.ctx         = state;
    sink.begin_table = observe_table;
    sink.begin_array = observe_table;
    sink.scalar      = observe_scalar;
    sink.end         = observe_end;
    return sink;
}

static void test_filter(void) {
    unwrap_state  state;
    cf_compose    spec = CF_COMPOSE_INIT;
    cf_codec      composed;
    cf_error*     err = fresh_error();
    cf_config*    c;
    observed_sink observed = {0, 0, 0};
    cf_doc_sink   sink     = observing(&observed);
    cf_blob       blob     = {"{a:{}}", 6, "x.json"};
    char          kind[100];

    spec.name       = "braces+unwrap";
    spec.base       = &braces;
    spec.filter     = unwrap;
    spec.filter_ctx = &state;
    check(cf_codec_compose(&composed, &spec, err), "make braces+unwrap");
    c = read_doc(&composed, "x.json", "{a:{2}, b:{2}}", spec.name, 0, err);
    check(c != NULL && cf_get_int(c, "a", 0) == 2 && cf_get_int(c, "b", 0) == 2,
          "the filter unwraps both tables into integers");
    cf_config_free(c);
    c = read_doc(&composed, "x.json", blob.p, spec.name, 0, fresh_error());
    check(c != NULL && cf_count(c) == 0, "an empty table has no scalar keys");
    cf_config_free(c);
    check(cf_codec_decode(&composed, &blob, &sink, fresh_error()),
          "the empty table decodes through the filter");
    check(observed.tables == 2 && observed.ends == 2, "both table pairs pass through");
    check((composed.caps & CF_CODEC_SELF_ENCODED) == 0, "the filter keeps input UTF-8");
    cf_codec_kind(&composed, kind, sizeof kind);
    check_text(kind, "text, self-describing, typed, no arrays, read", "braces kind");
}

static void test_both(void) {
    pass_state state;
    cf_compose spec = json5_repair();
    cf_codec   composed;
    cf_error*  err = fresh_error();
    cf_config* c;

    spec.name       = "json5+both";
    spec.filter     = rename_keys;
    spec.filter_ctx = &state;
    check(cf_codec_compose(&composed, &spec, err), "make json5+both");
    c = read_doc(&composed, "x.json", "{{a:1}}", spec.name, 0, err);
    check(c != NULL && cf_get_int(c, "a-x", 0) == 1 && !cf_has(c, "a"),
          "rename the key produced by the repaired document");
    cf_config_free(c);
}

static cf_error* refused(const cf_codec* composed, const char* name, const char* doc,
                         unsigned flags, const char* words) {
    cf_error*  err = fresh_error();
    cf_config* c   = read_doc(composed, name, doc, composed->name, flags, err);

    check(c == NULL, "the document is refused");
    cf_config_free(c);
    has(err->message, words, "the refusal says why");
    return err;
}

static void test_utf8(void) {
    static const char latin1[] = "{a:\"caf\xE9\"}";
    pass_state        state;
    cf_compose        spec = json5_repair();
    cf_codec          composed;
    cf_error*         err = fresh_error();
    cf_config*        c;

    spec.name   = "json5+latin1";
    spec.repair = latin1_repair;
    check(cf_codec_compose(&composed, &spec, err), "make json5+latin1");
    c = read_doc(&composed, "x.json", latin1, spec.name, 0, err);
    check(c != NULL, "recode Latin-1 before UTF-8 validation");
    check_text(cf_get_text(c, "a", ""), "caf\xC3\xA9", "the repair handed on UTF-8");
    cf_config_free(c);

    spec = json5_repair();
    check(cf_codec_compose(&composed, &spec, fresh_error()), "make json5+braces");
    err = refused(&composed, "x.json", latin1, 0,
                  "x.json (json5+braces):1:8: byte 0xE9 is not valid UTF-8");
    lacks(err->message, "; it may be", "no hint on the repaired path");

    spec.name       = "json5+pass";
    spec.repair     = NULL;
    spec.filter     = pass;
    spec.filter_ctx = &state;
    check(cf_codec_compose(&composed, &spec, fresh_error()), "make json5+pass");
    err = refused(&composed, "x.json", latin1, 0, "byte 0xE9 is not valid UTF-8");
    has(err->message, "; it may be", "the filter-only path keeps the hint");
}

static void test_caps_and_selection(void) {
    cf_compose      spec = json5_repair();
    cf_codec        composed;
    const cf_codec* own[1];
    cf_blob         blob = {"{a:1}", 5, "x.json"};
    cf_config*      c;
    char            kind[100];
    unsigned        caps = CF_CODEC_READ | CF_CODEC_NAMED_ONLY | CF_CODEC_SELF_ENCODED |
                           CF_CODEC_TYPED | CF_CODEC_ARRAYS;

    check(cf_codec_compose(&composed, &spec, fresh_error()), "make json5+braces");
    check((composed.caps & caps) == caps && (composed.caps & CF_CODEC_WRITE) == 0,
          "the composition keeps the base's reading caps");
    cf_codec_kind(&composed, kind, sizeof kind);
    check_text(kind, "text, self-describing, typed, arrays, read", "JSON5 kind");
    own[0] = &composed;
    check(cf_codec_detect(own, 1, &blob, NULL, NULL, NULL) != &composed,
          "content detection skips the composition");

    if (spill("x.json5", blob.p)) {
        cf_builder* b = cf_build(NULL);

        cf_builder_codec(b, &composed);
        cf_builder_file(b, "file", CF_RANK_USER_FILE, "x.json5", NULL, 0);
        c = cf_builder_create(b, fresh_error());
        check(c != NULL && cf_get_int(c, "a", 0) == 1, "the file reads");
        if (c != NULL) {
            const cf_source_view* sources = cf_sources(c);
            cf_detect_how         how     = CF_DETECT_NONE;

            check_text(cf_source_file_format(sources->items[0], &how), "json5",
                       "the extension chooses built-in JSON5");
            check(how == CF_DETECT_EXTENSION, "selected by its extension");
        }
        cf_config_free(c);
        remove("x.json5");
    } else {
        check(0, "write x.json5");
    }
    refused(&composed, "x.json", blob.p, CF_ENCODING_ISO_8859_1,
            "declares its own encoding or has none");
}

static void bad_spec(cf_codec* out, size_t size, const cf_compose* spec,
                     const char* words) {
    cf_error* err = fresh_error();

    check(!cf_codec_compose_sized(out, size, spec, err), "bad composition refused");
    check(err->code == CF_E_DEFINITION, "a definition error");
    has(err->message, words, "the definition error says why");
}

static void test_definition(void) {
    static const char* const names[] = {
        "Json",  "a.b",  "",    "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvw",
        "json5", "toml", "yaml"};
    cf_codec   composed;
    cf_compose spec       = json5_repair();
    cf_codec   write_only = *spec.base;
    size_t     i;

    write_only.caps        = CF_CODEC_WRITE;
    write_only.decode      = NULL;
    write_only.decode_self = NULL;
    bad_spec(&composed, 12, &spec, "has no room for a composition");
    spec.size = 0;
    bad_spec(&composed, sizeof composed, &spec, "without its size");
    spec.size = sizeof spec;
    for (i = 0; i < sizeof names / sizeof names[0]; ++i) {
        spec.name = names[i];
        bad_spec(&composed, sizeof composed, &spec,
                 i < 4 ? "is not a codec's name" : "the name of a built-in format");
    }
    spec.name = "json5+braces";
    spec.base = NULL;
    bad_spec(&composed, sizeof composed, &spec, "no base codec that reads");
    spec.base = &write_only;
    bad_spec(&composed, sizeof composed, &spec, "no base codec that reads");
    spec.base = &composed;
    bad_spec(&composed, sizeof composed, &spec, "the base is the codec being made");
    check(!cf_codec_can_read(&composed), "the refused codec cannot read");
    spec.base   = cf_codec_by_name("json5");
    spec.repair = NULL;
    bad_spec(&composed, sizeof composed, &spec, "a repair or a filter is needed");
}

static void runtime_refused(const cf_codec* composed, const char* doc,
                            const char* words) {
    cf_error* err = refused(composed, "x.json", doc, 0, words);

    check(err->code == CF_E_SOURCE, "a source error");
    lacks(err->message, "the content looks like",
          "no second opinion for a named codec");
}

static void test_runtime(void) {
    cf_compose spec = json5_repair();
    cf_codec   composed;
    pass_state state;

    spec.repair = broken_repair;
    check(cf_codec_compose(&composed, &spec, fresh_error()), "make a refusing repair");
    runtime_refused(&composed, "{a:1}", "the repair of 'json5+braces' refused");

    spec.repair = missing_memory;
    check(cf_codec_compose(&composed, &spec, fresh_error()), "make a broken output");
    runtime_refused(&composed, "{a:1}", "gave 3 bytes and no memory");

    spec.repair     = NULL;
    spec.filter     = lazy;
    spec.filter_ctx = &state;
    check(cf_codec_compose(&composed, &spec, fresh_error()), "make a lazy filter");
    runtime_refused(&composed, "{a:1}", "left a callback of its sink empty");

    spec = json5_repair();
    check(cf_codec_compose(&composed, &spec, fresh_error()), "make json5+braces");
    runtime_refused(&composed, "{{a:", "x.json (json5+braces):1:");
}

static char* observed_text(void* ctx, size_t n) {
    static char text[512];

    (void)ctx;
    return n <= sizeof text ? text : NULL;
}

static void test_storage(void) {
    static char   doc[307];
    pass_state    state;
    cf_compose    spec = json5_repair();
    cf_codec      composed;
    observed_sink observed = {0, 0, 0};
    cf_doc_sink   sink     = observing(&observed);
    cf_blob       blob     = {doc, sizeof doc - 1, "x.json"};
    cf_error*     err      = fresh_error();

    memcpy(doc, "{a:\"", 4);
    memset(doc + 4, 'x', 300);
    memcpy(doc + 304, "\"}", 3);
    spec.name       = "json5+pass";
    spec.repair     = NULL;
    spec.filter     = pass;
    spec.filter_ctx = &state;
    check(cf_codec_compose(&composed, &spec, err), "make json5+pass");
    sink.reserve_text = observed_text;
    sink.size         = offsetof(cf_doc_sink, reserve_text);
    check(!cf_codec_decode(&composed, &blob, &sink, err),
          "the 1.0 sink has no storage");
    has(err->message, "a long string needs sink text storage", "no storage is visible");
    sink.size = sizeof sink;
    check(cf_codec_decode(&composed, &blob, &sink, fresh_error()),
          "the filter passes down the storage request");
    check(observed.text == 300, "the entire string reaches the sink");
}

static void test_nested(void) {
    static char ex[512];
    char        long_name[101];
    cf_compose  inner = json5_repair();
    cf_compose  outer = CF_COMPOSE_INIT;
    cf_codec    inner_codec;
    cf_codec    outer_codec;
    pass_state  state;
    cf_config*  c;
    cf_error*   err;
    size_t      n;
    size_t      tail = sizeof "(json5+braces)" - 1;

    check(cf_codec_compose(&inner_codec, &inner, fresh_error()), "make inner codec");
    outer.name       = "json5+braces+x";
    outer.base       = &inner_codec;
    outer.filter     = pass;
    outer.filter_ctx = &state;
    check(cf_codec_compose(&outer_codec, &outer, fresh_error()), "make outer codec");
    c = read_doc(&outer_codec, "x.json", "{{a:1}}", outer.name, 0, fresh_error());
    check(c != NULL && cf_get_int(c, "a", 0) == 1, "two codecs compose");
    if (c != NULL) {
        cf_explain(c, "a", ex, sizeof ex);
        has(ex, "(json5+braces+x) (json5+braces)", "the place names both codecs");
    }
    cf_config_free(c);

    memset(long_name, 'x', sizeof long_name - 1);
    long_name[sizeof long_name - 1] = '\0';
    err = refused(&inner_codec, long_name, "{{a:", 0, "(json5+braces):1:");
    n   = strlen(err->source);
    check(n >= tail && n < CF_SOURCE_MAX &&
              strcmp(err->source + n - tail, "(json5+braces)") == 0,
          "the long source label keeps the codec's whole name");
}

#endif /* CF_WITH_COMPOSE && CF_WITH_JSON */

#if !CF_WITH_COMPOSE
static void test_off(void) {
    cf_compose spec = CF_COMPOSE_INIT;
    cf_codec   composed;
    cf_error*  err = fresh_error();

    spec.name = "json5+braces";
    check(!cf_codec_compose(&composed, &spec, err), "a build without compose refuses");
    check(err->code == CF_E_CAPABILITY, "a capability error");
    has(err->message, "CF_WITH_COMPOSE", "the refusal names the option");
}
#endif

int main(void) {
#if CF_WITH_COMPOSE && CF_WITH_JSON
    test_repair();
    test_filter();
    test_both();
    test_utf8();
    test_caps_and_selection();
    test_definition();
    test_runtime();
    test_storage();
    test_nested();
#elif !CF_WITH_COMPOSE
    test_off();
#endif
    return cf_test_done();
}
