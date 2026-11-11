# A secret's refusal says its kind and nothing of the value (Doctor.md,
# section 11). The report from fixtures.c has six sensitive keys, each
# refused for a different kind, each value a marker; every command that
# prints about a key must give the kind's wording and carry no byte of the
# value. Run before the change, each reason was the word "invalid".

cmake_minimum_required(VERSION 3.10)

set(work "${CMAKE_CURRENT_BINARY_DIR}")
set(report cf_fx_kinds.json)

# Each key and the wording its kind takes (cf_refusal_text).
set(kinds
    "k.type|wrong type"
    "k.range|out of range"
    "k.allowed|not one of the permitted values"
    "k.short|too short"
    "k.long|too long"
    "k.ref|a malformed reference")

# Run Doctor in the fixture directory; leaves rc and all (stdout and
# stderr together) set.
#
# ~~~
# doctor_run(<arg>...)
# ~~~
function(doctor_run)
  execute_process(
    COMMAND ${DOCTOR} ${ARGN}
    WORKING_DIRECTORY "${work}"
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err)
  set(rc
      "${rc}"
      PARENT_SCOPE)
  set(all
      "${out}${err}"
      PARENT_SCOPE)
endfunction()

# Show what Doctor said, then stop with the expectation that failed.
#
# ~~~
# fail(<what>)
# ~~~
function(fail what)
  message("${all}")
  message(FATAL_ERROR "${what}")
endfunction()

# The markers: the text one, and the digits the out-of-range one is.
set(leak "MARKER|s3cr3t|987654321")

# The report is read in each format Doctor writes.
foreach(format text markdown sarif)
  doctor_run(
    lint
    ${report}
    --fail-on
    never
    --format
    ${format})
  if(NOT rc EQUAL 0)
    fail("doctor lint ${format}: exit ${rc}")
  endif()
  if(all MATCHES "${leak}")
    fail("doctor lint ${format} printed a secret")
  endif()
  foreach(kind IN LISTS kinds)
    string(REPLACE "|" ";" kind "${kind}")
    list(GET kind 0 key)
    list(GET kind 1 wording)
    # Markdown writes the angle brackets as entities.
    if(NOT all MATCHES
       "${key}[^\n]*refused (<|&lt;)redacted(>|&gt;): ${wording}")
      fail("doctor lint ${format} did not say ${key} was refused: ${wording}")
    endif()
  endforeach()
endforeach()

# What a person looking into one key reads.
foreach(kind IN LISTS kinds)
  string(REPLACE "|" ";" kind "${kind}")
  list(GET kind 0 key)
  list(GET kind 1 wording)
  doctor_run(explain ${report} ${key})
  if(NOT rc EQUAL 0)
    fail("doctor explain ${key}: exit ${rc}")
  endif()
  if(all MATCHES "${leak}")
    fail("doctor explain ${key} printed a secret")
  endif()
  if(NOT all MATCHES "rejected[^\n]* ${wording}")
    fail("doctor explain ${key} did not give the kind: ${wording}")
  endif()
endforeach()

doctor_run(dump ${report})
if(all MATCHES "${leak}")
  fail("doctor dump printed a secret")
endif()

message("doctor refusals: clean")
