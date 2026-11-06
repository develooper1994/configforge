# The Python examples: the C ones through bindings/python, run with the
# shared library this build made and checked for the same lines as their
# C and Rust twins.
find_package(
  Python3
  COMPONENTS Interpreter
  QUIET)
if(NOT Python3_Interpreter_FOUND)
  message(STATUS "examples: no Python 3; the Python examples are not run")
  return()
endif()

# Register a run of one Python example.
#
# ~~~
# python_expect(<example> <working-dir> <regex>...)
# ~~~
function(python_expect name dir)
  string(REPLACE ";" "|||" wanted "${ARGN}")
  set(src ${CMAKE_SOURCE_DIR}/bindings/python/src)
  set(args
      "-E|||env|||CONFIGFORGE_LIB=$<TARGET_FILE:configforge>"
      "|||PYTHONPATH=${src}|||${Python3_EXECUTABLE}"
      "|||${CMAKE_CURRENT_SOURCE_DIR}/${name}.py")
  string(REPLACE ";" "" args "${args}")
  add_test(NAME example.${name}_python
           COMMAND ${CMAKE_COMMAND} -DEXE=${CMAKE_COMMAND} "-DARGS=${args}"
                   -DDIR=${dir} "-DEXPECT=${wanted}" -P ${CF_EXAMPLE_EXPECT})
endfunction()

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/tier0-run)
python_expect(tier0 ${CMAKE_CURRENT_BINARY_DIR}/tier0-run "tls=off")
if(CF_WITH_TOML
   AND CF_WITH_DIR
   AND CF_WITH_DOTENV)
  python_expect(
    layers
    ${CMAKE_SOURCE_DIR}/examples/c/layers
    "server.host = \"0.0.0.0\""
    "server.port = 9443"
    "server.workers = 12"
    "conf.d +12 +selected +conf.d/90-local.ini"
    "db.url = <sensitive"
    "log.level = \"warn\""
    "cache.size = 64")
endif()
