# The C++ examples: the C ones through the wrapper (configforge.hpp),
# each <name>.cpp beside <name>.c and saying the same thing. Built as
# C++98 on purpose: the wrapper claims C++03 and a claim the build does
# not check is a claim that decays.
foreach(
  ex
  tier0
  tier1
  layers
  mistakes
  runtime
  reload
  references
  migration)
  add_executable(${ex}_cpp ${ex}.cpp)
  target_link_libraries(${ex}_cpp PRIVATE configforge)
  set_target_properties(
    ${ex}_cpp
    PROPERTIES CXX_STANDARD 98
               CXX_STANDARD_REQUIRED ON
               CXX_EXTENSIONS OFF)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(${ex}_cpp PRIVATE -pedantic -Werror -Wall -Wextra)
  endif()
  configforge_apply_analysis(${ex}_cpp)
endforeach()

add_test(NAME example.tier0_cpp COMMAND tier0_cpp)
# It writes the configuration back out, which needs the emitter. Built
# either way, so the example compiles against every build.
if(CF_WITH_EMIT)
  add_test(NAME example.tier1_cpp COMMAND tier1_cpp)
endif()

# The same lines the C programs are checked for, under the same options.
if(CF_WITH_TOML
   AND CF_WITH_INI
   AND CF_WITH_DIR
   AND CF_WITH_DOTENV)
  # The files are the C program's; one fixture, two readers.
  example_expect(
    layers_cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../c/layers
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
    reload_cpp
    ${CMAKE_CURRENT_BINARY_DIR}/reload-run
    "server.port: 8080 -> 9443"
    "db.password: changed, <sensitive>"
    "now on port 9443")
endif()
if(CF_WITH_INI)
  example_expect(
    mistakes_cpp
    ${CMAKE_CURRENT_BINARY_DIR}/mistakes-run
    "started: port 9000, log level info"
    "unknown key 'server.pot'; did you mean 'server.port'\\?"
    "server.port: 99999 is outside 1..65535"
    "log.level: not one of debug info warn error"
    "'db.url' is required and has no value"
    "rejected: \"lots\" is not a valid int"
    "started: cache.size 64")
  example_expect(
    runtime_cpp
    ${CMAKE_CURRENT_BINARY_DIR}/runtime-run
    "the request in flight still sees: info"
    "a new request sees: +debug"
    "admin +\"debug\" +selected"
    "service.ini \"info\" +shadowed"
    "0 workers: server.workers: 0 is outside 1..64"
    "still running with 4 workers")
endif()

# One value made from others (D-10), the same lines in both languages.
if(CF_WITH_INI)
  example_expect(
    references_cpp
    ${CMAKE_CURRENT_BINARY_DIR}/references-run
    "url: https://web1:9443/"
    "selected, from .https://..server.host.:..server.port./."
    "with --server.host=web2, url: https://web2:9443/"
    "a cycle: server.(host|url): its references lead back to it")
endif()

# The same preflight from C++: the wrapper hands Doctor the configuration.
if(TARGET configforge_doctor)
  add_executable(preflight_cpp preflight.cpp)
  target_link_libraries(preflight_cpp PRIVATE configforge_doctor)
  set_target_properties(
    preflight_cpp
    PROPERTIES CXX_STANDARD 98
               CXX_STANDARD_REQUIRED ON
               CXX_EXTENSIONS OFF)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(preflight_cpp PRIVATE -pedantic -Werror -Wall
                                                 -Wextra)
  endif()
  configforge_apply_analysis(preflight_cpp)
  if(CF_WITH_INI
     AND CF_WITH_REPORT
     AND CF_WITH_JSON)
    example_expect(
      preflight_cpp
      ${CMAKE_CURRENT_BINARY_DIR}/preflight-run
      "preflight: CFD007 error db.password at argv.1."
      "refusing to serve"
      "preflight clean: serving on :9443, database db.internal")
  endif()
endif()

# The administrator's policy over the user's settings, from C++.
add_executable(policy_cpp policy.cpp)
target_link_libraries(policy_cpp PRIVATE configforge)
set_target_properties(
  policy_cpp
  PROPERTIES CXX_STANDARD 98
             CXX_STANDARD_REQUIRED ON
             CXX_EXTENSIONS OFF)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
  target_compile_options(policy_cpp PRIVATE -pedantic -Werror -Wall -Wextra)
endif()
configforge_apply_analysis(policy_cpp)
if(WIN32)
  example_expect(
    policy_cpp
    ${CMAKE_CURRENT_BINARY_DIR}/policy-run
    "font 12, theme dark, channel stable"
    "policy +12 +selected"
    "nightly.  +rejected: policy: 'updatechannel' may only be set from policy")
else()
  add_test(NAME example.policy_cpp COMMAND policy_cpp)
endif()

# Renaming a key without breaking the files written for the old name.
if(CF_WITH_INI)
  example_expect(
    migration_cpp
    ${CMAKE_CURRENT_BINARY_DIR}/migration-run
    "1. a file written for version 1"
    "connecting to db1.internal:5432"
    "app.ini:2:1 .as db.host."
    "connecting to db2.internal:5432")
endif()
