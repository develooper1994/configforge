// tinycbor: cbor_parser_init and the iteration calls, with cbor_value_validate
// first. Strict sets the flags whose check ConfigForge's CBOR reader also
// makes: valid UTF-8 in every text string, and no byte after the item. The map
// check for unique keys exists only with sorted keys (CborValidateMapIsSorted),
// which a document is not, so a duplicate key is not checked (bench/compare/
// probes.expected shows it).
#include "touch.h"

#include <cbor.h>

namespace {

bool walk(CborValue* it, int depth, Touch& t) {
    switch (cbor_value_get_type(it)) {
    case CborMapType:
    case CborArrayType: {
        bool      map = cbor_value_is_map(it);
        CborValue rec;
        if (depth > 0)
            ++t.containers;
        if (cbor_value_enter_container(it, &rec) != CborNoError)
            return false;
        while (!cbor_value_at_end(&rec)) {
            if (map) {
                size_t len = 0;
                if (cbor_value_is_text_string(&rec)) {
                    if (cbor_value_get_string_length(&rec, &len) != CborNoError)
                        return false;
                    touch_string(t, len);
                }
                if (cbor_value_advance(&rec) != CborNoError)
                    return false;
            }
            if (!walk(&rec, depth + 1, t))
                return false;
        }
        return cbor_value_leave_container(it, &rec) == CborNoError;
    }
    case CborTextStringType: {
        size_t len = 0;
        if (cbor_value_get_string_length(it, &len) != CborNoError)
            return false;
        touch_string(t, len);
        return cbor_value_advance(it) == CborNoError;
    }
    case CborIntegerType: {
        uint64_t raw = 0;
        cbor_value_get_raw_integer(it, &raw);
        touch_int(t, cbor_value_is_negative_integer(it) ? (int64_t)(-1 - (int64_t)raw)
                                                        : (int64_t)raw);
        return cbor_value_advance_fixed(it) == CborNoError;
    }
    case CborDoubleType: {
        double d = 0;
        cbor_value_get_double(it, &d);
        touch_real(t, d);
        return cbor_value_advance_fixed(it) == CborNoError;
    }
    case CborFloatType: {
        float f = 0;
        cbor_value_get_float(it, &f);
        touch_real(t, (double)f);
        return cbor_value_advance_fixed(it) == CborNoError;
    }
    case CborHalfFloatType: {
        float f = 0;
        cbor_value_get_half_float_as_float(it, &f);
        touch_real(t, (double)f);
        return cbor_value_advance_fixed(it) == CborNoError;
    }
    case CborBooleanType:
        ++t.bools;
        return cbor_value_advance_fixed(it) == CborNoError;
    case CborNullType:
        ++t.nulls;
        return cbor_value_advance_fixed(it) == CborNoError;
    default:
        return cbor_value_advance(it) == CborNoError;
    }
}

bool parse(const char* p, size_t n, bool strict, Touch& t) {
    CborParser parser;
    CborValue  it;
    if (cbor_parser_init(reinterpret_cast<const uint8_t*>(p), n, 0, &parser, &it) !=
        CborNoError)
        return false;
    uint32_t flags = strict ? (CborValidateUtf8 | (uint32_t)CborValidateCompleteData)
                            : (uint32_t)CborValidateBasic;
    if (cbor_value_validate(&it, flags) != CborNoError)
        return false;
    return walk(&it, 0, t);
}

const Adapter a = {"tinycbor", "tinycbor", "cbor", CFB_VERSION, parse};
Registrar     r(&a);

} // namespace
