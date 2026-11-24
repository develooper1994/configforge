# `configforge codec textconv <file> [--as <format>]`: a document as one line
# a key, for Git's textconv driver, and a file the readers refuse printed as
# it is, exit 0. It writes its own inputs.
#
#   -DCODEC=<the program>  -DWORK=<a directory of its own>  -DJSON=ON|OFF
#   -DINI=ON|OFF  -DYAML=ON|OFF

cmake_minimum_required(VERSION 3.10)

if(NOT JSON)
  message(STATUS "skipped: json is not in this build")
  return()
endif()

file(REMOVE_RECURSE ${WORK})
file(MAKE_DIRECTORY ${WORK})
file(WRITE ${WORK}/a.json
     "{\"s\": {\"p\": 8080}, \"t\": [\"x\"], \"n\": null, \"q\": \"a\\nb\"}")
file(WRITE ${WORK}/c.ini "[s]\nk = v\n")
file(WRITE ${WORK}/y.yaml "a: 1\n# comment\nb: [x]\n")
file(WRITE ${WORK}/bad.json "{\"a\": }")
file(WRITE ${WORK}/half.json "{\"p\": 1, \"q\": }")
file(WRITE ${WORK}/x.txt "{\"a\":")
file(WRITE ${WORK}/blob "{\"k\":1}")

# tc_case(<exit> <stdout regex or -> <stderr regex or -> <arg>...)
function(tc_case expect_rc out_re err_re)
  execute_process(
    COMMAND ${CODEC} textconv ${ARGN}
    WORKING_DIRECTORY ${WORK}
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err)
  set(shown
      "codec textconv ${ARGN}\nexit ${rc}\nstdout:\n${out}\nstderr:\n${err}")
  if(NOT rc EQUAL expect_rc)
    message(FATAL_ERROR "expected exit ${expect_rc}\n${shown}")
  endif()
  if(NOT out_re STREQUAL "-" AND NOT out MATCHES "${out_re}")
    message(FATAL_ERROR "stdout does not match '${out_re}'\n${shown}")
  endif()
  if(NOT err_re STREQUAL "-" AND NOT err MATCHES "${err_re}")
    message(FATAL_ERROR "stderr does not match '${err_re}'\n${shown}")
  endif()
  set(tc_out
      "${out}"
      PARENT_SCOPE)
  set(tc_err
      "${err}"
      PARENT_SCOPE)
endfunction()

# The bytes of a string, as hex, so that a stray CR is seen.
function(hex_of text result)
  file(WRITE ${WORK}/hex.tmp "${text}")
  file(READ ${WORK}/hex.tmp h HEX)
  set(${result}
      "${h}"
      PARENT_SCOPE)
endfunction()

# The file's own bytes must come back byte for byte.
function(expect_raw name)
  file(READ ${WORK}/${name} want HEX)
  hex_of("${tc_out}" got)
  if(NOT got STREQUAL want)
    message(FATAL_ERROR "${name}: stdout is not the file\n${got}\n${want}")
  endif()
endfunction()

# a. One line a key, in the file's order, with no place.
tc_case(0 - - a.json)
set(want
    ".\ttable\ns\ttable\ns.p\tint\t8080\nt\tarray\nt.0\ttext\t\"x\"\nn\tnull\tnull\nq\ttext\t\"a\\nb\"\n"
)
if(NOT tc_out STREQUAL want)
  message(FATAL_ERROR "a.json: unexpected lines\n${tc_out}\nwanted\n${want}")
endif()
if(tc_out MATCHES "a\\.json:" OR tc_out MATCHES "\r")
  message(FATAL_ERROR "a.json: a place or a CR in\n${tc_out}")
endif()
if(NOT tc_err STREQUAL "")
  message(FATAL_ERROR "a.json: stderr is not empty\n${tc_err}")
endif()

# b. The format named, and told by content.
tc_case(
  0
  -
  -
  --as
  json
  blob)
if(NOT tc_out STREQUAL ".\ttable\nk\tint\t1\n")
  message(FATAL_ERROR "blob --as json: unexpected\n${tc_out}")
endif()
tc_case(0 - - blob)
if(NOT tc_out STREQUAL ".\ttable\nk\tint\t1\n")
  message(FATAL_ERROR "blob: unexpected\n${tc_out}")
endif()

# c. Other formats, where the build has them.
if(INI)
  tc_case(0 "(^|\n)s\\.k\ttext\t\"v\" \\(textual\\)\n" - c.ini)
else()
  message(STATUS "skipped: ini is not in this build")
endif()
if(YAML)
  tc_case(0 "(^|\n)a\t(int|text)\t\"?1\"?" - y.yaml)
  if(NOT tc_out MATCHES "(^|\n)b\\.0\ttext\t\"x\"" OR tc_out MATCHES "comment")
    message(FATAL_ERROR "y.yaml: unexpected\n${tc_out}")
  endif()
else()
  message(STATUS "skipped: yaml is not in this build")
endif()

# d. A refusal is not a failure: the file as it is, exit 0, the reason on
# stderr, and nothing of the readable beginning.
tc_case(0 - "codec:" bad.json)
expect_raw(bad.json)
tc_case(0 - "codec:" half.json)
expect_raw(half.json)
if(tc_out MATCHES "p\tint")
  message(
    FATAL_ERROR "half.json: the readable beginning was printed\n${tc_out}")
endif()
tc_case(0 - "cannot tell the format" x.txt)
expect_raw(x.txt)

# e. Not a refusal: unusable input and the usage, exit 2, nothing on stdout.
tc_case(2 "^$" "cannot read" nosuch.json)
tc_case(
  2
  "^$"
  "no format called 'nosuch'"
  a.json
  --as
  nosuch)
tc_case(2 "^$" "textconv <file>")
tc_case(
  2
  "^$"
  "textconv <file>"
  a.json
  b.json)
