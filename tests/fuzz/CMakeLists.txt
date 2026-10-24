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

# The deterministic fuzzer's corpora, written as files.
add_executable(fuzz_seeds ${CMAKE_SOURCE_DIR}/tests/unit/test_fuzz.c)
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
    xml)
foreach(reader ${readers})
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
            -max_len=8192 -seed_inputs=seed_${reader} -print_final_stats=1)
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
