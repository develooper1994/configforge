// cJSON: cJSON_ParseWithLength, and in strict mode
// cJSON_ParseWithLengthOpts(..., require_null_terminated = 1) over the document
// and the NUL that follows it (every document read whole has one; a slice of
// many-tiny has none and is parsed by cJSON_ParseWithLength alone).
#include "touch.h"

#include <cJSON.h>

namespace {

void walk(const cJSON* v, Touch& t) {
    switch (v->type & 0xFF) {
    case cJSON_Object:
    case cJSON_Array:
        ++t.containers;
        for (const cJSON* c = v->child; c; c = c->next) {
            if ((v->type & 0xFF) == cJSON_Object && c->string)
                touch_string(t, std::strlen(c->string));
            walk(c, t);
        }
        break;
    case cJSON_String:
        touch_string(t, std::strlen(v->valuestring));
        break;
    case cJSON_Number:
        touch_real(t, v->valuedouble);
        break;
    case cJSON_True:
    case cJSON_False:
        ++t.bools;
        break;
    case cJSON_NULL:
        ++t.nulls;
        break;
    default:
        break;
    }
}

bool parse(const char* p, size_t n, bool strict, Touch& t) {
    cJSON* root;
    if (strict && p[n] == '\0')
        root = cJSON_ParseWithLengthOpts(p, n + 1, nullptr, 1);
    else
        root = cJSON_ParseWithLength(p, n);
    if (!root)
        return false;
    // the root container is not counted
    for (const cJSON* c = root->child; c; c = c->next) {
        if ((root->type & 0xFF) == cJSON_Object && c->string)
            touch_string(t, std::strlen(c->string));
        walk(c, t);
    }
    cJSON_Delete(root);
    return true;
}

const Adapter a = {"cjson", "cjson", "json", CFB_VERSION, parse};
Registrar     r(&a);

} // namespace
