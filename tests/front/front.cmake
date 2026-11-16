# Runs the front door, `configforge <command>`, and asserts what it
# prints and the exit code it returns: its own command line, the name
# rule that keeps it from running a path, and that the programs it
# runs get their arguments whole and their exit code passed back.
#
#   -DFRONT=<the program>  -DHAVE_DOCTOR=<ON|OFF>  -DUNIX_HOST=<0|1>
#   -DWORK=<a directory of its own>
#
# Run from where Doctor's fixtures are written.

# front_case(<exit> <stdout regex or -> <stderr regex or -> <args>...)
function(front_case expect_rc out_re err_re)
  execute_process(
    COMMAND ${FRONT} ${ARGN}
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err)
  front_check("${expect_rc}" "${out_re}" "${err_re}" "${rc}" "${out}" "${err}"
              "${ARGN}")
endfunction()

function(front_check expect_rc out_re err_re rc out err what)
  set(shown "configforge ${what}\nexit ${rc}\nstdout:\n${out}\nstderr:\n${err}")
  if(NOT rc EQUAL expect_rc)
    message(FATAL_ERROR "expected exit ${expect_rc}\n${shown}")
  endif()
  if(NOT out_re STREQUAL "-" AND NOT out MATCHES "${out_re}")
    message(FATAL_ERROR "stdout does not match '${out_re}'\n${shown}")
  endif()
  if(NOT err_re STREQUAL "-" AND NOT err MATCHES "${err_re}")
    message(FATAL_ERROR "stderr does not match '${err_re}'\n${shown}")
  endif()
endfunction()

# 1. The version.
front_case(0 "^ConfigForge [0-9]+\\.[0-9]+\\.[0-9]+\n$" - --version)

# 2. The help: on stdout when asked for, on stderr when it is not.
front_case(0 "configforge doctor" - help)
front_case(0 "configforge doctor" - --help)
front_case(0 "configforge doctor" - -h)
front_case(2 - "configforge doctor")

# 3. A command that is not there.
front_case(2 - "'nosuch' is not a configforge command" nosuch)

# 4. A name is lower case letters, digits and hyphens, so that no path
#    and no option is ever run.
front_case(2 - "is not a command name" ../doctor)
front_case(2 - "is not a command name" Doctor)
front_case(2 - "is not a command name" /bin/sh)
front_case(2 - "is not a command name" -x)

if(HAVE_DOCTOR)
  # 5. A program it runs.
  front_case(0 "^ConfigForge Doctor" - doctor --version)

  # 6. Its exit code passes through.
  front_case(1 - - doctor lint cf_fx_leak.json)

  # 7. An argument with spaces arrives whole.
  front_case(2 - "'no such key' is not in" doctor explain cf_fx_prod.json
             "no such key")

  # 8. A quote and a trailing backslash arrive as written.
  front_case(2 - "'a\"b\\\\' is not in" doctor explain cf_fx_prod.json
             "a\"b\\")
endif()

# 9. A program on the PATH and not beside the front door.
if(UNIX_HOST)
  file(REMOVE_RECURSE ${WORK})
  file(WRITE ${WORK}/src/configforge-hello
       "#!/bin/sh\necho \"hello $*\"\nexit 3\n")
  file(COPY ${WORK}/src/configforge-hello DESTINATION ${WORK}/path
       FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E env "PATH=${WORK}/path:$ENV{PATH}" ${FRONT}
            hello a b
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err)
  front_check(3 "hello a b" - "${rc}" "${out}" "${err}" "hello a b")
endif()
