# The formats, codecs and document events. It may include the headers
# listed in cmake/ConfigForgeSerialize.cmake and no others, and calls only
# itself and the leaf files its dependencies declare (SERIALIZE_SOURCES);
# gate.layering checks it, so that this layer can be a library of its own.
#
# It is: configforge_serialize (D-90). Both libraries compile these files,
# so libconfigforge carries every function of the serialization library.
cmake_minimum_required(VERSION 3.10)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(cf_serialize C)
  include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/ConfigForgeOptions.cmake)
endif()
include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/ConfigForgeComponent.cmake)

include(${CMAKE_CURRENT_LIST_DIR}/backends.cmake)

set(cf_serialize_sources
    src/blob.c
    src/codec.c
    src/detect.c
    src/tlv.c
    src/version.c)
foreach(
  f
  INI:ini
  JSON:json
  TOML:toml
  PROPERTIES:properties
  CBOR:cbor)
  string(REPLACE ":" ";" parts "${f}")
  list(GET parts 0 option)
  list(GET parts 1 file)
  if(CF_WITH_${option})
    list(APPEND cf_serialize_sources src/${file}.c)
  endif()
endforeach()
list(APPEND cf_serialize_sources ${cf_serialize_files})

cf_component(
  serialize
  DEPENDS
  port
  numeric
  base
  lanes
  platform:cf_platform_file.h
  SOURCES
  ${cf_serialize_sources}
  SERIALIZE_SOURCES
  ${cf_serialize_sources})

foreach(t ${CF_TARGETS})
  if(cf_serialize_defs)
    target_compile_definitions(${t} PRIVATE ${cf_serialize_defs})
  endif()
  if(cf_serialize_sysinclude)
    target_include_directories(${t} SYSTEM PRIVATE ${cf_serialize_sysinclude})
  endif()
  if(cf_serialize_include)
    target_include_directories(${t} PRIVATE ${cf_serialize_include})
  endif()
endforeach()

# Alone, the target is made here, so its parser is linked and yaml.c
# compiled as C11 here, as the top-level CMakeLists.txt does for the two
# libraries (a source file's properties and a link belong to the directory
# of the target under the policies of CMake 3.10).
if(NOT CF_WHOLE_PROJECT)
  if(CF_WITH_YAML)
    target_link_libraries(cf_serialize PRIVATE ${CF_YAML_LIBRARY})
    if(_CF_YAML_LIBFYAML AND NOT MSVC)
      set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/src/yaml.c
                                  PROPERTIES COMPILE_OPTIONS "-std=c11")
    endif()
  endif()
  if(CF_WITH_XML)
    target_link_libraries(cf_serialize PRIVATE ${CF_EXPAT_LIBRARY})
  endif()
endif()
