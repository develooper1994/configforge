# Every public function's header comment states one failure guarantee
# (nothrow, strong or basic), docs/Guarantees.md lists them, and no library
# source ends the program. See scripts/check/guarantees.py.
find_package(
  Python3
  COMPONENTS Interpreter
  QUIET)
if(NOT Python3_Interpreter_FOUND)
  message("guarantees gate: no python, skipped")
  return()
endif()

execute_process(
  COMMAND ${Python3_EXECUTABLE} "${SRC}/scripts/check/guarantees.py" "${SRC}"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE out)
message("${out}")
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "guarantees gate failed")
endif()
