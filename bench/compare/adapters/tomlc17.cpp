// tomlc17: toml_parse over the bytes and their length. Strict turns on its
// UTF-8 check (a global option, off by default). Dates and times are not in
// the documents.
#include "touch.h"

#include <tomlc17.h>

#include <cstdio>

namespace {

void walk(const toml_datum_t& d, Touch& t) {
    switch (d.type) {
    case TOML_TABLE:
        ++t.containers;
        for (int i = 0; i < d.u.tab.size; ++i) {
            touch_string(t, (size_t)d.u.tab.len[i]);
            walk(d.u.tab.value[i], t);
        }
        break;
    case TOML_ARRAY:
        ++t.containers;
        for (int i = 0; i < d.u.arr.size; ++i)
            walk(d.u.arr.elem[i], t);
        break;
    case TOML_STRING:
        touch_string(t, (size_t)d.u.str.len);
        break;
    case TOML_INT64:
        touch_int(t, d.u.int64);
        break;
    case TOML_FP64:
        touch_real(t, d.u.fp64);
        break;
    case TOML_BOOLEAN:
        ++t.bools;
        break;
    default:
        break;
    }
}

bool parse(const char* p, size_t n, bool strict, Touch& t) {
    static bool have = false, was = false;
    if (!have || was != strict) {
        toml_option_t opt = toml_default_option();
        opt.check_utf8    = strict;
        toml_set_option(opt);
        have = true;
        was  = strict;
    }
    toml_result_t r = toml_parse(p, (int)n);
    if (!r.ok) {
        if (std::getenv("CFB_DEBUG"))
            std::fprintf(stderr, "tomlc17: %s\n", r.errmsg);
        toml_free(r);
        return false;
    }
    // the root container is not counted
    for (int i = 0; i < r.toptab.u.tab.size; ++i) {
        touch_string(t, (size_t)r.toptab.u.tab.len[i]);
        walk(r.toptab.u.tab.value[i], t);
    }
    toml_free(r);
    return true;
}

const Adapter a = {"tomlc17", "tomlc17", "toml", CFB_VERSION, parse};
Registrar     r(&a);

} // namespace
