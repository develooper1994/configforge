// expat: XML_Parse in one call, handlers for start, end and character data. The
// structure ConfigForge reports (D-23) is rebuilt from the events: the root's
// name is not a key; an element with only text is a value, trimmed; a run of
// elements of one name is an array, its elements without names; attributes are
// not in the documents. ConfigForge's XML reader is expat with the layer's own
// handlers on top, so this row is the floor of that one.
#include "touch.h"

#include <expat.h>

#include <string>
#include <vector>

namespace {

struct Frame {
    std::string name;
    std::string text;
    bool        has_children = false;
    // the run of same-named children, open until a different name or the end
    std::string run_name;
    size_t      run_len          = 0;
    bool        run_is_container = false;
};

struct State {
    Touch*             t;
    std::vector<Frame> stack;
};

// A run ends: its key is one string, and an array container when it has two or more.
void flush_run(Frame& f, Touch& t) {
    if (f.run_len == 0)
        return;
    touch_string(t, f.run_name.size());
    if (f.run_len >= 2)
        ++t.containers;
    f.run_len = 0;
}

void on_start(void* ud, const XML_Char* name, const XML_Char**) {
    State* s = static_cast<State*>(ud);
    if (!s->stack.empty())
        s->stack.back().has_children = true;
    Frame f;
    f.name = name;
    s->stack.push_back(std::move(f));
}

void on_text(void* ud, const XML_Char* p, int n) {
    State* s = static_cast<State*>(ud);
    if (!s->stack.empty())
        s->stack.back().text.append(p, (size_t)n);
}

void on_end(void* ud, const XML_Char*) {
    State* s = static_cast<State*>(ud);
    Touch& t = *s->t;
    Frame  f = std::move(s->stack.back());
    s->stack.pop_back();
    flush_run(f, t);
    if (s->stack.empty())
        return; // the root element
    if (f.has_children) {
        ++t.containers;
    } else {
        size_t b = 0, e = f.text.size();
        while (b < e && (f.text[b] == ' ' || f.text[b] == '\n' || f.text[b] == '\t' ||
                         f.text[b] == '\r'))
            ++b;
        while (e > b && (f.text[e - 1] == ' ' || f.text[e - 1] == '\n' ||
                         f.text[e - 1] == '\t' || f.text[e - 1] == '\r'))
            --e;
        touch_string(t, e - b);
    }
    Frame& parent = s->stack.back();
    if (parent.run_len > 0 && parent.run_name == f.name) {
        ++parent.run_len;
    } else {
        flush_run(parent, t);
        parent.run_name = f.name;
        parent.run_len  = 1;
    }
}

bool parse(const char* p, size_t n, bool, Touch& t) {
    State s;
    s.t               = &t;
    XML_Parser parser = XML_ParserCreate(nullptr);
    if (!parser)
        return false;
    XML_SetUserData(parser, &s);
    XML_SetElementHandler(parser, on_start, on_end);
    XML_SetCharacterDataHandler(parser, on_text);
    bool ok = XML_Parse(parser, p, (int)n, 1) != XML_STATUS_ERROR;
    XML_ParserFree(parser);
    return ok;
}

const Adapter a = {"expat", "expat", "xml", CFB_VERSION, parse};
Registrar     r(&a);

} // namespace
