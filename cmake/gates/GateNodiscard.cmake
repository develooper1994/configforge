# Every public function whose result is the only way to know it failed is
# CF_NODISCARD. See scripts/check/nodiscard.py.
find_package(
  Python3
  COMPONENTS Interpreter
  QUIET)
if(NOT Python3_Interpreter_FOUND)
  message("nodiscard gate: no python, skipped")
  return()
endif()

execute_process(
  COMMAND ${Python3_EXECUTABLE} "${SRC}/scripts/check/nodiscard.py" "${SRC}"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE out)
message("${out}")
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "nodiscard gate failed")
endif()
