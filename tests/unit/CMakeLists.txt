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
    test_lists
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
    test_json5
    test_codec
    test_detect
    test_features
    test_wipe
    test_secrets
    test_marker
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
    test_predict
    test_log
    test_edit
    test_lifetime
    test_truncate
    test_roundtrip
    test_bom
    test_budget
    test_dirlimits
    test_credentials)

# The directories test_starve's every-source run reads. C89 has no mkdir.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cf_starve.d
     ${CMAKE_CURRENT_BINARY_DIR}/cf_starve_secrets)
# And test_codec's drop-in directory of a codec's own files.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cf_codec.d)
# And test_detect's, whose files have no extension.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cf_detect.d)

# WebAssembly (WASI preview 1) has no file notification, so nothing for
# test_watch to watch with; and no shell, so test_file's drop-in
# directory is made here rather than by the test.
if(CMAKE_SYSTEM_NAME STREQUAL "WASI")
  list(REMOVE_ITEM CF_UNIT_TESTS test_watch)
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cf_confd)
endif()

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
  # D-75 in C11: each definition that must not compile, refused by
  # the words of the check that refused it.
  include(ConfigForgeMustNotCompile)
  set(cf_c11_refused
      "1:a default outside its own range"
      "2:a range whose low end is above its high end"
      "3:a text default longer than its max_len"
      "4:a text default shorter than its min_len"
      "5:a length range whose low end is above its high end"
      "6:a key empty or longer than CF_KEY_MAX"
      "7:required and defaulted at once")
  foreach(entry ${cf_c11_refused})
    string(REPLACE ":" ";" parts "${entry}")
    list(GET parts 0 n)
    list(GET parts 1 why)
    cf_must_not_compile(
      unit.refused.define${n}.c11
      SOURCE
      test_c11.c
      LANG
      C
      STD
      11
      DEFINE
      CF_TEST_REFUSED=${n}
      EXPECT
      "${why}")
  endforeach()
endif()

# The documents test_codec_edges reads, where they are in the source tree.
target_compile_definitions(
  test_codec_edges PRIVATE CF_EDGES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
# And the inputs a YAML parser once hung on, which test_yaml reads: a hang
# is this test's timeout.
set(conform_data "${CMAKE_SOURCE_DIR}/tests/conformance/data")
target_compile_definitions(test_yaml PRIVATE CF_CONFORM_DATA="${conform_data}")
set_tests_properties(unit.test_yaml PROPERTIES TIMEOUT 60)
# test_serialize reads an INI section of 60,000 distinct arrays, which
# took a minute when each rescanned the lines above it and takes seconds
# now: the timeout is what fails if that comes back.
set_tests_properties(unit.test_serialize PROPERTIES TIMEOUT 30)

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
   AND NOT CMAKE_SYSTEM_NAME MATCHES "BSD|DragonFly"
   AND TARGET test_watch)
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
# And test_dirlimits's, in a working directory of its own: it fills a
# directory with 257 files and makes FIFOs, which nothing else should
# list. The timeout is the test of the cases that used to hang.
set(cf_dirlimits ${CMAKE_CURRENT_BINARY_DIR}/dirlimits)
file(
  MAKE_DIRECTORY
  ${cf_dirlimits}/cf_many.d
  ${cf_dirlimits}/cf_many_secrets
  ${cf_dirlimits}/cf_l
  ${cf_dirlimits}/cf_fifo.d
  ${cf_dirlimits}/cf_fifo_secrets)
set_tests_properties(unit.test_dirlimits PROPERTIES WORKING_DIRECTORY
                                                    ${cf_dirlimits} TIMEOUT 60)
# FIFOs and symbolic links are POSIX's.
if(UNIX AND NOT CMAKE_SYSTEM_NAME STREQUAL "WASI")
  target_compile_definitions(test_dirlimits PRIVATE CF_TEST_FIFO=1)
endif()
# And test_credentials's: it sets $CREDENTIALS_DIRECTORY to a relative
# path, which is only the test's own in a working directory of its own.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/credentials/cf_creds)
set_tests_properties(
  unit.test_credentials PROPERTIES WORKING_DIRECTORY
                                   ${CMAKE_CURRENT_BINARY_DIR}/credentials)
if(TARGET test_watch)
  set_tests_properties(
    unit.test_watch PROPERTIES WORKING_DIRECTORY
                               ${CMAKE_CURRENT_BINARY_DIR}/watch)
endif()

# Change notification through the C++03 wrapper, built as C++98 like
# every other wrapper consumer, in a directory of its own for the same
# reason as the C test.
if(CF_WITH_CXX)
  add_executable(test_wrapper_watch test_wrapper_watch.cpp)
  target_link_libraries(test_wrapper_watch PRIVATE configforge)
  # C++98 on purpose: the floor, kept for good (D-72).
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
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/watch_cxx
       ${CMAKE_CURRENT_BINARY_DIR}/cf_wcreds) # test_wrapper_access's
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
  add_test(NAME unit.test_wrapper_write.rsp COMMAND test_wrapper_write --rsp
                                                    @cf_wrsp_args.txt)
  add_test(NAME unit.test_wrapper_write.rsp_absent
           COMMAND test_wrapper_write --rsp @cf_wrsp_absent.txt)

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

  # D-75: what the compiler checks of a key. test_wrapper_access compiled
  # as C++11 and C++14 as well, since the check is written for each (a
  # recursion, then a loop); and each case that must not compile, under
  # every standard from C++11 the compiler has, refused by name.
  include(ConfigForgeMustNotCompile)
  set(cf_refused "1:not_a_canonical_key" "2:a_default_outside_its_own_range"
                 "3:a_range_whose_low_end_is_above_its_high_end")
  foreach(std 11 14 17 20)
    if("cxx_std_${std}" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
      if(std LESS 17)
        add_library(test_wrapper_access_cxx${std} OBJECT
                    test_wrapper_access.cpp)
        target_link_libraries(test_wrapper_access_cxx${std} PRIVATE configforge)
        set_target_properties(
          test_wrapper_access_cxx${std}
          PROPERTIES CXX_STANDARD ${std}
                     CXX_STANDARD_REQUIRED ON
                     CXX_EXTENSIONS OFF)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
          target_compile_options(test_wrapper_access_cxx${std}
                                 PRIVATE -pedantic -Werror -Wall -Wextra)
        elseif(MSVC)
          configforge_apply_warnings(test_wrapper_access_cxx${std})
        endif()
      endif()
      foreach(entry ${cf_refused})
        string(REPLACE ":" ";" parts "${entry}")
        list(GET parts 0 n)
        list(GET parts 1 why)
        cf_must_not_compile(
          unit.refused.key${n}.cxx${std}
          SOURCE
          test_wrapper_access.cpp
          LANG
          CXX
          STD
          ${std}
          DEFINE
          CF_TEST_REFUSED=${n}
          EXPECT
          ${why})
      endforeach()
    endif()
  endforeach()
  if(TARGET test_wrapper_cxx17)
    cf_must_not_compile(
      unit.refused.fields.cxx17
      SOURCE
      test_wrapper_cxx17.cpp
      LANG
      CXX
      STD
      17
      DEFINE
      CF_TEST_REFUSED=1
      EXPECT
      not_a_canonical_key)
  endif()
  if(TARGET test_wrapper_cxx20)
    cf_must_not_compile(
      unit.refused.constinit.cxx20
      SOURCE
      test_wrapper_cxx20.cpp
      LANG
      CXX
      STD
      20
      DEFINE
      CF_TEST_REFUSED=1
      EXPECT
      a_default_outside_its_own_range)
  endif()
endif()

# A non-ASCII argument, round-tripped. On Windows this is the only test
# of the wide command-line path: ctest starts the process with the
# argument in UTF-16, the CRT's argv holds the active code page's
# approximation of it, and cf_cli_args has to recover the original.
add_test(NAME unit.test_lex.utf8 COMMAND test_lex --utf8 "çğı")

# Response files are opt-in (D-79). The arguments have to be the process's
# real ones, as above, so each case is its own run: a file that is there,
# and one that is not.
add_test(NAME unit.test_lex.rsp COMMAND test_lex --rsp @cf_rsp_args.txt)
add_test(NAME unit.test_lex.rsp_absent COMMAND test_lex --rsp
                                               @cf_rsp_absent.txt)

# A ConfigMap is updated through symbolic links, which only POSIX has.
if(UNIX)
  target_compile_definitions(test_live PRIVATE CF_TEST_SYMLINKS=1)
endif()

# Threads are not in C89. Where they exist the contract in D-14 is
# exercised for real; where they do not, the same test still checks the
# invariants the contract rests on - that a read touches nothing
# mutable, and that the reference count is the only thing that moves.
find_package(Threads QUIET)
if(CMAKE_USE_PTHREADS_INIT AND NOT CF_SINGLE_THREADED)
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
