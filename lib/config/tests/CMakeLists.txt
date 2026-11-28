# Every unit test is a plain C89 program returning non-zero on failure.
# No test framework: a framework would be the library's first external
# dependency, and the tests are simple enough not to need one.
#
# These are the tests of the configuration library (D-100): a test belongs to
# the lowest component that holds every configforge/ header it includes,
# and each of these includes configforge.h or another header of config's.
cf_component_tests(config)

set(CF_UNIT_TESTS
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
    test_writefile
    test_lock
    test_cloexec
    test_fork
    test_stable
    test_report
    test_image
    test_iterate
    test_emit
    test_threads
    test_fuzz
    test_starve
    test_oom
    test_oom_paths
    test_derive
    test_watch
    test_complexity
    test_reserved
    test_unicode
    test_usage
    test_help
    test_toml
    test_json5
    test_codec
    test_detect
    test_features
    test_wipe
    test_secrets
    test_marker
    test_digestkey
    test_sizing
    test_secretlike
    test_diff
    test_schema
    test_yaml
    test_xml
    test_store
    test_secretsdir
    test_properties
    test_write
    test_registry
    test_preferences
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
    test_reason
    test_dirlimits
    test_trusted
    test_credentials
    test_cbor
    test_tlv
    test_compose
    test_kinds
    test_encoding
    test_utf8_refused
    test_reader_corpus
    test_places)

# The directories test_starve's every-source run reads. C89 has no mkdir.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cf_starve.d
     ${CMAKE_CURRENT_BINARY_DIR}/cf_starve_secrets)
# And test_oom_paths's, swept with one allocation refused at a time.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cf_oom.d)
# And test_codec's drop-in directory of a codec's own files.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cf_codec.d)
# And test_detect's, whose files have no extension.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cf_detect.d)
# And test_encoding's, whose files are in another encoding.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cf_encoding.d)

# WebAssembly (WASI preview 1) has no file notification, so nothing for
# test_watch to watch with; and no shell, so test_file's drop-in
# directory is made here rather than by the test.
if(CMAKE_SYSTEM_NAME STREQUAL "WASI")
  list(REMOVE_ITEM CF_UNIT_TESTS test_watch)
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cf_confd)
endif()

cf_unit_tests(${CF_UNIT_TESTS})

# What configforge.h adds for a C11 program (D-53), where the compiler
# can build one. CF_FORCE_PORTABLE turns every later-standard path off,
# so the test has nothing to test there.
if("c_std_11" IN_LIST CMAKE_C_COMPILE_FEATURES AND NOT CF_FORCE_PORTABLE)
  add_executable(test_c11 test_c11.c)
  target_link_libraries(test_c11 PRIVATE ${CF_TEST_LIB})
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

# The internal tier is not declared to a program (D-51, D-88): the
# source is a program's, and the compiler's message names the function.
include(ConfigForgeMustNotCompile)
cf_must_not_compile(
  unit.internal_fenced
  SOURCE
  ${CMAKE_CURRENT_SOURCE_DIR}/fenced.c
  LANG
  C
  STD
  90
  DEFINE
  CF_FENCED=1
  EXPECT
  "cf_ks_count")

# The documents test_codec_edges reads, where they are in the source tree
# (tests/support/data, shared with the tests of the other components).
target_compile_definitions(
  test_codec_edges PRIVATE CF_EDGES_DIR="${CF_REPO_ROOT}/tests/support/data")
# M-12: the major 1 image written before the header grew, kept as bytes.
target_compile_definitions(
  test_image PRIVATE CF_IMAGE_DATA="${CF_REPO_ROOT}/tests/support/data")
# A-7: the hashes of every short document each reader reads, written on the
# code before the escapes were made one function, and held to since.
set(corpus_golden "${CF_REPO_ROOT}/tests/support/data/reader_corpus.golden")
target_compile_definitions(test_reader_corpus
                           PRIVATE CF_READER_CORPUS_GOLDEN="${corpus_golden}")
set_tests_properties(unit.test_reader_corpus PROPERTIES TIMEOUT 600)
# The golden is the built-in YAML parser's: the test must know when it is not.
if(_CF_YAML_LIBYAML)
  target_compile_definitions(test_reader_corpus PRIVATE CF_YAML_LIBYAML=1)
  target_compile_definitions(test_places PRIVATE CF_YAML_LIBYAML=1)
  target_compile_definitions(test_reason PRIVATE CF_YAML_LIBYAML=1)
endif()
if(_CF_YAML_LIBFYAML)
  target_compile_definitions(test_reader_corpus PRIVATE CF_YAML_LIBFYAML=1)
  target_compile_definitions(test_places PRIVATE CF_YAML_LIBFYAML=1)
  target_compile_definitions(test_reason PRIVATE CF_YAML_LIBFYAML=1)
endif()
# And the inputs a YAML parser once hung on, which test_yaml reads: a hang
# is this test's timeout.
set(conform_data "${CF_REPO_ROOT}/tests/conformance/data")
target_compile_definitions(test_yaml PRIVATE CF_CONFORM_DATA="${conform_data}")
set_tests_properties(unit.test_yaml PROPERTIES TIMEOUT 60)
# test_serialize reads an INI section of 60,000 distinct arrays, which
# took a minute when each rescanned the lines above it and takes seconds
# now: the timeout is what fails if that comes back.
set_tests_properties(unit.test_serialize PROPERTIES TIMEOUT 30)

# Under the libkqueue test build one scenario is the stand-in's to fail,
# not this code's; test_watch.c says which and why.
if(CF_WAKE_KQUEUE
   AND NOT APPLE
   AND NOT CMAKE_SYSTEM_NAME MATCHES "BSD|DragonFly"
   AND TARGET test_watch)
  target_compile_definitions(test_watch PRIVATE CF_TEST_LIBKQUEUE=1)
  # libkqueue opens descriptors of its own (epoll, eventfd) that are not
  # close-on-exec; test_cloexec.c checks the library's own ones only.
  target_compile_definitions(test_cloexec PRIVATE CF_TEST_LIBKQUEUE=1)
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
# test_limits reads docs/Limits.md, so it is no entry of CF_UNIT_TESTS (its
# loop registers a test with no argument). Only a default build compares
# its compiled limits with the defaults documented on the page.
cf_unit_test_executable(test_limits ${CF_TEST_LIB})
if(CF_DEFAULT_LIMITS AND CF_STACK_BUDGET EQUAL 2048)
  target_compile_definitions(test_limits PRIVATE CF_LIMITS_DEFAULT=1)
endif()
add_test(NAME unit.test_limits COMMAND test_limits
                                       ${CF_REPO_ROOT}/docs/Limits.md)
# And test_trusted's: it makes directories, sets their modes and owners,
# and lists them as secrets, which nothing else should see (D-83).
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/trusted)
set_tests_properties(
  unit.test_trusted PROPERTIES WORKING_DIRECTORY
                               ${CMAKE_CURRENT_BINARY_DIR}/trusted)
# Modes, owners and groups are POSIX's.
if(UNIX AND NOT CMAKE_SYSTEM_NAME STREQUAL "WASI")
  target_compile_definitions(test_trusted PRIVATE CF_TEST_POSIX=1)
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
