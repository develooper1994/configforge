# The serialization library: what it is made of and what it may
# include. One list, read by gate.layering and by the build.
set(CF_SERIALIZE_LEAVES
    core/alloc.c
    core/port.c
    core/error.c
    core/numeric.c
    core/value.c
    platform/posix/file.c
    platform/win32/file.c
    platform/none/file.c)
set(CF_SERIALIZE_HEADERS
    cf_build_options.h
    cf_port.h
    cf_error.h
    cf_numeric.h
    cf_value.h
    cf_alloc.h
    cf_platform_file.h
    cf_serialize.h
    cf_yamlparse.h)
