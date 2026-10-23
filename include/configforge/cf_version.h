/*
 * cf_version.h - one place the version exists.
 *
 * A version that lives in two places is a version that disagrees with
 * itself. These macros are the source; the build reads them and a gate
 * checks that the number CMake reports still matches, so a release that
 * bumps one and forgets the other fails before it ships rather than
 * after.
 *
 * Compatibility, restated from the design:
 *
 *     0.x   nothing is promised; the C interface is not frozen
 *     1.0   the C interface freezes and semantic versioning begins
 *
 * After 1.0 the C interface is additive only: no signature changes, no
 * struct layout changes. Descriptor structs carry a size field so that
 * fields can be added without either.
 */

#ifndef CONFIGFORGE_CF_VERSION_H
#define CONFIGFORGE_CF_VERSION_H

#include "configforge/cf_port.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CF_VERSION_MAJOR 0
#define CF_VERSION_MINOR 2
#define CF_VERSION_PATCH 0

#define CF_VERSION_STRINGIFY_(x) #x
#define CF_VERSION_STRINGIFY(x) CF_VERSION_STRINGIFY_(x)

#define CF_VERSION_STRING                                                              \
    CF_VERSION_STRINGIFY(CF_VERSION_MAJOR)                                             \
    "." CF_VERSION_STRINGIFY(CF_VERSION_MINOR) "." CF_VERSION_STRINGIFY(               \
        CF_VERSION_PATCH)

/* Comparable in one integer, for the `#if` a consumer writes when a
   feature arrived in a known release. */
#define CF_VERSION_NUMBER                                                              \
    (CF_VERSION_MAJOR * 10000 + CF_VERSION_MINOR * 100 + CF_VERSION_PATCH)

/* The version the library was *compiled* as, which is not necessarily
   the header the caller compiled against. A program that links a shared
   build can compare the two and say so, which is the only way that
   mismatch is ever noticed before it misbehaves. */
CF_API const char* cf_version(void);
CF_API int         cf_version_number(void);

/* What this build can do, as a NUL-terminated list: "ini", "json",
   "real", and so on. Recorded in the provenance report as well, so a
   report from a restricted build is distinguishable from a full one. */
CF_API size_t      cf_feature_count(void);
CF_API const char* cf_feature_at(size_t i);
CF_API cf_bool     cf_has_feature(const char* name);

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_CF_VERSION_H */
