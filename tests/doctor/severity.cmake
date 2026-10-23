# Severity is the project's to choose, a profile can imply one, and the
# same findings can be written as SARIF for a forge to show on the line.

set(work "${CMAKE_CURRENT_BINARY_DIR}")

# Run Doctor in the fixture directory; leaves rc, out and err set.
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
  set(out
      "${out}"
      PARENT_SCOPE)
  set(err
      "${err}"
      PARENT_SCOPE)
endfunction()

# Show what Doctor said, then stop with the expectation that failed.
#
# ~~~
# fail(<what>)
# ~~~
function(fail what)
  message("${out}${err}")
  message(FATAL_ERROR "${what}")
endfunction()

# A project that wants a dead source to fail the build says so.
file(WRITE "${work}/sev_error.txt"
     "# a dead source is wrong here\nCFD010 error\n")
doctor_run(lint cf_fx_dead.json --severity sev_error.txt)
if(NOT rc EQUAL 1)
  fail("CFD010 raised to error should fail the run")
endif()
if(NOT out MATCHES "CFD010  error")
  fail("and print the severity the project chose")
endif()

# And one that does not want it at all turns it off.
file(WRITE "${work}/sev_off.txt" "CFD010 off\n")
doctor_run(
  lint
  cf_fx_dead.json
  --severity
  sev_off.txt
  --fail-on
  info)
if(out MATCHES "CFD010")
  fail("a rule turned off was reported anyway")
endif()

# CFD008 is "default-only in production": a warning in a production
# report, information anywhere else.
doctor_run(lint cf_fx_prod.json)
if(NOT out MATCHES "CFD008  warning")
  fail("default-only is a warning in production")
endif()
doctor_run(lint cf_fx_staging.json)
if(NOT out MATCHES "CFD008  info")
  fail("and information in staging")
endif()

# A profile line beats a plain one, for that profile only.
file(WRITE "${work}/sev_prof.txt" "CFD008 info\nCFD008 error production\n")
doctor_run(lint cf_fx_prod.json --severity sev_prof.txt)
if(NOT out MATCHES "CFD008  error" OR NOT rc EQUAL 1)
  fail("the production line applies in production, and fails the run")
endif()
doctor_run(lint cf_fx_staging.json --severity sev_prof.txt)
if(NOT out MATCHES "CFD008  info")
  fail("and the plain line everywhere else")
endif()

# Half understood is worse than not understood: the other half would be
# silently at its default.
file(WRITE "${work}/sev_bad.txt" "CFD010 loud\n")
doctor_run(lint cf_fx_dead.json --severity sev_bad.txt)
if(NOT rc EQUAL 2 OR NOT err MATCHES "sev_bad.txt:1")
  fail("an unknown severity is unusable input, and the line is named")
endif()

# SARIF: one document, the rule, the file and the line.
doctor_run(lint cf_fx_twice.json --format sarif)
if(NOT rc EQUAL 1)
  fail("SARIF output keeps the exit code")
endif()
foreach(needle "\"version\": \"2.1.0\"" "\"ruleId\": \"CFD001\""
               "cf_fx_twice.ini" "\"startLine\": 2")
  string(FIND "${out}" "${needle}" at)
  if(at EQUAL -1)
    fail("SARIF is missing ${needle}")
  endif()
endforeach()
if(NOT CMAKE_VERSION VERSION_LESS 3.19)
  # A document a forge cannot parse is no document at all.
  string(JSON n ERROR_VARIABLE json_err LENGTH "${out}" runs 0 results)
  if(NOT json_err STREQUAL "NOTFOUND" OR NOT n GREATER 0)
    fail("the SARIF is not valid JSON with results in it: ${json_err}")
  endif()
endif()

message("doctor severity: clean")
