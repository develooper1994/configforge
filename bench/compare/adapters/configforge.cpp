// ConfigForge: cf_codec_by_name(<format>) and cf_codec_decode into a
// cf_doc_sink whose callbacks touch the value, supplying reserve_text from
// one buffer kept between documents and, in strict mode, reserve_work from
// blocks freed between documents.
#include "touch.h"

#include <configforge/cf_serialize.h>

#include <cstdlib>
#include <cstring>

namespace {

struct Ctx {
    Touch* t    = nullptr;
    char*  room = nullptr;
    size_t cap  = 0;
    void*  work = nullptr; // reserve_work's blocks, freed together
    int    depth =
        0; // containers open: the root, and YAML's empty second one, are not counted
};

union Align {
    double d;
    long   l;
    void*  p;
};

void name_of(Ctx* c, size_t n) {
    if (n > 0)
        touch_string(*c->t, n);
}

cf_bool open_table(void* c, const char*, size_t n) {
    Ctx* x = static_cast<Ctx*>(c);
    if (x->depth > 0) {
        ++x->t->containers;
        name_of(x, n);
    }
    ++x->depth;
    return CF_TRUE;
}

cf_bool scalar(void* c, const char*, size_t n, const cf_value* v, cf_bool,
               const cf_loc*) {
    Ctx*   x = static_cast<Ctx*>(c);
    Touch& t = *x->t;
    name_of(x, n);
    switch (v->type) {
    case CF_TYPE_TEXT:
        touch_string(t, v->u.t.n);
        break;
    case CF_TYPE_INT:
        touch_int(t, (int64_t)v->u.i);
        break;
    case CF_TYPE_REAL:
        touch_real(t, v->u.r);
        break;
    case CF_TYPE_BOOL:
        ++t.bools;
        break;
    default:
        ++t.nulls;
        break;
    }
    return CF_TRUE;
}

cf_bool close_it(void* c) {
    --static_cast<Ctx*>(c)->depth;
    return CF_TRUE;
}

char* reserve_text(void* c, size_t n) {
    Ctx* x = static_cast<Ctx*>(c);
    if (n > x->cap) {
        std::free(x->room);
        x->room = static_cast<char*>(std::malloc(n));
        x->cap  = x->room ? n : 0;
    }
    return x->room;
}

void* reserve_work(void* c, size_t n) {
    Ctx*  x = static_cast<Ctx*>(c);
    char* b = static_cast<char*>(std::malloc(sizeof(Align) * 2 + n));
    if (!b)
        return nullptr;
    *reinterpret_cast<void**>(b) = x->work;
    x->work                      = b;
    return b + sizeof(Align) * 2;
}

void release_work(Ctx& x) {
    while (x.work) {
        void* next = *static_cast<void**>(x.work);
        std::free(x.work);
        x.work = next;
    }
}

Ctx g_ctx;

bool run(const char* format, const char* p, size_t n, bool strict, Touch& t) {
    const cf_codec* codec = cf_codec_by_name(format);
    cf_blob         blob;
    cf_doc_sink     sink;
    cf_error        err = CF_ERROR_INIT;
    if (!codec)
        return false;
    cf_blob_mem(&blob, p, n, format);
    std::memset(&sink, 0, sizeof sink);
    sink.size         = sizeof sink;
    sink.begin_table  = open_table;
    sink.begin_array  = open_table;
    sink.scalar       = scalar;
    sink.end          = close_it;
    g_ctx.t           = &t;
    g_ctx.depth       = 0;
    sink.ctx          = &g_ctx;
    sink.reserve_text = reserve_text;
    // TOML checks what it defined twice only when it is given room; YAML cannot
    // read without it
    if (strict || std::strcmp(format, "yaml") == 0)
        sink.reserve_work = reserve_work;
    bool ok = cf_codec_decode(codec, &blob, &sink, &err) != 0;
    release_work(g_ctx);
    return ok;
}

bool parse_json(const char* p, size_t n, bool s, Touch& t) {
    return run("json", p, n, s, t);
}
bool parse_yaml(const char* p, size_t n, bool s, Touch& t) {
    return run("yaml", p, n, s, t);
}
bool parse_toml(const char* p, size_t n, bool s, Touch& t) {
    return run("toml", p, n, s, t);
}
bool parse_cbor(const char* p, size_t n, bool s, Touch& t) {
    return run("cbor", p, n, s, t);
}
bool parse_ini(const char* p, size_t n, bool s, Touch& t) {
    return run("ini", p, n, s, t);
}
#if CFB_WITH_XML
bool parse_xml(const char* p, size_t n, bool s, Touch& t) {
    return run("xml", p, n, s, t);
}
#endif

const Adapter a_json = {"configforge", "configforge", "json", CFB_CF_VERSION,
                        parse_json};
const Adapter a_yaml = {"configforge", "configforge", "yaml", CFB_CF_VERSION,
                        parse_yaml};
const Adapter a_toml = {"configforge", "configforge", "toml", CFB_CF_VERSION,
                        parse_toml};
const Adapter a_cbor = {"configforge", "configforge", "cbor", CFB_CF_VERSION,
                        parse_cbor};
const Adapter a_ini  = {"configforge", "configforge", "ini", CFB_CF_VERSION, parse_ini};
Registrar     r1(&a_json), r2(&a_yaml), r3(&a_toml), r4(&a_cbor), r5(&a_ini);
#if CFB_WITH_XML
const Adapter a_xml = {"configforge", "configforge", "xml", CFB_CF_VERSION, parse_xml};
Registrar     r6(&a_xml);
#endif

} // namespace
