# What to run before staging a change, in minutes rather than a verify's
# hour: the things that failed late and one at a time in the round of
# 2026-11-05, each costing a whole verify to find.
#
#   cmake -P scripts/prestage.cmake            [-DWORK=dir] [-DANALYZE=OFF]
#
# 1. format and lint what changed and regenerate the README feature matrix
#    (scripts/format.cmake, gate.matrix);
# 2. regenerate docs/Codemap.md and docs/API.md, whose line references
#    move with edits (gate.codemap and gate.apiref);
# 3. the text gate: no NUL and no CR (gate.text);
# 4. the analyze build - GCC's -fanalyzer and cppcheck - where GCC is the
#    compiler, since it found what the default build did not.
#
# It updates formatting, Codemap.md and API.md in the working tree.
# Review the changes, then stage and verify. Like verify, this script
# uses CMake so it can run wherever the build does.

cmake_minimum_required(VERSION 3.10)

get_filename_component(REPO "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
if(NOT DEFINED WORK)
  set(WORK "${REPO}/build-prestage")
endif()
if(NOT DEFINED ANALYZE)
  set(ANALYZE ON)
endif()

# Runs one step in the repository and stops the script if it fails.
function(stage name)
  execute_process(
    COMMAND ${ARGN}
    WORKING_DIRECTORY "${REPO}"
    RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "prestage: ${name} failed")
  endif()
  message("prestage: ${name} ok")
endfunction()

stage("format" ${CMAKE_COMMAND} -P "${REPO}/scripts/format.cmake")
stage("tree map" ${CMAKE_COMMAND} -P "${REPO}/scripts/generate/codemap.cmake")
stage("API reference" ${CMAKE_COMMAND} -P
      "${REPO}/scripts/generate/apiref.cmake")
find_program(PYTHON_EXE NAMES python3 python)
if(PYTHON_EXE)
  stage("text" ${PYTHON_EXE} "${REPO}/scripts/check/check_text.py" "${REPO}")
endif()

if(ANALYZE)
  find_program(GCC_EXE gcc)
  if(NOT GCC_EXE)
    message("prestage: analyze skipped: it is GCC's (-fanalyzer)")
  else()
    stage(
      "analyze configure"
      ${CMAKE_COMMAND}
      --preset
      analyze
      -B
      "${WORK}")
    stage(
      "analyze build"
      ${CMAKE_COMMAND}
      --build
      "${WORK}"
      --parallel)
  endif()
endif()
message("prestage: done; look at the diff, stage it, then verify")
