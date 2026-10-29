# Run an example and require both a clean exit and the lines that show
# it did what its comment claims.
#
#   cmake -DEXE=<program> -DDIR=<dir> -DEXPECT="re1|||re2" -P expect.cmake
#
# -DARGS="a1|||a2" passes arguments, for a program that is a command -
# cargo running a Rust example.
#
# Not PASS_REGULAR_EXPRESSION, which ignores the exit code: an example
# that printed the right line and then crashed would pass.
cmake_minimum_required(VERSION 3.10)

file(MAKE_DIRECTORY "${DIR}")
string(REPLACE "|||" ";" args "${ARGS}")
execute_process(
  COMMAND "${EXE}" ${args}
  WORKING_DIRECTORY "${DIR}"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE out)
message("${out}")
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "exit ${rc}")
endif()
string(REPLACE "|||" ";" wanted "${EXPECT}")
foreach(re ${wanted})
  if(NOT out MATCHES "${re}")
    message(FATAL_ERROR "no line matching '${re}'")
  endif()
endforeach()
