# The portability layer: compiler, limits, bounded formatting. It uses
# nothing.
cmake_minimum_required(VERSION 3.10)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(cf_port C)
  include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/ConfigForgeOptions.cmake)
endif()
include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/ConfigForgeComponent.cmake)

cf_component(
  port
  SOURCES
  src/port.c
  SERIALIZE_SOURCES
  src/port.c)
