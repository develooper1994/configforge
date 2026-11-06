# The program the conformance run (scripts/check/conformance.py, verify's
# EXTRA=conformance) puts each suite document through. Built with the
# tests, so it compiles on every preset; it is run only by that step,
# which needs the suites and a network to fetch them.
add_executable(conform conform.c)
target_link_libraries(conform PRIVATE configforge)
set_target_properties(
  conform
  PROPERTIES C_STANDARD 90
             C_STANDARD_REQUIRED ON
             C_EXTENSIONS OFF)
configforge_apply_warnings(conform)
configforge_apply_analysis(conform)

# A codec from a shared library (conform --codec, D-68): dlopen where the
# platform has it, LoadLibrary on Windows, none under WASI.
if(UNIX AND NOT CMAKE_SYSTEM_NAME STREQUAL "WASI")
  target_compile_definitions(conform PRIVATE CONFORM_DLOPEN=1)
  target_link_libraries(conform PRIVATE ${CMAKE_DL_LIBS})
endif()

# The module that proves it, in a shared build: conform and the codecs
# it loads then use the one library, as a program and its plugin would.
# A static library is not built position-independent, so it cannot be
# linked into a module.
if(BUILD_SHARED_LIBS AND NOT CMAKE_SYSTEM_NAME STREQUAL "WASI")
  add_library(conform_codecs MODULE codec_module.c)
  target_link_libraries(conform_codecs PRIVATE configforge)
  set_target_properties(
    conform_codecs
    PROPERTIES C_STANDARD 90
               C_STANDARD_REQUIRED ON
               C_EXTENSIONS OFF)
  configforge_apply_warnings(conform_codecs)
  configforge_apply_analysis(conform_codecs)
  set(data ${CMAKE_CURRENT_SOURCE_DIR}/data)
  add_test(NAME conformance.codec_kv
           COMMAND conform kv ${data}/module.kv --keys --codec
                   $<TARGET_FILE:conform_codecs> conform_kv)
  set_tests_properties(
    conformance.codec_kv PROPERTIES PASS_REGULAR_EXPRESSION
                                    "server.port\tuntyped\t9443")
  if(CF_WITH_JSON)
    add_test(NAME conformance.codec_json
             COMMAND conform json ${data}/module.json --keys --codec
                     $<TARGET_FILE:conform_codecs> conform_json)
    set_tests_properties(
      conformance.codec_json PROPERTIES PASS_REGULAR_EXPRESSION
                                        "server.port\tint\t9443")
  endif()
  add_test(NAME conformance.codec_missing
           COMMAND conform kv ${data}/module.kv --codec
                   $<TARGET_FILE:conform_codecs> no_such_codec)
  set_tests_properties(conformance.codec_missing PROPERTIES WILL_FAIL TRUE)
endif()
