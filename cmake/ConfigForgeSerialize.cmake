# What the serialization library may include: the names of the headers, in
# one list, read by gate.layering and by gate.abi. The files it is made of
# are not listed here: they are the SERIALIZE_SOURCES of the components'
# declarations (cmake/ConfigForgeComponent.cmake), which the gate reads.
set(CF_SERIALIZE_HEADERS
    cf_build_options.h
    cf_port.h
    cf_error.h
    cf_numeric.h
    cf_text.h
    cf_value.h
    cf_alloc.h
    cf_platform_file.h
    cf_serialize_version.h
    cf_serialize.h
    cf_yamlparse.h)
