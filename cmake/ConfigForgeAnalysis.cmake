# Static and dynamic analysis.
#
# Off by default because both are slow, on in CI because both find
# things that reading does not.

option(CF_ANALYZER "GCC -fanalyzer symbolic execution" OFF)
option(CF_SANITIZE "Address and undefined behaviour" OFF)
# Data races. The threading contract (D-14) says a configuration is read
# from any number of threads without a lock; this is the check that the
# claim holds under a tool that watches every access, not only under a
# test that did not happen to race.
option(CF_SANITIZE_THREAD "Thread sanitiser" OFF)
# Line coverage, read by gcovr in `verify.cmake -DEXTRA=coverage`.
option(CF_COVERAGE "Coverage instrumentation (gcov)" OFF)
# Coverage-guided fuzzing: every target instrumented for libFuzzer and
# built with the address and undefined-behaviour sanitisers, and the
# harnesses in tests/fuzz. Clang only.
option(CF_FUZZ "libFuzzer harnesses (clang)" OFF)
set(CF_FUZZ_SANITIZE
    "address,undefined"
    CACHE STRING "The sanitisers CF_FUZZ adds; empty for the flags' own")
if(CF_FUZZ AND NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
  message(FATAL_ERROR "CF_FUZZ needs clang: libFuzzer is part of it")
endif()
if(CF_FUZZ AND CF_SANITIZE_THREAD)
  message(FATAL_ERROR "CF_FUZZ uses the address sanitiser, which the thread "
                      "sanitiser excludes")
endif()
if(CF_SANITIZE AND CF_SANITIZE_THREAD)
  message(FATAL_ERROR "CF_SANITIZE and CF_SANITIZE_THREAD cannot be combined: "
                      "the address and thread sanitisers exclude each other")
endif()
# Either sanitiser instruments every frame and every access, so the
# gates that compare against an uninstrumented build, and the stack
# budget, step aside for both.
if(CF_SANITIZE
   OR CF_SANITIZE_THREAD
   OR CF_COVERAGE
   OR CF_FUZZ)
  set(CF_INSTRUMENTED ON)
else()
  set(CF_INSTRUMENTED OFF)
endif()
# MSVC links AddressSanitizer's runtime as a DLL that sits beside cl.exe,
# on no PATH outside a developer prompt. A program started without it
# does not fail: Windows stops it with a dialog and waits for someone to
# press OK, once per program, so a test run hangs on a desktop and times
# out on a server. Each program gets a copy beside it instead.
set(CF_ASAN_RUNTIME "")
if(CF_SANITIZE AND CMAKE_C_COMPILER_ID STREQUAL "MSVC")
  get_filename_component(CF_CL_DIR "${CMAKE_C_COMPILER}" DIRECTORY)
  file(GLOB CF_ASAN_RUNTIME "${CF_CL_DIR}/clang_rt.asan*_dynamic-*.dll")
  if(NOT CF_ASAN_RUNTIME)
    message(
      FATAL_ERROR
        "CF_SANITIZE: no clang_rt.asan_dynamic-*.dll beside "
        "${CMAKE_C_COMPILER}; install Visual Studio's "
        "C++ AddressSanitizer component")
  endif()
endif()
option(CF_CPPCHECK "cppcheck, when present" OFF)
option(CF_CLANG_TIDY "clang-tidy, when present" OFF)

# Apply the optional analysers to a target: -fanalyzer and sanitisers.
#
# ~~~
# configforge_apply_analysis(<target>)
# ~~~
function(configforge_apply_analysis tgt)
  if(CF_ANALYZER AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
    # Symbolic execution over the whole function: finds leaks, double
    # frees, NULL dereferences and use-after-free that no warning based
    # on syntax can see. It found a real double lookup in Doctor.
    #
    # C only. GCC documents the analyzer as supported for C; on C++ it
    # reports uninitialised values inside ordinary member functions
    # that are plainly initialised. Suppressing those findings would
    # teach us to ignore the tool, so the tool is simply not pointed at
    # a language it does not claim to handle.
    target_compile_options(${tgt} PRIVATE $<$<COMPILE_LANGUAGE:C>:-fanalyzer>)
  elseif(CF_ANALYZER AND MSVC)
    # MSVC's own analyzer: the C6000 warnings - a NULL dereference, a
    # buffer overrun, an unchecked result - from a different engine than
    # GCC's, so it finds different things. Errors under /WX like any
    # warning. C only, as above, and every target, the tests included: a
    # test that went on to use what a failed check found NULL crashed
    # instead of reporting, which the analyzer found in nine of them.
    target_compile_options(${tgt} PRIVATE $<$<COMPILE_LANGUAGE:C>:/analyze>)
  endif()

  if(CF_SANITIZE)
    target_compile_options(
      ${tgt} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer
                     -fno-sanitize-recover=all -g)
    target_link_options(${tgt} PRIVATE -fsanitize=address,undefined)
    get_target_property(kind ${tgt} TYPE)
    if(CF_ASAN_RUNTIME AND kind STREQUAL "EXECUTABLE")
      add_custom_command(
        TARGET ${tgt}
        POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${CF_ASAN_RUNTIME}
                "$<TARGET_FILE_DIR:${tgt}>")
    endif()
  endif()
  if(CF_FUZZ)
    target_compile_options(${tgt} PRIVATE -fsanitize=fuzzer-no-link
                                          -fno-omit-frame-pointer -g)
    # Empty when the environment's flags choose the sanitisers, as
    # ClusterFuzzLite's do: one sanitiser a build, and a build with
    # another added is refused as a bad build.
    if(CF_FUZZ_SANITIZE)
      target_compile_options(${tgt} PRIVATE -fsanitize=${CF_FUZZ_SANITIZE})
      target_link_options(${tgt} PRIVATE -fsanitize=${CF_FUZZ_SANITIZE})
    endif()
  endif()
  if(CF_COVERAGE)
    target_compile_options(${tgt} PRIVATE --coverage)
    target_link_options(${tgt} PRIVATE --coverage)
  endif()
  if(CF_SANITIZE_THREAD)
    target_compile_options(${tgt} PRIVATE -fsanitize=thread
                                          -fno-omit-frame-pointer -g)
    target_link_options(${tgt} PRIVATE -fsanitize=thread)
  endif()
endfunction()

# Route compilation through cppcheck when it is present and requested.
#
# ~~~
# configforge_setup_cppcheck()
# ~~~
function(configforge_setup_cppcheck)
  if(NOT CF_CPPCHECK)
    return()
  endif()
  find_program(CPPCHECK_EXE cppcheck)
  if(NOT CPPCHECK_EXE)
    message(STATUS "cppcheck requested but not found; skipping")
    return()
  endif()
  # cppcheck defines none of the compiler's own macros, so cf_port.h saw
  # a C89 compiler that was neither GCC nor MSVC and stopped at its
  # #error for want of a 64-bit type - before analysing a line. Every
  # build of this preset failed that way and the analysis never ran.
  # Saying which compiler this is gives it the configuration the
  # compiler actually compiles; Clang defines __GNUC__ as well.
  # The version too: with a bare -D__GNUC__ (that is, 1) cppcheck took
  # the path for a compiler with no atomic builtins, analysed code no
  # build compiles, and stopped there once cf_port.h refused it. GCC is
  # its own version; Clang says 4.2.
  set(compiler)
  if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    string(REGEX MATCH "^([0-9]+)\\.([0-9]+)" _ "${CMAKE_C_COMPILER_VERSION}")
    set(compiler "-D__GNUC__=${CMAKE_MATCH_1}"
                 "-D__GNUC_MINOR__=${CMAKE_MATCH_2}")
  elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
    set(compiler "-D__GNUC__=4" "-D__GNUC_MINOR__=2")
  endif()
  set(common
      "${CPPCHECK_EXE}"
      "--std=c89"
      ${compiler}
      "--inline-suppr"
      "--error-exitcode=1"
      "--suppress=missingIncludeSystem"
      "--suppress=unusedFunction")
  set(CMAKE_C_CPPCHECK
      ${common} "--enable=warning,style,performance,portability"
      PARENT_SCOPE)
  # Tests without the style class. A test asserting that
  # cf_to_upper('i') is 'I' is asserting a constant on purpose, and
  # "condition is always true" is that class's name for the point of the
  # test. Warnings still apply: a test that reads memory it does not own
  # is broken, whatever it is for.
  set(CF_CPPCHECK_TESTS
      ${common} "--enable=warning,performance,portability"
      PARENT_SCOPE)
endfunction()
