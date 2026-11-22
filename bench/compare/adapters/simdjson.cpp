// simdjson: the runtime-dispatched kernel it picks, padded input, and one
// parser reused between documents (its documented fast path). Two rows:
// `simdjson-dom` (dom::parser) and `simdjson-ondemand`, which is lazy, so the
// adapter touches every value, which makes the work equal. Strict and loose
// are the same: simdjson always validates UTF-8 and has no option for more.
#include "touch.h"

#include <simdjson.h>

namespace {

using namespace simdjson;

// ---- DOM ----
void walk_dom(dom::element e, Touch& t) {
    switch (e.type()) {
    case dom::element_type::OBJECT:
        ++t.containers;
        for (auto f : dom::object(e)) {
            touch_string(t, f.key.size());
            walk_dom(f.value, t);
        }
        break;
    case dom::element_type::ARRAY:
        ++t.containers;
        for (dom::element c : dom::array(e))
            walk_dom(c, t);
        break;
    case dom::element_type::STRING:
        touch_string(t, std::string_view(e).size());
        break;
    case dom::element_type::INT64:
        touch_int(t, (int64_t)e);
        break;
    case dom::element_type::UINT64:
        touch_int(t, (int64_t)(uint64_t)e);
        break;
    case dom::element_type::DOUBLE:
        touch_real(t, (double)e);
        break;
    case dom::element_type::BOOL:
        ++t.bools;
        break;
    case dom::element_type::NULL_VALUE:
        ++t.nulls;
        break;
    }
}

bool parse_dom(const char* p, size_t n, bool, Touch& t) {
    static dom::parser parser;
    try {
        dom::element root = parser.parse(p, n, false);
        // the root container is not counted
        if (root.type() == dom::element_type::OBJECT) {
            for (auto f : dom::object(root)) {
                touch_string(t, f.key.size());
                walk_dom(f.value, t);
            }
        } else if (root.type() == dom::element_type::ARRAY) {
            for (dom::element c : dom::array(root))
                walk_dom(c, t);
        } else {
            walk_dom(root, t);
        }
        return true;
    } catch (simdjson_error&) {
        return false;
    }
}

// ---- On-Demand ----
void walk_od(ondemand::value v, Touch& t) {
    switch (v.type()) {
    case ondemand::json_type::object:
        ++t.containers;
        for (auto f : v.get_object()) {
            touch_string(t, std::string_view(f.unescaped_key()).size());
            walk_od(f.value(), t);
        }
        break;
    case ondemand::json_type::array:
        ++t.containers;
        for (auto c : v.get_array())
            walk_od(c.value(), t);
        break;
    case ondemand::json_type::string:
        touch_string(t, std::string_view(v.get_string()).size());
        break;
    case ondemand::json_type::number: {
        ondemand::number num = v.get_number();
        switch (num.get_number_type()) {
        case ondemand::number_type::floating_point_number:
            touch_real(t, num.get_double());
            break;
        case ondemand::number_type::signed_integer:
            touch_int(t, num.get_int64());
            break;
        default:
            touch_int(t, (int64_t)num.get_uint64());
            break;
        }
        break;
    }
    case ondemand::json_type::boolean:
        (void)v.get_bool();
        ++t.bools;
        break;
    case ondemand::json_type::null:
        ++t.nulls;
        break;
    }
}

bool parse_od(const char* p, size_t n, bool, Touch& t) {
    static ondemand::parser parser;
    try {
        ondemand::document doc =
            parser.iterate(padded_string_view(p, n, n + SIMDJSON_PADDING));
        // the root container is not counted
        if (doc.type() == ondemand::json_type::object) {
            for (auto f : doc.get_object()) {
                touch_string(t, std::string_view(f.unescaped_key()).size());
                walk_od(f.value(), t);
            }
        } else if (doc.type() == ondemand::json_type::array) {
            for (auto c : doc.get_array())
                walk_od(c.value(), t);
        } else {
            walk_od(doc.get_value(), t);
        }
        return true;
    } catch (simdjson_error&) {
        return false;
    }
}

const Adapter a = {"simdjson-dom", "simdjson", "json", CFB_VERSION, parse_dom};
const Adapter b = {"simdjson-ondemand", "simdjson", "json", CFB_VERSION, parse_od};
Registrar     r(&a), r2(&b);

} // namespace
