# The generators of docs/compliance and the like, each against a fixture.
add_test(
  NAME check.cert
  COMMAND
    ${CMAKE_COMMAND}
    -DGEN=${CMAKE_SOURCE_DIR}/scripts/generate/cert_matrix.cmake
    -DFIX=${CMAKE_CURRENT_SOURCE_DIR}/cert
    -DBIN=${CMAKE_CURRENT_BINARY_DIR}/cert -P
    ${CMAKE_CURRENT_SOURCE_DIR}/cert/check.cmake)

# scripts/check/mcdc.py against a two-condition fixture built with GCC's
# condition coverage (D-233); skips under another compiler.
find_package(
  Python3
  COMPONENTS Interpreter
  QUIET)
if(Python3_Interpreter_FOUND)
  set(_cf_py "${Python3_EXECUTABLE}")
else()
  set(_cf_py "")
endif()
add_test(
  NAME check.mcdc
  COMMAND
    ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR}
    -DBIN=${CMAKE_CURRENT_BINARY_DIR}/mcdc -DCC=${CMAKE_C_COMPILER}
    -DCC_ID=${CMAKE_C_COMPILER_ID} -DCC_VER=${CMAKE_C_COMPILER_VERSION}
    -DPY=${_cf_py} -P ${CMAKE_CURRENT_SOURCE_DIR}/mcdc/check.cmake)
