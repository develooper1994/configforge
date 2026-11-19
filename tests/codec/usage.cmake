# Anything configforge-codec does not know is the usage on stderr, and
# exit 2.
cmake_minimum_required(VERSION 3.10)

foreach(args "" "--nonsense" "nosuch")
  execute_process(
    COMMAND ${CODEC} ${args}
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err)
  if(NOT rc EQUAL 2)
    message(FATAL_ERROR "'${args}': expected exit 2, got ${rc}")
  endif()
  if(NOT err MATCHES "configforge codec formats" OR NOT out STREQUAL "")
    message(FATAL_ERROR "'${args}': the usage is not on stderr alone:\n${err}")
  endif()
endforeach()
