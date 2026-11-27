# consumer.nodiscard: under C23 an ignored failure does not compile.
#
# A program that checks every result compiles with -std=c23 -Werror
# (nodiscard_c23.c, the control), and one that drops the result of
# cf_builder_create does not (nodiscard_ignored.c). Only GCC and Clang:
# MSVC has no C23 [[nodiscard]]. Compiled with -fsyntax-only against the
# source tree's headers, so nothing is linked or installed.
#
# Inputs: SRC, BIN, CC (the C compiler), CC_ID (its CMake id).
if(NOT CC_ID MATCHES "^(GNU|Clang|AppleClang)$")
  message("consumer.nodiscard: skipped, the C23 proof needs GCC or Clang "
          "(this compiler is ${CC_ID})")
  return()
endif()

file(GLOB incs "${SRC}/lib/*/include")
set(flags -std=c23 -Werror -fsyntax-only)
foreach(inc ${incs} "${BIN}/generated")
  list(APPEND flags "-I${inc}")
endforeach()

execute_process(
  COMMAND ${CC} ${flags} -x c "${SRC}/tests/consumer/nodiscard_c23.c"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE out)
if(NOT rc EQUAL 0 AND out MATCHES "std=c23")
  message("${out}")
  message("consumer.nodiscard: skipped, ${CC} does not take -std=c23")
  return()
endif()
if(NOT rc EQUAL 0)
  message("${out}")
  message(FATAL_ERROR "nodiscard_c23.c does not compile as C23")
endif()

execute_process(
  COMMAND ${CC} ${flags} -x c "${SRC}/tests/consumer/nodiscard_ignored.c"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE out)
if(rc EQUAL 0)
  message(FATAL_ERROR "a dropped cf_builder_create result compiles as C23: "
                      "CF_NODISCARD does nothing")
endif()
if(NOT out MATCHES "nodiscard|unused")
  message("${out}")
  message(
    FATAL_ERROR "nodiscard_ignored.c failed, but not for its ignored result")
endif()
message("consumer.nodiscard ok: the ignored result is refused")
