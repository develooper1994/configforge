# The Rust examples: the C ones through the binding in bindings/rust,
# run with cargo and checked for the same lines as their C twins. Cargo
# builds the library again, from this checkout, with no dependency to
# download (--offline), into a target directory inside this build.
find_program(CARGO_EXE cargo)
if(NOT CARGO_EXE)
  message(STATUS "CF_RUST: cargo not found; the Rust examples are not run")
  return()
endif()

set(manifest ${CMAKE_SOURCE_DIR}/bindings/rust/Cargo.toml)
set(target ${CMAKE_CURRENT_BINARY_DIR}/target)

# Register a run of one Rust example under cargo.
#
# ~~~
# rust_expect(<example> <working-dir> <regex>...)
# ~~~
function(rust_expect name dir)
  string(REPLACE ";" "|||" wanted "${ARGN}")
  set(args
      "-E|||env|||CARGO_TARGET_DIR=${target}|||${CARGO_EXE}|||run|||--offline"
      "|||--quiet|||--manifest-path|||${manifest}|||--example|||${name}")
  string(REPLACE ";" "" args "${args}")
  add_test(NAME example.${name}_rust
           COMMAND ${CMAKE_COMMAND} -DEXE=${CMAKE_COMMAND} "-DARGS=${args}"
                   -DDIR=${dir} "-DEXPECT=${wanted}" -P ${CF_EXAMPLE_EXPECT})
  set_tests_properties(example.${name}_rust PROPERTIES RUN_SERIAL TRUE)
endfunction()

rust_expect(tier0 ${CMAKE_CURRENT_BINARY_DIR}/tier0-run "tls=off")
rust_expect(
  layers
  ${CMAKE_SOURCE_DIR}/examples/c/layers
  "server.host = \"0.0.0.0\""
  "server.port = 9443"
  "server.workers = 12"
  "conf.d +12 +selected +conf.d/90-local.ini"
  "db.url = <sensitive"
  "log.level = \"warn\""
  "cache.size = 64")
rust_expect(
  mistakes
  ${CMAKE_CURRENT_BINARY_DIR}/mistakes-run
  "started: port 9000, log level info"
  "unknown key 'server.pot'; did you mean 'server.port'\\?"
  "server.port: 99999 is outside 1..65535"
  "log.level: not one of debug info warn error"
  "'db.url' is required and has no value"
  "rejected: \"lots\" is not a valid int"
  "started: cache.size 64")
rust_expect(
  runtime
  ${CMAKE_CURRENT_BINARY_DIR}/runtime-run
  "the request in flight still sees: info"
  "a new request sees: +debug"
  "admin +\"debug\" +selected"
  "service.ini \"info\" +shadowed"
  "0 workers: server.workers: 0 is outside 1..64"
  "still running with 4 workers")
rust_expect(
  references
  ${CMAKE_CURRENT_BINARY_DIR}/references-run
  "url: https://web1:9443/"
  "selected, from .https://..server.host.:..server.port./."
  "with --server.host=web2, url: https://web2:9443/"
  "a cycle: server.(host|url): its references lead back to it")
