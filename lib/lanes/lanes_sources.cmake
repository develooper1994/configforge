# What lanes is made of, and the one option that says how much of it is
# built. Included by lib/lanes/CMakeLists.txt, and by whoever compiles the
# sources into a library of its own (ConfigForge's, under its prefix:
# cmake/ConfigForgeComponent.cmake). It reads no variable of ConfigForge's.
#
# LANES_SOURCES is every .c file, as absolute paths.

# What is compiled (D-101): `scalar`, the reference alone; `c89`, scalar and
# the word-at-a-time backends; `all`, every backend the compiler can build.
# Until the other backends exist every value builds scalar alone.
set(CF_LANES_BACKENDS
    "all"
    CACHE STRING "The backends of lanes to build: scalar, c89 or all")
set_property(CACHE CF_LANES_BACKENDS PROPERTY STRINGS scalar c89 all)
if(NOT CF_LANES_BACKENDS MATCHES "^(scalar|c89|all)$")
  message(
    FATAL_ERROR
      "CF_LANES_BACKENDS is '${CF_LANES_BACKENDS}': it is scalar, c89 or all")
endif()

set(LANES_SOURCES "${CMAKE_CURRENT_LIST_DIR}/src/lanes.c"
                  "${CMAKE_CURRENT_LIST_DIR}/src/scalar.c")
