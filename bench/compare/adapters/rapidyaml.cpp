// rapidyaml: ryml::parse_in_arena (a copy of the input; parse_in_place destroys
// its input and is the in-situ row of a later card), then the tree walked. Every
// scalar is text. Errors throw instead of aborting, so that a refused document is
// an answer. Strict and loose are the same: rapidyaml has no option that checks
// more.
#include "touch.h"

#include <ryml.hpp>

#include <stdexcept>

namespace {

struct Refused {};

void on_error(ryml::csubstr, ryml::ErrorDataBasic const&, void*) {
    throw Refused();
}
void on_parse_error(ryml::csubstr, ryml::ErrorDataParse const&, void*) {
    throw Refused();
}
void on_visit_error(ryml::csubstr, ryml::ErrorDataVisit const&, void*) {
    throw Refused();
}

void install() {
    static bool done = false;
    if (done)
        return;
    done               = true;
    ryml::Callbacks cb = ryml::get_callbacks();
    cb.m_error_basic   = on_error;
    cb.m_error_parse   = on_parse_error;
    cb.m_error_visit   = on_visit_error;
    ryml::set_callbacks(cb);
}

void walk(const ryml::Tree& tr, ryml::id_type id, int depth, Touch& t) {
    if (tr.has_key(id))
        touch_string(t, tr.key(id).len);
    if (tr.is_container(id)) {
        // the root container is not counted
        if (depth > 0)
            ++t.containers;
        for (ryml::id_type c = tr.first_child(id); c != ryml::NONE;
             c               = tr.next_sibling(c))
            walk(tr, c, depth + 1, t);
    } else if (tr.has_val(id)) {
        touch_string(t, tr.val(id).len);
    }
}

bool parse(const char* p, size_t n, bool, Touch& t) {
    install();
    try {
        ryml::Tree tree = ryml::parse_in_arena(ryml::csubstr(p, n));
        if (tree.empty())
            return true;
        walk(tree, tree.root_id(), 0, t);
        return true;
    } catch (Refused&) {
        return false;
    }
}

const Adapter a = {"rapidyaml", "rapidyaml", "yaml", CFB_VERSION, parse};
Registrar     r(&a);

} // namespace
