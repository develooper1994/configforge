# A baseline accepts what is already there, so only new findings fail.
#
# The point being tested is adoption: the commonest way a lint tool dies
# is two hundred findings on its first run, and a tool that cannot be
# adopted gradually is a tool nobody adopts.

set(work "${CMAKE_CURRENT_BINARY_DIR}")

# Without one, the dead-source report fails - at warning, which is where
# CFD010 sits by default, so the threshold is said out loud.
execute_process(
  COMMAND ${DOCTOR} lint cf_fx_dead.json --fail-on warning
  WORKING_DIRECTORY "${work}"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE out)
if(NOT rc EQUAL 1)
  message("${out}")
  message(FATAL_ERROR "expected the dead-source report to fail")
endif()

# With the finding accepted, the same report passes and says how much it
# is carrying.
file(WRITE "${work}/base.txt" "CFD010 user-file\n")
execute_process(
  COMMAND ${DOCTOR} lint cf_fx_dead.json --baseline base.txt --fail-on warning
  WORKING_DIRECTORY "${work}"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE out)
message("${out}")
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "a baselined finding should not fail the run")
endif()
if(NOT out MATCHES "1 accepted by the baseline")
  message(FATAL_ERROR "the summary does not say what is being carried")
endif()
if(out MATCHES "CFD010")
  message(FATAL_ERROR "a baselined finding was printed anyway, which is the "
                      "noise a baseline exists to remove")
endif()

# An entry that matches nothing is reported. A rule that stopped firing
# is either a fix worth noticing or a rule that quietly broke.
file(WRITE "${work}/base2.txt" "CFD010 user-file\nCFD099 gone.away\n")
execute_process(
  COMMAND ${DOCTOR} lint cf_fx_dead.json --baseline base2.txt --fail-on warning
  WORKING_DIRECTORY "${work}"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE out)
message("${out}")
if(NOT out MATCHES "stale")
  message(FATAL_ERROR "a baseline entry matching nothing was not reported")
endif()
# And it does not fail the build: failing would make people stop keeping
# the file rather than start pruning it.
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "a stale entry should not fail the run")
endif()

# A baseline that does not exist is an empty one, so a first run needs
# no file before the tool will say anything.
execute_process(
  COMMAND ${DOCTOR} lint cf_fx_dead.json --baseline absent.txt --fail-on warning
  WORKING_DIRECTORY "${work}"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE out)
if(NOT rc EQUAL 1)
  message("${out}")
  message(FATAL_ERROR "a missing baseline should accept nothing")
endif()

message("doctor baseline: clean")
