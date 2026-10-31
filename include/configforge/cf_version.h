/*
 * cf_version.h - one place the version exists.
 *
 * A version that lives in two places is a version that disagrees with
 * itself. These three macros are the only place it is written by hand:
 * CMake reads them before project(), the Conan recipe reads them, and
 * the one copy that cannot read anything - the vcpkg manifest - is
 * rewritten with this file by `cmake -DTO=x.y.z -P scripts/version.cmake`
 * and checked against it by gate.version, as is the CHANGELOG's newest
 * section. A release is the tag vX.Y.Z on a commit where these say
 * X.Y.Z; the release workflow refuses any other.
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

#define CF_VERSION_MAJOR 1
#define CF_VERSION_MINOR 0
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

/* The commit the library was built from, as git abbreviates it -
   "20d0f4b" - with "-dirty" when the tree had uncommitted changes, or ""
   when the build could not know (no git, or a copy that is neither a
   checkout nor a git archive). Build metadata, not part of the version:
   two builds of 0.2.0 compare equal by cf_version_number() and can still
   be told apart by this. Never NULL. Set at build time, not configure
   time, so it follows a checkout without reconfiguring; a build without
   CMake can pass -DCF_BUILD_ID="\"...\"". */
CF_API const char* cf_build_id(void);

/* What this build can do, as a NUL-terminated list: "ini", "json",
   "real", and so on. Recorded in the provenance report as well, so a
   report from a restricted build is distinguishable from a full one. */
CF_API size_t      cf_feature_count(void);
CF_API const char* cf_feature_at(size_t i);
CF_API cf_bool     cf_has_feature(const char* name);

/* A build limit, by name: "key" (CF_KEY_MAX), "source" (CF_SOURCE_MAX),
   "message" (CF_MESSAGE_MAX), "path" (CF_PATH_MAX) and "depth"
   (CF_MAX_DEPTH); 0 for a name it does not know. The first three size
   cf_error's buffers, so they are part of a build's ABI (D-45): a
   program, or a binding that mirrors cf_error, compiled against other
   limits must not trust the layout, and this is how it can tell. */
CF_API size_t cf_build_limit(const char* name);

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_CF_VERSION_H */
