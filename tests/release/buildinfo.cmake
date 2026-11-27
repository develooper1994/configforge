# scripts/release/buildinfo.cmake run on the build directory of this very
# test: the heading, the three lines and one option with its value.
cmake_minimum_required(VERSION 3.10)

execute_process(
  COMMAND "${CMAKE_COMMAND}" "-DBUILD=${BUILD}" -DNAME=test-archive
          "-DOUT=${OUT}" -P "${SRC}/scripts/release/buildinfo.cmake"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "buildinfo.cmake: exit ${result}: ${output}${error}")
endif()

file(READ "${OUT}" text)
set(cache "${BUILD}/CMakeCache.txt")
file(STRINGS "${cache}" path_max REGEX "^CF_PATH_MAX:[A-Z]+=")
string(REGEX REPLACE "^[^=]*=" "" path_max_value "${path_max}")

foreach(
  pattern
  "^### test-archive\n"
  "\nC compiler: [A-Za-z]+ [0-9]"
  "\nGenerator: [^\n]+\n"
  "\nBuild type: [^\n]+\n"
  "\| CF_PATH_MAX \| ${path_max_value} \|")
  if(NOT text MATCHES "${pattern}")
    message(FATAL_ERROR "no match for ${pattern} in:\n${text}")
  endif()
endforeach()
