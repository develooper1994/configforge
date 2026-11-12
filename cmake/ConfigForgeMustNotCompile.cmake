# A check made at compile time is held by a program that must not
# compile (D-75). Nothing else in the build can show it: a test that runs
# proves only what compiled.
#
#   cf_must_not_compile(<name> SOURCE <file> LANG C|CXX STD <n>
#                       DEFINE <macro> EXPECT <regex>)
#
# An object library of the one source, with <macro> defined, left out of
# the build; and a test, <name>, that builds it and passes only when the
# build's output matches <regex> - the name or the message of the check
# that refused it. A source that fails for another reason, a header not
# found or a typo, does not match, and so does not pass; one that
# compiles does not either. Built through the build tool rather than by
# calling the compiler, so that MSVC's environment and every generator's
# flags are the ones the rest of the tests get. One at a time: two build
# tools in one tree at once are not safe on every generator.
function(cf_must_not_compile name)
  cmake_parse_arguments(
    MNC
    ""
    "SOURCE;LANG;STD;DEFINE;EXPECT"
    ""
    ${ARGN})
  string(REPLACE "." "_" t "mnc_${name}")
  add_library(${t} OBJECT EXCLUDE_FROM_ALL ${MNC_SOURCE})
  set_target_properties(
    ${t}
    PROPERTIES ${MNC_LANG}_STANDARD ${MNC_STD}
               ${MNC_LANG}_STANDARD_REQUIRED ON
               ${MNC_LANG}_EXTENSIONS OFF
               EXCLUDE_FROM_DEFAULT_BUILD ON)
  # The library's headers and definitions, without depending on the
  # library: building the case must not build anything else.
  target_include_directories(
    ${t} PRIVATE $<TARGET_PROPERTY:configforge,INTERFACE_INCLUDE_DIRECTORIES>
                 ${CMAKE_CURRENT_SOURCE_DIR})
  target_compile_definitions(
    ${t} PRIVATE ${MNC_DEFINE}
                 $<TARGET_PROPERTY:configforge,INTERFACE_COMPILE_DEFINITIONS>)
  add_test(NAME ${name} COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
                                --target ${t} --config $<CONFIG>)
  set_tests_properties(
    ${name} PROPERTIES PASS_REGULAR_EXPRESSION "${MNC_EXPECT}" RESOURCE_LOCK
                       cf_must_not_compile)
endfunction()
