# The benchmark: not a test, not a gate, not built by default.
#
#   cmake -B build-bench -DCMAKE_BUILD_TYPE=Release -DCF_BUILD_BENCH=ON
#   cmake --build build-bench --target cf_bench && build-bench/bench/cf_bench
#
# Release, or the numbers are the debug build's.
add_executable(cf_bench bench.c)
target_link_libraries(cf_bench PRIVATE configforge configforge_doctor)
set_target_properties(
  cf_bench
  PROPERTIES C_STANDARD 90
             C_STANDARD_REQUIRED ON
             C_EXTENSIONS OFF)
configforge_apply_warnings(cf_bench)
