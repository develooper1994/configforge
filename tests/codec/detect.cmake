# `configforge codec detect <file>`: the format of a file and how it was
# told. It writes its own inputs, so it needs no fixture.
#
#   -DCODEC=<the program>  -DWORK=<a directory of its own>  -DJSON=ON|OFF

cmake_minimum_required(VERSION 3.10)

# Its documents are JSON.
if(NOT JSON)
  message(STATUS "skipped: json is not in this build")
  return()
endif()

file(REMOVE_RECURSE ${WORK})
file(MAKE_DIRECTORY ${WORK})
file(WRITE ${WORK}/a.json "{\"a\": 1}")
file(WRITE ${WORK}/noext "{\"a\": 1}")
file(WRITE ${WORK}/empty "")

# detect_case(<exit> <stdout regex or -> <stderr regex or -> <arg>...)
function(detect_case expect_rc out_re err_re)
  execute_process(
    COMMAND ${CODEC} detect ${ARGN}
    WORKING_DIRECTORY ${WORK}
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err)
  set(shown
      "codec detect ${ARGN}\nexit ${rc}\nstdout:\n${out}\nstderr:\n${err}")
  if(NOT rc EQUAL expect_rc)
    message(FATAL_ERROR "expected exit ${expect_rc}\n${shown}")
  endif()
  if(NOT out_re STREQUAL "-" AND NOT out MATCHES "${out_re}")
    message(FATAL_ERROR "stdout does not match '${out_re}'\n${shown}")
  endif()
  if(NOT err_re STREQUAL "-" AND NOT err MATCHES "${err_re}")
    message(FATAL_ERROR "stderr does not match '${err_re}'\n${shown}")
  endif()
endfunction()

# The extension tells it; the content tells it where the name does not.
detect_case(0 "^json \\(extension\\)\n$" - a.json)
detect_case(0 "^json \\(content\\)\n$" - noext)

# Nothing to read is no format, and the bytes are the answer.
detect_case(1 - "cannot tell the format of empty" empty)

# A file that is not there is unusable input.
detect_case(2 - "cannot read missing.json" missing.json)

# Exactly one file: none, or two, is the usage. (Two loose formats
# reading one text differently is cf_codec_detect's own test's to show;
# the message that passes it on is read in the code.)
detect_case(2 - "codec: detect <file>")
detect_case(
  2
  -
  "codec: detect <file>"
  a.json
  noext)
