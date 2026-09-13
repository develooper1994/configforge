# Can anyone actually use this?
#
# Everything else here tests the library from inside its own build,
# where every header is on the include path whether or not it is
# installed and every target property is already set. A consumer has
# none of that: it finds a package, links a target, and includes the
# umbrella header. A header left out of the install list breaks there
# and nowhere else.
#
# Installing from the existing build tree rather than making a second
# one, so the whole check costs a copy and one small compile.

# Not under a sanitiser. The installed library is instrumented and the
# consumer is not, so it fails to link - correctly. A sanitised build is
# not a shippable artifact, so "can a consumer use this" is not a
# question that applies there, and a gate that fires on correct code
# teaches people to ignore it.
if(SANITIZE)
  message("install gate: skipped under the sanitiser")
  return()
endif()

set(prefix "${BIN}/install-check")
set(work "${BIN}/consumer-check")
file(REMOVE_RECURSE "${prefix}" "${work}")

execute_process(
  COMMAND ${CMAKE_COMMAND} --install "${BIN}" --prefix "${prefix}"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE out)
if(NOT rc EQUAL 0)
  message("${out}")
  message(FATAL_ERROR "the library does not install")
endif()

execute_process(
  COMMAND
    ${CMAKE_COMMAND} -S "${SRC}/tests/consumer" -B "${work}"
    -DCMAKE_PREFIX_PATH=${prefix} -DCF_CONSUMER_CXX=${WITH_CXX}
    -DCMAKE_C_COMPILER=${CC}
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE out)
if(NOT rc EQUAL 0)
  message("${out}")
  message(FATAL_ERROR "find_package(ConfigForge) does not work")
endif()

execute_process(
  COMMAND ${CMAKE_COMMAND} --build "${work}"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE out)
if(NOT rc EQUAL 0)
  message("${out}")
  message(FATAL_ERROR "a consumer cannot compile against the installed package")
endif()

execute_process(
  COMMAND "${work}/consumer"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE out)
if(NOT rc EQUAL 0)
  message("${out}")
  message(FATAL_ERROR "the consumer compiles but does not run")
endif()
message("${out}")

if(WITH_CXX AND EXISTS "${work}/consumer_cpp")
  execute_process(
    COMMAND "${work}/consumer_cpp"
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE out)
  if(NOT rc EQUAL 0)
    message("${out}")
    message(FATAL_ERROR "the C++ consumer compiles but does not run")
  endif()
  message("${out}")
endif()

message("install gate: clean")
