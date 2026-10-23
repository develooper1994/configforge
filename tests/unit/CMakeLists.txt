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
    test_threads
    test_fuzz
    test_starve
    test_oom
    test_derive
    test_watch
    test_complexity)

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
  if(CF_CPPCHECK_TESTS)
    set_target_properties(${t} PROPERTIES C_CPPCHECK "${CF_CPPCHECK_TESTS}")
  endif()
  add_test(NAME unit.${t} COMMAND ${t})
endforeach()

# The same suite under a comma-decimal locale and a Turkish locale.
# The Turkish case is the standard test for the tolower hazard.
foreach(loc de_DE.UTF-8 tr_TR.UTF-8)
  add_test(NAME unit.test_numeric.${loc} COMMAND test_numeric ${loc})
  add_test(NAME unit.test_value.${loc} COMMAND test_value ${loc})
endforeach()

# Under the libkqueue test build one scenario is the stand-in's to fail,
# not this code's; test_watch.c says which and why.
if(CF_WAKE_KQUEUE
   AND NOT APPLE
   AND NOT CMAKE_SYSTEM_NAME MATCHES "BSD|DragonFly")
  target_compile_definitions(test_watch PRIVATE CF_TEST_LIBKQUEUE=1)
endif()

# A directory of its own for the watch test. It asserts that a handle
# goes quiet, and every other test writes its files into the shared
# working directory - which is exactly what the handle watches. Run in
# parallel, the neighbours kept it awake and the test failed on correct
# code.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/watch)
set_tests_properties(
  unit.test_watch PROPERTIES WORKING_DIRECTORY
                             ${CMAKE_CURRENT_BINARY_DIR}/watch)

# Change notification through the C++03 wrapper, built as C++98 like
# every other wrapper consumer, in a directory of its own for the same
# reason as the C test.
if(CF_WITH_CXX)
  add_executable(test_wrapper_watch test_wrapper_watch.cpp)
  target_link_libraries(test_wrapper_watch PRIVATE configforge)
  set_target_properties(
    test_wrapper_watch
    PROPERTIES CXX_STANDARD 98
               CXX_STANDARD_REQUIRED ON
               CXX_EXTENSIONS OFF)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(test_wrapper_watch PRIVATE -pedantic -Werror -Wall
                                                      -Wextra)
  elseif(MSVC)
    # The C set is GCC's C flags; MSVC's is language-neutral and applies.
    configforge_apply_warnings(test_wrapper_watch)
  endif()
  configforge_apply_analysis(test_wrapper_watch)
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/watch_cxx)
  add_test(NAME unit.test_wrapper_watch COMMAND test_wrapper_watch)
  set_tests_properties(
    unit.test_wrapper_watch PROPERTIES WORKING_DIRECTORY
                                       ${CMAKE_CURRENT_BINARY_DIR}/watch_cxx)
endif()

# A non-ASCII argument, round-tripped. On Windows this is the only test
# of the wide command-line path: ctest starts the process with the
# argument in UTF-16, the CRT's argv holds the active code page's
# approximation of it, and cf_cli_args has to recover the original.
add_test(NAME unit.test_lex.utf8 COMMAND test_lex --utf8 "çğı")

# Threads are not in C89. Where they exist the contract in D-14 is
# exercised for real; where they do not, the same test still checks the
# invariants the contract rests on - that a read touches nothing
# mutable, and that the reference count is the only thing that moves.
find_package(Threads QUIET)
if(CMAKE_USE_PTHREADS_INIT)
  target_compile_definitions(test_threads PRIVATE CF_TEST_PTHREADS=1)
  target_link_libraries(test_threads PRIVATE Threads::Threads)
endif()

# The fuzzer is the slowest test here and the one most worth running
# under the sanitiser, where a read one byte past the end is a report
# rather than a pass. Given a longer timeout because a future corpus
# will make it slower and a timeout failure reads like a hang.
set_tests_properties(unit.test_fuzz PROPERTIES TIMEOUT 120)
