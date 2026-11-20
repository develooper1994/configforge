# What every component shares: alloc, error, value.
cmake_minimum_required(VERSION 3.10)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(cf_base C)
  include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/ConfigForgeOptions.cmake)
endif()
include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/ConfigForgeComponent.cmake)

cf_component(
  base
  DEPENDS
  port
  numeric
  SOURCES
  src/error.c
  src/alloc.c
  src/value.c
  SERIALIZE_SOURCES
  src/error.c
  src/alloc.c
  src/value.c)
