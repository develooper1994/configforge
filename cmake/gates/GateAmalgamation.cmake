# The amalgamation is a distribution form, so it is built and run on
# every commit rather than generated at release time and hoped over.
# It also catches the one class of error that only a single translation
# unit can have: two static functions in different files sharing a name.
find_package(
  Python3
  COMPONENTS Interpreter
  QUIET)
if(NOT Python3_Interpreter_FOUND)
  message("amalgamation gate: no python, skipped")
  return()
endif()

execute_process(
  COMMAND ${Python3_EXECUTABLE} "${SRC}/scripts/amalgamate.py" "${SRC}"
          "${BIN}/amalgamation"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE out)
if(NOT rc EQUAL 0)
  message("${out}")
  message(FATAL_ERROR "amalgamation could not be generated")
endif()
message("${out}")

file(
  WRITE "${BIN}/amalgamation/smoke.c"
  "#include <configforge/configforge.h>
#include <stdio.h>
#include <string.h>
int main(int argc, char** argv)
{
    cf_error   err = CF_ERROR_INIT;
    cf_config* cfg;
    cf_i64     port;

    cfg = cf_app(\"amalg\", argc, argv, &err);
    if (cfg == NULL) { fputs(err.message, stderr); return 1; }
    port = cf_get_int(cfg, \"server.port\", 8080);
    cf_config_free(cfg);
    return (port == 8080) ? 0 : 1;
}
")
execute_process(
  COMMAND
    ${CC} -std=c89 -pedantic -Werror -Wall -Wextra -Wshadow -I
    "${BIN}/amalgamation" -o "${BIN}/amalgamation/smoke"
    "${BIN}/amalgamation/smoke.c" "${BIN}/amalgamation/configforge.c"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE out)
if(NOT rc EQUAL 0)
  message("${out}")
  message(FATAL_ERROR "the amalgamation does not compile as C89")
endif()

execute_process(COMMAND "${BIN}/amalgamation/smoke" RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "the amalgamation compiles but does not work")
endif()

# Not under a sanitiser: the differential program links the instrumented
# library without the sanitiser runtime and fails, correctly. The
# amalgamation half is compiled fresh and uninstrumented, so it is only
# this comparison that does not apply there.
if(SANITIZE)
  message("amalgamation gate: clean (differential skipped under the "
          "sanitiser)")
  return()
endif()

# The options, built here from one value each rather than received as a
# pre-joined list of flags: a list of -D flags passed through a -D of
# its own arrives quoted in a way that depends on three layers agreeing,
# and when they did not the compiler saw CF_WITH_INI defined as
# something containing an equals sign. FEATURES names them, separated
# by '|' for the same reason; O_<name> is each one's value, L_<limit>
# each limit's. All of them: when only the formats were passed, the
# minimal preset compared a library built at 64-byte keys and without
# the emitter against an amalgamation built with neither difference.
set(DEFS "")
string(REPLACE "|" ";" features "${FEATURES}")
foreach(name ${features})
  set(value "${O_${name}}")
  if(NOT value)
    set(value 0)
  endif()
  list(APPEND DEFS "-DCF_WITH_${name}=${value}")
endforeach()
foreach(lim CF_KEY_MAX CF_MESSAGE_MAX CF_PATH_MAX CF_MAX_DEPTH)
  if(DEFINED L_${lim})
    list(APPEND DEFS "-D${lim}=${L_${lim}}")
  endif()
endforeach()

# The two sides learn their options differently, on purpose. The library
# side gets the generated cf_build_options.h on its include path, which
# is exactly how an installed consumer learns them. The amalgamation has
# no generated header - it is meant to be compiled by whoever receives
# it - so its options arrive as -D.
#
# Without this the amalgamation would carry the source tree's defaults
# while the library was compiled with something else, and the two would
# be compared as though the difference were a defect rather than the
# point of having options.
#
# Compiling and running proves it is *a* library. This proves it is
# *the* library: the same program, built both ways, has to produce the
# same configuration. A source file left out of the generator's list, an
# X-macro inlined at the wrong point, two static functions that merged -
# all three have happened here, and a smoke test survives all three.
execute_process(
  COMMAND
    ${CC} -std=c89 ${DEFS} -I "${BIN}/amalgamation" -o
    "${BIN}/amalgamation/digest_amalg" "${SRC}/tests/consumer/digest.c"
    "${BIN}/amalgamation/configforge.c"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE out)
if(NOT rc EQUAL 0)
  message("${out}")
  message(FATAL_ERROR "the differential program does not build against the "
                      "amalgamation")
endif()

execute_process(
  COMMAND
    ${CC} -std=c89 -I "${BIN}/generated" -I "${SRC}/include" -o
    "${BIN}/amalgamation/digest_lib" "${SRC}/tests/consumer/digest.c" "${LIB}"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE out)
if(NOT rc EQUAL 0)
  message("${out}")
  message(FATAL_ERROR "the differential program does not build against the "
                      "library")
endif()

execute_process(
  COMMAND "${BIN}/amalgamation/digest_amalg"
  OUTPUT_VARIABLE from_amalg
  RESULT_VARIABLE rc1)
execute_process(
  COMMAND "${BIN}/amalgamation/digest_lib"
  OUTPUT_VARIABLE from_lib
  RESULT_VARIABLE rc2)
if(NOT rc1 EQUAL 0 OR NOT rc2 EQUAL 0)
  message(FATAL_ERROR "the differential program does not run")
endif()
if(NOT from_amalg STREQUAL from_lib)
  message("amalgamation:")
  message("${from_amalg}")
  message("library:")
  message("${from_lib}")
  message(FATAL_ERROR "the amalgamation is not the library")
endif()
message("${from_lib}")

message("amalgamation gate: clean, and identical to the library")
