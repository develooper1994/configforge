find_package(
  Python3
  COMPONENTS Interpreter
  QUIET)
if(Python3_Interpreter_FOUND)
  add_test(
    NAME work.brief
    COMMAND
      "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_brief.py"
      "${CMAKE_SOURCE_DIR}/scripts/work/brief.py"
      "${CMAKE_CURRENT_SOURCE_DIR}/data" "${CMAKE_CURRENT_BINARY_DIR}")
else()
  message(STATUS "work.brief skipped: Python 3 interpreter unavailable")
endif()
