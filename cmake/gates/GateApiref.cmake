# docs/API.md says what the headers say.
#
# The reference is generated from the header comments by
# scripts/apiref.cmake. This regenerates it into the build directory and
# compares: a function added, removed or re-documented without running
# the script is a reference that has started to lie, and this is where
# it stops. Line endings are ignored, because a Windows checkout may
# convert them and the words are what matter.

cmake_minimum_required(VERSION 3.10)

set(fresh "${BIN}/API.md")
execute_process(
  COMMAND ${CMAKE_COMMAND} -DOUT=${fresh} -P ${SRC}/scripts/apiref.cmake
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE out)
if(NOT rc EQUAL 0)
  message("${out}")
  message(FATAL_ERROR "the API reference could not be generated")
endif()

file(READ "${fresh}" want)
file(READ "${SRC}/docs/API.md" have)
string(REPLACE "\r" "" want "${want}")
string(REPLACE "\r" "" have "${have}")
if(NOT want STREQUAL have)
  message("VIOLATION: docs/API.md does not match the headers; "
          "run `cmake -P scripts/apiref.cmake` and commit the result")
  message(FATAL_ERROR "API reference gate failed")
endif()
message("API reference gate: clean")
