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
  reload)
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
