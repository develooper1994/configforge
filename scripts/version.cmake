# The version: print it, or change it everywhere at once.
#
#   cmake -P scripts/version.cmake                 prints x.y.z
#   cmake -DTO=x.y.z -P scripts/version.cmake      changes it
#
# cf_version.h is the one place the version is written; CMake and the
# Conan recipe read it. Changing it here also rewrites the copy that
# cannot read it - the vcpkg manifest, the Rust crate's and the Python
# package's - and opens a "## x.y.z — unreleased"
# section at the top of CHANGELOG.md if there is none, so that
# gate.version passes the moment this has run. A release is then the
# CHANGELOG heading dated and the tag vx.y.z pushed; the release workflow
# refuses a tag that disagrees with cf_version.h.

cmake_minimum_required(VERSION 3.10)

get_filename_component(root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
include("${root}/cmake/ConfigForgeVersion.cmake")
configforge_read_version(current)

if(NOT DEFINED TO)
  # stdout, so a shell can take it: built=$(cmake -P scripts/version.cmake)
  execute_process(COMMAND ${CMAKE_COMMAND} -E echo "${current}")
  return()
endif()

if(NOT TO MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
  message(FATAL_ERROR "version: TO must be MAJOR.MINOR.PATCH, not \"${TO}\"")
endif()
set(major "${CMAKE_MATCH_1}")
set(minor "${CMAKE_MATCH_2}")
set(patch "${CMAKE_MATCH_3}")
# CF_VERSION_NUMBER is MAJOR * 10000 + MINOR * 100 + PATCH.
if(minor GREATER 99 OR patch GREATER 99)
  message(FATAL_ERROR "version: minor and patch must be below 100 "
                      "(CF_VERSION_NUMBER packs them in two digits each)")
endif()

# A file rewritten byte for byte except for what changes.
function(rewrite rel)
  file(READ "${root}/${rel}" text)
  set(new "${text}")
  set(i 0)
  list(LENGTH ARGN n)
  while(i LESS n)
    list(GET ARGN ${i} re)
    math(EXPR j "${i} + 1")
    list(GET ARGN ${j} with)
    string(REGEX REPLACE "${re}" "${with}" new "${new}")
    math(EXPR i "${i} + 2")
  endwhile()
  if(NOT new STREQUAL text)
    configforge_write_lf("${root}/${rel}" "${new}")
    message("version: ${rel}")
  endif()
endfunction()

rewrite(
  include/configforge/cf_version.h
  "#define CF_VERSION_MAJOR [0-9]+"
  "#define CF_VERSION_MAJOR ${major}"
  "#define CF_VERSION_MINOR [0-9]+"
  "#define CF_VERSION_MINOR ${minor}"
  "#define CF_VERSION_PATCH [0-9]+"
  "#define CF_VERSION_PATCH ${patch}")
rewrite(packaging/vcpkg/configforge/vcpkg.json "\"version\": \"[^\"]*\""
        "\"version\": \"${TO}\"")
# The bindings' packages, whose tools read only their own manifests.
rewrite(bindings/rust/Cargo.toml "
version = \"[^\"]*\"" "
version = \"${TO}\"")
rewrite(bindings/python/pyproject.toml "
version = \"[^\"]*\"" "
version = \"${TO}\"")
rewrite(bindings/python/src/configforge/__init__.py "__version__ = \"[^\"]*\""
        "__version__ = \"${TO}\"")

file(STRINGS "${root}/CHANGELOG.md" sections REGEX "^## ")
list(GET sections 0 newest)
if(NOT newest MATCHES "^## ${TO}( |$)")
  file(READ "${root}/CHANGELOG.md" log)
  string(FIND "${log}" "${newest}" at)
  string(SUBSTRING "${log}" 0 ${at} head)
  string(SUBSTRING "${log}" ${at} -1 tail)
  configforge_write_lf("${root}/CHANGELOG.md"
                       "${head}## ${TO} — unreleased\n\n${tail}")
  message("version: CHANGELOG.md, a section for ${TO}")
endif()
message("version: ${current} -> ${TO}")
