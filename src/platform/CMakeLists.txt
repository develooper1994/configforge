# One platform, chosen by the build.
#
# The whole point of this directory is that nothing above it names a
# platform macro: the build picks the file. Every implementation is also
# guarded internally, so the amalgamation - one translation unit with no
# build to choose for it - can carry all of them.
if(CF_PLATFORM STREQUAL "none")
  target_sources(configforge PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/none/none.c)
  target_compile_definitions(configforge PUBLIC CF_PLATFORM_NONE=1)
elseif(WIN32)
  target_sources(configforge PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/win32/win32.c)
else()
  target_sources(configforge PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/posix/posix.c)
endif()

# kqueue is in the C library on macOS and the BSDs. Anywhere else it is
# libkqueue, which only the test build asks for.
if(CF_WAKE_KQUEUE)
  target_compile_definitions(configforge PRIVATE CF_WAKE_KQUEUE=1)
  if(NOT APPLE AND NOT CMAKE_SYSTEM_NAME MATCHES "BSD|DragonFly")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(KQUEUE REQUIRED libkqueue)
    # For the SBOM, which is written from the top-level directory.
    set(_CF_KQUEUE_VERSION
        "${KQUEUE_VERSION}"
        CACHE INTERNAL "libkqueue version, for the SBOM")
    target_include_directories(configforge PRIVATE ${KQUEUE_INCLUDE_DIRS})
    # A property rather than target_link_libraries, which CMake 3.10 does
    # not allow on a target from another directory.
    set_property(
      TARGET configforge
      APPEND
      PROPERTY LINK_LIBRARIES ${KQUEUE_LIBRARIES})
    set_property(
      TARGET configforge
      APPEND
      PROPERTY INTERFACE_LINK_LIBRARIES ${KQUEUE_LIBRARIES})
  endif()
endif()

message(STATUS "platform: ${CF_PLATFORM}")
