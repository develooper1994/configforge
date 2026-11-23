# Exercise the public front door with files in a private directory.
cmake_minimum_required(VERSION 3.16)
file(MAKE_DIRECTORY "${WORK}")

# Check the status and both streams, preserving all whitespace except CRLF.
function(check expected stdout stderr)
  execute_process(
    COMMAND "${FRONT}" diff ${ARGN}
    WORKING_DIRECTORY "${WORK}"
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err)
  string(REPLACE "\r\n" "\n" out "${out}")
  string(REPLACE "\r\n" "\n" err "${err}")
  if(out MATCHES "SENTINEL" OR err MATCHES "SENTINEL")
    message(FATAL_ERROR "diff exposed a value or input path")
  endif()
  if(NOT "${rc}" STREQUAL "${expected}"
     OR NOT out STREQUAL stdout
     OR NOT err STREQUAL stderr)
    message(FATAL_ERROR "diff ${ARGN}: expected ${expected}, got ${rc}"
                        "; out=[${out}] err=[${err}]")
  endif()
endfunction()

set(left [[{"z":true,"n":1,"s":"SENTINEL-A"}]])
set(right [[{"s":"SENTINEL-A","n":1,"z":true}]])
file(WRITE "${WORK}/a.json" "${left}")
file(WRITE "${WORK}/b.json" "${right}")
check(
  0
  ""
  ""
  a.json
  b.json)
file(WRITE "${WORK}/a.unknown" "${left}")
file(WRITE "${WORK}/b.unknown" "${right}")
check(
  0
  ""
  ""
  a.unknown
  b.unknown)
check(
  0
  ""
  ""
  --as-right
  json
  --as-left
  json
  a.unknown
  b.unknown)
if(HAVE_YAML)
  set(yaml "n: 1\ns: SENTINEL-A\nz: true\n")
  set(json [[{"s":"SENTINEL-A","n":"1","z":"true"}]])
  file(WRITE "${WORK}/a.yaml" "${yaml}")
  file(WRITE "${WORK}/a.unknown" "${yaml}")
  file(WRITE "${WORK}/b.json" "${json}")
  file(WRITE "${WORK}/b.unknown" "${json}")
  check(
    0
    ""
    ""
    a.yaml
    b.json)
  check(
    0
    ""
    ""
    --as-left
    yaml
    --as-right
    json
    a.unknown
    b.unknown)
endif()

string(REPEAT "q" 300 long)
file(
  WRITE "${WORK}/a.json"
  [[{"a":1,"a.":0,"a\"":1,"ordinary":"SENTINEL-L",]]
  [["password":"SENTINEL-P","number":2,"dup":7,"dup":8,]]
  "\"long\":\"${long}1\"}")
file(
  WRITE "${WORK}/b.json"
  [[{"a":1,"a.":"0","a\\":3,"ordinary":"SENTINEL-R",]]
  [["password":"SENTINEL-Q","number":3,"dup":8,"":false,"a\t":0,]]
  "\"long\":\"${long}2\",\"or\":4}")
set(changes
    [=[added	$[""]
added	$["a\x09"]
removed	$["a\""]
changed	$["a."]
added	$["a\\"]
changed	$["long"]
changed	$["number"]
added	$["or"]
changed	$["ordinary"]
changed	$["password"]
]=])
check(
  1
  "${changes}"
  ""
  a.json
  b.json)

set(usage "usage: configforge diff [--as-left FORMAT] [--as-right FORMAT] ")
string(APPEND usage "[--] LEFT RIGHT\n")
check(0 "${usage}" "" --help)
check(2 "" "${usage}")
check(2 "" "${usage}" a.json)
check(
  2
  ""
  "${usage}"
  a.json
  b.json
  extra)
check(
  2
  ""
  "${usage}"
  --other
  a.json
  b.json)
check(2 "" "${usage}" --as-left)
check(
  2
  ""
  "${usage}"
  --as-right
  --
  a.json
  b.json)
check(
  2
  ""
  "${usage}"
  --as-left
  json
  --as-left
  json
  a.json
  b.json)
check(
  2
  ""
  "${usage}"
  a.json
  --as-right
  json
  b.json)
check(
  2
  ""
  "${usage}"
  a.json
  -)
file(WRITE "${WORK}/-" "{}")
check(
  0
  ""
  ""
  --as-left
  json
  --as-right
  json
  --
  -
  -)
set(lerr "diff: left input refused\n")
set(rerr "diff: right input refused\n")
check(
  2
  ""
  "${lerr}"
  missing-SENTINEL.json
  b.json)
check(
  2
  ""
  "${lerr}"
  --as-left
  SENTINEL
  a.json
  b.json)
check(
  2
  ""
  "${rerr}"
  --as-right
  SENTINEL
  a.json
  b.json)
foreach(bad IN ITEMS [[{"bad":SENTINEL}]] [[{"a":{"b":[]}}]] [[{"a":[]}]] "[]")
  file(WRITE "${WORK}/bad.json" "${bad}")
  check(
    2
    ""
    "${rerr}"
    a.json
    bad.json)
endforeach()

# Nested tables are compared by their whole path.
function(
  nested
  left_text
  right_text
  expected
  stdout)
  file(WRITE "${WORK}/nl.json" "${left_text}")
  file(WRITE "${WORK}/nr.json" "${right_text}")
  check(
    ${expected}
    "${stdout}"
    ""
    nl.json
    nr.json)
endfunction()
nested([[{"z":{"password":"SENTINEL-A","k":1},"a":2}]]
       [[{"a":2,"z":{"k":1,"password":"SENTINEL-A"}}]] 0 "")
nested(
  [[{"db":{"host":"h","port":1,"old":true},"top":1}]]
  [[{"db":{"host":"h","port":2,"new":true},"top":1}]]
  1
  "added\t$[\"db\"][\"new\"]\nremoved\t$[\"db\"][\"old\"]\nchanged\t$[\"db\"][\"port\"]\n"
)
nested([[{"a":1}]] [[{"a":{"b":1}}]] 1
       "removed\t$[\"a\"]\nadded\t$[\"a\"][\"b\"]\n")
nested([[{"ab":1,"a":{"z":1}}]] [[{}]] 1
       "removed\t$[\"a\"][\"z\"]\nremoved\t$[\"ab\"]\n")
nested([[{"a.b":1}]] [[{"a":{"b":1}}]] 1
       "added\t$[\"a\"][\"b\"]\nremoved\t$[\"a.b\"]\n")
nested([[{"a":{}}]] [[{}]] 0 "")
nested([[{"":{"":1}}]] [[{"":{"":2}}]] 1 "changed\t$[\"\"][\"\"]\n")
nested([[{"a":{"q\"":1}}]] [[{"a":{"q\"":2}}]] 1
       "changed\t$[\"a\"][\"q\\\"\"]\n")
set(deep_l "1")
set(deep_r "2")
set(path "")
foreach(level RANGE 1 20)
  set(deep_l "{\"k\":${deep_l}}")
  set(deep_r "{\"k\":${deep_r}}")
  string(APPEND path "[\"k\"]")
endforeach()
nested("${deep_l}" "${deep_r}" 1 "changed\t$${path}\n")
set(deep_l "1")
foreach(level RANGE 1 40)
  set(deep_l "{\"k\":${deep_l}}")
endforeach()
file(WRITE "${WORK}/nl.json" "{}")
file(WRITE "${WORK}/nr.json" "${deep_l}")
check(
  2
  ""
  "${rerr}"
  nl.json
  nr.json)
if(HAVE_YAML)
  file(WRITE "${WORK}/ny.yaml" "a:\n  b: x\n  c: y\n")
  file(WRITE "${WORK}/nj.json" [[{"a":{"c":"y","b":"x"}}]])
  check(
    0
    ""
    ""
    ny.yaml
    nj.json)
endif()
