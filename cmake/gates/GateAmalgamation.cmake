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
  "#include \"configforge.h\"
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
message("amalgamation gate: clean")
