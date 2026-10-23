# The generated SBOM is a valid SPDX document describing this build.
#
# A bill of materials nobody can parse is no bill of materials, and one
# whose version or licence disagrees with the project is worse than
# none, because somebody's compliance check believes it.

# A script runs with every policy unset, and IN_LIST below needs CMP0057;
# without this line it worked under CMake 4 and failed under 3.28.
cmake_minimum_required(VERSION 3.10)

if(CMAKE_VERSION VERSION_LESS 3.19)
  message("sbom gate: CMake ${CMAKE_VERSION} cannot read JSON, skipped")
  return()
endif()

file(READ "${SBOM}" doc)

string(
  JSON
  spdx
  ERROR_VARIABLE
  err
  GET
  "${doc}"
  spdxVersion)
if(NOT err STREQUAL "NOTFOUND" OR NOT spdx STREQUAL "SPDX-2.3")
  message(FATAL_ERROR "the SBOM is not an SPDX 2.3 document: ${err}")
endif()

string(
  JSON
  name
  GET
  "${doc}"
  packages
  0
  name)
string(
  JSON
  version
  GET
  "${doc}"
  packages
  0
  versionInfo)
string(
  JSON
  licence
  GET
  "${doc}"
  packages
  0
  licenseDeclared)
if(NOT name STREQUAL "configforge"
   OR NOT version STREQUAL "${PROJECT_VER}"
   OR NOT licence STREQUAL "Zlib")
  message(FATAL_ERROR "the SBOM describes ${name} ${version} under ${licence}, "
                      "not configforge ${PROJECT_VER} under Zlib")
endif()

# Every relationship points at a package the document declares.
string(JSON npkg LENGTH "${doc}" packages)
string(JSON nrel LENGTH "${doc}" relationships)
set(ids "SPDXRef-DOCUMENT")
set(i 0)
while(i LESS npkg)
  string(
    JSON
    id
    GET
    "${doc}"
    packages
    ${i}
    SPDXID)
  list(APPEND ids "${id}")
  math(EXPR i "${i} + 1")
endwhile()
set(i 0)
while(i LESS nrel)
  string(
    JSON
    to
    GET
    "${doc}"
    relationships
    ${i}
    relatedSpdxElement)
  if(NOT to IN_LIST ids)
    message(FATAL_ERROR "the SBOM relates to '${to}', which it never declares")
  endif()
  math(EXPR i "${i} + 1")
endwhile()

message("sbom gate: clean, ${npkg} package(s)")
