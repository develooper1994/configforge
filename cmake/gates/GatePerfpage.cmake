# docs/Performance.md's static block (the libraries, the documents, what each
# library checks) is generated from files in the repository and committed. This
# regenerates it and fails when the committed one differs, so the page cannot
# stay behind bench/compare/libraries.cmake, settings.json, documents.json or
# probes.expected. See scripts/bench/page.py.
find_package(
  Python3
  COMPONENTS Interpreter
  QUIET)
if(NOT Python3_Interpreter_FOUND)
  message("performance page gate: no python, skipped")
  return()
endif()

execute_process(
  COMMAND ${Python3_EXECUTABLE} "${SRC}/scripts/bench/page.py" --check
          "${SRC}/docs/Performance.md"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE out)
message("${out}")
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "performance page gate failed")
endif()
