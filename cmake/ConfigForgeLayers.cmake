# What may use what (D-100): one line per component, the components it
# may include a header of, besides its own. A name with a colon,
# <component>:<header>, is one header of that component and no more.
# Read by cmake/gates/GateLayering.cmake. A component that does not exist
# yet is a line that waits for it.
set(CF_LAYER_COMPONENTS
    port
    lanes
    numeric
    base
    text
    platform
    serialize
    config
    cxx)

set(CF_LAYER_port "")
set(CF_LAYER_lanes "")
set(CF_LAYER_numeric port lanes)
set(CF_LAYER_base port numeric)
set(CF_LAYER_text port numeric base lanes)
set(CF_LAYER_platform port numeric base)
set(CF_LAYER_serialize
    port
    numeric
    base
    text
    lanes
    platform:cf_platform_file.h)
set(CF_LAYER_config
    port
    lanes
    numeric
    base
    text
    platform
    serialize)
set(CF_LAYER_cxx
    port
    lanes
    numeric
    base
    text
    platform
    serialize
    config)
