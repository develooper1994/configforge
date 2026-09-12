/*
 * version.c - the version and the feature list. C89.
 */

#include "configforge/cf_version.h"

#include <string.h>

const char* cf_version(void) {
    return CF_VERSION_STRING;
}

int cf_version_number(void) {
    return CF_VERSION_NUMBER;
}

/* Built from the same options the rest of the library compiles under,
   so the list cannot describe a build that was not made. */
static const char* const cf_features[] = {
#if CF_WITH_INI
    "ini",
#endif
#if CF_WITH_JSON
    "json",
#endif
#if CF_WITH_TOML
    "toml",
#endif
#if CF_WITH_XML
    "xml",
#endif
#if CF_WITH_SQLITE
    "sqlite",
#endif
#if CF_WITH_REAL
    "real",
#endif
#if CF_WITH_WATCH
    "watch",
#endif
#ifdef CF_SINGLE_THREADED
    "single-threaded",
#endif
#ifdef CF_FORCE_PORTABLE
    "portable",
#endif
    "image",
    NULL};

size_t cf_feature_count(void) {
    size_t n = 0;
    while (cf_features[n] != NULL) {
        ++n;
    }
    return n;
}

const char* cf_feature_at(size_t i) {
    return (i < cf_feature_count()) ? cf_features[i] : NULL;
}

cf_bool cf_has_feature(const char* name) {
    size_t i;

    if (name == NULL) {
        return CF_FALSE;
    }
    for (i = 0; cf_features[i] != NULL; ++i) {
        if (strcmp(cf_features[i], name) == 0) {
            return CF_TRUE;
        }
    }
    return CF_FALSE;
}
