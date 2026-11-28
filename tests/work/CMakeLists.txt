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
  add_test(
    NAME work.index
    COMMAND
      "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_index.py"
      "${CMAKE_SOURCE_DIR}/scripts/work/index.py"
      "${CMAKE_CURRENT_SOURCE_DIR}/data/index" "${CMAKE_CURRENT_BINARY_DIR}")
  add_test(
    NAME work.links
    COMMAND
      "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_links.py"
      "${CMAKE_SOURCE_DIR}/scripts/work/links.py" "${CMAKE_CURRENT_BINARY_DIR}")
  add_test(
    NAME work.ready
    COMMAND
      "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_ready.py"
      "${CMAKE_SOURCE_DIR}/scripts/check/ready.py"
      "${CMAKE_CURRENT_SOURCE_DIR}/data/ready" "${CMAKE_CURRENT_BINARY_DIR}")
  add_test(
    NAME work.guarantees
    COMMAND
      "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_guarantees.py"
      "${CMAKE_SOURCE_DIR}/scripts/check/guarantees.py"
      "${CMAKE_CURRENT_BINARY_DIR}")
else()
  message(STATUS "work.brief skipped: Python 3 interpreter unavailable")
endif()
