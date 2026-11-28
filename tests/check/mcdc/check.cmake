# check.mcdc: scripts/check/mcdc.py reads GCC's condition coverage. The
# fixture t.c is built with -fcondition-coverage, run, and read: 2 of 4
# outcomes (50%) pass --min 50 and fail --min 51; a file outside the root is
# a failure.
#   cmake -DSRC=<source root> -DBIN=<dir> -DCC=<gcc> -DCC_ID=<id>
#         -DCC_VER=<version> -DPY=<python or empty> -P check.cmake
cmake_minimum_required(VERSION 3.10)

if(NOT CC_ID STREQUAL "GNU" OR CC_VER VERSION_LESS 14)
  message(STATUS "check.mcdc skipped: needs GCC 14 or later (have ${CC_ID} "
                 "${CC_VER})")
  return()
endif()
if(NOT PY)
  message(STATUS "check.mcdc skipped: no Python 3")
  return()
endif()

# gcov of the compiler's own version: gcc-15 -> gcov-15.
get_filename_component(cc_dir "${CC}" DIRECTORY)
get_filename_component(cc_name "${CC}" NAME)
set(gcov_name gcov)
if(cc_name MATCHES "^(.*)gcc(.*)$")
  set(gcov_name "${CMAKE_MATCH_1}gcov${CMAKE_MATCH_2}")
endif()
find_program(
  GCOV
  NAMES "${gcov_name}" gcov
  HINTS "${cc_dir}")
if(NOT GCOV)
  message(STATUS "check.mcdc skipped: no gcov")
  return()
endif()

set(work "${BIN}/work")
file(REMOVE_RECURSE "${work}")
file(MAKE_DIRECTORY "${work}")
file(COPY "${SRC}/tests/check/mcdc/t.c" DESTINATION "${work}")
execute_process(
  COMMAND "${CC}" -fcondition-coverage --coverage -o t t.c
  WORKING_DIRECTORY "${work}"
  RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "check.mcdc: the fixture did not build")
endif()
execute_process(COMMAND "${work}/t" RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "check.mcdc: the fixture did not run")
endif()

set(mcdc "${SRC}/scripts/check/mcdc.py")
set(report "${work}/report.txt")
foreach(case 50:0 51:1)
  string(REPLACE ":" ";" parts "${case}")
  list(GET parts 0 min)
  list(GET parts 1 want)
  execute_process(
    COMMAND "${PY}" "${mcdc}" "${work}" --root "${work}" --filter "t[.]c$" --min
            ${min} --report "${report}" --gcov "${GCOV}"
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err)
  if(NOT rc EQUAL want)
    message(FATAL_ERROR "check.mcdc: --min ${min} exited ${rc}, wanted "
                        "${want}\n${out}${err}")
  endif()
  if(NOT out MATCHES "mcdc: 2/4 outcomes [(]50[.]0%[)] in 1 files")
    message(FATAL_ERROR "check.mcdc: the summary line is wrong:\n${out}")
  endif()
endforeach()
file(READ "${report}" lines)
if(NOT lines MATCHES "^50[.]0% 2/4 t[.]c")
  message(FATAL_ERROR "check.mcdc: the report line is wrong:\n${lines}")
endif()

file(MAKE_DIRECTORY "${BIN}/elsewhere")
# A file that is not under the root is a failure, whatever the floor.
execute_process(
  COMMAND "${PY}" "${mcdc}" "${work}" --root "${BIN}/elsewhere" --filter "."
          --min 0 --gcov "${GCOV}"
  RESULT_VARIABLE rc
  OUTPUT_QUIET ERROR_QUIET)
if(rc EQUAL 0)
  message(FATAL_ERROR "check.mcdc: a file outside the root was accepted")
endif()
message(STATUS "check.mcdc: ok")
