# The serialization layer. It may use the leaf types - values, errors,
# arenas - and may not touch the key space, the definitions or the
# sources. The boundary is checked in CI so that extracting this layer
# later stays a directory move and a build file.
target_sources(
  configforge
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/blob.c
          ${CMAKE_CURRENT_SOURCE_DIR}/codec.c
          ${CMAKE_CURRENT_SOURCE_DIR}/detect.c)

if(CF_WITH_INI)
  target_sources(configforge PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/ini.c)
endif()
if(CF_WITH_JSON)
  target_sources(configforge PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/json.c)
endif()
if(CF_WITH_TOML)
  target_sources(configforge PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/toml.c)
endif()
if(CF_WITH_PROPERTIES)
  target_sources(configforge PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/properties.c)
endif()

# YAML, read by one of three parsers behind the one codec (D-62, D-69):
# the built-in one (yamlparse.c), which reads all of yaml-test-suite and
# needs nothing, and which auto takes; libfyaml, complete too; and
# libyaml, which follows YAML 1.1. A library is found as a header and a
# library rather than through a package, because Debian, Homebrew and
# vcpkg agree on those two names and on nothing else. The build says which
# parser it took.
set(CF_YAML_BACKEND
    auto
    CACHE STRING
          "YAML parser: auto (the built-in one), builtin, libfyaml or libyaml")
set_property(
  CACHE CF_YAML_BACKEND
  PROPERTY STRINGS
           auto
           builtin
           libfyaml
           libyaml)
set(_CF_YAML_LIBFYAML
    OFF
    CACHE INTERNAL "The YAML codec reads through libfyaml")
set(_CF_YAML_LIBYAML
    OFF
    CACHE INTERNAL "The YAML codec reads through libyaml")
if(CF_WITH_YAML)
  if(NOT CF_YAML_BACKEND MATCHES "^(auto|builtin|libfyaml|libyaml)$")
    message(
      FATAL_ERROR "CF_YAML_BACKEND is auto, builtin, libfyaml or libyaml, "
                  "not \"${CF_YAML_BACKEND}\"")
  endif()
  # libfyaml before 0.9.1 hangs on malformed UTF-8 in a directive
  # (libfyaml issue 101, fixed by 7f7be36); this library's fuzzer found it
  # in Ubuntu 24.04's 0.8. The least version is held in three places, all
  # from this one: here, where the header carries no version and it is
  # read from the pkg-config file beside the library (a libfyaml whose
  # version cannot be read is not taken either); in yaml.c, which asks the
  # library it runs against, because 0.8 and 0.9 share a soname and a
  # program built against one loads the other; and in configforge.pc's
  # Requires.private, for a consumer that links it statically.
  set(fyaml_min 0.9.1)
  set(_CF_FYAML_MIN
      ${fyaml_min}
      CACHE INTERNAL "The least libfyaml this library reads YAML through")
  set(include "")
  set(library "")
  if(CF_YAML_BACKEND STREQUAL "libfyaml")
    set(fyaml_ok FALSE)
    set(fyaml_why "it is not installed")
    find_path(CF_FYAML_INCLUDE_DIR libfyaml.h)
    find_library(CF_FYAML_LIBRARY NAMES fyaml libfyaml)
    if(CF_FYAML_INCLUDE_DIR AND CF_FYAML_LIBRARY)
      get_filename_component(fyaml_dir "${CF_FYAML_LIBRARY}" DIRECTORY)
      set(fyaml_version "")
      if(EXISTS "${fyaml_dir}/pkgconfig/libfyaml.pc")
        file(STRINGS "${fyaml_dir}/pkgconfig/libfyaml.pc" fyaml_line
             REGEX "^Version:")
        string(REGEX REPLACE "^Version:[ 	]*" "" fyaml_version "${fyaml_line}")
      endif()
      if(fyaml_version STREQUAL "")
        set(fyaml_why
            "its version is unknown (no ${fyaml_dir}/pkgconfig/libfyaml.pc)")
      elseif(fyaml_version VERSION_LESS fyaml_min)
        string(
          CONCAT fyaml_why "it is ${fyaml_version}, and before "
                 "${fyaml_min} it hangs on malformed input (libfyaml issue 101)"
        )
      else()
        set(fyaml_ok TRUE)
      endif()
    endif()
    if(NOT fyaml_ok)
      message(
        FATAL_ERROR
          "CF_YAML_BACKEND=libfyaml needs libfyaml ${fyaml_min} or later, and "
          "${fyaml_why}: install a newer one or point CMAKE_PREFIX_PATH at it")
    endif()
    set(_CF_YAML_LIBFYAML
        ON
        CACHE INTERNAL "The YAML codec reads through libfyaml")
    string(REPLACE "." ";" fyaml_parts "${fyaml_min}")
    list(GET fyaml_parts 0 fyaml_major)
    list(GET fyaml_parts 1 fyaml_minor)
    list(GET fyaml_parts 2 fyaml_patch)
    target_compile_definitions(
      configforge
      PRIVATE CF_YAML_LIBFYAML=1 CF_FYAML_MIN_MAJOR=${fyaml_major}
              CF_FYAML_MIN_MINOR=${fyaml_minor}
              CF_FYAML_MIN_PATCH=${fyaml_patch})
    set(include "${CF_FYAML_INCLUDE_DIR}")
    set(library "${CF_FYAML_LIBRARY}")
    message(STATUS "YAML: libfyaml ${fyaml_version} (${library})")
  elseif(CF_YAML_BACKEND STREQUAL "libyaml")
    find_path(CF_LIBYAML_INCLUDE_DIR yaml.h)
    find_library(CF_LIBYAML_LIBRARY NAMES yaml libyaml yaml-0)
    if(NOT CF_LIBYAML_INCLUDE_DIR OR NOT CF_LIBYAML_LIBRARY)
      message(
        FATAL_ERROR
          "CF_YAML_BACKEND=libyaml needs libyaml: install it (libyaml-dev, "
          "brew install libyaml, vcpkg install libyaml) or point "
          "CMAKE_PREFIX_PATH at it")
    endif()
    set(_CF_YAML_LIBYAML
        ON
        CACHE INTERNAL "The YAML codec reads through libyaml")
    target_compile_definitions(configforge PRIVATE CF_YAML_LIBYAML=1)
    set(include "${CF_LIBYAML_INCLUDE_DIR}")
    set(library "${CF_LIBYAML_LIBRARY}")
    message(STATUS "YAML: libyaml (${library})")
  else()
    message(STATUS "YAML: the built-in parser")
  endif()
  # The chosen parser, for the link, the gates and the package files: none
  # for the built-in one.
  set(CF_YAML_INCLUDE_DIR
      "${include}"
      CACHE PATH "The YAML parser's headers" FORCE)
  set(CF_YAML_LIBRARY
      "${library}"
      CACHE FILEPATH "The YAML parser's library" FORCE)
  target_sources(configforge PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/yaml.c
                                     ${CMAKE_CURRENT_SOURCE_DIR}/yamlparse.c)
  if(_CF_YAML_LIBFYAML)
    # libfyaml's header is a system one: this library's warnings are for
    # its own code, and yaml.c still gets every one of them. It needs C11,
    # so yaml.c is compiled as C11 (the top-level CMakeLists.txt, where the
    # target is made).
    target_include_directories(configforge SYSTEM
                               PRIVATE ${CF_YAML_INCLUDE_DIR})
  elseif(_CF_YAML_LIBYAML)
    target_include_directories(configforge PRIVATE ${CF_YAML_INCLUDE_DIR})
  endif()
  # Linked in the top-level CMakeLists.txt, where the target is made:
  # under the policies of CMake 3.10 a subdirectory may not link it.
endif()

# XML through expat, for the same reason and in the same way: expat's
# header and library carry the same names everywhere it is packaged.
if(CF_WITH_XML)
  find_path(CF_EXPAT_INCLUDE_DIR expat.h)
  find_library(CF_EXPAT_LIBRARY NAMES expat libexpat libexpatMD)
  if(NOT CF_EXPAT_INCLUDE_DIR OR NOT CF_EXPAT_LIBRARY)
    message(
      FATAL_ERROR
        "CF_WITH_XML needs expat: install it (libexpat1-dev, brew install "
        "expat, vcpkg install expat) or point CMAKE_PREFIX_PATH at it")
  endif()
  target_sources(configforge PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/xml.c)
  target_include_directories(configforge PRIVATE ${CF_EXPAT_INCLUDE_DIR})
endif()
