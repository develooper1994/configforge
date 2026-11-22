# AFL++ on the libFuzzer harnesses of tests/fuzz, built by the `afl` preset
# (afl-clang-fast, which links AFL++'s own main in place of libFuzzer's: the
# program reads one input from the file named on its command line).
#
#   cmake -DBUILD=build-afl -DSECONDS=<n> -P scripts/check/afl.cmake
#
# Runs fuzz_seeds once, which writes the seeds as files (seed_<reader>,
# seed_<reader>_utf8), then every fuzz_<reader> the build made for SECONDS
# each, one after the other: afl-fuzz binds a core, and a runner has few.
# A file named id:* in <out>/<reader>/default/crashes/ fails the script,
# which names it and leaves the directory.
#
# Run by `verify.cmake -DEXTRA=afl`, which also configures and builds.

cmake_minimum_required(VERSION 3.10)

if(NOT DEFINED BUILD)
  message(FATAL_ERROR "afl.cmake: -DBUILD=<the afl preset's build directory>")
endif()
if(NOT DEFINED SECONDS)
  set(SECONDS 10)
endif()

get_filename_component(fuzzdir "${BUILD}/tests/fuzz" ABSOLUTE)
get_filename_component(outroot "${BUILD}/afl-out" ABSOLUTE)
get_filename_component(seedroot "${BUILD}/afl-seeds" ABSOLUTE)

set(asan_options
    "abort_on_error=1:detect_leaks=0:symbolize=0:allocator_may_return_null=1")
find_program(AFL_FUZZ_EXE afl-fuzz)
if(NOT AFL_FUZZ_EXE)
  message(FATAL_ERROR "afl.cmake: no afl-fuzz")
endif()
if(NOT EXISTS "${fuzzdir}/fuzz_seeds")
  message(FATAL_ERROR "afl.cmake: ${fuzzdir}/fuzz_seeds is not built")
endif()

execute_process(
  COMMAND "${fuzzdir}/fuzz_seeds"
  WORKING_DIRECTORY "${fuzzdir}"
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE out)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "afl.cmake: fuzz_seeds failed (${rc}):\n${out}")
endif()

file(REMOVE_RECURSE "${outroot}" "${seedroot}")
# afl-fuzz creates the last directory of -o and no more.
file(MAKE_DIRECTORY "${outroot}")

# The harnesses are the fuzz_* programs; the fuzz_seeds writer and the
# fuzz_chain_seed<n> files its harness starts from are not.
file(GLOB candidates "${fuzzdir}/fuzz_*")
set(targets)
foreach(c ${candidates})
  get_filename_component(name "${c}" NAME)
  if(IS_DIRECTORY "${c}" OR name MATCHES "_seed")
    continue()
  endif()
  list(APPEND targets "${name}")
endforeach()
list(SORT targets)
if(NOT targets)
  message(FATAL_ERROR "afl.cmake: no fuzz_<reader> in ${fuzzdir}")
endif()

foreach(name ${targets})
  string(REGEX REPLACE "^fuzz_" "" reader "${name}")
  set(seeds "${seedroot}/${reader}")
  file(MAKE_DIRECTORY "${seeds}")
  if(reader STREQUAL "chain")
    file(GLOB found "${fuzzdir}/fuzz_chain_seed*")
  else()
    file(GLOB found "${fuzzdir}/seed_${reader}"
         "${fuzzdir}/seed_${reader}_utf8")
  endif()
  foreach(f ${found})
    file(COPY "${f}" DESTINATION "${seeds}")
  endforeach()
  # AFL++ refuses an empty seed directory; a reader with none is a fault
  # of the seeds, not a reason to skip the reader.
  if(NOT found)
    message(FATAL_ERROR "afl.cmake: no seed for ${reader} in ${fuzzdir}")
  endif()

  set(out "${outroot}/${reader}")
  message("afl-fuzz ${reader} for ${SECONDS} s")
  # A runner has no core_pattern to set and no frequency governor to
  # change. The sanitisers' options are the ones afl-fuzz insists on
  # (a finding must abort, to be a crash) and the harness's own
  # allocator limits stay as libFuzzer's would have them.
  execute_process(
    COMMAND
      "${CMAKE_COMMAND}" -E env AFL_NO_UI=1 AFL_SKIP_CPUFREQ=1
      AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1 AFL_FORKSRV_INIT_TMOUT=10000
      "ASAN_OPTIONS=${asan_options}"
      "UBSAN_OPTIONS=halt_on_error=1:abort_on_error=1:symbolize=0"
      "${AFL_FUZZ_EXE}" -V ${SECONDS} -m none -i "${seeds}" -o "${out}" --
      "${fuzzdir}/${name}" @@
    WORKING_DIRECTORY "${fuzzdir}"
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE log
    ERROR_VARIABLE log)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "afl-fuzz ${reader} exited ${rc}:\n${log}")
  endif()

  file(GLOB crashes "${out}/default/crashes/id:*")
  if(crashes)
    # AFL names a finding id:NNNNNN,sig:...; an artifact may not hold ':'
    # or ',', so CI uploads a copy under a plain name.
    foreach(c ${crashes})
      get_filename_component(base "${c}" NAME)
      message("  crash: ${base}")
      string(REGEX REPLACE "[:,]" "_" plain "${base}")
      file(MAKE_DIRECTORY "${BUILD}/afl-findings/${reader}")
      configure_file("${c}" "${BUILD}/afl-findings/${reader}/${plain}" COPYONLY)
    endforeach()
    message(FATAL_ERROR "afl-fuzz found a crash in ${reader}; the inputs are "
                        "in ${out}/default/crashes")
  endif()
  if(EXISTS "${out}/default/fuzzer_stats")
    file(STRINGS "${out}/default/fuzzer_stats" stats
         REGEX "^(execs_done|corpus_count|edges_found)")
    string(REPLACE ";" " " stats "${stats}")
    string(REGEX REPLACE " +" " " stats "${stats}")
    message("  ${stats}")
  endif()
endforeach()
