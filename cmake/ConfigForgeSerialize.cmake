# The serialization library: what it is made of and what it may
# include. One list, read by gate.layering and by the build.
set(CF_SERIALIZE_LEAVES
    port/src/port.c
    numeric/src/numeric.c
    base/src/alloc.c
    base/src/error.c
    base/src/value.c
    platform/src/posix/file.c
    platform/src/win32/file.c
    platform/src/none/file.c)
set(CF_SERIALIZE_HEADERS
    cf_build_options.h
    cf_port.h
    cf_error.h
    cf_numeric.h
    cf_value.h
    cf_alloc.h
    cf_platform_file.h
    cf_serialize_version.h
    cf_serialize.h
    cf_yamlparse.h)
