// RapidJSON: Document::Parse over the bytes and their length. The headline row
// `rapidjson` uses kParseValidateEncodingFlag in strict mode (ConfigForge
// refuses invalid UTF-8, D-108) and the default flags in loose mode;
// `rapidjson-nocheck` always uses the default flags, which do not check the
// encoding.
#include "touch.h"

#include <rapidjson/document.h>

namespace {

using namespace rapidjson;

void walk(const Value& v, Touch& t) {
    if (v.IsObject()) {
        ++t.containers;
        for (auto m = v.MemberBegin(); m != v.MemberEnd(); ++m) {
            touch_string(t, m->name.GetStringLength());
            walk(m->value, t);
        }
    } else if (v.IsArray()) {
        ++t.containers;
        for (auto e = v.Begin(); e != v.End(); ++e)
            walk(*e, t);
    } else if (v.IsString()) {
        touch_string(t, v.GetStringLength());
    } else if (v.IsNumber()) {
        if (v.IsDouble())
            touch_real(t, v.GetDouble());
        else if (v.IsUint64() && !v.IsInt64())
            touch_int(t, (int64_t)v.GetUint64());
        else
            touch_int(t, v.GetInt64());
    } else if (v.IsBool()) {
        ++t.bools;
    } else if (v.IsNull()) {
        ++t.nulls;
    }
}

template <unsigned Flags> bool run(const char* p, size_t n, Touch& t) {
    Document d;
    d.Parse<Flags>(p, n);
    if (d.HasParseError())
        return false;
    // the root container is not counted
    if (d.IsObject()) {
        for (auto m = d.MemberBegin(); m != d.MemberEnd(); ++m) {
            touch_string(t, m->name.GetStringLength());
            walk(m->value, t);
        }
    } else if (d.IsArray()) {
        for (auto e = d.Begin(); e != d.End(); ++e)
            walk(*e, t);
    } else {
        walk(d, t);
    }
    return true;
}

bool parse(const char* p, size_t n, bool strict, Touch& t) {
    return strict ? run<kParseValidateEncodingFlag>(p, n, t)
                  : run<kParseDefaultFlags>(p, n, t);
}

bool parse_nocheck(const char* p, size_t n, bool, Touch& t) {
    return run<kParseDefaultFlags>(p, n, t);
}

const Adapter a = {"rapidjson", "rapidjson", "json", CFB_VERSION, parse};
const Adapter b = {"rapidjson-nocheck", "rapidjson", "json", CFB_VERSION,
                   parse_nocheck};
Registrar     r(&a), r2(&b);

} // namespace
