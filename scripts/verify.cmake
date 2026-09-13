# Everything that has to be true before a commit.
#
#   cmake -P scripts/verify.cmake
#   cmake -DMODE=quick -P scripts/verify.cmake
#
# Written in CMake rather than in a shell, because a shell script covers
# half the platforms this library claims and the other half would need a
# batch file kept in step with it - which is the duplication the
# hand-written Makefile was deleted to avoid (D-31). CMake is already
# required to build, so this driver adds nothing at all.
#
# It exists because chaining the steps by hand got a failing tree
# committed once: `ctest | grep passed` matched the word inside "98%
# tests passed". A checkpoint of a broken tree is worse than none, so
# this stops at the first failure and prints that step's output.

cmake_minimum_required(VERSION 3.10)

get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

if(NOT DEFINED MODE)
  set(MODE "full")
endif()

if(MODE STREQUAL "quick")
  set(PRESETS default)
else()
  set(PRESETS default sanitize portable minimal analyze)
endif()

# Run one step. Anything other than a zero exit stops the whole run and
# shows the output, because a verification that continues past a failure
# is a verification that reports the last thing instead of the first
# broken thing.
function(step label)
  # Padded so the column of results lines up: the output is what a
  # person scans to find the first failure.
  set(padded "${label}")
  string(LENGTH "${padded}" len)
  while(len LESS 22)
    string(APPEND padded " ")
    math(EXPR len "${len} + 1")
  endwhile()

  execute_process(
    COMMAND ${ARGN}
    WORKING_DIRECTORY "${ROOT}"
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE out)
  if(rc EQUAL 0)
    message("${padded}ok")
  else()
    message("${padded}FAILED")
    message("${out}")
    message(FATAL_ERROR "verification stopped at: ${label}")
  endif()
endfunction()

foreach(p ${PRESETS})
  step("${p} configure" ${CMAKE_COMMAND} --preset ${p})
  step("${p} build" ${CMAKE_COMMAND} --build --preset ${p})
  step("${p} test" ${CMAKE_CTEST_COMMAND} --preset ${p})
endforeach()

# The formatter rewrites and the gate reports, so this only checks:
# `cmake --build build --target format` is the fix. A verification that
# edited the tree would hide a disagreement instead of surfacing it.
message("")
message("all green")
