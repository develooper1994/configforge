# The text the readers share: UTF-8, a hexadecimal digit, a table's hash.
cmake_minimum_required(VERSION 3.10)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(cf_text C)
  include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/ConfigForgeOptions.cmake)
endif()
include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/ConfigForgeComponent.cmake)

cf_component(
  text
  DEPENDS
  port
  numeric
  base
  lanes
  SOURCES
  src/utf8.c
  src/hex.c
  src/hash.c
  src/encoding.c
  src/place.c
  src/escape.c
  src/refs.c
  src/space.c
  src/track.c
  src/fail.c
  SERIALIZE_SOURCES
  src/utf8.c
  src/hex.c
  src/hash.c
  src/encoding.c
  src/place.c
  src/escape.c
  src/refs.c
  src/space.c
  src/track.c
  src/fail.c)
