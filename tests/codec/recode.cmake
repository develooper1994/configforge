# `configforge codec recode <file> --from <encoding> [--out <file>]` and
# `recode <file> --check` (D-108). The fixtures are bytes in other
# encodings, kept byte for byte (.gitattributes), copied beside the
# program's working directory so that its messages show bare file names.
#
#   -DCODEC=<the program>  -DWORK=<a directory of its own>  -DDATA=<fixtures>

cmake_minimum_required(VERSION 3.10)

file(REMOVE_RECURSE ${WORK})
file(MAKE_DIRECTORY ${WORK})
foreach(f w1254.ini w1254.utf8.ini bad1254.ini u16le.json u16le.utf8.json
               utf8.toml)
  file(COPY ${DATA}/${f} DESTINATION ${WORK})
endforeach()

# recode_case(<exit> <stdout regex or -> <stderr regex or -> <arg>...)
function(recode_case expect_rc out_re err_re)
  execute_process(
    COMMAND ${CODEC} recode ${ARGN}
    WORKING_DIRECTORY ${WORK}
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err)
  set(shown
      "codec recode ${ARGN}\nexit ${rc}\nstdout:\n${out}\nstderr:\n${err}")
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

# same_bytes(<file> <file>): equal byte for byte, never through a variable.
function(same_bytes a b)
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E compare_files ${WORK}/${a} ${WORK}/${b}
    RESULT_VARIABLE same)
  if(NOT same EQUAL 0)
    message(FATAL_ERROR "${a} and ${b} differ")
  endif()
endfunction()

# The Windows-1254 INI, with --out: the expected bytes, line endings kept.
recode_case(0 "^$" "^$" w1254.ini --from windows-1254 --out got.ini)
same_bytes(got.ini w1254.utf8.ini)

# Without --out the same bytes go to standard output.
execute_process(
  COMMAND ${CODEC} recode w1254.ini --from cp1254
  WORKING_DIRECTORY ${WORK}
  OUTPUT_FILE ${WORK}/stdout.ini
  RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "recode to stdout: exit ${rc}")
endif()
same_bytes(stdout.ini w1254.utf8.ini)

# A UTF-16LE file with its mark needs no --from; the mark is dropped, and
# nothing is added.
recode_case(0 "^$" "^$" u16le.json --out got.json)
same_bytes(got.json u16le.utf8.json)
# --from naming the mark's own encoding is accepted; another is not.
recode_case(0 "^$" "^$" u16le.json --from utf-16le --out got2.json)
same_bytes(got2.json u16le.utf8.json)
recode_case(2 - "codec: 'u16le.json' starts with a UTF-16LE byte-order mark; leave out --from"
            u16le.json --from windows-1254)

# A byte with no character: A-4's message with the place, nothing written.
recode_case(1 - "^codec: bad1254.ini:2:5: byte 0x81 has no character in windows-1254\n$"
            bad1254.ini --from windows-1254 --out never.ini)
if(EXISTS ${WORK}/never.ini)
  message(FATAL_ERROR "a refused recode wrote its --out file")
endif()

# --check: UTF-8 is exit 0 on stdout.
recode_case(0 "^'utf8.toml' is UTF-8\n$" - utf8.toml --check)
# A UTF-16 mark: exit 1, how to read it.
recode_case(
  1 -
  "^codec: 'u16le.json' is UTF-16LE .by its byte-order mark.: recode it with --from utf-16le\n$"
  u16le.json --check)
# Bytes that are not UTF-8: A-5's refusal, hint included.
recode_case(
  1 - "^codec: bad1254.ini:2:5: byte 0x81 is not valid UTF-8" bad1254.ini --check)
recode_case(
  1 - "^codec: w1254.ini:2:1: byte 0xFE is not valid UTF-8.*windows-1254"
  w1254.ini --check)
# Unreadable: exit 2.
recode_case(2 - "codec: cannot read missing.ini" missing.ini --check)

# The refusals of the command line.
recode_case(2 - "codec: write to a new file and review the difference"
            w1254.ini --from windows-1254 --out w1254.ini)
recode_case(2 - "codec: 'w1254.ini' is to be UTF-8 already; use --check"
            w1254.ini --from utf-8)
recode_case(
  2 -
  "codec: no encoding 'nosuch'; there are: utf-8, iso-8859-1, windows-1252, windows-1254, iso-8859-9, utf-16le, utf-16be"
  w1254.ini --from nosuch)
recode_case(2 - "codec: recode <file> --from" w1254.ini)
recode_case(2 - "codec: recode <file> --from" utf8.toml --check --from windows-1254)
recode_case(2 - "codec: recode <file> --from" utf8.toml --check --out x)
recode_case(2 - "codec: recode <file> --from")
