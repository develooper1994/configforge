# Every format string in the library, checked against what cf_fmt
# actually supports. See scripts/check_formats.py for the reasoning.
#
# In Python rather than CMake because the check needs to tell a comment
# from code: the comments here discuss the unsupported conversions by
# name, which is where the reasons live, and a checker that could not
# tell them apart would make the reasons unwritable. A first attempt in
# CMake also miscounted lines, because file(STRINGS) splits on
# semicolons.
find_package(
  Python3
  COMPONENTS Interpreter
  QUIET)
if(NOT Python3_Interpreter_FOUND)
  message("format string gate: no python, skipped")
  return()
endif()

file(GLOB_RECURSE sources "${SRC}/src/*.c" "${SRC}/include/configforge/*.h")

execute_process(
  COMMAND ${Python3_EXECUTABLE} "${SRC}/scripts/check_formats.py" ${sources}
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE out)
message("${out}")
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "format gate failed")
endif()
