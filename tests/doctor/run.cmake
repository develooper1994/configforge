# Runs Doctor and asserts both the exit code and that the findings it
# was supposed to produce are in the output. Checking only the exit
# code would pass on a tool that printed nothing.
string(REPLACE "|" ";" argv "${ARGS}")
execute_process(COMMAND ${DOCTOR} ${argv}
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE errout)
message("${out}${errout}")
if(NOT rc EQUAL EXPECT_FAIL)
  message(FATAL_ERROR "expected exit ${EXPECT_FAIL}, got ${rc}")
endif()

list(GET argv 0 cmd)

# Each lint case asserts the rule it exists for. A case that only
# checked the exit code would pass on a tool that printed nothing, and a
# case that checked the wrong rule would pass on a tool that found
# something else entirely.
if(cmd STREQUAL "lint" AND argv MATCHES "dead")
  if(NOT out MATCHES "CFD010")
    message(FATAL_ERROR "a dead source was not reported")
  endif()
  if(NOT out MATCHES "wins none")
    message(FATAL_ERROR "the finding does not say what is wrong")
  endif()
endif()

if(cmd STREQUAL "lint" AND argv MATCHES "leak")
  # A secret that reached the command line is the finding that matters
  # for the case policy exists for: rejecting it did not un-leak it.
  if(NOT out MATCHES "CFD016")
    message(FATAL_ERROR "a policy violation was not reported")
  endif()
  if(NOT out MATCHES "db.password")
    message(FATAL_ERROR "the policy finding does not name the key")
  endif()
endif()

if(cmd STREQUAL "lint" AND argv MATCHES "rules")
  # The three a single report can prove from its definitions alone.
  foreach(rule CFD005 CFD009 CFD011)
    if(NOT out MATCHES "${rule}")
      message(FATAL_ERROR "lint did not produce ${rule}")
    endif()
  endforeach()
  if(NOT out MATCHES "nonesuch.key")
    message(FATAL_ERROR "the undefined key was not named")
  endif()
  if(NOT out MATCHES "old.name")
    message(FATAL_ERROR "the deprecated key was not named")
  endif()
endif()
if(cmd STREQUAL "diff")
  foreach(rule CFD002 CFD003 CFD014)
    if(NOT out MATCHES "${rule}")
      message(FATAL_ERROR "diff did not produce ${rule}")
    endif()
  endforeach()
endif()
if(cmd STREQUAL "explain" AND EXPECT_FAIL EQUAL 0)
  if(NOT out MATCHES "shadowed")
    message(FATAL_ERROR "explain did not show the losing candidate")
  endif()
endif()
