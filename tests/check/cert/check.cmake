# check.cert: the generator's page for the fixture list is the expected page,
# and it refuses a list that breaks one of its rules.
#   cmake -DGEN=<cert_matrix.cmake> -DFIX=<fixture dir> -DBIN=<dir> -P check.cmake
cmake_minimum_required(VERSION 3.10)

file(MAKE_DIRECTORY "${BIN}")
set(page "${BIN}/cert.page.md")
file(REMOVE "${page}")
execute_process(COMMAND "${CMAKE_COMMAND}" "-DIN=${FIX}/cert.txt"
                        "-DOUT=${page}" -P "${GEN}" RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "check.cert: the generator failed on the fixture")
endif()
file(READ "${page}" have)
file(READ "${FIX}/Cert.expected.md" want)
string(REPLACE "\r" "" have "${have}")
string(REPLACE "\r" "" want "${want}")
if(NOT have STREQUAL want)
  message(
    FATAL_ERROR "check.cert: the page differs from ${FIX}/Cert.expected.md")
endif()

# Each bad list is refused, and the page is not written.
set(bad_names
    empty-field
    unknown-kind
    duplicate
    not-c
    na-without-grep
    no-argument
    design-without-number
    extra-argument
    two-pipes)
set(bad_empty-field "ARR30-C | review | ")
set(bad_unknown-kind "ARR30-C | magic | x")
set(bad_duplicate "ARR30-C | review | x\nARR30-C | review | y")
set(bad_not-c "ARR30-CPP | review | x")
set(bad_na-without-grep "ARR30-C | n/a | nothing here")
set(bad_no-argument "ARR30-C | clang-tidy | x")
set(bad_design-without-number "ARR30-C | design | x")
set(bad_extra-argument "ARR30-C | review now | x")
set(bad_two-pipes "ARR30-C | review | x | y")
foreach(name IN LISTS bad_names)
  set(list_file "${BIN}/bad-${name}.txt")
  set(out_file "${BIN}/bad-${name}.md")
  file(REMOVE "${out_file}")
  file(WRITE "${list_file}" "${bad_${name}}\n")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DIN=${list_file}" "-DOUT=${out_file}" -P
            "${GEN}"
    RESULT_VARIABLE rc
    OUTPUT_QUIET ERROR_QUIET)
  if(rc EQUAL 0 OR EXISTS "${out_file}")
    message(FATAL_ERROR "check.cert: the generator took the bad list '${name}'")
  endif()
endforeach()
message("check.cert: clean")
