# Fetches the libraries the benchmark's comparison reads documents through,
# each at the commit bench/compare/libraries.cmake pins (D-111):
#
#   cmake -DLIBS=<dir> -P scripts/bench/fetch.cmake
#
# Done the way scripts/verify.cmake fetches the conformance suites (git init,
# a shallow fetch of the commit, checkout), and not by FetchContent, which
# would fetch at configure time, and not by submodules. A library whose HEAD
# already is the commit is left alone. Optional: -DONLY=<name>[;<name>].
cmake_minimum_required(VERSION 3.16)

if(NOT LIBS)
  message(FATAL_ERROR "fetch: give the directory: -DLIBS=<dir>")
endif()
get_filename_component(here "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
include("${here}/../../bench/compare/libraries.cmake")
find_program(GIT_EXE git REQUIRED)

set(names ${CFB_LIB_NAMES})
if(ONLY)
  set(names ${ONLY})
endif()

# cfb_git(<dir> <git arguments>...): git in <dir>, and a stop with its output on
# failure.
function(cfb_git dir)
  execute_process(
    COMMAND "${GIT_EXE}" -C "${dir}" -c init.defaultBranch=main -c
            core.autocrlf=false ${ARGN}
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE out)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "fetch: git ${ARGN} in ${dir}: ${out}")
  endif()
endfunction()

foreach(name ${names})
  set(url "${LIB_${name}_URL}")
  set(commit "${LIB_${name}_COMMIT}")
  if(NOT url OR NOT commit)
    message(
      FATAL_ERROR "fetch: ${name} is not in bench/compare/libraries.cmake")
  endif()
  set(dir "${LIBS}/${name}")
  execute_process(
    COMMAND "${GIT_EXE}" -C "${dir}" rev-parse HEAD
    OUTPUT_VARIABLE have
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET
    RESULT_VARIABLE rc)
  if(rc EQUAL 0 AND have STREQUAL commit)
    message(STATUS "fetch: ${name} is at ${commit}")
    continue()
  endif()
  file(REMOVE_RECURSE "${dir}")
  file(MAKE_DIRECTORY "${dir}")
  cfb_git("${dir}" init -q)
  cfb_git(
    "${dir}"
    fetch
    -q
    --depth
    1
    "${url}"
    "${commit}")
  cfb_git("${dir}" checkout -q FETCH_HEAD)
  execute_process(
    COMMAND "${GIT_EXE}" -C "${dir}" rev-parse HEAD
    OUTPUT_VARIABLE have
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT have STREQUAL commit)
    message(FATAL_ERROR "fetch: ${name}: HEAD is ${have}, not ${commit}")
  endif()
  message(STATUS "fetch: ${name} at ${commit}")
endforeach()
