# `configforge codec check <file>... [--as <format>]`: whether each file
# is well formed in its format. It writes its own inputs.
#
#   -DCODEC=<the program>  -DWORK=<a directory of its own>

cmake_minimum_required(VERSION 3.10)

file(REMOVE_RECURSE ${WORK})
file(MAKE_DIRECTORY ${WORK})
file(WRITE ${WORK}/good.json "{\"a\": [1, 2]}")
file(WRITE ${WORK}/good.toml "a = 1\n")
file(WRITE ${WORK}/bad.json "{\"a\": }")
string(REPEAT "x" 300 long)
file(WRITE ${WORK}/long.json "{\"a\": \"${long}\"}")
file(WRITE ${WORK}/x "{}")

# check_case(<exit> <stdout regex or -> <stderr regex or -> <arg>...)
function(check_case expect_rc out_re err_re)
  execute_process(
    COMMAND ${CODEC} check ${ARGN}
    WORKING_DIRECTORY ${WORK}
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err)
  set(shown "codec check ${ARGN}\nexit ${rc}\nstdout:\n${out}\nstderr:\n${err}")
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

# Two files that are well formed: one line each, and the exit is 0.
check_case(
  0
  "^good.json: ok\ngood.toml: ok\n$"
  -
  good.json
  good.toml)

# One that is not: its name and the reader's message, and exit 1.
check_case(1 "bad.json: [^\n]+" - bad.json)
check_case(
  1
  "good.json: ok\nbad.json: "
  -
  good.json
  bad.json)

# The format named: the same bytes are not TOML.
check_case(
  1
  "bad.json: "
  -
  bad.json
  --as
  toml)
check_case(
  0
  "good.json: ok"
  -
  good.json
  --as
  json)

# A format nothing calls so is unusable input.
check_case(
  2
  -
  "no format called 'nosuch'"
  x
  --as
  nosuch)

# A file that is not there is unusable input, and the worst of the run.
check_case(
  2
  -
  "cannot read missing.json"
  bad.json
  missing.json)

# A JSON string of 300 bytes: the sink lends the reader room for it.
check_case(0 "^long.json: ok\n$" - long.json)

# No file is the usage.
check_case(2 - "codec: check <file>")
