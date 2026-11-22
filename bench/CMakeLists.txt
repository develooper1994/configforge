# The benchmark: not a test, not a gate, not built by default.
#
#   cmake -B build-bench -DCMAKE_BUILD_TYPE=Release -DCF_BUILD_BENCH=ON
#   cmake --build build-bench --target cf_bench && build-bench/bench/cf_bench
#
#   build-bench/bench/cf_bench docs [--large]     large documents (P-1)
#   build-bench/bench/cf_bench --emit-docs <dir>  the documents as files, which
#                                                 bench/compare/ reads (P-1b)
#
# Release, or the numbers are the debug build's. The comparison with other
# libraries is bench/compare/, a CMake project of its own that this one never
# adds: nothing fetched for it can reach this build.
add_definitions(-DCF_INTERNAL_API=1)
add_executable(cf_bench bench.c)
target_link_libraries(cf_bench PRIVATE configforge configforge_doctor)
set_target_properties(
  cf_bench
  PROPERTIES C_STANDARD 90
             C_STANDARD_REQUIRED ON
             C_EXTENSIONS OFF)
configforge_apply_warnings(cf_bench)

# The documents are the ones committed (bench/docs-1mib.fnv), and the gate on
# instructions has its own tests (scripts/bench/test_gate.py).
add_test(
  NAME bench.docs
  COMMAND
    ${CMAKE_COMMAND} -DBENCH=$<TARGET_FILE:cf_bench>
    -DEXPECT=${CMAKE_CURRENT_SOURCE_DIR}/docs-1mib.fnv
    -DWORK=${CMAKE_CURRENT_BINARY_DIR}/docs-1mib -P
    ${CMAKE_SOURCE_DIR}/scripts/bench/check_docs.cmake)
find_package(
  Python3
  COMPONENTS Interpreter
  QUIET)
if(Python3_Interpreter_FOUND)
  add_test(
    NAME bench.gate
    COMMAND ${Python3_EXECUTABLE} -m unittest scripts/bench/test_gate.py
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()
