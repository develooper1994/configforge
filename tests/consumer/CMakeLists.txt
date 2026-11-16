# A project that has never heard of this source tree.
#
# Configured against an installed ConfigForge, not against the build
# directory, because that is what every consumer does and it is the only
# thing that proves the install list is complete.
cmake_minimum_required(VERSION 3.10)
project(cf_consumer LANGUAGES C)

find_package(ConfigForge REQUIRED COMPONENTS serialize)

add_executable(consumer consumer.c)
target_link_libraries(consumer PRIVATE ConfigForge::configforge)
set_target_properties(
  consumer
  PROPERTIES C_STANDARD 90
             C_STANDARD_REQUIRED ON
             C_EXTENSIONS OFF)

# The serialization library alone: no ConfigForge::configforge here, so
# the export set must not make one depend on the other.
add_executable(consumer_serialize serialize.c)
target_link_libraries(consumer_serialize
                      PRIVATE ConfigForge::configforge_serialize)
set_target_properties(
  consumer_serialize
  PROPERTIES C_STANDARD 90
             C_STANDARD_REQUIRED ON
             C_EXTENSIONS OFF)

if(CF_CONSUMER_CXX)
  enable_language(CXX)
  add_executable(consumer_cpp consumer.cpp)
  target_link_libraries(consumer_cpp PRIVATE ConfigForge::configforge)
  set_target_properties(consumer_cpp PROPERTIES CXX_STANDARD 98
                                                CXX_STANDARD_REQUIRED ON)
endif()
