/*
 * A codec made from a reader, a repair of its bytes and a filter of its
 * events (D-103). The description and both callbacks belong to the program.
 */

#include "configforge/cf_serialize.h"
#include "configforge/cf_encoding.h"
#include "configforge/cf_build_options.h"
#include "configforge/cf_error.h"
#include "configforge/cf_port.h"
#include "configforge/cf_value.h"

#include <stddef.h>
#include <string.h>

#if CF_WITH_COMPOSE

typedef struct compose_frame {
    const cf_doc_sink* down;
    cf_doc_sink        up;
} compose_frame;

static cf_bool compose_table(void* ctx, const char* name, size_t n) {
    const compose_frame* f = (const compose_frame*)ctx;

    return f->up.begin_table(f->up.ctx, name, n);
}

static cf_bool compose_array(void* ctx, const char* name, size_t n) {
    const compose_frame* f = (const compose_frame*)ctx;

    return f->up.begin_array(f->up.ctx, name, n);
}

static cf_bool compose_scalar(void* ctx, const char* name, size_t n, const cf_value* v,
                              cf_bool textual, const cf_loc* at) {
    const compose_frame* f = (const compose_frame*)ctx;

    return f->up.scalar(f->up.ctx, name, n, v, textual, at);
}

static cf_bool compose_end(void* ctx) {
    const compose_frame* f = (const compose_frame*)ctx;

    return f->up.end(f->up.ctx);
}

static cf_bool compose_has_text(const cf_doc_sink* sink) {
    return sink->size >=
               offsetof(cf_doc_sink, reserve_text) + sizeof(sink->reserve_text) &&
           sink->reserve_text != NULL;
}

static cf_bool compose_has_work(const cf_doc_sink* sink) {
    return sink->size >=
               offsetof(cf_doc_sink, reserve_work) + sizeof(sink->reserve_work) &&
           sink->reserve_work != NULL;
}

static char* compose_text(void* ctx, size_t n) {
    const compose_frame* f    = (const compose_frame*)ctx;
    const cf_doc_sink*   sink = compose_has_text(&f->up) ? &f->up : f->down;

    return sink->reserve_text(sink->ctx, n);
}

static void* compose_work(void* ctx, size_t n) {
    const compose_frame* f    = (const compose_frame*)ctx;
    const cf_doc_sink*   sink = compose_has_work(&f->up) ? &f->up : f->down;

    return sink->reserve_work(sink->ctx, n);
}

static cf_bool compose_refused(cf_error* err, const char* name, const char* codec,
                               const char* callback) {
    if (err != NULL && err->code != CF_OK) {
        return CF_FALSE;
    }
    cf_error_set(err, CF_E_SOURCE, NULL, name,
                 "%s: the %s of '%s' refused the document", name, callback, codec);
    cf_error_place(err, CF_REASON_SYNTAX, 0, 0);
    return CF_FALSE;
}

static cf_bool compose_filtered(const cf_compose* spec, const cf_blob* cur,
                                const cf_doc_sink* down, const char* name,
                                cf_error* err) {
    compose_frame frame;
    cf_doc_sink   top;

    frame.down = down;
    memset(&frame.up, 0, sizeof frame.up);
    frame.up.size = sizeof frame.up;
    cf_error_clear(err);
    if (!spec->filter(spec->filter_ctx, down, &frame.up, err)) {
        return compose_refused(err, name, spec->name, "filter");
    }
    if (frame.up.begin_table == NULL || frame.up.begin_array == NULL ||
        frame.up.scalar == NULL || frame.up.end == NULL) {
        return cf_error_set(err, CF_E_SOURCE, NULL, name,
                            "%s: the filter of '%s' left a callback of its sink empty",
                            name, spec->name);
    }
    top.size        = sizeof top;
    top.ctx         = &frame;
    top.begin_table = compose_table;
    top.begin_array = compose_array;
    top.scalar      = compose_scalar;
    top.end         = compose_end;
    top.reserve_text =
        compose_has_text(&frame.up) || compose_has_text(down) ? compose_text : NULL;
    top.reserve_work =
        compose_has_work(&frame.up) || compose_has_work(down) ? compose_work : NULL;
    return cfi_codec_decode_raw(spec->base, cur, &top, err);
}

static cf_bool compose_decode(const cf_codec* self, const cf_blob* in,
                              const cf_doc_sink* down, cf_error* err) {
    const cf_compose* spec = (const cf_compose*)self->data;
    const char*       name = in->name != NULL ? in->name : "<memory>";
    char              label[CF_SOURCE_MAX];
    size_t            room = sizeof label - 1 - 3 - strlen(self->name);
    size_t            n    = strlen(name);
    cf_blob           cur  = *in;

    cf_fmt(label, sizeof label, "%S (%s)", name, n < room ? n : room, self->name);
    if (spec->repair != NULL) {
        cur.p = NULL;
        cur.n = 0;
        cf_error_clear(err);
        if (!spec->repair(spec->repair_ctx, in, &cur, err)) {
            return compose_refused(err, name, self->name, "repair");
        }
        if (cur.p == NULL && cur.n > 0) {
            return cf_error_set(err, CF_E_SOURCE, NULL, name,
                                "%s: the repair of '%s' gave %zu bytes and no memory",
                                name, self->name, cur.n);
        }
        if ((spec->base->caps & (CF_CODEC_BINARY | CF_CODEC_SELF_ENCODED)) == 0 &&
            !cf_encoding_check(label, cur.p, cur.n, err)) {
            if (err != NULL) {
                char* hint = strstr(err->message, "; it may be");

                if (hint != NULL) {
                    *hint = '\0';
                }
            }
            return CF_FALSE;
        }
    }
    cur.name = label;
    if (spec->filter != NULL) {
        return compose_filtered(spec, &cur, down, name, err);
    }
    return cfi_codec_decode_raw(spec->base, &cur, down, err);
}

static cf_bool compose_name_valid(const char* name) {
    size_t n;

    if (name == NULL || name[0] < 'a' || name[0] > 'z') {
        return CF_FALSE;
    }
    for (n = 1; name[n] != '\0'; ++n) {
        char ch = name[n];

        if (n >= 48 || !((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') ||
                         ch == '_' || ch == '-' || ch == '+')) {
            return CF_FALSE;
        }
    }
    return CF_TRUE;
}

cf_bool cf_codec_compose_sized(cf_codec* out, size_t out_size, const cf_compose* spec,
                               cf_error* err) {
    const char*     source = spec != NULL ? spec->name : NULL;
    const char*     name   = source != NULL ? source : "";
    const cf_codec* base;

    if (out == NULL || out_size < offsetof(cf_codec, data) + sizeof(const void*)) {
        return cf_error_set(err, CF_E_DEFINITION, NULL, source,
                            "this cf_codec is %zu bytes, from before 1.5, and has "
                            "no room for a composition",
                            out_size);
    }
    memset(out, 0, out_size);
    if (spec == NULL || spec->size < offsetof(cf_compose, filter_ctx) + sizeof(void*)) {
        return cf_error_set(err, CF_E_DEFINITION, NULL, source,
                            "composition '%s': no description, or one without its "
                            "size (CF_COMPOSE_INIT)",
                            name);
    }
    if (!compose_name_valid(source)) {
        return cf_error_set(err, CF_E_DEFINITION, NULL, source,
                            "composition '%s': '%s' is not a codec's name: 1 to 48 of "
                            "lowercase letters, digits, '_', '-' and '+', the first a "
                            "letter",
                            name, name);
    }
    if (cf_codec_by_name(name) != NULL || cf_format_option(name) != NULL) {
        return cf_error_set(err, CF_E_DEFINITION, NULL, source,
                            "composition '%s': '%s' is the name of a built-in format; "
                            "choose another",
                            name, name);
    }
    base = spec->base;
    if (base == out) {
        return cf_error_set(err, CF_E_DEFINITION, NULL, source,
                            "composition '%s': the base is the codec being made", name);
    }
    if (!cf_codec_can_read(base)) {
        return cf_error_set(err, CF_E_DEFINITION, NULL, source,
                            "composition '%s': no base codec that reads", name);
    }
    if (spec->repair == NULL && spec->filter == NULL) {
        return cf_error_set(err, CF_E_DEFINITION, NULL, source,
                            "composition '%s': a repair or a filter is needed; with "
                            "neither it is the base under another name",
                            name);
    }
    out->size = out_size < sizeof *out ? out_size : sizeof *out;
    out->name = source;
    out->caps =
        (base->caps & ~(unsigned)CF_CODEC_WRITE) | CF_CODEC_READ | CF_CODEC_NAMED_ONLY;
    if (spec->repair != NULL &&
        (base->caps & (CF_CODEC_BINARY | CF_CODEC_SELF_ENCODED)) == 0) {
        out->caps |= CF_CODEC_SELF_ENCODED;
    }
    out->decode_self = compose_decode;
    out->data        = spec;
    return CF_TRUE;
}

#else /* !CF_WITH_COMPOSE */

cf_bool cf_codec_compose_sized(cf_codec* out, size_t out_size, const cf_compose* spec,
                               cf_error* err) {
    (void)out;
    (void)out_size;
    if (spec != NULL && spec->name != NULL) {
        return cf_error_set(err, CF_E_CAPABILITY, NULL, spec->name,
                            "'%s' is a composition, and this build was compiled "
                            "without CF_WITH_COMPOSE",
                            spec->name);
    }
    return cf_error_set(err, CF_E_CAPABILITY, NULL, NULL,
                        "a composition, and this build was compiled without "
                        "CF_WITH_COMPOSE");
}

#endif /* CF_WITH_COMPOSE */
