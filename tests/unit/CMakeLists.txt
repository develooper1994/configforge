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
    test_complexity
    test_reserved
    test_unicode
    test_usage
    test_toml
    test_codec
    test_features
    test_wipe
    test_secrets
    test_diff
    test_schema
    test_yaml
    test_xml
    test_store
    test_secretsdir
    test_properties
    test_write
    test_registry
    test_interpolate
    test_rename
    test_null
    test_edges
    test_codec_edges
    test_lex_edges
    test_report_edges
    test_live
    test_log
    test_edit)

# The directories test_starve's every-source run reads. C89 has no mkdir.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cf_starve.d
     ${CMAKE_CURRENT_BINARY_DIR}/cf_starve_secrets)

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

# What configforge.h adds for a C11 program (D-53), where the compiler
# can build one. CF_FORCE_PORTABLE turns every later-standard path off,
# so the test has nothing to test there.
if("c_std_11" IN_LIST CMAKE_C_COMPILE_FEATURES AND NOT CF_FORCE_PORTABLE)
  add_executable(test_c11 test_c11.c)
  target_link_libraries(test_c11 PRIVATE configforge)
  set_target_properties(
    test_c11
    PROPERTIES C_STANDARD 11
               C_STANDARD_REQUIRED ON
               C_EXTENSIONS OFF)
  configforge_apply_warnings(test_c11)
  configforge_apply_analysis(test_c11)
  # Analysed as what it is: cppcheck is told C89 for everything else,
  # and would stop at this test's own check that it is C11.
  if(CF_CPPCHECK_TESTS)
    string(REPLACE "--std=c89" "--std=c11" c11_cppcheck "${CF_CPPCHECK_TESTS}")
    set_target_properties(test_c11 PROPERTIES C_CPPCHECK "${c11_cppcheck}")
  endif()
  add_test(NAME unit.test_c11 COMMAND test_c11)
endif()

# The documents test_codec_edges reads, where they are in the source tree.
target_compile_definitions(
  test_codec_edges PRIVATE CF_EDGES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")

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

# The directories test_secretsdir reads, made here because C89 has no
# way to make one.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cf_secrets
     ${CMAKE_CURRENT_BINARY_DIR}/cf_secrets_bad)
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

  # Changing and keeping, and reading without repeating a default,
  # through the wrapper, built the same way.
  foreach(t test_wrapper_write test_wrapper_access)
    add_executable(${t} ${t}.cpp)
    target_link_libraries(${t} PRIVATE configforge)
    set_target_properties(
      ${t}
      PROPERTIES CXX_STANDARD 98
                 CXX_STANDARD_REQUIRED ON
                 CXX_EXTENSIONS OFF)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
      target_compile_options(${t} PRIVATE -pedantic -Werror -Wall -Wextra)
    elseif(MSVC)
      configforge_apply_warnings(${t})
    endif()
    configforge_apply_analysis(${t})
    add_test(NAME unit.${t} COMMAND ${t})
  endforeach()

  # The same reads under C++17. The header is C++03 but is included by
  # programs on every later standard, and there cf_i64 can be long: the
  # wrapper compiled as C++98 for years and not at all as C++11 on
  # LP64, because nothing built it any other way.
  add_executable(test_wrapper_access_cxx17 test_wrapper_access.cpp)
  target_link_libraries(test_wrapper_access_cxx17 PRIVATE configforge)
  set_target_properties(test_wrapper_access_cxx17 PROPERTIES CXX_STANDARD 17
                                                             CXX_EXTENSIONS OFF)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(test_wrapper_access_cxx17 PRIVATE -pedantic -Werror
                                                             -Wall -Wextra)
  elseif(MSVC)
    configforge_apply_warnings(test_wrapper_access_cxx17)
  endif()
  configforge_apply_analysis(test_wrapper_access_cxx17)
  add_test(NAME unit.test_wrapper_access_cxx17
           COMMAND test_wrapper_access_cxx17)

  # The wrapper's C++17 and C++20 parts, each where the header itself
  # turns it on: a test of the C++20 part built where the library has no
  # <format> would test nothing, so it is asked of the header, not the
  # compiler.
  include(CheckCXXSourceCompiles)

  # Whether configforge.hpp, compiled as C++<std>, turns on CF_CPP<std>.
  function(cf_wrapper_part std var)
    set(CMAKE_CXX_STANDARD ${std})
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_REQUIRED_INCLUDES ${PROJECT_SOURCE_DIR}/include)
    set(probe "#include <configforge/configforge.hpp>\n")
    string(APPEND probe "#if !CF_CPP${std}\n#error off\n#endif\n")
    string(APPEND probe "int main(){return 0;}")
    check_cxx_source_compiles("${probe}" ${var})
  endfunction()
  set(cf_modern)
  if("cxx_std_17" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
    cf_wrapper_part(17 CF_WRAPPER_CXX17)
    if(CF_WRAPPER_CXX17)
      list(APPEND cf_modern "test_wrapper_cxx17:17")
    endif()
  endif()
  if("cxx_std_20" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
    cf_wrapper_part(20 CF_WRAPPER_CXX20)
    if(CF_WRAPPER_CXX20)
      list(APPEND cf_modern "test_wrapper_cxx20:20")
    endif()
  endif()
  foreach(entry ${cf_modern})
    string(REPLACE ":" ";" parts "${entry}")
    list(GET parts 0 t)
    list(GET parts 1 std)
    add_executable(${t} ${t}.cpp)
    target_link_libraries(${t} PRIVATE configforge)
    set_target_properties(
      ${t}
      PROPERTIES CXX_STANDARD ${std}
                 CXX_STANDARD_REQUIRED ON
                 CXX_EXTENSIONS OFF)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
      target_compile_options(${t} PRIVATE -pedantic -Werror -Wall -Wextra)
    elseif(MSVC)
      configforge_apply_warnings(${t})
    endif()
    configforge_apply_analysis(${t})
    add_test(NAME unit.${t} COMMAND ${t})
  endforeach()
endif()

# A non-ASCII argument, round-tripped. On Windows this is the only test
# of the wide command-line path: ctest starts the process with the
# argument in UTF-16, the CRT's argv holds the active code page's
# approximation of it, and cf_cli_args has to recover the original.
add_test(NAME unit.test_lex.utf8 COMMAND test_lex --utf8 "çğı")

# A ConfigMap is updated through symbolic links, which only POSIX has.
if(UNIX)
  target_compile_definitions(test_live PRIVATE CF_TEST_SYMLINKS=1)
endif()

# Threads are not in C89. Where they exist the contract in D-14 is
# exercised for real; where they do not, the same test still checks the
# invariants the contract rests on - that a read touches nothing
# mutable, and that the reference count is the only thing that moves.
find_package(Threads QUIET)
if(CMAKE_USE_PTHREADS_INIT)
  target_compile_definitions(test_threads PRIVATE CF_TEST_PTHREADS=1)
  target_link_libraries(test_threads PRIVATE Threads::Threads)
  target_compile_definitions(test_live PRIVATE CF_TEST_PTHREADS=1)
  target_link_libraries(test_live PRIVATE Threads::Threads)
endif()

# The fuzzer is the slowest test here and the one most worth running
# under the sanitiser, where a read one byte past the end is a report
# rather than a pass. Given a longer timeout because a future corpus
# will make it slower and a timeout failure reads like a hang.
set_tests_properties(unit.test_fuzz PROPERTIES TIMEOUT 120)
