# Numbers read, and floating point written.
cmake_minimum_required(VERSION 3.10)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(cf_numeric C)
  include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/ConfigForgeOptions.cmake)
endif()
include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/ConfigForgeComponent.cmake)

cf_component(
  numeric
  DEPENDS
  port
  SOURCES
  src/numeric.c
  SERIALIZE_SOURCES
  src/numeric.c)
