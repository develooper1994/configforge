# The C examples: every one is C89, as the library is.
foreach(ex tier0 tier1 explain_demo)
  add_executable(${ex} ${ex}.c)
  target_link_libraries(${ex} PRIVATE configforge)
  set_target_properties(
    ${ex}
    PROPERTIES C_STANDARD 90
               C_STANDARD_REQUIRED ON
               C_EXTENSIONS OFF)
  configforge_apply_warnings(${ex})
  configforge_apply_analysis(${ex})
  add_test(NAME example.${ex} COMMAND ${ex})
endforeach()

add_executable(settings settings.c)
target_link_libraries(settings PRIVATE configforge)
set_target_properties(
  settings
  PROPERTIES C_STANDARD 90
             C_STANDARD_REQUIRED ON
             C_EXTENSIONS OFF)
configforge_apply_warnings(settings)
configforge_apply_analysis(settings)

# The whole lifecycle, driven from a script so that every line of the
# README's output is produced by the code rather than transcribed from
# it. Run in its own directory because it writes a file.
if(CF_WITH_EMIT)
  add_test(NAME example.settings
           COMMAND ${CMAKE_COMMAND} -DSETTINGS=$<TARGET_FILE:settings> -P
                   ${CMAKE_CURRENT_SOURCE_DIR}/settings_test.cmake)
endif()

# The failure paths are part of the documentation too, so they are run
# and their exit status is asserted rather than described.
add_test(NAME example.tier1.range COMMAND tier1 --server.port=99999)
add_test(NAME example.tier1.typo COMMAND tier1 --server.pot=1)
add_test(NAME example.tier1.enum COMMAND tier1 --log.level=verbose)
set_tests_properties(example.tier1.range example.tier1.typo example.tier1.enum
                     PROPERTIES WILL_FAIL TRUE)

# The examples docs/Guide.md walks through. Each is built in every build,
# so it compiles against any set of options, and run where the parts it
# uses are compiled in - checked for a clean exit and for the lines that
# show it did what its comment says (expect.cmake).
foreach(
  ex
  layers
  reload
  embedded
  schema
  mistakes
  secrets
  plugins
  runtime)
  add_executable(${ex} ${ex}.c)
  target_link_libraries(${ex} PRIVATE configforge)
  set_target_properties(
    ${ex}
    PROPERTIES C_STANDARD 90
               C_STANDARD_REQUIRED ON
               C_EXTENSIONS OFF)
  configforge_apply_warnings(${ex})
  configforge_apply_analysis(${ex})
endforeach()

# Register a run of one example.
#
# ~~~
# example_expect(<name> <working-dir> <regex>...)
# ~~~
function(example_expect name dir)
  string(REPLACE ";" "|||" wanted "${ARGN}")
  add_test(
    NAME example.${name}
    COMMAND ${CMAKE_COMMAND} -DEXE=$<TARGET_FILE:${name}> -DDIR=${dir}
            "-DEXPECT=${wanted}" -P ${CMAKE_CURRENT_SOURCE_DIR}/expect.cmake)
endfunction()

if(CF_WITH_TOML
   AND CF_WITH_INI
   AND CF_WITH_DIR
   AND CF_WITH_DOTENV)
  # Read-only: it reads the files that ship beside it.
  example_expect(
    layers
    ${CMAKE_CURRENT_SOURCE_DIR}/layers
    "server.host = \"0.0.0.0\""
    "server.port = 9443"
    "server.workers = 12"
    "conf.d +12 +selected +conf.d/90-local.ini"
    "db.url = <sensitive"
    "log.level = \"warn\""
    "cache.size = 64")
endif()
if(CF_WITH_TOML)
  example_expect(
    reload
    ${CMAKE_CURRENT_BINARY_DIR}/reload-run
    "server.port: 8080 -> 9443"
    "db.password: changed, <sensitive>"
    "now on port 9443")
endif()
if(CF_WITH_IMAGE)
  example_expect(
    embedded ${CMAKE_CURRENT_BINARY_DIR}/embedded-run
    "channel 11, power 14 dBm, period 1000 ms"
    "firmware +11 +selected +release.ini:2:")
endif()
if(CF_WITH_REPORT AND CF_WITH_JSON)
  example_expect(
    schema
    ${CMAKE_CURRENT_BINARY_DIR}/schema-run
    "\"maximum\": 65535"
    "\"enum\": .\"debug\""
    "\"writeOnly\": true")
endif()

# What a user sees when the configuration is wrong: each message is the
# library's, so a change to one shows up here.
if(CF_WITH_INI)
  example_expect(
    mistakes
    ${CMAKE_CURRENT_BINARY_DIR}/mistakes-run
    "started: port 9000, log level info"
    "unknown key 'server.pot'; did you mean 'server.port'\\?"
    "server.port: 99999 is outside 1..65535"
    "log.level: not one of debug info warn error"
    "'db.url' is required and has no value"
    "rejected: \"lots\" is not a valid int"
    "started: cache.size 64")
  example_expect(
    runtime
    ${CMAKE_CURRENT_BINARY_DIR}/runtime-run
    "the request in flight still sees: info"
    "a new request sees: +debug"
    "admin +\"debug\" +selected"
    "service.ini \"info\" +shadowed"
    "0 workers: server.workers: 0 is outside 1..64"
    "still running with 4 workers")
endif()
if(CF_WITH_INI AND CF_WITH_REPORT)
  example_expect(
    secrets
    ${CMAKE_CURRENT_BINARY_DIR}/secrets-run
    "with a password of 13 characters"
    "rejected: policy: 'db.password' may only be set from env"
    "db.password: changed, <sensitive>"
    "the password is not in it"
    "after free: still there"
    "after wipe: gone")
endif()
if(CF_WITH_TOML)
  example_expect(
    plugins
    ${CMAKE_CURRENT_BINARY_DIR}/plugins-run
    "plugin auth: on"
    "plugins.auth.audiences = \\[\"web\", \"mobile\"\\]"
    "plugin cache: off"
    "plugins.ratelimit.per_minute = 600")
endif()
