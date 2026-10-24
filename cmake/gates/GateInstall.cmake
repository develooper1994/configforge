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
# Warnings are errors unless the build said otherwise (CF_WERROR): the
# Portable C Compiler warns at the C library's own headers.
set(werror -Werror)
if(DEFINED WERROR AND NOT WERROR)
  set(werror "")
endif()

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

# The path with no CMake: a plain -I against the install prefix, which
# is where nothing hands over the feature definitions and where the
# defaults used to win. A consumer who disagrees with the library about
# CF_WITH_REAL has a different sizeof for cf_value, so this is checked
# at compile time and again at run time - a header can say one thing
# while the archive was built from another.
# The installed library as a consumer links it: the archive of a static
# build, or -l and a run-time path for a shared one, which installs no
# archive and which the program would not find without being told.
if(EXISTS "${prefix}/lib/libconfigforge.a")
  set(installed_lib "${prefix}/lib/libconfigforge.a")
else()
  set(installed_lib -L${prefix}/lib -lconfigforge -Wl,-rpath,${prefix}/lib)
endif()
execute_process(
  COMMAND
    ${CC} -std=c89 -pedantic ${werror} -I "${prefix}/include"
    -DCF_EXPECT_REAL=${EXPECT_REAL} -DCF_EXPECT_JSON=${EXPECT_JSON} -o
    "${work}/options" "${SRC}/tests/consumer/options.c" ${installed_lib}
    ${YAML_LIB}
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE out)
if(NOT rc EQUAL 0)
  message("${out}")
  message(
    FATAL_ERROR "the installed headers disagree with the installed library")
endif()

execute_process(
  COMMAND "${work}/options"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE out)
message("${out}")
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "the installed headers and library disagree at run time")
endif()

# The path for a build that is not CMake: pkg-config, from the installed
# .pc file and nothing else. Skipped where there is no pkg-config.
find_program(PKG_CONFIG_EXE pkg-config)
if(PKG_CONFIG_EXE)
  file(GLOB_RECURSE pcs "${prefix}/*/configforge.pc")
  if(NOT pcs)
    message(FATAL_ERROR "no configforge.pc was installed")
  endif()
  list(GET pcs 0 pc)
  get_filename_component(pcdir "${pc}" DIRECTORY)
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E env PKG_CONFIG_PATH=${pcdir} ${PKG_CONFIG_EXE}
            --cflags --libs --static configforge
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE flags
    ERROR_VARIABLE err
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT rc EQUAL 0)
    message("${err}")
    message(FATAL_ERROR "pkg-config does not understand configforge.pc")
  endif()
  separate_arguments(flags UNIX_COMMAND "${flags}")
  execute_process(
    COMMAND ${CC} -std=c89 -pedantic ${werror} -o "${work}/consumer_pc"
            "${SRC}/tests/consumer/consumer.c" ${flags}
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE out)
  if(NOT rc EQUAL 0)
    message("${out}")
    message(FATAL_ERROR "a consumer cannot build from what pkg-config says")
  endif()
  # Run as a user of a shared build in a private prefix runs it: with the
  # loader told where the library is. The link is left as pkg-config
  # said, which is the thing under test.
  get_filename_component(libdir "${pcdir}" DIRECTORY)
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E env LD_LIBRARY_PATH=${libdir}
            DYLD_LIBRARY_PATH=${libdir} "${work}/consumer_pc"
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE out)
  if(NOT rc EQUAL 0)
    message("${out}")
    message(FATAL_ERROR "the pkg-config consumer builds but does not run")
  endif()
  message("install gate: pkg-config works")
endif()

message("install gate: clean")
