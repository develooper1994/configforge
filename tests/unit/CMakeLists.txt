# Every unit test is a plain C89 program returning non-zero on failure.
# No test framework: a framework would be the library's first external
# dependency, and the tests are simple enough not to need one.
set(CF_UNIT_TESTS
    test_port
    test_numeric
    test_alloc
    test_value
    test_keyspace
    test_lex
    test_sources
    test_explain
    test_scale
    test_define
    test_config
    test_golden
    test_serialize
    test_file
    test_report
    test_image
    test_iterate
    test_emit
    test_threads)

foreach(t ${CF_UNIT_TESTS})
  add_executable(${t} ${t}.c)
  target_link_libraries(${t} PRIVATE configforge)
  set_target_properties(
    ${t}
    PROPERTIES C_STANDARD 90
               C_STANDARD_REQUIRED ON
               C_EXTENSIONS OFF)
  configforge_apply_warnings(${t})
  configforge_apply_analysis(${t})
  add_test(NAME unit.${t} COMMAND ${t})
endforeach()

# The same suite under a comma-decimal locale and a Turkish locale.
# The Turkish case is the standard test for the tolower hazard.
foreach(loc de_DE.UTF-8 tr_TR.UTF-8)
  add_test(NAME unit.test_numeric.${loc} COMMAND test_numeric ${loc})
  add_test(NAME unit.test_value.${loc} COMMAND test_value ${loc})
endforeach()

# Threads are not in C89. Where they exist the contract in D-14 is
# exercised for real; where they do not, the same test still checks the
# invariants the contract rests on - that a read touches nothing
# mutable, and that the reference count is the only thing that moves.
find_package(Threads QUIET)
if(CMAKE_USE_PTHREADS_INIT)
  target_compile_definitions(test_threads PRIVATE CF_TEST_PTHREADS=1)
  target_link_libraries(test_threads PRIVATE Threads::Threads)
endif()
