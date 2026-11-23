# The tree map must describe the current source and document line ranges.
cmake_minimum_required(VERSION 3.10)

set(fresh "${BIN}/Codemap.md")
execute_process(
  COMMAND "${CMAKE_COMMAND}" "-DOUT=${fresh}" -P
          "${SRC}/scripts/generate/codemap.cmake"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE out)
if(NOT rc STREQUAL "0")
  message(FATAL_ERROR "scripts/generate/codemap.cmake: cannot be run\n${out}")
endif()

set(page "${SRC}/docs/Codemap.md")
if(NOT EXISTS "${page}")
  message(FATAL_ERROR "docs/Codemap.md is missing")
endif()

file(READ "${page}" have)
file(READ "${fresh}" want)
string(REPLACE "\r\n" "\n" have "${have}")
string(REPLACE "\r\n" "\n" want "${want}")
if(NOT have STREQUAL want)
  # Compare consecutive lines without treating Markdown as a CMake list.
  set(line_number 1)
  while(TRUE)
    string(REGEX MATCH "^([^\n]+\n?|\n)" have_line "${have}")
    string(REGEX MATCH "^([^\n]+\n?|\n)" want_line "${want}")
    if(NOT have_line STREQUAL want_line)
      break()
    endif()
    string(LENGTH "${have_line}" line_length)
    string(SUBSTRING "${have}" ${line_length} -1 have)
    string(SUBSTRING "${want}" ${line_length} -1 want)
    math(EXPR line_number "${line_number} + 1")
  endwhile()
  string(REGEX REPLACE "\n$" "" have_line "${have_line}")
  string(REGEX REPLACE "\n$" "" want_line "${want_line}")
  message(
    FATAL_ERROR
      "docs/Codemap.md differs at line ${line_number}\n"
      "  checked in: ${have_line}\n  generated: ${want_line}\n"
      "Run cmake -P scripts/generate/codemap.cmake")
endif()
