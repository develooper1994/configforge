# Every public options struct has one shape (size first, an _init function),
# and every CF_MAX_* macro says where its limit went (D-220).
# See scripts/check/options.py.
find_package(
  Python3
  COMPONENTS Interpreter
  QUIET)
if(NOT Python3_Interpreter_FOUND)
  message("options gate: no python, skipped")
  return()
endif()

execute_process(
  COMMAND ${Python3_EXECUTABLE} "${SRC}/scripts/check/options.py" "${SRC}"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE out)
message("${out}")
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "options gate failed")
endif()
