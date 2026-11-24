# --redact-all shows no value of any key, as if each were secret (D-86).
# The pair from fixtures.c holds plain values: server.port is 9090 in
# production, from the environment, and server.host is 127.0.0.1 in both.
# No command, in no format, may print either with the flag; without it,
# dump does print 9090, so the test can fail.

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

set(prod cf_fx_prod.json)
set(staging cf_fx_staging.json)

# Every command that prints something about a key, in every format it
# has. A run Doctor refuses outright would print nothing and pass, so
# each must also have exited 0.
set(runs
    "diff|${prod}|${staging}|--redact-all|--fail-on|never"
    "diff|${prod}|${staging}|--redact-all|--fail-on|never|--format|markdown"
    "diff|${prod}|${staging}|--redact-all|--fail-on|never|--format|sarif"
    "lint|${prod}|--redact-all|--fail-on|never"
    "lint|${prod}|--redact-all|--fail-on|never|--format|markdown"
    "lint|${prod}|--redact-all|--fail-on|never|--format|sarif"
    "history|${prod}|${staging}|--redact-all|--fail-on|never"
    "drift|${prod}|${staging}|--redact-all|--fail-on|never"
    "drift|${prod}|${staging}|--redact-all|--fail-on|never|--format|markdown"
    "drift|${prod}|${staging}|--redact-all|--fail-on|never|--format|sarif"
    "blame|server.host|${prod}|${staging}|--redact-all|--fail-on|never"
    "dump|${prod}|--redact-all"
    "explain|${prod}|server.host|--redact-all"
    "explain|${prod}|server.port|--redact-all")
foreach(run IN LISTS runs)
  string(REPLACE "|" ";" argv "${run}")
  doctor_run(${argv})
  if(NOT rc EQUAL 0)
    fail("doctor ${run}: exit ${rc}")
  endif()
  # Markdown escapes the dots of an address, so it is looked for both ways.
  foreach(needle 9090 127.0.0.1 "127\\.0\\.0\\.1")
    string(FIND "${all}" "${needle}" at)
    if(NOT at EQUAL -1)
      fail("doctor ${run} printed a value: ${needle}")
    endif()
  endforeach()
endforeach()

# A flag that took a value, or was left out, would not be caught above:
# dump without it must show the value.
doctor_run(dump ${prod})
if(NOT rc EQUAL 0)
  fail("doctor dump ${prod}: exit ${rc}")
endif()
string(FIND "${all}" 9090 at)
if(at EQUAL -1)
  fail("doctor dump without --redact-all did not show 9090")
endif()

# The diff still says that the port changed, and what it did not say it
# changed to.
doctor_run(diff ${prod} ${staging} --redact-all --fail-on never)
string(FIND "${all}" "differs (not shown: --redact-all)" at)
if(at EQUAL -1)
  fail("doctor diff --redact-all did not say server.port differs")
endif()

message("doctor redact: clean")
