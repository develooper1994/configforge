# The version is written in cf_version.h and nowhere else by hand.
# CMake reads it (project() gets it from there), the Conan recipe reads
# it, and the copies that cannot read anything are checked here: the
# vcpkg manifest and the CHANGELOG's newest section. A release that bumps
# one and forgets another ships a package whose version and reported
# version disagree, which is discovered by whoever files the bug rather
# than by whoever made it. `cmake -DTO=x.y.z -P scripts/version.cmake`
# changes them all.
cmake_minimum_required(VERSION 3.10)

include("${SRC}/cmake/ConfigForgeVersion.cmake")
configforge_read_version(from_header)
set(bad "")

if(DEFINED PROJECT_VER AND NOT PROJECT_VER STREQUAL from_header)
  list(APPEND bad "project() says ${PROJECT_VER}")
endif()

file(READ "${SRC}/packaging/vcpkg/configforge/vcpkg.json" manifest)
if(NOT manifest MATCHES "\"version\": \"([^\"]*)\"")
  list(APPEND bad "packaging/vcpkg/configforge/vcpkg.json has no version")
elseif(NOT CMAKE_MATCH_1 STREQUAL from_header)
  list(APPEND bad
       "packaging/vcpkg/configforge/vcpkg.json says ${CMAKE_MATCH_1}")
endif()

# The recipe has to read the header, not carry a number of its own.
file(READ "${SRC}/conanfile.py" recipe)
if(recipe MATCHES "\n[ \t]*version[ \t]*=")
  list(APPEND bad "conanfile.py sets a version instead of reading cf_version.h")
endif()

# The newest section is the version being built: dated once released,
# "unreleased" until then.
file(STRINGS "${SRC}/CHANGELOG.md" sections REGEX "^## ")
list(GET sections 0 newest)
if(NOT newest MATCHES "^## ${from_header}( |$)")
  list(APPEND bad "CHANGELOG.md's newest section is \"${newest}\"")
endif()

if(bad)
  foreach(b IN LISTS bad)
    message("VIOLATION: cf_version.h says ${from_header}, but ${b}")
  endforeach()
  message(FATAL_ERROR "version gate failed")
endif()
message("version gate: ${from_header} everywhere")
