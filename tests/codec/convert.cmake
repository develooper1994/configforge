# `configforge codec convert` and `configforge codec format`: a document
# written in another format, and what the target could not hold. It writes
# its own inputs.
#
#   -DCODEC=<the program>  -DWORK=<a directory of its own>  -DCBOR=ON|OFF

cmake_minimum_required(VERSION 3.10)

file(REMOVE_RECURSE ${WORK})
file(MAKE_DIRECTORY ${WORK})

# cv_case(<name> <expected-exit> <stderr regex or -> <args>...): runs the
# program on `codec <args>`, checks the exit code and stderr, and leaves
# the output in cv_out and cv_err.
function(cv_case name expect_rc err_re)
  execute_process(
    COMMAND ${CODEC} ${ARGN}
    WORKING_DIRECTORY ${WORK}
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err)
  set(shown
      "${name}: codec ${ARGN}\nexit ${rc}\nstdout:\n${out}\nstderr:\n${err}")
  if(NOT rc EQUAL expect_rc)
    message(FATAL_ERROR "expected exit ${expect_rc}\n${shown}")
  endif()
  if(NOT err_re STREQUAL "-" AND NOT err MATCHES "${err_re}")
    message(FATAL_ERROR "stderr does not match '${err_re}'\n${shown}")
  endif()
  set(cv_out
      "${out}"
      PARENT_SCOPE)
  set(cv_err
      "${err}"
      PARENT_SCOPE)
endfunction()

# cv_has(<text> <needle>) and cv_lacks: plain substring checks.
function(cv_has text needle)
  string(FIND "${text}" "${needle}" at)
  if(at EQUAL -1)
    message(FATAL_ERROR "'${needle}' is not in:\n${text}")
  endif()
endfunction()

function(cv_lacks text needle)
  string(FIND "${text}" "${needle}" at)
  if(NOT at EQUAL -1)
    message(FATAL_ERROR "'${needle}' is in:\n${text}")
  endif()
endfunction()

# cv_times(<text> <needle> <n>): the needle is there exactly n times.
function(cv_times text needle n)
  string(REPLACE "${needle}" "#" cut "${text}")
  string(REGEX MATCHALL "#" hits "${cut}")
  list(LENGTH hits k)
  if(NOT k EQUAL n)
    message(
      FATAL_ERROR "'${needle}' is in the text ${k} times, not ${n}:\n${text}")
  endif()
endfunction()

# JSON to TOML and back.
file(
  WRITE ${WORK}/a.json
  "{\"server\": {\"port\": 8080, \"host\": \"web\"}, \"tags\": [\"a\", \"b\"]}")
cv_case(
  json_toml
  0
  "^$"
  convert
  a.json
  --to
  toml
  --out
  a.toml)
file(READ ${WORK}/a.toml toml)
cv_has("${toml}" "port = 8080")
cv_has("${toml}" "host = \"web\"")
cv_case(
  json_toml_back
  0
  "note: toml can hold comments"
  convert
  a.toml
  --to
  json)
cv_has("${cv_out}" "\"port\": 8080")

# A table's values come first, then its tables.
file(WRITE ${WORK}/o.json "{\"s\": {\"k\": 1}, \"r\": 2}")
cv_case(
  order
  0
  -
  convert
  o.json
  --to
  ini)
string(FIND "${cv_out}" "r = 2" at_r)
string(FIND "${cv_out}" "[s]" at_s)
if(at_r EQUAL -1
   OR at_s EQUAL -1
   OR at_r GREATER at_s)
  message(FATAL_ERROR "order: 'r = 2' must come before '[s]':\n${cv_out}")
endif()

# TOML has no null: a finding, and under --strict nothing is written.
file(WRITE ${WORK}/n.json "{\"a\": null, \"b\": 1}")
cv_case(
  null_toml
  0
  "a: a null, which toml cannot hold; left out"
  convert
  n.json
  --to
  toml
  --out
  n.toml)
file(READ ${WORK}/n.toml ntoml)
cv_has("${ntoml}" "b = 1")
cv_lacks("${ntoml}" "a")
cv_case(
  null_strict
  1
  "a: a null, which toml cannot hold; left out"
  convert
  n.json
  --to
  toml
  --strict
  --out
  s.toml)
cv_has("${cv_err}" "nothing written")
if(EXISTS ${WORK}/s.toml)
  message(FATAL_ERROR "null_strict: s.toml was written")
endif()

# Dropping the only leaf of a named table leaves the array slot in place.
file(WRITE ${WORK}/inline.json
     "{\"a\": [{\"nested\": {\"inner\": {\"omitted\": null}}}, {\"keep\": 1}]}")
cv_case(
  inline_empty_toml
  0
  "a\\.0\\.nested\\.inner\\.omitted: a null, which toml cannot hold"
  convert
  inline.json
  --to
  toml)
cv_has("${cv_out}" "a = [{  }, { keep = 1 }]")
cv_lacks("${cv_err}" "an empty table")

# What INI cannot hold: an array of tables, an empty array, a line break.
file(WRITE ${WORK}/t.json
     "{\"k\": \"v\", \"n\": 5, \"s\": {\"list\": [{\"x\": 1}]}}")
cv_case(
  tables_ini
  0
  "s.list: an array of tables, which ini cannot hold; left out"
  convert
  t.json
  --to
  ini)
cv_has("${cv_err}" "ini carries no types: 1 value(s)")
cv_has("${cv_out}" "k = v")
file(WRITE ${WORK}/e.json "{\"e\": [], \"k\": \"v\"}")
cv_case(
  empty_ini
  0
  "e: an empty array"
  convert
  e.json
  --to
  ini)
file(WRITE ${WORK}/m.json "{\"m\": \"a\\nb\", \"k\": \"v\"}")
cv_case(
  lines_ini
  0
  "m: text with a line break, which ini cannot hold; left out"
  convert
  m.json
  --to
  ini)

# A name TOML would write unquoted; JSON writes any name as a string.
file(WRITE ${WORK}/names.json "{\"Name\": 1, \"a b\": 2}")
cv_case(
  names
  0
  "a b: a name it would write unquoted"
  convert
  names.json
  --to
  toml)
cv_has("${cv_out}" "Name = 1")
cv_case(
  names_json
  0
  "^$"
  convert
  names.json
  --to
  json)
cv_has("${cv_out}" "\"a b\": 2")

# What a target cannot hold is named from one table (cf_format_holds,
# D-109): an empty table, which TOML and INI write as nothing...
file(WRITE ${WORK}/et.json "{\"e\": {}, \"k\": 1}")
cv_case(
  empty_toml
  0
  "e: an empty table, which toml cannot hold; left out"
  convert
  et.json
  --to
  toml)
cv_case(
  empty_toml_strict
  1
  "e: an empty table, which toml cannot hold; left out"
  convert
  et.json
  --to
  toml
  --strict)
# ...a null YAML does write as a null, so it is not counted as text...
file(WRITE ${WORK}/yn.json "{\"a\": null, \"b\": 1}")
cv_case(
  yaml_null
  0
  "yaml carries no types: 1 value\\(s\\)"
  convert
  yn.json
  --to
  yaml)
cv_has("${cv_out}" "a: null")
# ...an empty name, which TOML would otherwise call a name it would write
# unquoted...
file(WRITE ${WORK}/en.json "{\"\": 1, \"k\": 2}")
cv_case(
  empty_name
  0
  "an empty name, which toml cannot hold"
  convert
  en.json
  --to
  toml)
cv_lacks("${cv_err}" "a name it would write unquoted")
# YAML bounds each mapping key separately from its path.
if(YAML)
  set(yaml_long "k")
  while(1)
    string(LENGTH "${yaml_long}" yaml_len)
    if(yaml_len GREATER_EQUAL KEY_MAX)
      break()
    endif()
    string(APPEND yaml_long "${yaml_long}")
  endwhile()
  string(SUBSTRING "${yaml_long}" 0 ${KEY_MAX} yaml_long)
  math(EXPR yaml_key_len "${KEY_MAX} - 1")
  string(SUBSTRING "${yaml_long}" 0 ${yaml_key_len} yaml_key)
  file(WRITE ${WORK}/long-name.ini
       "[${yaml_key}]\n${yaml_long} = 1\nkept = 2\n")
  cv_case(
    yaml_long_key
    0
    "a name too long, which yaml cannot hold"
    convert
    long-name.ini
    --to
    yaml
    --out
    long-name.yaml)
  cv_case(yaml_long_key_output 0 "-" check long-name.yaml)
  file(READ ${WORK}/long-name.yaml yaml_out)
  cv_has("${yaml_out}" "${yaml_key}:")
  cv_has("${yaml_out}" "kept:")
  cv_lacks("${yaml_out}" "${yaml_long}")
endif()
# ...and a path too long for a TOML or INI writer to write and read back.
set(cv_n "n")
foreach(i RANGE 7)
  string(APPEND cv_n "${cv_n}")
endforeach()
function(cv_path_doc file inner)
  string(SUBSTRING "${cv_n}" 0 ${inner} name)
  file(WRITE ${WORK}/${file} "{\"a\": {\"${name}\": {\"k\": 1}}}")
endfunction()
set(cv_six "")
foreach(i RANGE 5)
  string(SUBSTRING "${cv_n}" 0 50 part)
  string(APPEND cv_six "{\"${part}\": ")
endforeach()
file(WRITE ${WORK}/lp.json "${cv_six}{\"k\": 1}}}}}}}")
cv_case(
  long_path
  0
  "a table path too long"
  convert
  lp.json
  --to
  ini)
cv_path_doc(p_long.json 251)
cv_case(
  path_long
  0
  "a table path too long"
  convert
  p_long.json
  --to
  ini)
cv_path_doc(p_ok.json 250)
cv_case(
  path_ok
  0
  -
  convert
  p_ok.json
  --to
  ini)
cv_lacks("${cv_err}" "path too long")

# A section met twice is one table.
file(WRITE ${WORK}/d.ini "[s]\na = 1\n[t]\nc = 0\n[s]\nb = 2\n")
cv_case(
  twice
  0
  -
  convert
  d.ini
  --to
  json)
cv_times("${cv_out}" "\"s\"" 1)
cv_has("${cv_out}" "\"a\": \"1\"")
cv_has("${cv_out}" "\"b\": \"2\"")

# Two notes, which are not findings: --strict still passes.
file(WRITE ${WORK}/c.ini "[s]\nk = 8080\n")
cv_case(
  ini_json
  0
  "note: ini can hold comments"
  convert
  c.ini
  --to
  json)
cv_has("${cv_err}" "note: ini carries no types")
cv_has("${cv_out}" "\"k\": \"8080\"")
cv_case(
  ini_json_strict
  0
  "note: ini carries no types"
  convert
  c.ini
  --to
  json
  --strict)

# ${ is written as it was read.
file(WRITE ${WORK}/r.json "{\"u\": \"\${HOME}/x\", \"v\": \"$\${y}\"}")
cv_case(
  refs
  0
  -
  convert
  r.json
  --to
  toml)
cv_times("${cv_out}" "\${HOME}/x" 1)
cv_times("${cv_out}" "$\${y}" 1)

# No extension: the content tells.
file(WRITE ${WORK}/noext "{\"a\": 1}")
cv_case(
  content
  0
  -
  convert
  noext
  --to
  toml)
cv_has("${cv_out}" "a = 1")

# format: the document in its own format, which reads back the same.
file(WRITE ${WORK}/f.json "{\"b\":1,\"a\":[1,2]}")
cv_case(
  format
  0
  "^$"
  format
  f.json
  --out
  f1.json)
execute_process(
  COMMAND ${CODEC} convert - --as json --to json --out f2.json
  WORKING_DIRECTORY ${WORK}
  INPUT_FILE ${WORK}/f1.json
  RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "format: reading it back failed: ${rc}")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} -E compare_files ${WORK}/f1.json
                        ${WORK}/f2.json RESULT_VARIABLE same)
if(NOT same EQUAL 0)
  message(FATAL_ERROR "format: the rewritten document is not the same bytes")
endif()

# CBOR in a file, and on standard output, is its bytes.
if(CBOR)
  cv_case(
    cbor_to
    0
    "^$"
    convert
    a.json
    --to
    cbor
    --out
    a.cbor)
  cv_case(
    cbor_back
    0
    "^$"
    convert
    a.cbor
    --to
    json
    --out
    back.json)
  cv_case(
    cbor_same
    0
    "^$"
    convert
    a.json
    --to
    json
    --out
    same.json)
  execute_process(COMMAND ${CMAKE_COMMAND} -E compare_files ${WORK}/back.json
                          ${WORK}/same.json RESULT_VARIABLE same)
  if(NOT same EQUAL 0)
    message(FATAL_ERROR "cbor: the round trip is not the same bytes")
  endif()
  execute_process(
    COMMAND ${CODEC} convert a.json --to cbor
    WORKING_DIRECTORY ${WORK}
    OUTPUT_FILE ${WORK}/piped.cbor
    RESULT_VARIABLE rc)
  execute_process(COMMAND ${CMAKE_COMMAND} -E compare_files ${WORK}/a.cbor
                          ${WORK}/piped.cbor RESULT_VARIABLE same)
  if(NOT rc EQUAL 0 OR NOT same EQUAL 0)
    message(FATAL_ERROR "cbor: standard output is not the file's bytes")
  endif()
endif()

# D-82's bound: an anchor of 64 keys and 2,000 aliases of it.
set(yaml "base: &b\n")
foreach(i RANGE 63)
  string(APPEND yaml "  k${i}: ${i}\n")
endforeach()
foreach(i RANGE 1999)
  string(APPEND yaml "a${i}: *b\n")
endforeach()
file(WRITE ${WORK}/bomb.yaml "${yaml}")
cv_case(
  bound
  2
  "refused|alias"
  convert
  bomb.yaml
  --to
  json)

# Unusable input and options.
cv_case(
  unknown_format
  2
  "no format called 'nosuch'"
  convert
  a.json
  --to
  nosuch)
cv_case(
  read_only
  2
  "can be read but not written"
  convert
  a.json
  --to
  properties)
cv_case(
  missing
  2
  "cannot read"
  convert
  missing.json
  --to
  toml)
cv_case(
  no_to
  2
  "codec: convert <file> --to"
  convert
  a.json)
cv_case(
  format_to
  2
  "codec: format <file>"
  format
  a.json
  --to
  toml)
cv_case(
  bogus
  2
  "codec: convert <file> --to"
  convert
  a.json
  --to
  toml
  --bogus)
cv_case(
  two_inputs
  2
  "codec: convert <file> --to"
  convert
  a.json
  o.json
  --to
  toml)
