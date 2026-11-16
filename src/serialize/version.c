/*
 * version.c - the serialization library's version. C89.
 *
 * Both libraries compile this file, so a program that links either can ask
 * which version it has.
 */

#include "configforge/cf_serialize_version.h"

/* A literal built from the three numbers: the header's own macros are the
   one place the version is written. */
#define SV_STR_(x) #x
#define SV_STR(x) SV_STR_(x)

const char* cf_serialize_version(void) {
    return SV_STR(CF_SERIALIZE_VERSION_MAJOR) "." SV_STR(
        CF_SERIALIZE_VERSION_MINOR) "." SV_STR(CF_SERIALIZE_VERSION_PATCH);
}

int cf_serialize_version_number(void) {
    return CF_SERIALIZE_VERSION_NUMBER;
}
