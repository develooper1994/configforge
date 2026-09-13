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

message(STATUS "platform: ${CF_PLATFORM}")
