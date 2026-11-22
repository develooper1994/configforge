// toml++: toml::parse of a string_view with TOML_EXCEPTIONS=0 and
// TOML_HEADER_ONLY=1, the parse_result checked. Strict and loose are the same:
// toml++ has no option that checks more.
#include "touch.h"

#include <toml++/toml.hpp>

namespace {

void walk(const toml::node& n, Touch& t) {
    switch (n.type()) {
    case toml::node_type::table: {
        ++t.containers;
        for (auto&& [k, v] : *n.as_table()) {
            touch_string(t, k.str().size());
            walk(v, t);
        }
        break;
    }
    case toml::node_type::array:
        ++t.containers;
        for (auto&& e : *n.as_array())
            walk(e, t);
        break;
    case toml::node_type::string:
        touch_string(t, n.as_string()->get().size());
        break;
    case toml::node_type::integer:
        touch_int(t, n.as_integer()->get());
        break;
    case toml::node_type::floating_point:
        touch_real(t, n.as_floating_point()->get());
        break;
    case toml::node_type::boolean:
        ++t.bools;
        break;
    default:
        break;
    }
}

bool parse(const char* p, size_t n, bool, Touch& t) {
    toml::parse_result r = toml::parse(std::string_view(p, n));
    if (!r)
        return false;
    // the root container is not counted
    for (auto&& [k, v] : r.table()) {
        touch_string(t, k.str().size());
        walk(v, t);
    }
    return true;
}

const Adapter a = {"tomlpp", "tomlpp", "toml", CFB_VERSION, parse};
Registrar     r(&a);

} // namespace
