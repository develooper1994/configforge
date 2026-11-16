/*
 * cf_serialize_version.h - the serialization library's version (D-90).
 *
 * configforge_serialize is made of the headers cf_port.h, cf_error.h,
 * cf_numeric.h, cf_value.h, cf_alloc.h, cf_platform_file.h, cf_serialize.h
 * and this one. It does not include cf_version.h, which declares what this
 * library does not define.
 */

#ifndef CONFIGFORGE_CF_SERIALIZE_VERSION_H
#define CONFIGFORGE_CF_SERIALIZE_VERSION_H

#include "configforge/cf_port.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The serialization library's version: ConfigForge's, for as long as the
   two are in one repository (D-90). A copy of cf_version.h's numbers, which
   scripts/release/version.cmake rewrites with them and gate.version holds
   to them. */
#define CF_SERIALIZE_VERSION_MAJOR 1
#define CF_SERIALIZE_VERSION_MINOR 7
#define CF_SERIALIZE_VERSION_PATCH 0

#define CF_SERIALIZE_VERSION_NUMBER                                                    \
    (CF_SERIALIZE_VERSION_MAJOR * 10000 + CF_SERIALIZE_VERSION_MINOR * 100 +           \
     CF_SERIALIZE_VERSION_PATCH)

/* The version the serialization library was compiled as: "1.7.0", and the
   same as one number. (1.7) */
CF_API const char* cf_serialize_version(void);
CF_API int         cf_serialize_version_number(void);

#ifdef __cplusplus
}
#endif

#endif /* CONFIGFORGE_CF_SERIALIZE_VERSION_H */
