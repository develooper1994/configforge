# A secret's digest made under a key (D-85) is told from one made under
# another key, or under none: Doctor says the two cannot be compared
# (CFD020), not that the secret changed (CFD014). The reports are
# fixtures.c's: one contract, db.password sensitive, as key A with the
# value "one" and then "two", as key B with "one", and as no key.

cmake_minimum_required(VERSION 3.10)

set(work "${CMAKE_CURRENT_BINARY_DIR}")

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

set(a cf_fx_keyed_a.json)
set(a2 cf_fx_keyed_a2.json)
set(b cf_fx_keyed_b.json)
set(n cf_fx_keyed_n.json)

# One run, which must succeed.
#
# ~~~
# run_ok(<arg>...)
# ~~~
function(run_ok)
  doctor_run(${ARGN})
  if(NOT rc EQUAL 0)
    fail("doctor ${ARGN}: exit ${rc}")
  endif()
  set(all
      "${all}"
      PARENT_SCOPE)
endfunction()

# Under one key, a different value is a change.
run_ok(
  diff
  ${a}
  ${a2}
  --fail-on
  never)
if(NOT all MATCHES "CFD014[^\n]*db.password")
  fail("diff a a2 did not report CFD014")
endif()
if(all MATCHES "CFD020")
  fail("diff a a2 reported CFD020")
endif()

# Under two keys, the same value cannot be told from a different one.
foreach(pair "${a}|${b}" "${b}|${a}" "${n}|${a}" "${a}|${n}")
  string(REPLACE "|" ";" two "${pair}")
  run_ok(diff ${two} --fail-on never)
  if(NOT all MATCHES "CFD020[^\n]*db.password")
    fail("diff ${two} did not report CFD020")
  endif()
  if(all MATCHES "CFD014")
    fail("diff ${two} reported CFD014")
  endif()
endforeach()

# The kinds are named, a key by its id and no key as fnv1a, and no digest
# is shown.
run_ok(
  diff
  ${a}
  ${n}
  --fail-on
  never)
if(NOT all MATCHES "CFD020[^\n]*sip24:[^ ]* here, fnv1a there")
  fail("diff a n did not name the two kinds")
endif()
if(all MATCHES "[0-9a-f]{16}")
  fail("diff a n showed a digest")
endif()

# A history and a blame meet the same pair.
run_ok(
  history
  ${a}
  ${b}
  --fail-on
  never)
if(NOT all MATCHES "CFD020[^\n]*db.password")
  fail("history a b did not report CFD020")
endif()
if(all MATCHES "value changed")
  fail("history a b said the value changed")
endif()
run_ok(
  blame
  db.password
  ${a}
  ${b}
  --fail-on
  never)
if(NOT all MATCHES "CFD020")
  fail("blame a b did not report CFD020")
endif()
if(all MATCHES "value changed")
  fail("blame a b said the value changed")
endif()

# Under a key the fingerprint is still one Doctor can recompute.
foreach(report ${a} ${a2} ${b})
  run_ok(lint ${report} --fail-on never)
  if(all MATCHES "CFD019")
    fail("lint ${report} reported CFD019")
  endif()
endforeach()

message("doctor keyed: clean")
