# configforge doctor drift: a set of reports, one a host, grouped by value.
# The reports are written here, by hand, in the shape a report has; the one
# case that needs digests made under different keys uses the fixtures'.

cmake_minimum_required(VERSION 3.10)

set(work "${CMAKE_CURRENT_BINARY_DIR}")
set(dir "${work}/drift")
file(REMOVE_RECURSE "${dir}")
file(MAKE_DIRECTORY "${dir}")

# One key of a hand-written report.
function(
  key
  result
  name
  value
  definition)
  set(${result}
      "\"${name}\":{${definition}\"candidates\":[{\"source\":\"env\",\"value\":\"${value}\"}],\"selected\":{\"source\":\"env\"}}"
      PARENT_SCOPE)
endfunction()

# A host's report. `flag` empty leaves feature.flag out. db.password is a
# sensitive key in a full report: its value is in the file.
function(
  host
  file
  level
  pass
  flag)
  key(port server.port 8080 "")
  key(lvl log.level "${level}" "")
  key(pw db.password "${pass}"
      "\"definition\":{\"type\":\"text\",\"sensitive\":true},")
  set(keys "${port},${lvl},${pw}")
  if(NOT flag STREQUAL "")
    key(fl feature.flag "${flag}" "")
    set(keys "${keys},${fl}")
  endif()
  file(WRITE "${dir}/${file}" "{\"configforge_report\":1,\"keys\":{${keys}}}")
endfunction()

# Run Doctor in `wd`; leaves rc, out, err and all (both) set.
function(doctor_run wd)
  execute_process(
    COMMAND ${DOCTOR} ${ARGN}
    WORKING_DIRECTORY "${wd}"
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err)
  set(rc
      "${rc}"
      PARENT_SCOPE)
  set(out
      "${out}"
      PARENT_SCOPE)
  set(err
      "${err}"
      PARENT_SCOPE)
  set(all
      "${out}${err}"
      PARENT_SCOPE)
endfunction()

function(fail what)
  message("${all}")
  message(FATAL_ERROR "${what}")
endfunction()

# Run in the hosts' directory and expect an exit code.
function(drift expect)
  doctor_run("${dir}" drift ${ARGN})
  if(NOT rc EQUAL expect)
    fail("doctor drift ${ARGN}: exit ${rc}, not ${expect}")
  endif()
  set(rc
      "${rc}"
      PARENT_SCOPE)
  set(out
      "${out}"
      PARENT_SCOPE)
  set(err
      "${err}"
      PARENT_SCOPE)
  set(all
      "${all}"
      PARENT_SCOPE)
endfunction()

function(want text)
  if(NOT all MATCHES "${text}")
    fail("doctor drift did not say: ${text}")
  endif()
endfunction()

function(refuse text)
  if(all MATCHES "${text}")
    fail("doctor drift said what it must not: ${text}")
  endif()
endfunction()

host(web-1.json info MARKER-FLEET-ONE on)
host(web-2.json info MARKER-FLEET-ONE on)
host(web-3.json debug MARKER-FLEET-ONE on)
host(web-4.json info MARKER-FLEET-TWO on)
host(web-5.json info MARKER-FLEET-ONE "")
set(five
    web-1.json
    web-2.json
    web-3.json
    web-4.json
    web-5.json)

# 1. Three keys differ, in the three ways; the port agrees.
drift(1 ${five})
want(
  "DRIFT +warning +log\\.level +differs: 'debug' in web-3\\.json; 'info' in web-1\\.json, web-2\\.json, web-4\\.json, web-5\\.json"
)
want(
  "DRIFT +warning +db\\.password +differs \\(by digest\\): value 2 in web-4\\.json; value 1 in web-1\\.json, web-2\\.json, web-3\\.json, web-5\\.json"
)
want(
  "DRIFT +warning +feature\\.flag +differs: absent from web-5\\.json; 'on' in web-1\\.json, web-2\\.json, web-3\\.json, web-4\\.json"
)
want("3 new finding\\(s\\): 0 error, 3 warning, 0 info")
refuse("server\\.port")
refuse("MARKER")

# 2. The threshold.
drift(0 ${five} --fail-on never)
drift(0 ${five} --fail-on error)

# 3. Hosts that agree.
drift(0 web-1.json web-2.json)
want("0 new finding\\(s\\)")

# 4. One key.
drift(
  1
  web-1.json
  web-2.json
  web-3.json
  --key
  log.level)
want("1 new finding\\(s\\)")
drift(
  0
  web-1.json
  web-2.json
  web-3.json
  --key
  server.port)
want("0 new finding\\(s\\)")
drift(
  2
  web-1.json
  web-2.json
  web-3.json
  --key
  no.such)

# 5. Input Doctor cannot use.
drift(2)
drift(2 web-1.json)
drift(2 web-1.json absent.json)
refuse("DRIFT")
drift(
  2
  web-1.json
  web-2.json
  --fail-on
  bogus)
drift(2 web-1.json web-2.json --nope)

# 6. The other formats.
foreach(format markdown sarif)
  drift(1 ${five} --format ${format})
  want("DRIFT")
  want("log\\.level")
  refuse("MARKER")
endforeach()

# 7. Nothing shown.
drift(1 ${five} --redact-all)
want(
  "log\\.level +differs \\(not shown: --redact-all\\): value 2 in web-3\\.json; value 1 in web-1\\.json, web-2\\.json, web-4\\.json, web-5\\.json"
)
refuse("'debug'")
refuse("'info'")
refuse("MARKER")

# 8. A baseline accepts a drift.
file(WRITE "${dir}/base.txt" "DRIFT log.level\n")
drift(
  0
  web-1.json
  web-2.json
  web-3.json
  --baseline
  base.txt)
want("1 accepted by the baseline")

# 9. A group of more than five names its first five.
foreach(n RANGE 1 7)
  host(many-${n}.json info MARKER-FLEET-ONE on)
endforeach()
host(many-8.json debug MARKER-FLEET-ONE on)
drift(
  1
  many-1.json
  many-2.json
  many-3.json
  many-4.json
  many-5.json
  many-6.json
  many-7.json
  many-8.json
  --key
  log.level)
want(
  "'debug' in many-8\\.json; 'info' in many-1\\.json, many-2\\.json, many-3\\.json, many-4\\.json, many-5\\.json and 2 more"
)

# 10. More than sixteen groups for one key.
set(uniq)
foreach(n RANGE 1 18)
  string(LENGTH "${n}" len)
  if(len EQUAL 1)
    set(n "0${n}")
  endif()
  host(uniq-${n}.json level-${n} MARKER-FLEET-ONE on)
  list(APPEND uniq uniq-${n}.json)
endforeach()
drift(1 ${uniq} --key log.level)
want("log\\.level +differs: more than 16 distinct values in 18 reports")

# 11. The limit: 1025 copies of one report is too many, 1024 is not.
set(copies)
set(first1024)
foreach(n RANGE 1 1025)
  string(LENGTH "${n}" len)
  string(SUBSTRING "0000${n}" ${len} 4 tail)
  set(name "c${tail}.json")
  configure_file("${dir}/web-1.json" "${dir}/${name}" COPYONLY)
  list(APPEND copies ${name})
  if(n LESS 1025)
    list(APPEND first1024 ${name})
  endif()
endforeach()
drift(2 ${copies})
want("at most 1024")
drift(0 ${first1024})
want("0 new finding\\(s\\)")

# 12. Digests made under different keys cannot be compared.
doctor_run(
  "${work}"
  drift
  cf_fx_keyed_a.json
  cf_fx_keyed_a2.json
  cf_fx_keyed_b.json)
if(NOT rc EQUAL 0)
  fail("drift over digests of different keys: exit ${rc}")
endif()
want("CFD020 +info +db\\.password")
refuse("DRIFT")
doctor_run(
  "${work}"
  drift
  cf_fx_keyed_a.json
  cf_fx_keyed_a2.json
  cf_fx_keyed_b.json
  --fail-on
  info)
if(NOT rc EQUAL 1)
  fail("drift over digests of different keys, --fail-on info: exit ${rc}")
endif()
doctor_run("${work}" drift cf_fx_keyed_a.json cf_fx_keyed_a2.json)
if(NOT rc EQUAL 1)
  fail("drift over two digests under one key: exit ${rc}")
endif()
want("db\\.password +differs \\(by digest\\)")

message("doctor drift: clean")
