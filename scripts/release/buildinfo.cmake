# How one archive was built, read from the build's own cache (L-23):
#
#   cmake -DBUILD=<build dir> -DNAME=<archive> -DOUT=<file> -P buildinfo.cmake
#
# Writes Markdown: a heading, the compiler, the generator, the build type and
# a table of every CF_* option with its value. The cache is the only source:
# what it says is what was built. The release workflow puts the file in the
# release's notes.
cmake_minimum_required(VERSION 3.10)

foreach(var BUILD NAME OUT)
  if(NOT DEFINED ${var})
    message(
      FATAL_ERROR "usage: cmake -DBUILD=<dir> -DNAME=<archive> -DOUT=<file> "
                  "-P buildinfo.cmake")
  endif()
endforeach()
set(cache "${BUILD}/CMakeCache.txt")
if(NOT EXISTS "${cache}")
  message(FATAL_ERROR "${cache}: no such file")
endif()
file(STRINGS "${cache}" lines)

set(compiler_id "")
set(compiler_version "")
set(generator "")
set(build_type "")
set(options "")
foreach(line IN LISTS lines)
  # NAME:TYPE=value; a comment or an empty line does not match.
  if(NOT line MATCHES "^([A-Za-z0-9_]+):([A-Z]+)=(.*)$")
    continue()
  endif()
  set(key "${CMAKE_MATCH_1}")
  set(type "${CMAKE_MATCH_2}")
  set(value "${CMAKE_MATCH_3}")
  if(key STREQUAL "CMAKE_C_COMPILER_ID")
    set(compiler_id "${value}")
  elseif(key STREQUAL "CMAKE_C_COMPILER_VERSION")
    set(compiler_version "${value}")
  elseif(key STREQUAL "CMAKE_GENERATOR")
    set(generator "${value}")
  elseif(key STREQUAL "CMAKE_BUILD_TYPE")
    set(build_type "${value}")
  elseif(key MATCHES "^CF_" AND NOT type MATCHES "^(INTERNAL|FILEPATH|PATH)$")
    list(APPEND options "${key}")
    set(value_of_${key} "${value}")
  endif()
endforeach()
if(build_type STREQUAL "")
  set(build_type "multi-config, Release")
endif()
list(SORT options)

set(text "### ${NAME}\n\n")
string(APPEND text "C compiler: ${compiler_id} ${compiler_version}\n")
string(APPEND text "Generator: ${generator}\n")
string(APPEND text "Build type: ${build_type}\n\n")
string(APPEND text "| Option | Value |\n| ------ | ----- |\n")
foreach(key IN LISTS options)
  string(APPEND text "| ${key} | ${value_of_${key}} |\n")
endforeach()
file(WRITE "${OUT}" "${text}")
