# The settings example, end to end.
#
# Checked rather than described: an example whose output is transcribed
# into a README is an example that drifts from it on the next change.

set(work "${CMAKE_CURRENT_BINARY_DIR}/settings-run")
file(REMOVE_RECURSE "${work}")
file(MAKE_DIRECTORY "${work}")

# Run the example and require both an exit code and a piece of output.
#
# ~~~
# run(<expected-exit> <expected-substring> <arg>...)
# ~~~
function(run expect_rc expect_out)
  execute_process(
    COMMAND ${SETTINGS} ${ARGN}
    WORKING_DIRECTORY "${work}"
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE out)
  if(NOT rc EQUAL expect_rc)
    message("${out}")
    message(FATAL_ERROR "expected exit ${expect_rc}, got ${rc}: ${ARGN}")
  endif()
  if(NOT out MATCHES "${expect_out}")
    message("${out}")
    message(FATAL_ERROR "expected output matching '${expect_out}': ${ARGN}")
  endif()
  message("$ settings ${ARGN}\n${out}")
endfunction()

# Nothing configured yet, so every value is its default.
run(0 "0\\.0\\.0\\.0:8080")

# No settings file, and that is not an error. explain() lists the source
# anyway, saying it had nothing for this key - which is a different
# answer from not mentioning it, and the one a reader needs.
run(0 "settings +not present" --explain=server.port)

# Change a setting. The value is validated against the contract that
# travelled with the derived configuration, then written.
run(0 "saved settings.json" --set=server.port=9000)
run(0 "0\\.0\\.0\\.0:9000")

# And the file holds what was configured, not every default.
file(READ "${work}/settings.json" saved)
if(NOT saved MATCHES "9000")
  message(FATAL_ERROR "the saved file does not contain the new value")
endif()
if(saved MATCHES "log")
  message(FATAL_ERROR "the saved file froze a default nobody touched")
endif()
if(saved MATCHES "explain|watch")
  message(FATAL_ERROR "a command was written into the settings file")
endif()

# Provenance survives the round trip: the file now wins, and the
# default it beat is still there to be seen.
run(0 "settings.json" --explain=server.port)

# The contract travels. A value the original would have refused is
# refused here too, before anything is written.
run(1 "outside 1\\.\\.65535" --set=server.port=99999)

# The whole watch cycle in one run: quiet, then an edit, then a reload
# that is quiet again. A one-shot process cannot observe a change on its
# own, because it stamps the file as it builds - so the example imitates
# the edit, which is the only honest way to show it here.
run(0 "after an edit, changed: yes" --watch)
run(0 "reloaded, changed: no" --watch)

message("settings example: clean")
