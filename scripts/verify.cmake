# Everything that has to be true before a commit.
#
#   cmake -P scripts/verify.cmake                     every preset
#   cmake -DMODE=quick -P scripts/verify.cmake        the default preset only
#   cmake -DSOURCE=index -P scripts/verify.cmake      what is staged, alone
#   cmake -DEXTRA="kqueue;tidy;clang" -P ...          and the extra builds
#   cmake -DJOBS=2 -P scripts/verify.cmake            at most two jobs at once
#   cmake -DSOURCE=index -DWORK=$HOME/cfv -P ...      the staged copy elsewhere
#
# Written in CMake rather than in a shell, because a shell script covers
# half the platforms this library claims and the other half would need a
# batch file kept in step with it - which is the duplication the
# hand-written Makefile was deleted to avoid (D-31). CMake is already
# required to build, so this driver adds nothing at all.
#
# It exists because chaining the steps by hand got a failing tree
# committed once: `ctest | grep passed` matched the word inside "98%
# tests passed". A checkpoint of a broken tree is worse than none, so
# this stops at the first failure and prints that step's output.
#
# SOURCE=index verifies the staged tree in a copy of its own, under
# build-verify/, which is what a commit will contain - not the working
# tree, which may hold the next change half written. The copy is removed
# when everything passes and kept when something fails, so the failure
# can be looked at where it happened.
#
# EXTRA steps, each skipped with a note where its tool is missing:
#   kqueue   the macOS and BSD wait handle on Linux, through libkqueue
#   tidy     clang-tidy over the library and Doctor (CF_CLANG_TIDY=ON)
#   clang    the whole suite built with clang
#   shared   the whole suite against a shared library (BUILD_SHARED_LIBS),
#            CI's shared job; the Python binding's tests run only here,
#            since ctypes loads a shared library
#   tsan     the whole suite under the thread sanitiser (the tsan preset)
#   yaml     the whole suite with CF_WITH_YAML, where libyaml is installed
#   compilers the whole suite against musl (musl-gcc) and with the Tiny C
#            Compiler and the Portable C Compiler, each where installed:
#            the claim that the library needs a C89 compiler and a C
#            library, and nothing about which
#   fuzz     libFuzzer on every reader for FUZZ_SECONDS each (default 10),
#            clang only
#   coverage line coverage of src/ by the suite, failing below COVERAGE_MIN
#            percent of lines (default 89) or COVERAGE_BRANCH_MIN of
#            branches (default 80): ratchets, raised as tests are added
#
# WORK puts the staged copy somewhere other than build-verify/. Under
# WSL, verifying a checkout on /mnt/c needs it: that filesystem has no
# inotify and no Unix permissions, so the watcher test and Doctor's
# file-permission rule fail there for reasons that are not the code's.
#
# JOBS bounds the parallelism of every build. A full run builds five
# presets, and unbounded parallel builds have filled a workstation's
# memory doing it.

cmake_minimum_required(VERSION 3.10)

get_filename_component(REPO "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

if(NOT DEFINED MODE)
  set(MODE "full")
endif()
if(NOT DEFINED SOURCE)
  set(SOURCE "tree")
endif()
if(NOT DEFINED EXTRA)
  set(EXTRA "")
endif()

if(MODE STREQUAL "quick")
  set(PRESETS default)
else()
  set(PRESETS
      default
      sanitize
      portable
      minimal
      analyze)
endif()

set(parallel "")
if(DEFINED JOBS)
  set(parallel --parallel ${JOBS})
endif()

# Run one step. Anything other than a zero exit stops the whole run and
# shows the output, because a verification that continues past a failure
# is a verification that reports the last thing instead of the first
# broken thing.
function(step label)
  # Padded so the column of results lines up: the output is what a
  # person scans to find the first failure.
  set(padded "${label}")
  string(LENGTH "${padded}" len)
  while(len LESS 22)
    string(APPEND padded " ")
    math(EXPR len "${len} + 1")
  endwhile()

  execute_process(
    COMMAND ${ARGN}
    WORKING_DIRECTORY "${ROOT}"
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE out)
  if(rc EQUAL 0)
    message("${padded}ok")
  else()
    message("${padded}FAILED")
    message("${out}")
    if(SOURCE STREQUAL "index")
      message("the staged copy is kept at ${ROOT}")
    endif()
    message(FATAL_ERROR "verification stopped at: ${label}")
  endif()
endfunction()

# Where the verification runs: the working tree, or a fresh copy of the
# staged tree. git archive of `git write-tree` is exactly what a commit
# would record, untracked and unstaged files excluded.
if(SOURCE STREQUAL "index")
  find_program(GIT_EXE git)
  if(NOT GIT_EXE)
    message(FATAL_ERROR "SOURCE=index needs git")
  endif()
  if(DEFINED WORK)
    get_filename_component(ROOT "${WORK}/staged" ABSOLUTE)
  else()
    set(ROOT "${REPO}/build-verify/staged")
  endif()
  file(REMOVE_RECURSE "${ROOT}")
  file(MAKE_DIRECTORY "${ROOT}")
  execute_process(
    COMMAND "${GIT_EXE}" write-tree
    WORKING_DIRECTORY "${REPO}"
    OUTPUT_VARIABLE tree
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "git write-tree failed: is this a repository?")
  endif()
  execute_process(
    COMMAND "${GIT_EXE}" archive --format=tar -o "${ROOT}/../staged.tar" ${tree}
    WORKING_DIRECTORY "${REPO}"
    RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "git archive failed")
  endif()
  execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf "${ROOT}/../staged.tar"
                  WORKING_DIRECTORY "${ROOT}")
  file(REMOVE "${ROOT}/../staged.tar")
  message("verifying the staged tree ${tree}")
else()
  set(ROOT "${REPO}")
endif()

foreach(p ${PRESETS})
  step("${p} configure" ${CMAKE_COMMAND} --preset ${p})
  step(
    "${p} build"
    ${CMAKE_COMMAND}
    --build
    --preset
    ${p}
    ${parallel})
  step("${p} test" ${CMAKE_CTEST_COMMAND} --preset ${p})
endforeach()

# Build and test commands for the extra steps. Debug is named because a
# multi-config generator, Visual Studio among them, builds nothing and
# tests nothing without it.
set(build ${CMAKE_COMMAND} --build)
set(test ${CMAKE_CTEST_COMMAND} -C Debug --test-dir)
set(debug --config Debug ${parallel})

foreach(x ${EXTRA})
  if(x STREQUAL "kqueue")
    find_program(PKG_CONFIG_EXE pkg-config)
    set(have 0)
    if(PKG_CONFIG_EXE)
      execute_process(COMMAND "${PKG_CONFIG_EXE}" --exists libkqueue
                      RESULT_VARIABLE have_rc)
      if(have_rc EQUAL 0)
        set(have 1)
      endif()
    endif()
    if(NOT have)
      message("kqueue                skipped: no libkqueue")
      continue()
    endif()
    step(
      "kqueue configure"
      ${CMAKE_COMMAND}
      -S
      .
      -B
      build-kqueue
      -DCF_WAKE_KQUEUE=ON)
    step("kqueue build" ${build} build-kqueue ${debug})
    step(
      "kqueue test"
      ${test}
      build-kqueue
      -R
      "^unit\\.")
  elseif(x STREQUAL "tidy")
    find_program(CLANG_TIDY_EXE clang-tidy)
    if(NOT CLANG_TIDY_EXE)
      message("tidy                  skipped: no clang-tidy")
      continue()
    endif()
    step(
      "tidy configure"
      ${CMAKE_COMMAND}
      -S
      .
      -B
      build-tidy
      -DCF_CLANG_TIDY=ON)
    step("tidy build" ${build} build-tidy ${debug})
  elseif(x STREQUAL "clang")
    find_program(CLANG_EXE clang)
    find_program(CLANGXX_EXE clang++)
    if(NOT CLANG_EXE OR NOT CLANGXX_EXE)
      message("clang                 skipped: no clang")
      continue()
    endif()
    step(
      "clang configure"
      ${CMAKE_COMMAND}
      -S
      .
      -B
      build-clang
      -DCMAKE_C_COMPILER=${CLANG_EXE}
      -DCMAKE_CXX_COMPILER=${CLANGXX_EXE})
    step("clang build" ${build} build-clang ${debug})
    step("clang test" ${test} build-clang)
  elseif(x STREQUAL "shared")
    step(
      "shared configure"
      ${CMAKE_COMMAND}
      -S
      .
      -B
      build-shared
      -DBUILD_SHARED_LIBS=ON)
    step("shared build" ${build} build-shared ${debug})
    step("shared test" ${test} build-shared)
  elseif(x STREQUAL "compilers")
    # name:compiler:extra option. pcc has no atomics, so its build is the
    # single-threaded one the support matrix promises for such a
    # toolchain. C++ is off in all three: the wrapper's tests would link
    # a C++ runtime built for another C library.
    # pcc also warns at glibc's own headers (__cold__ in stdio.h), which
    # nothing can mark as system headers there, so its warnings are shown
    # and not fatal.
    foreach(entry musl:musl-gcc: tcc:tcc:
                  pcc:pcc:-DCF_SINGLE_THREADED=ON|-DCF_WERROR=OFF)
      string(REPLACE ":" ";" parts "${entry}")
      list(GET parts 0 name)
      list(GET parts 1 cc)
      list(LENGTH parts n)
      set(extra "")
      if(n GREATER 2)
        list(GET parts 2 extra)
        string(REPLACE "|" ";" extra "${extra}")
      endif()
      find_program(CC_${name} ${cc})
      if(NOT CC_${name})
        message("${name}                  skipped: no ${cc}")
        continue()
      endif()
      step(
        "${name} configure"
        ${CMAKE_COMMAND}
        -S
        .
        -B
        build-${name}
        -DCMAKE_C_COMPILER=${CC_${name}}
        -DCF_WITH_CXX=OFF
        ${extra})
      step("${name} build" ${build} build-${name} ${debug})
      step("${name} test" ${test} build-${name})
    endforeach()
  elseif(x STREQUAL "yaml")
    # Named, because a -P script has no platform search path: the
    # configure step below finds it properly; this only decides whether
    # to try.
    find_path(YAML_H yaml.h PATHS /usr/include /usr/local/include
                                  /opt/homebrew/include /opt/local/include)
    if(NOT YAML_H)
      message("yaml                  skipped: no libyaml")
      continue()
    endif()
    step(
      "yaml configure"
      ${CMAKE_COMMAND}
      -S
      .
      -B
      build-yaml
      -DCF_WITH_YAML=ON)
    step("yaml build" ${build} build-yaml ${debug})
    step("yaml test" ${test} build-yaml)
  elseif(x STREQUAL "fuzz")
    find_program(CLANG_EXE clang)
    if(NOT CLANG_EXE OR WIN32)
      message("fuzz                  skipped: no clang with libFuzzer")
      continue()
    endif()
    if(NOT DEFINED FUZZ_SECONDS)
      set(FUZZ_SECONDS 10)
    endif()
    step(
      "fuzz configure"
      ${CMAKE_COMMAND}
      --preset
      fuzz
      -DCF_FUZZ_SECONDS=${FUZZ_SECONDS})
    step(
      "fuzz build"
      ${CMAKE_COMMAND}
      --build
      --preset
      fuzz
      ${parallel})
    step("fuzz run" ${CMAKE_CTEST_COMMAND} --preset fuzz)
  elseif(x STREQUAL "coverage")
    find_program(GCOVR_EXE gcovr)
    if(NOT GCOVR_EXE OR MSVC)
      message("coverage              skipped: no gcovr")
      continue()
    endif()
    if(NOT DEFINED COVERAGE_MIN)
      set(COVERAGE_MIN 89)
    endif()
    if(NOT DEFINED COVERAGE_BRANCH_MIN)
      set(COVERAGE_BRANCH_MIN 80)
    endif()
    step("coverage configure" ${CMAKE_COMMAND} --preset coverage)
    step(
      "coverage build"
      ${CMAKE_COMMAND}
      --build
      --preset
      coverage
      ${parallel})
    step("coverage test" ${CMAKE_CTEST_COMMAND} --preset coverage)
    # gcov's branch counts can come out negative (GCC bug 68080), which
    # gcovr refuses by default; they are branches, and the gate is on
    # lines.
    step(
      "coverage >= ${COVERAGE_MIN}/${COVERAGE_BRANCH_MIN}%"
      ${GCOVR_EXE}
      -r
      .
      --filter
      src/
      --gcov-ignore-parse-errors=negative_hits.warn_once_per_file
      --fail-under-line
      ${COVERAGE_MIN}
      --fail-under-branch
      ${COVERAGE_BRANCH_MIN}
      --txt-summary
      build-coverage)
  elseif(x STREQUAL "tsan")
    if(WIN32)
      message("tsan                  skipped: no thread sanitiser here")
      continue()
    endif()
    step("tsan configure" ${CMAKE_COMMAND} --preset tsan)
    step(
      "tsan build"
      ${CMAKE_COMMAND}
      --build
      --preset
      tsan
      ${parallel})
    # The thread sanitiser maps its shadow memory at fixed addresses, and
    # a kernel that randomises mmap with more bits than the runtime
    # expects (Linux 6.x defaults to 32) puts a library in the way: the
    # tests then die with "unexpected memory mapping" before running a
    # line. With address randomisation off for the run, they do not.
    find_program(SETARCH_EXE setarch)
    if(SETARCH_EXE)
      execute_process(
        COMMAND uname -m
        OUTPUT_VARIABLE arch
        OUTPUT_STRIP_TRAILING_WHITESPACE)
      step(
        "tsan test"
        ${SETARCH_EXE}
        ${arch}
        -R
        ${CMAKE_CTEST_COMMAND}
        --preset
        tsan)
    else()
      step("tsan test" ${CMAKE_CTEST_COMMAND} --preset tsan)
    endif()
  else()
    message(FATAL_ERROR "unknown EXTRA step '${x}': kqueue, tidy, clang, "
                        "shared, tsan, yaml, compilers, fuzz or coverage")
  endif()
endforeach()

# Only what this script made: WORK may name a directory that holds
# other things too.
if(SOURCE STREQUAL "index")
  if(DEFINED WORK)
    file(REMOVE_RECURSE "${ROOT}")
  else()
    file(REMOVE_RECURSE "${REPO}/build-verify")
  endif()
endif()

# The formatter rewrites and the gate reports, so this only checks:
# `cmake -P scripts/format.cmake` is the fix. A verification that edited
# the tree would hide a disagreement instead of surfacing it.
message("")
message("all green")
