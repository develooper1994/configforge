# `configforge codec dump <file> [--as <format>]`: every event of a document,
# with its place. It writes its own inputs.
#
#   -DCODEC=<the program>  -DWORK=<a directory of its own>  -DCBOR=ON|OFF
#   -DJSON=ON|OFF  -DINI=ON|OFF

cmake_minimum_required(VERSION 3.10)

# Its documents are JSON, and INI where the build has it.
if(NOT JSON)
  message(STATUS "skipped: json is not in this build")
  return()
endif()

file(REMOVE_RECURSE ${WORK})
file(MAKE_DIRECTORY ${WORK})
file(WRITE ${WORK}/a.json "{\"s\": {\"p\": 8080}, \"t\": [\"x\"]}")
file(WRITE ${WORK}/c.ini "[s]\nk = v\n")
file(WRITE ${WORK}/e.json "{\"a\": \"a\\nb\"}")
file(WRITE ${WORK}/bad.json "{\"a\": }")

# dump_case(<exit> <stdout regex or -> <stderr regex or -> <arg>...)
function(dump_case expect_rc out_re err_re)
  execute_process(
    COMMAND ${CODEC} dump ${ARGN}
    WORKING_DIRECTORY ${WORK}
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err)
  set(shown "codec dump ${ARGN}\nexit ${rc}\nstdout:\n${out}\nstderr:\n${err}")
  if(NOT rc EQUAL expect_rc)
    message(FATAL_ERROR "expected exit ${expect_rc}\n${shown}")
  endif()
  if(NOT out_re STREQUAL "-" AND NOT out MATCHES "${out_re}")
    message(FATAL_ERROR "stdout does not match '${out_re}'\n${shown}")
  endif()
  if(NOT err_re STREQUAL "-" AND NOT err MATCHES "${err_re}")
    message(FATAL_ERROR "stderr does not match '${err_re}'\n${shown}")
  endif()
  set(dump_out
      "${out}"
      PARENT_SCOPE)
endfunction()

# A table, a value with its place, an array and its first member.
dump_case(0 "^-\t\\.\ttable\n" - a.json)
foreach(re "(^|\n)-\t\\.\ttable\n" "(^|\n)a.json:1:[0-9]+\ts.p\tint\t8080\n"
           "(^|\n)-\tt\tarray\n" "(^|\n)a.json:1:[0-9]+\tt.0\ttext\t\"x\"\n")
  if(NOT dump_out MATCHES "${re}")
    message(FATAL_ERROR "a.json: no line matches '${re}'\n${dump_out}")
  endif()
endforeach()

# Text that came untyped says so.
if(INI)
  dump_case(0 "(^|\n)c.ini:2:[0-9]+\ts.k\ttext\t\"v\" \\(textual\\)\n" - c.ini)
else()
  message(STATUS "skipped: ini is not in this build")
endif()

# A byte offset for CBOR: a map, "a" to 1.
if(CBOR)
  string(
    ASCII 161
          97
          97
          1
          bytes)
  file(WRITE ${WORK}/m.cbor "${bytes}")
  dump_case(0 "(^|\n)m.cbor, byte 3\ta\tint\t1\n" - m.cbor)
endif()

# A newline in a value is written out, and every event is one line.
dump_case(0 "\ta\ttext\t\"a\\\\nb\"\n" - e.json)
string(REGEX MATCHALL "\n" lines "${dump_out}")
list(LENGTH lines n)
if(NOT n EQUAL 2)
  message(FATAL_ERROR "expected 2 lines for 2 events, got ${n}\n${dump_out}")
endif()

# A document the reader refuses: exit 1, and its message on stderr.
dump_case(1 - "codec: [^\n]+" bad.json)

# --as names the format; a name no codec has is unusable input.
dump_case(
  0
  "(^|\n)-\t\\.\ttable\n"
  -
  a.json
  --as
  json)
dump_case(
  2
  -
  "no format called 'nosuch'"
  a.json
  --as
  nosuch)

# Not exactly one file, or --as twice, is the usage.
dump_case(2 - "codec: dump <file>")
dump_case(
  2
  -
  "codec: dump <file>"
  a.json
  c.ini)
dump_case(
  2
  -
  "codec: dump <file>"
  a.json
  --as
  json
  --as
  json)
