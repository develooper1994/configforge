# The version exists in cf_version.h and is repeated in project(). A
# release that bumps one and forgets the other ships a library whose
# package version and reported version disagree, which is discovered by
# whoever files the bug rather than by whoever made it.
file(READ "${SRC}/include/configforge/cf_version.h" text)

foreach(part MAJOR MINOR PATCH)
  string(REGEX MATCH "#define CF_VERSION_${part}[ \t]+([0-9]+)" _m "${text}")
  if(NOT _m)
    message(FATAL_ERROR "cf_version.h has no CF_VERSION_${part}")
  endif()
  set(HDR_${part} "${CMAKE_MATCH_1}")
endforeach()

set(from_header "${HDR_MAJOR}.${HDR_MINOR}.${HDR_PATCH}")
if(NOT from_header STREQUAL PROJECT_VER)
  message(FATAL_ERROR
          "version drift: cf_version.h says ${from_header}, "
          "CMakeLists.txt says ${PROJECT_VER}")
endif()
message("version gate: ${from_header}")
