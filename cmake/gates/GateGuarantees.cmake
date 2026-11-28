# Every public function's header comment states one failure guarantee
# (nothrow, strong or basic), docs/Guarantees.md lists them, and no library
# source ends the program; a Signal: safe or Alloc: none line holds against
# the call data in ${BIN}. See scripts/check/guarantees.py.
find_package(
  Python3
  COMPONENTS Interpreter
  QUIET)
if(NOT Python3_Interpreter_FOUND)
  message("guarantees gate: no python, skipped")
  return()
endif()

# The call data the library's build wrote (GCC's -fcallgraph-info), for the
# Signal: safe and Alloc: none lines; none, and the check says it skipped.
set(callgraph "")
if(DEFINED BIN AND NOT "${BIN}" STREQUAL "")
  set(callgraph --callgraph "${BIN}")
endif()

execute_process(
  COMMAND ${Python3_EXECUTABLE} "${SRC}/scripts/check/guarantees.py"
          ${callgraph} "${SRC}"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE out)
message("${out}")
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "guarantees gate failed")
endif()
