# libFuzzer harnesses, one per reader of untrusted bytes (CF_FUZZ, clang).
#
# The fuzz.* tests run each for CF_FUZZ_SECONDS from test_fuzz.c's own
# corpora, which fuzz.seeds writes first, and keep what they learn in
# corpus_<reader>: every input that reached new code, which the next run
# starts from instead of learning it again (CI caches the directories
# and minimises them). A crash, a leak or a sanitiser report fails the
# test, and libFuzzer leaves the input that caused it in the test's
# directory as crash-<hash>: that file is the bug report.
set(CF_FUZZ_SECONDS
    10
    CACHE STRING "How long each libFuzzer test runs, in seconds")

# test_fuzz.c, which both programs here are made from, is in lib/config/tests
# and includes the helpers every test shares.
include_directories(${CMAKE_SOURCE_DIR}/tests/support)

# The deterministic fuzzer's corpora, written as files.
add_executable(fuzz_seeds ${CMAKE_SOURCE_DIR}/lib/config/tests/test_fuzz.c)
target_compile_definitions(fuzz_seeds PRIVATE CF_FUZZ_SEEDS=1)
target_link_libraries(fuzz_seeds PRIVATE configforge)
configforge_apply_analysis(fuzz_seeds)
add_test(NAME fuzz.seeds COMMAND fuzz_seeds)
set_tests_properties(fuzz.seeds PROPERTIES FIXTURES_SETUP FUZZ_SEEDS LABELS
                                           fuzz)

# The order is CF_FUZZ_TARGET's numbering in libfuzzer.c; YAML only in a
# build that has it.
set(readers
    json
    json5
    ini
    toml
    dotenv
    image
    replay
    argv)
if(CF_WITH_YAML)
  list(APPEND readers yaml)
endif()
if(CF_WITH_XML)
  list(APPEND readers xml)
endif()
if(CF_WITH_PROPERTIES)
  list(APPEND readers properties)
endif()
if(CF_WITH_CBOR)
  list(APPEND readers cbor)
endif()
if(CF_WITH_TLV)
  list(APPEND readers tlv ber)
endif()
# Each reader's number is its place in the whole list, not in the list
# this build has: with YAML out and XML in, counting would have given
# XML the number libfuzzer.c reads as YAML's.
set(all_readers
    json
    ini
    toml
    dotenv
    image
    replay
    argv
    yaml
    xml
    properties
    json5
    cbor
    tlv
    ber)
# The readers of text that is UTF-8 and nothing else (D-108): each has a
# second seed, a document with a byte that is not, so the refusal is fuzzed.
set(utf8_readers
    json
    json5
    ini
    toml
    yaml
    properties
    dotenv)
foreach(reader ${readers})
  set(seeds seed_${reader})
  if(reader STREQUAL "image")
    # M-12: a major 1 image beside the major 2 one the seeds program writes.
    set(seeds ${seeds},${CF_REPO_ROOT}/tests/support/data/image-v1.cfgb)
  endif()
  if(reader IN_LIST utf8_readers)
    set(seeds ${seeds},seed_${reader}_utf8)
  endif()
  list(FIND all_readers ${reader} target)
  math(EXPR target "${target} + 1")
  add_executable(fuzz_${reader} libfuzzer.c)
  target_compile_definitions(fuzz_${reader} PRIVATE CF_FUZZ_TARGET=${target})
  target_link_libraries(fuzz_${reader} PRIVATE configforge)
  configforge_apply_analysis(fuzz_${reader})
  target_compile_options(fuzz_${reader} PRIVATE -fsanitize=fuzzer)
  target_link_options(fuzz_${reader} PRIVATE -fsanitize=fuzzer)
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/corpus_${reader}")
  add_test(
    NAME fuzz.${reader}
    COMMAND fuzz_${reader} corpus_${reader} -max_total_time=${CF_FUZZ_SECONDS}
            -max_len=8192 -seed_inputs=${seeds} -print_final_stats=1)
  math(EXPR limit "${CF_FUZZ_SECONDS} * 3 + 60")
  set_tests_properties(
    fuzz.${reader}
    PROPERTIES FIXTURES_REQUIRED
               FUZZ_SEEDS
               LABELS
               fuzz
               TIMEOUT
               ${limit})
endforeach()

# D-109: a random document through a random chain of the six writing
# formats, each hop held to the table of what a format can hold
# (tests/codec/chain.h). Not one of the readers: it has no reader number.
# The seeds are written here: the bytes are ASCII digits, which modulo 8, 4
# and 6 are the numbers (source, hops less one, one writer a hop), so a
# digit is its own number.
if(CF_WITH_EMIT
   AND CF_WITH_REAL
   AND CF_WITH_INI
   AND CF_WITH_JSON
   AND CF_WITH_TOML
   AND CF_WITH_YAML
   AND CF_WITH_CBOR)
  set(chain_dir "${CMAKE_CURRENT_BINARY_DIR}")
  file(WRITE "${chain_dir}/fuzz_chain_seed1"
       "1131{\"a\": 1, \"b\": [true, null]}")
  file(WRITE "${chain_dir}/fuzz_chain_seed2" "001[s]\nk = v\n")
  file(WRITE "${chain_dir}/fuzz_chain_seed3" "3154[s]\nk = 1\n")
  # A table whose only member is a list of one multi-line text, YAML to INI:
  # INI holds neither the text nor an empty list nor, then, the table.
  file(WRITE "${chain_dir}/fuzz_chain_seed4" "400b:\n  b: [t\n\n    v]\n")
  add_executable(fuzz_chain chain.c
                            ${CMAKE_SOURCE_DIR}/tools/codec/convert_core.c)
  target_include_directories(fuzz_chain PRIVATE ${CMAKE_SOURCE_DIR}/tools/codec
                                                ${CMAKE_SOURCE_DIR}/tests/codec)
  target_link_libraries(fuzz_chain PRIVATE configforge_serialize)
  configforge_apply_analysis(fuzz_chain)
  target_compile_options(fuzz_chain PRIVATE -fsanitize=fuzzer)
  target_link_options(fuzz_chain PRIVATE -fsanitize=fuzzer)
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/corpus_chain")
  set(chain_seeds fuzz_chain_seed1 fuzz_chain_seed2 fuzz_chain_seed3
                  fuzz_chain_seed4)
  string(REPLACE ";" "," chain_seeds "${chain_seeds}")
  add_test(
    NAME fuzz.chain
    COMMAND fuzz_chain corpus_chain -max_total_time=${CF_FUZZ_SECONDS}
            -max_len=8192 -seed_inputs=${chain_seeds} -print_final_stats=1)
  math(EXPR limit "${CF_FUZZ_SECONDS} * 3 + 60")
  set_tests_properties(fuzz.chain PROPERTIES LABELS fuzz TIMEOUT ${limit})
endif()

# N-12: the same bytes to the YAML codec read through the built-in parser
# and to the one read through libyaml, in one process; a difference that
# yamldiff.known does not document aborts. Built where libyaml is found,
# by the names backends.cmake uses; without it there is no such target.
# An exploratory target, run by hand and nightly (the yamldiff job of
# ci.yml), never by ctest: a new class of difference must not turn a
# per-push run red.
if(CF_WITH_YAML)
  find_path(CF_LIBYAML_INCLUDE_DIR yaml.h)
  find_library(CF_LIBYAML_LIBRARY NAMES yaml libyaml yaml-0)
  if(CF_LIBYAML_INCLUDE_DIR AND CF_LIBYAML_LIBRARY)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/seed_yamldiff"
         "a: 1\nb: [x, y]\nc:\n  d: \"e\"\n")
    add_executable(fuzz_yamldiff yamldiff.c yamldiff_builtin.c
                                 yamldiff_libyaml.c)
    target_include_directories(
      fuzz_yamldiff PRIVATE ${CF_LIBYAML_INCLUDE_DIR}
                            ${CMAKE_SOURCE_DIR}/lib/serialize/src)
    target_compile_definitions(
      fuzz_yamldiff
      PRIVATE "YD_KNOWN=\"${CMAKE_CURRENT_SOURCE_DIR}/yamldiff.known\"")
    target_link_libraries(fuzz_yamldiff PRIVATE configforge_serialize
                                                ${CF_LIBYAML_LIBRARY})
    configforge_apply_analysis(fuzz_yamldiff)
    target_compile_options(fuzz_yamldiff PRIVATE -fsanitize=fuzzer)
    target_link_options(fuzz_yamldiff PRIVATE -fsanitize=fuzzer)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/corpus_yamldiff")
  else()
    message(STATUS "fuzz.yamldiff: libyaml not found, not built")
  endif()
endif()
