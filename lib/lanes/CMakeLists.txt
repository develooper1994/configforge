# lanes: span operations over bytes, a scalar reference and the processor's
# detection (D-101). Not ConfigForge's: it reads no variable of its, so
# `cmake -S lib/lanes` builds it alone and so does add_subdirectory() from
# any other project.
#
#   cmake -S lib/lanes -B build-lanes && cmake --build build-lanes
#   ctest --test-dir build-lanes
#
# LANES_BUILD_TESTS says whether its tests are built (with BUILD_TESTING):
# on when this is the top project, off when another project adds it, which
# sets it on to run them, or adds tests/ itself once its own testing is on
# (ConfigForge's tests/CMakeLists.txt does).
cmake_minimum_required(VERSION 3.10)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(
    lanes
    VERSION 0.1.0
    LANGUAGES C)
  include(CTest)
  set(lanes_top ON)
else()
  set(lanes_top OFF)
endif()
if(NOT DEFINED LANES_BUILD_TESTS)
  set(LANES_BUILD_TESTS ${lanes_top})
endif()

include(${CMAKE_CURRENT_LIST_DIR}/lanes_sources.cmake)

# C89, with the warnings a portable library keeps clean. A project that
# embeds lanes may add its own on top: it defines a function
# lanes_configure_target(<target>), which is called for every target made
# here, the tests' too.
function(lanes_target tgt)
  set_target_properties(
    ${tgt}
    PROPERTIES C_STANDARD 90
               C_STANDARD_REQUIRED ON
               C_EXTENSIONS OFF)
  if(MSVC)
    target_compile_options(${tgt} PRIVATE /W4)
  else()
    target_compile_options(${tgt} PRIVATE -Wall -Wextra -pedantic)
  endif()
  if(COMMAND lanes_configure_target)
    lanes_configure_target(${tgt})
  endif()
endfunction()

add_library(lanes STATIC ${LANES_SOURCES})
target_include_directories(
  lanes PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>)
target_compile_definitions(lanes PRIVATE LANES_BUILDING_LIBRARY=1)
lanes_target(lanes)

if(BUILD_TESTING AND LANES_BUILD_TESTS)
  add_subdirectory(tests)
endif()
