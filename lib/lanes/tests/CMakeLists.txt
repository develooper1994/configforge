# The tests of lanes, as plain C89 programs a ctest runs.
#
# test_lanes         every operation of lanes_scalar() against an oracle
#                    written in the test, and the detection, printed
# test_lanes_prefix  the sources built under LANES_PREFIX_CF, beside the
#                    unprefixed library: no clash, the same behaviour
# fuzz_lanes         lanes_best() and every built backend against
#                    lanes_scalar(), on random input; with LANES_FUZZ a
#                    libFuzzer harness (clang)
option(LANES_FUZZ "Build fuzz_lanes as a libFuzzer harness (clang)" OFF)

# Added by lanes' own build file, or by a project that registers the tests
# itself, from where lanes_sources.cmake has not been read.
if(NOT LANES_SOURCES)
  include(${CMAKE_CURRENT_LIST_DIR}/../lanes_sources.cmake)
endif()

# One test program: built from the sources named, a ctest unit.<name>.
function(lanes_test name)
  add_executable(${name} ${ARGN})
  lanes_target(${name})
  add_test(NAME unit.${name} COMMAND ${name})
endfunction()

lanes_test(test_lanes test_lanes.c)
target_link_libraries(test_lanes PRIVATE lanes)

# The same sources, compiled again under the prefix: its symbols are
# cf_lanes_*, so the unprefixed archive links beside them.
lanes_test(test_lanes_prefix test_lanes_prefix.c ${LANES_SOURCES})
target_compile_definitions(test_lanes_prefix PRIVATE LANES_PREFIX_CF=1)
target_link_libraries(test_lanes_prefix PRIVATE lanes)

lanes_test(fuzz_lanes fuzz_lanes.c)
target_link_libraries(fuzz_lanes PRIVATE lanes)
if(LANES_FUZZ)
  target_compile_definitions(fuzz_lanes PRIVATE LANES_LIBFUZZER=1)
  target_compile_options(fuzz_lanes PRIVATE -fsanitize=fuzzer)
  target_link_options(fuzz_lanes PRIVATE -fsanitize=fuzzer)
  # A libFuzzer binary fuzzes until stopped; ctest runs it for a moment.
  add_test(NAME unit.fuzz_lanes_libfuzzer COMMAND fuzz_lanes -max_total_time=10
                                                  -max_len=512)
  set_tests_properties(unit.fuzz_lanes_libfuzzer PROPERTIES TIMEOUT 120)
  set_tests_properties(unit.fuzz_lanes PROPERTIES DISABLED ON)
endif()
