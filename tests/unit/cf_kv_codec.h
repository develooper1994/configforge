/*
 * cf_kv_codec.h - a codec written outside the library, shared by the
 * programs that need one: test_codec.c, and the module conform loads
 * (tests/conformance/codec_module.c).
 *
 * The shape of an integration with another parser - libyaml, expat,
 * anything that streams: translate its events into cf_doc_sink calls,
 * with locations. Nothing here uses anything but the public headers,
 * which is the point. The format is deliberately tiny ("key: value" per
 * line, "#" comments) so that what is left is the integration itself.
 *
 * One document, one table, a scalar per line. A dotted key needs no
 * nesting of its own - "server.port" is one key to the flattener - and
 * the values are text, because the format carries no types: the
 * definitions decide, as they do for INI. Written, each scalar is a
 * line under its full key; the format has no arrays.
 */

#ifndef CONFIGFORGE_TESTS_CF_KV_CODEC_H
#define CONFIGFORGE_TESTS_CF_KV_CODEC_H

#include <configforge/configforge.h>
#include <configforge/cf_serialize.h>

#include <stddef.h>
#include <string.h>

static cf_bool kv_decode(const cf_blob* in, const cf_doc_sink* out, cf_error* err) {
    const char*  p    = in->p;
    const char*  end  = in->p + in->n;
    unsigned int line = 1;

    if (!out->begin_table(out->ctx, NULL, 0)) {
        return cf_error_set(err, CF_E_SOURCE, NULL, in->name, "sink refused");
    }
    while (p < end) {
        const char* eol = p;
        const char* colon;
        cf_value    v;
        cf_loc      at;
        const char* val;
        size_t      klen, vlen;

        while (eol < end && *eol != '\n') {
            ++eol;
        }
        if (eol > p && *p != '#') {
            colon = p;
            while (colon < eol && *colon != ':') {
                ++colon;
            }
            if (colon == eol) {
                return cf_error_set(err, CF_E_SOURCE, NULL, in->name,
                                    "%s:%u: expected 'key: value'", in->name, line);
            }
            klen = (size_t)(colon - p);
            val  = colon + 1;
            while (val < eol && *val == ' ') {
                ++val;
            }
            vlen = (size_t)(eol - val);
            if (vlen > 0 && val[vlen - 1] == '\r') {
                --vlen;
            }
            at.file = in->name;
            at.line = line;
            at.col  = (unsigned int)(val - p) + 1u;
            cf_value_text(&v, val, vlen);
            if (!out->scalar(out->ctx, p, klen, &v, CF_TRUE, &at)) {
                return cf_error_set(err, CF_E_KEY, NULL, in->name,
                                    "%s:%u: not a valid key", in->name, line);
            }
        }
        p = (eol < end) ? eol + 1 : eol;
        ++line;
    }
    return out->end(out->ctx);
}

/* Writing: the path of open tables, and each scalar as a line. */
typedef struct kv_enc {
    cf_sink* out;
    char     path[CF_KEY_MAX];
    size_t   len[CF_MAX_DEPTH + 2];
    int      depth;
} kv_enc;

static cf_bool kv_name(kv_enc* e, const char* name, size_t n) {
    size_t at = e->len[e->depth];

    if (name == NULL) {
        return (e->depth == 0) ? CF_TRUE : CF_FALSE; /* an element: no arrays */
    }
    if (at + n + 2 > sizeof e->path) {
        return CF_FALSE;
    }
    if (at > 0) {
        e->path[at++] = '.';
    }
    memcpy(e->path + at, name, n);
    e->path[at + n] = '\0';
    return CF_TRUE;
}

static cf_bool kv_table(void* ctx, const char* name, size_t n) {
    kv_enc* e = (kv_enc*)ctx;

    if (e->depth < 0) {
        e->depth  = 0;
        e->len[0] = 0;
        return CF_TRUE; /* the document */
    }
    if (e->depth + 1 > CF_MAX_DEPTH || !kv_name(e, name, n)) {
        return CF_FALSE;
    }
    e->len[e->depth + 1] = strlen(e->path);
    ++e->depth;
    return CF_TRUE;
}

static cf_bool kv_array(void* ctx, const char* name, size_t n) {
    (void)ctx;
    (void)name;
    (void)n;
    return CF_FALSE;
}

static cf_bool kv_scalar(void* ctx, const char* name, size_t n, const cf_value* v,
                         cf_bool textual, const cf_loc* at) {
    kv_enc* e = (kv_enc*)ctx;
    char    text[256];
    size_t  k;

    (void)textual;
    (void)at;
    if (e->depth < 0 || !kv_name(e, name, n)) {
        return CF_FALSE;
    }
    k = (v->type == CF_TYPE_TEXT) ? v->u.t.n : cf_value_to_text(v, text, sizeof text);
    cf_sink_puts(e->out, e->path);
    cf_sink_puts(e->out, ": ");
    cf_sink_write(e->out, (v->type == CF_TYPE_TEXT) ? v->u.t.p : text, k);
    cf_sink_puts(e->out, "\n");
    e->path[e->len[e->depth]] = '\0';
    return CF_TRUE;
}

static cf_bool kv_end(void* ctx) {
    kv_enc* e = (kv_enc*)ctx;

    if (e->depth < 0) {
        return CF_FALSE;
    }
    --e->depth;
    if (e->depth >= 0) {
        e->path[e->len[e->depth]] = '\0';
    }
    return CF_TRUE;
}

static cf_bool kv_encoder(cf_sink* out, void* state, cf_doc_sink* sink) {
    kv_enc* e = (kv_enc*)state;

    memset(e, 0, sizeof *e);
    e->out            = out;
    e->depth          = -1;
    sink->size        = offsetof(cf_doc_sink, reserve_text);
    sink->begin_table = kv_table;
    sink->begin_array = kv_array;
    sink->scalar      = kv_scalar;
    sink->end         = kv_end;
    sink->ctx         = e;
    return CF_TRUE;
}

static const cf_codec kv_codec = {sizeof(cf_codec), "kv",       ".kv",
                                  CF_CODEC_READ | CF_CODEC_WRITE,
                                  kv_decode,        kv_encoder, sizeof(kv_enc)};

#endif /* CONFIGFORGE_TESTS_CF_KV_CODEC_H */
