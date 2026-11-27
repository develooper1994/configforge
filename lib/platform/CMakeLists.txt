# The operating system: posix, win32, none.
#
# One platform, chosen by the build. The whole point of the directory is
# that nothing above it names a platform macro: the build picks the file.
# Every implementation is also guarded internally, so the amalgamation -
# one translation unit with no build to choose for it - can carry all of
# them.
cmake_minimum_required(VERSION 3.10)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(cf_platform C)
  include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/ConfigForgeOptions.cmake)
endif()
if(NOT CF_WHOLE_PROJECT)
  include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/ConfigForgeCVersion.cmake)
endif()
include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/ConfigForgeComponent.cmake)

if(CF_PLATFORM STREQUAL "none")
  set(cf_platform_dir none)
elseif(WIN32)
  set(cf_platform_dir win32)
else()
  set(cf_platform_dir posix)
endif()

# The file opener is the one part the serialization library takes.
cf_component(
  platform
  DEPENDS
  port
  numeric
  base
  SOURCES
  src/${cf_platform_dir}/${cf_platform_dir}.c
  src/${cf_platform_dir}/file.c
  src/file_layer.c
  SERIALIZE_SOURCES
  src/${cf_platform_dir}/file.c
  src/file_layer.c)

if(CF_PLATFORM STREQUAL "none")
  foreach(t ${CF_TARGETS})
    target_compile_definitions(${t} PUBLIC CF_PLATFORM_NONE=1)
  endforeach()
endif()

# kqueue is in the C library on macOS and the BSDs. Anywhere else it is
# libkqueue, which only the test build asks for.
if(CF_WAKE_KQUEUE)
  target_compile_definitions(${CF_FULL_TARGET} PRIVATE CF_WAKE_KQUEUE=1)
  if(NOT APPLE AND NOT CMAKE_SYSTEM_NAME MATCHES "BSD|DragonFly")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(KQUEUE REQUIRED libkqueue)
    # For the SBOM, which is written from the top-level directory.
    set(_CF_KQUEUE_VERSION
        "${KQUEUE_VERSION}"
        CACHE INTERNAL "libkqueue version, for the SBOM")
    target_include_directories(${CF_FULL_TARGET} PRIVATE ${KQUEUE_INCLUDE_DIRS})
    # A property rather than target_link_libraries, which CMake 3.10 does
    # not allow on a target from another directory.
    set_property(
      TARGET ${CF_FULL_TARGET}
      APPEND
      PROPERTY LINK_LIBRARIES ${KQUEUE_LIBRARIES})
    set_property(
      TARGET ${CF_FULL_TARGET}
      APPEND
      PROPERTY INTERFACE_LINK_LIBRARIES ${KQUEUE_LIBRARIES})
  endif()
endif()

# macOS's user defaults are read through CoreFoundation, whose headers are
# not C89: posix.c, the one file that includes them, is compiled as C99
# there. Alone, here; in the whole project the top-level CMakeLists.txt.
if(NOT CF_WHOLE_PROJECT
   AND CF_WITH_PREFERENCES
   AND APPLE)
  cf_source_needs_c(src/posix/posix.c 99 REASON
                    "CoreFoundation's headers use long long (SInt64)")
  target_link_libraries(cf_platform PUBLIC "-framework CoreFoundation")
endif()

message(STATUS "platform: ${CF_PLATFORM}")
