# The project's own rules, in .semgrep/ (gate.semgrep):
#
#   cmake -DSEMGREP=<exe> -DSRC=<root> -P scripts/check/semgrep.cmake
#
# 1. Every rule's fixture: the lines marked `ruleid:` are matched and the
#    lines marked `ok:` are not (semgrep --test). A fixture is tested one
#    at a time, with its rule alone: --test takes one target and does not
#    look into a directory whose name starts with a dot.
# 2. The rules over lib and the Doctor's headers, nothing from the
#    registry, no metrics, no check for a newer version.
# 3. An exception, `nosemgrep: <rule id> -- <reason>`, says why: one
#    without a reason fails.
#
# The first that fails ends the run.

cmake_minimum_required(VERSION 3.10)

if(NOT SEMGREP OR NOT SRC)
  message(FATAL_ERROR "usage: -DSEMGREP=<exe> -DSRC=<root> -P semgrep.cmake")
endif()

set(common --metrics=off --disable-version-check)

file(GLOB rules "${SRC}/.semgrep/*.yaml")
if(NOT rules)
  message(FATAL_ERROR "no rule in ${SRC}/.semgrep")
endif()
foreach(rule ${rules})
  string(REGEX REPLACE "\.yaml$" ".c" fixture "${rule}")
  if(NOT EXISTS "${fixture}")
    message(FATAL_ERROR "${rule} has no fixture ${fixture}")
  endif()
  execute_process(
    COMMAND ${SEMGREP} --test --config "${rule}" "${fixture}" ${common}
    WORKING_DIRECTORY "${SRC}"
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE out)
  # --test exits 0 when a fixture fails: its report is what says.
  if(NOT rc EQUAL 0
     OR out MATCHES "did not pass"
     OR NOT out MATCHES "All tests passed")
    message("${out}")
    message(FATAL_ERROR "the fixture of ${rule} does not pass")
  endif()
endforeach()

execute_process(
  COMMAND ${SEMGREP} scan --config .semgrep --error --quiet ${common} lib
          tools/doctor/include
  WORKING_DIRECTORY "${SRC}"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE out)
if(NOT rc EQUAL 0)
  message("${out}")
  message(FATAL_ERROR "semgrep found what the project's rules refuse")
endif()

file(
  GLOB_RECURSE
  files
  "${SRC}/lib/*.c"
  "${SRC}/lib/*.h"
  "${SRC}/tools/*.c"
  "${SRC}/tools/*.h")
set(bad 0)
foreach(f ${files})
  file(STRINGS "${f}" lines REGEX "nosemgrep")
  foreach(line ${lines})
    if(NOT line MATCHES "nosemgrep:[ \t]*[A-Za-z0-9_-]+[ \t]+--[ \t]+[^ \t]")
      message("${f}: a nosemgrep without its reason (nosemgrep: <id> -- <why>)")
      set(bad 1)
    endif()
  endforeach()
endforeach()
if(bad)
  message(FATAL_ERROR "an exception without its reason")
endif()
message("semgrep: the fixtures pass, the tree is clean")
