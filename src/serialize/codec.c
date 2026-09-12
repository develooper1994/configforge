/*
 * codec.c - the codec registry. C89.
 *
 * The table is static const, so it is not mutable global state: a
 * build's format set is fixed when the build is, which is the whole
 * point of the compile-time options.
 */

#include "configforge/cf_serialize.h"

#include <string.h>

#if CF_WITH_INI
extern const cf_codec cf_codec_ini;
#endif
#if CF_WITH_JSON
extern const cf_codec cf_codec_json;
#endif

static const cf_codec* const cf_codecs[] = {
#if CF_WITH_INI
    &cf_codec_ini,
#endif
#if CF_WITH_JSON
    &cf_codec_json,
#endif
    NULL};

size_t cf_codec_count(void) {
    size_t n = 0;
    while (cf_codecs[n] != NULL) {
        ++n;
    }
    return n;
}

const cf_codec* cf_codec_at(size_t i) {
    return (i < cf_codec_count()) ? cf_codecs[i] : NULL;
}

const cf_codec* cf_codec_by_name(const char* name) {
    size_t i;

    if (name == NULL) {
        return NULL;
    }
    for (i = 0; cf_codecs[i] != NULL; ++i) {
        if (cf_cmp_ascii_ci(name, strlen(name), cf_codecs[i]->name,
                            strlen(cf_codecs[i]->name)) == 0) {
            return cf_codecs[i];
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------ *
 * Extensions
 *
 * Detection is by extension, with an explicit override available per
 * source. Never by sniffing the content: sniffing is the kind of
 * implicit behaviour this library rejects, and it fails in the most
 * confusing possible way the first time a file is renamed.
 * ------------------------------------------------------------------ */

static const char* path_ext(const char* path) {
    const char* dot = NULL;
    size_t      i;

    if (path == NULL) {
        return NULL;
    }
    for (i = 0; path[i] != '\0'; ++i) {
        if (path[i] == '/' || path[i] == '\\') {
            dot = NULL;
        } else if (path[i] == '.') {
            dot = path + i;
        }
    }
    return dot;
}

static cf_bool ext_matches(const char* list, const char* ext) {
    const char* p    = list;
    size_t      elen = strlen(ext);

    while (*p != '\0') {
        const char* semi = strchr(p, ';');
        size_t      n    = (semi != NULL) ? (size_t)(semi - p) : strlen(p);
        if (n == elen && cf_cmp_ascii_ci(p, n, ext, elen) == 0) {
            return CF_TRUE;
        }
        if (semi == NULL) {
            break;
        }
        p = semi + 1;
    }
    return CF_FALSE;
}

const cf_codec* cf_codec_by_path(const char* path) {
    const char* ext = path_ext(path);
    size_t      i;

    if (ext == NULL) {
        return NULL;
    }
    for (i = 0; cf_codecs[i] != NULL; ++i) {
        if (ext_matches(cf_codecs[i]->extensions, ext)) {
            return cf_codecs[i];
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------ *
 * Formats this library knows about, compiled in or not
 *
 * A file the build cannot read has to produce an error that names the
 * option to turn on. Reporting "unknown format" for a .toml file would
 * send the reader looking for a typo that is not there.
 * ------------------------------------------------------------------ */

typedef struct cf_format_info {
    const char* format;
    const char* extensions;
    const char* option;
} cf_format_info;

static const cf_format_info cf_formats[] = {
    {"ini", ".ini;.cfg;.conf", "CF_WITH_INI"}, {"json", ".json;.jsn", "CF_WITH_JSON"},
    {"toml", ".toml", "CF_WITH_TOML"},         {"xml", ".xml", "CF_WITH_XML"},
    {"yaml", ".yaml;.yml", "(not supported)"}, {NULL, NULL, NULL}};

const char* cf_format_of_path(const char* path) {
    const char* ext = path_ext(path);
    size_t      i;

    if (ext == NULL) {
        return NULL;
    }
    for (i = 0; cf_formats[i].format != NULL; ++i) {
        if (ext_matches(cf_formats[i].extensions, ext)) {
            return cf_formats[i].format;
        }
    }
    return NULL;
}

const char* cf_format_option(const char* format) {
    size_t i;

    if (format == NULL) {
        return NULL;
    }
    for (i = 0; cf_formats[i].format != NULL; ++i) {
        if (strcmp(cf_formats[i].format, format) == 0) {
            return cf_formats[i].option;
        }
    }
    return NULL;
}
