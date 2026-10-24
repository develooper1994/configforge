/*
 * version.c - the version and the feature list. C89.
 */

#include "configforge/cf_platform.h"
#include "configforge/cf_version.h"

#include <string.h>

const char* cf_version(void) {
    return CF_VERSION_STRING;
}

int cf_version_number(void) {
    return CF_VERSION_NUMBER;
}

/* The CMake build writes cf_build_id.h on every build
   (scripts/buildid.cmake) and says so with CF_HAVE_BUILD_ID_H; any
   other build either defines CF_BUILD_ID itself or has none. */
#if defined(CF_HAVE_BUILD_ID_H) && !defined(CF_BUILD_ID)
#include "cf_build_id.h"
#endif
#ifndef CF_BUILD_ID
#define CF_BUILD_ID ""
#endif

const char* cf_build_id(void) {
    return CF_BUILD_ID;
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
#if CF_WITH_PROPERTIES
    "properties",
#endif
#if CF_WITH_XML
    "xml",
#endif
#if CF_WITH_YAML
    "yaml",
#endif
#if CF_WITH_SQLITE
    "sqlite",
#endif
#if CF_WITH_REAL
    "real",
#endif
#ifdef CF_SINGLE_THREADED
    "single-threaded",
#endif
#ifdef CF_FORCE_PORTABLE
    "portable",
#endif
#if CF_WITH_DIR
    "dir",
#endif
#if CF_WITH_DOTENV
    "dotenv",
#endif
#if CF_WITH_EMIT
    "emit",
#endif
#if CF_WITH_IMAGE
    "image",
#endif
#if CF_WITH_REPORT
    "report",
#endif
    NULL};

static size_t compiled_count(void) {
    size_t n = 0;
    while (cf_features[n] != NULL) {
        ++n;
    }
    return n;
}

/* "watch" is the one feature a build option cannot state, because
   whether there is a handle to wait on is the platform's answer: inotify,
   kqueue and Windows have one, a target with no operating system has
   none. It used to be a CF_WITH_WATCH option that switched nothing, off
   by default, so every report said "no watch" from a build that had it.
   The list reports what is true instead. */
size_t cf_feature_count(void) {
    return compiled_count() + (cf_platform_has_wake() ? 1u : 0u);
}

const char* cf_feature_at(size_t i) {
    size_t n = compiled_count();
    if (i < n) {
        return cf_features[i];
    }
    return (i == n && cf_platform_has_wake()) ? "watch" : NULL;
}

cf_bool cf_has_feature(const char* name) {
    size_t i;

    if (name == NULL) {
        return CF_FALSE;
    }
    for (i = 0; i < cf_feature_count(); ++i) {
        const char* f = cf_feature_at(i);
        if (f != NULL && strcmp(f, name) == 0) {
            return CF_TRUE;
        }
    }
    return CF_FALSE;
}
