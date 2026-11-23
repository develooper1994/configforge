cmake_minimum_required(VERSION 3.10)

set(data "${CMAKE_CURRENT_LIST_DIR}/data")
if(MARKERS AND DEFINED CASE)
  include("${SRC}/scripts/generate/matrix.cmake")
  file(READ "${data}/markers_${CASE}.md" readme)
  set(block_one "new one\n")
  set(block_two "new two\n")
  cf_matrix_replace("${readme}" "one;two" block actual)
  file(READ "${data}/markers_expected.md" expected)
  if(NOT actual STREQUAL expected)
    message(FATAL_ERROR "marker replacement differs from markers_expected.md")
  endif()
  return()
endif()

if(DEFINED CASE)
  include("${SRC}/scripts/generate/matrix.cmake")
  cf_targets_read("${data}/targets_${CASE}.txt" "${data}/ci_jobs.txt" rows)
  if(NOT rows)
    message(FATAL_ERROR "no targets read")
  endif()
  return()
endif()

function(check_case group case expected)
  execute_process(
    COMMAND
      "${CMAKE_COMMAND}" "-DSRC=${SRC}" -DMATRIX_LIB=ON "-DMARKERS=${MARKERS}"
      "-DCASE=${case}" -P "${CMAKE_CURRENT_LIST_FILE}"
    RESULT_VARIABLE result
    ERROR_VARIABLE error)
  if(expected STREQUAL "")
    if(NOT result EQUAL 0)
      message(FATAL_ERROR "${group}_${case}: ${error}")
    endif()
  else()
    string(REGEX REPLACE " " "[ \n]+" pattern "${expected}")
    if(result EQUAL 0 OR NOT error MATCHES "${pattern}")
      message(
        FATAL_ERROR "${group}_${case}: expected '${expected}'; got ${error}")
    endif()
  endif()
endfunction()

if(MARKERS)
  check_case(markers ok "")
  check_case(markers missing "README.md: marker 'two' is missing")
  check_case(markers duplicate "README.md: marker 'one' is duplicated")
  check_case(markers unpaired "README.md: marker 'one' is unpaired")
  check_case(markers unknown "README.md: marker 'three' is unknown")
else()
  check_case(targets ok "")
  check_case(targets date_shape "")
  check_case(targets local "")
  check_case(targets how
             "how 'bogus' is not one of hardware, runner, qemu, built, none")
  check_case(targets job "job 'nosuchjob' is not in ci.yml")
  check_case(targets date
             "a hardware line needs a board, a date YYYY-MM-DD and a commit")
  check_case(targets none "a 'none' line names no job")
  check_case(targets duplicate "target 'x' is there twice")
  check_case(targets no_note "a runner line with no job needs a note")
endif()
