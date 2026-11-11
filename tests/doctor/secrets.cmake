# A secret is compared by its digest and never shown (Doctor.md,
# section 11), in a full report as in a blind one. The pair from
# fixtures.c holds every value, and each secret's is a marker: no
# command, in no format, may print one. Run before the fix, diff printed
# both passwords in the clear, in every format, Markdown included -
# which is what the GitHub Action posts as a pull-request comment. One
# refusal's reason quotes a marker too, as an older library's could.

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

set(a cf_fx_secret_a.json)
set(b cf_fx_secret_b.json)

# Every command that prints something about a key, in every format it
# has. A run Doctor refuses outright would print nothing and pass, so
# each must also have run.
set(runs
    "diff|${a}|${b}|--fail-on|never"
    "diff|${a}|${b}|--fail-on|never|--format|markdown"
    "diff|${a}|${b}|--fail-on|never|--format|sarif"
    "diff|${b}|${a}|--fail-on|never"
    "lint|${a}|--fail-on|never"
    "lint|${b}|--fail-on|never|--format|markdown"
    "lint|${a}|--fail-on|never|--format|sarif"
    "history|${a}|${b}|--fail-on|never"
    "blame|db.password|${a}|${b}|--fail-on|never"
    "dump|${a}"
    "explain|${a}|db.password"
    "explain|${a}|api.pin"
    "explain|${b}|api.seed")
foreach(run IN LISTS runs)
  string(REPLACE "|" ";" argv "${run}")
  doctor_run(${argv})
  if(NOT rc EQUAL 0)
    fail("doctor ${run}: exit ${rc}")
  endif()
  if(all MATCHES "MARKER")
    fail("doctor ${run} printed a secret")
  endif()
  # What it does print is still worth reading: the key, and how the
  # value was compared.
  if(run MATCHES "^diff\\|${a}" AND NOT run MATCHES "format")
    set(on "[^\n]*")
    foreach(
      want
      "CFD014${on}db.password${on}by digest"
      "CFD017${on}api.salt${on}defaults differ \\(by digest\\)"
      "CFD017${on}api.seed${on}defaults differ \\(by digest\\)"
      "CFD014${on}api.seed${on}by digest")
      if(NOT all MATCHES "${want}")
        fail("doctor ${run} did not say ${want}")
      endif()
    endforeach()
  endif()
  # The PIN's refusal carries a reason that quotes the value, as a
  # report from 1.4.0 or before could: for a secret the reason is a
  # fixed word, whatever the report says.
  if(run MATCHES "^lint\\|${a}\\|--fail-on\\|never$"
     AND NOT all MATCHES "CFD006[^\n]*api.pin[^\n]*refused <redacted>: invalid")
    fail("doctor ${run} did not report the refused secret, redacted")
  endif()
  if(run MATCHES "^explain\\|${a}\\|api.pin$")
    if(NOT all MATCHES "rejected[^\n]*invalid")
      fail("doctor ${run} did not give the refusal's reason as a fixed word")
    endif()
  endif()
  if(run MATCHES "^history" AND NOT all MATCHES
                                "HISTORY[^\n]*db.password[^\n]*\\(by digest\\)")
    fail("doctor ${run} did not say the secret changed by its digest")
  endif()
  if(run MATCHES "^explain" AND NOT all MATCHES "<redacted>")
    fail("doctor ${run} did not show the secret redacted")
  endif()
endforeach()

message("doctor secrets: clean")
