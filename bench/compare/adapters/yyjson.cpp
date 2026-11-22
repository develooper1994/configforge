// yyjson: yyjson_read with no flag (it validates structure and UTF-8, and reads
// a copy of the input), then every value touched. Strict and loose are the same:
// the library has no option that checks more.
#include "touch.h"

#include <yyjson.h>

namespace {

void walk(yyjson_val* v, Touch& t) {
    switch (yyjson_get_type(v)) {
    case YYJSON_TYPE_OBJ: {
        ++t.containers;
        yyjson_obj_iter it = yyjson_obj_iter_with(v);
        yyjson_val*     key;
        while ((key = yyjson_obj_iter_next(&it))) {
            touch_string(t, yyjson_get_len(key));
            walk(yyjson_obj_iter_get_val(key), t);
        }
        break;
    }
    case YYJSON_TYPE_ARR: {
        ++t.containers;
        yyjson_arr_iter it = yyjson_arr_iter_with(v);
        yyjson_val*     e;
        while ((e = yyjson_arr_iter_next(&it)))
            walk(e, t);
        break;
    }
    case YYJSON_TYPE_STR:
        touch_string(t, yyjson_get_len(v));
        break;
    case YYJSON_TYPE_NUM:
        if (yyjson_is_real(v))
            touch_real(t, yyjson_get_real(v));
        else
            touch_int(t, yyjson_is_uint(v) ? (int64_t)yyjson_get_uint(v)
                                           : yyjson_get_sint(v));
        break;
    case YYJSON_TYPE_BOOL:
        ++t.bools;
        break;
    case YYJSON_TYPE_NULL:
        ++t.nulls;
        break;
    default:
        break;
    }
}

bool parse(const char* p, size_t n, bool, Touch& t) {
    yyjson_doc* doc = yyjson_read(p, n, 0);
    if (!doc)
        return false;
    yyjson_val* root = yyjson_doc_get_root(doc);
    // the root container is not counted
    if (yyjson_is_obj(root)) {
        yyjson_obj_iter it = yyjson_obj_iter_with(root);
        yyjson_val*     key;
        while ((key = yyjson_obj_iter_next(&it))) {
            touch_string(t, yyjson_get_len(key));
            walk(yyjson_obj_iter_get_val(key), t);
        }
    } else {
        walk(root, t);
    }
    yyjson_doc_free(doc);
    return true;
}

const Adapter a = {"yyjson", "yyjson", "json", CFB_VERSION, parse};
Registrar     r(&a);

} // namespace
