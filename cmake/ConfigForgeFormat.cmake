# Formatting and tidy as build targets, so they are one command rather
# than a paragraph in a contributing guide nobody reads.

find_program(CLANG_FORMAT_EXE NAMES clang-format)
find_program(CLANG_TIDY_EXE   NAMES clang-tidy)

file(GLOB_RECURSE CF_ALL_SOURCES
     "${CMAKE_SOURCE_DIR}/src/*.c"
     "${CMAKE_SOURCE_DIR}/include/configforge/*.h"
     "${CMAKE_SOURCE_DIR}/tests/*.c"
     "${CMAKE_SOURCE_DIR}/tools/*.c"
     "${CMAKE_SOURCE_DIR}/examples/*.c")

if(CLANG_FORMAT_EXE)
  add_custom_target(format
    COMMAND ${CLANG_FORMAT_EXE} -i ${CF_ALL_SOURCES}
    COMMENT "clang-format -i")

  # The check target is what CI runs: it reports rather than rewrites,
  # because a CI job that edits the tree is a CI job that hides a
  # disagreement instead of surfacing it.
  add_test(NAME gate.format
           COMMAND ${CLANG_FORMAT_EXE} --dry-run -Werror ${CF_ALL_SOURCES})
endif()

if(CLANG_TIDY_EXE AND CF_CLANG_TIDY)
  # Through CMAKE_C_CLANG_TIDY so every translation unit is checked with
  # the flags it is actually compiled with, rather than a guess at them.
  #
  # No PARENT_SCOPE: include() shares the caller's scope, so the first
  # version set the variable one directory up, where nothing read it,
  # and clang-tidy reported a clean run by never running.
  #
  # clang-tidy reads the compile command recorded for the real compiler,
  # so on a GCC build it meets GCC-only warning flags and reports each
  # one as an error. Silencing that single diagnostic is not hiding a
  # problem: the flags are correct for the compiler that will use them,
  # and a different front end not recognising them says nothing about
  # the code.
  set(CMAKE_C_CLANG_TIDY "${CLANG_TIDY_EXE}"
      "--extra-arg=-Wno-unknown-warning-option")
  message(STATUS "clang-tidy: ${CLANG_TIDY_EXE}")
endif()
