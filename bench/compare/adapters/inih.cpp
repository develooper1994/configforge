// inih: ini_parse_string with INI_STOP_ON_FIRST_ERROR 1, INI_USE_STACK 0 and
// INI_MAX_LINE 8192 (its default of 200 bytes would refuse the long-line
// documents; no INI document has a line over 4096). Every value is a string. The
// handler is given a name and a section; the structure ConfigForge reports is
// rebuilt from them: a section is a container with its name, a name ending in
// [] a container of its own for the run of lines that follow (D-60).
#include "touch.h"

#include <ini.h>

#include <cstdio>
#include <string>

namespace {

struct State {
    Touch*      t;
    std::string section;
    std::string array; // the open name[] run, without the brackets
    bool        have_section = false;
    bool        in_array     = false;
};

int handler(void* user, const char* section, const char* name, const char* value) {
    State* s = static_cast<State*>(user);
    Touch& t = *s->t;
    if (!s->have_section || s->section != section) {
        s->have_section = true;
        s->section      = section;
        s->in_array     = false;
        if (!s->section.empty()) {
            ++t.containers;
            touch_string(t, s->section.size());
        }
    }
    size_t nl = std::strlen(name);
    if (nl >= 2 && name[nl - 2] == '[' && name[nl - 1] == ']') {
        if (!s->in_array || s->array.size() != nl - 2 ||
            std::memcmp(s->array.data(), name, nl - 2) != 0) {
            s->in_array = true;
            s->array.assign(name, nl - 2);
            ++t.containers;
            touch_string(t, nl - 2);
        }
    } else {
        s->in_array = false;
        touch_string(t, nl);
    }
    touch_string(t, std::strlen(value));
    return 1;
}

bool parse(const char* p, size_t n, bool, Touch& t) {
    State s;
    s.t = &t;
    if (p[n] == '\0') {
        int rc = ini_parse_string(p, handler, &s);
        if (rc != 0 && std::getenv("CFB_DEBUG"))
            std::fprintf(stderr, "inih: line %d\n", rc);
        return rc == 0;
    }
    // a slice with no NUL after it (many-tiny): ini_parse_string needs one
    std::string copy(p, n);
    return ini_parse_string(copy.c_str(), handler, &s) == 0;
}

const Adapter a = {"inih", "inih", "ini", CFB_VERSION, parse};
Registrar     r(&a);

} // namespace
