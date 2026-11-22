// libyaml: the yaml_parser_parse event loop over yaml_parser_set_input_string.
// Every scalar is text to it, and to ConfigForge's YAML reader, so every scalar
// is a string here. Strict and loose are the same: libyaml has no option that
// checks more.
#include "touch.h"

#include <yaml.h>

namespace {

bool parse(const char* p, size_t n, bool, Touch& t) {
    yaml_parser_t parser;
    yaml_event_t  ev;
    int           depth = 0;
    bool          ok    = true;
    if (!yaml_parser_initialize(&parser))
        return false;
    yaml_parser_set_input_string(&parser, reinterpret_cast<const unsigned char*>(p), n);
    for (;;) {
        if (!yaml_parser_parse(&parser, &ev)) {
            ok = false;
            break;
        }
        yaml_event_type_t type = ev.type;
        switch (type) {
        case YAML_MAPPING_START_EVENT:
        case YAML_SEQUENCE_START_EVENT:
            // the root container is not counted
            if (depth > 0)
                ++t.containers;
            ++depth;
            break;
        case YAML_MAPPING_END_EVENT:
        case YAML_SEQUENCE_END_EVENT:
            --depth;
            break;
        case YAML_SCALAR_EVENT:
            touch_string(t, ev.data.scalar.length);
            break;
        default:
            break;
        }
        yaml_event_delete(&ev);
        if (type == YAML_STREAM_END_EVENT)
            break;
    }
    yaml_parser_delete(&parser);
    return ok;
}

const Adapter a = {"libyaml", "libyaml", "yaml", CFB_VERSION, parse};
Registrar     r(&a);

} // namespace
