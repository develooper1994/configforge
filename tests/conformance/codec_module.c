/*
 * codec_module.c - codecs in a shared library, the way a format of
 * one's own reaches `conform` (conform <format> <file> --codec <library>
 * <symbol>, D-68): each exported as a pointer to its cf_codec.
 *
 *   conform_kv     the "key: value" codec the unit tests use
 *                  (tests/unit/cf_kv_codec.h)
 *   conform_json   a codec of its own that reads through this library's
 *                  JSON reader, so the conformance run scores a loaded
 *                  codec against JSONTestSuite and its floors
 */

#include "../unit/cf_kv_codec.h"

#if defined(_WIN32)
#define CONFORM_EXPORT __declspec(dllexport)
#else
#define CONFORM_EXPORT
#endif

#if CF_WITH_JSON
static cf_bool via_json(const cf_blob* in, const cf_doc_sink* out, cf_error* err) {
    const cf_codec* json = cf_codec_by_name("json");
    return (json != NULL) ? json->decode(in, out, err) : CF_FALSE;
}

static const cf_codec json_codec = {sizeof(cf_codec),
                                    "json-module",
                                    ".json",
                                    CF_CODEC_READ | CF_CODEC_TYPED | CF_CODEC_ARRAYS,
                                    via_json,
                                    NULL,
                                    0};

CONFORM_EXPORT extern const cf_codec* const conform_json;
CONFORM_EXPORT const cf_codec* const        conform_json = &json_codec;
#endif

CONFORM_EXPORT extern const cf_codec* const conform_kv;
CONFORM_EXPORT const cf_codec* const        conform_kv = &kv_codec;
