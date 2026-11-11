# Text a report carries reaches a CI log, where a line that begins with
# "::" is a GitHub Actions workflow command, and a pull-request comment,
# where "@org/team" notifies a team and a bare URL becomes a link. The
# fixture's refusal quotes a reason with a line break and a command in
# it, and a value with a mention and a link; a second fixture carries a
# command in every field of the report that dump and explain print.

cmake_minimum_required(VERSION 3.10)

set(work "${CMAKE_CURRENT_BINARY_DIR}")

# Run Doctor in the fixture directory; leaves rc and all set.
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

# Text: the finding is there, on one line, with the line break shown as
# an escape rather than made.
doctor_run(lint cf_fx_inject.json)
if(NOT rc EQUAL 1)
  fail("the refusal should fail the run")
endif()
if(NOT all MATCHES "CFD006[^\n]*app.mode[^\n]*safe\\\\n::error")
  fail("the refusal was not reported on one line with its break escaped")
endif()
string(REGEX MATCH "(^|\n)::" command "${all}")
if(NOT command STREQUAL "")
  fail("a line of the output begins with '::', a workflow command")
endif()

# Markdown: neither the mention nor the link survives as one.
doctor_run(lint cf_fx_inject.json --format markdown)
if(NOT all MATCHES "CFD006")
  fail("the Markdown has no finding")
endif()
foreach(needle "@org" "https://" "://")
  string(FIND "${all}" "${needle}" at)
  if(NOT at EQUAL -1)
    fail("the Markdown carries '${needle}'")
  endif()
endforeach()

# dump and explain print the report's own fields as well as the
# candidates: its mode, profile and fingerprint, a key's type, and the
# format a document was read as and how. cf_fx_fields.json ends each of
# them with a line break and a workflow command.
set(runs "dump|cf_fx_fields.json" "explain|cf_fx_fields.json|server.port"
         "dump|cf_fx_inject.json" "explain|cf_fx_inject.json|app.mode")
foreach(run IN LISTS runs)
  string(REPLACE "|" ";" argv "${run}")
  doctor_run(${argv})
  if(NOT rc EQUAL 0)
    fail("doctor ${run}: exit ${rc}")
  endif()
  string(REGEX MATCH "(^|\n)::" command "${all}")
  if(NOT command STREQUAL "")
    fail("doctor ${run}: a line of the output begins with '::'")
  endif()
  if(run MATCHES "fields" AND NOT all MATCHES "\\\\n::add-mask::injected")
    fail("doctor ${run}: the field was not shown with its break escaped")
  endif()
endforeach()

message("doctor escape: clean")
