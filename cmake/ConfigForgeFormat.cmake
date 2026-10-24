# Formatting and static checking as build targets, so they are one
# command rather than a paragraph in a contributing guide nobody reads.

find_program(CLANG_FORMAT_EXE NAMES clang-format)
find_program(CLANG_TIDY_EXE NAMES clang-tidy)
find_program(CMAKE_FORMAT_EXE NAMES cmake-format)
find_program(CMAKE_LINT_EXE NAMES cmake-lint)

file(
  GLOB_RECURSE
  CF_C_SOURCES
  "${CMAKE_SOURCE_DIR}/src/*.c"
  "${CMAKE_SOURCE_DIR}/include/configforge/*.h"
  "${CMAKE_SOURCE_DIR}/tests/*.c"
  "${CMAKE_SOURCE_DIR}/tools/*.c"
  "${CMAKE_SOURCE_DIR}/examples/*.c")

# The build files are source code too, and they had drifted into three
# styles before anything checked them.
file(
  GLOB_RECURSE
  CF_CMAKE_SOURCES
  "${CMAKE_SOURCE_DIR}/CMakeLists.txt"
  "${CMAKE_SOURCE_DIR}/cmake/*.cmake"
  "${CMAKE_SOURCE_DIR}/cmake/gates/*.cmake"
  "${CMAKE_SOURCE_DIR}/scripts/*.cmake"
  "${CMAKE_SOURCE_DIR}/src/CMakeLists.txt"
  "${CMAKE_SOURCE_DIR}/src/*/CMakeLists.txt"
  "${CMAKE_SOURCE_DIR}/tests/CMakeLists.txt"
  "${CMAKE_SOURCE_DIR}/tests/*/CMakeLists.txt"
  "${CMAKE_SOURCE_DIR}/tools/CMakeLists.txt"
  "${CMAKE_SOURCE_DIR}/tools/*/CMakeLists.txt"
  "${CMAKE_SOURCE_DIR}/examples/CMakeLists.txt"
  "${CMAKE_SOURCE_DIR}/examples/*/CMakeLists.txt")

set(CF_FORMAT_COMMANDS)
if(CLANG_FORMAT_EXE)
  list(
    APPEND
    CF_FORMAT_COMMANDS
    COMMAND
    ${CLANG_FORMAT_EXE}
    -i
    ${CF_C_SOURCES})
endif()
if(CMAKE_FORMAT_EXE)
  list(
    APPEND
    CF_FORMAT_COMMANDS
    COMMAND
    ${CMAKE_FORMAT_EXE}
    -i
    ${CF_CMAKE_SOURCES})
endif()
if(CF_FORMAT_COMMANDS)
  add_custom_target(
    format
    ${CF_FORMAT_COMMANDS}
    COMMENT "formatting C and CMake sources")
endif()

# The checks report rather than rewrite. A CI job that edits the tree
# hides a disagreement instead of surfacing it.
if(CLANG_FORMAT_EXE)
  add_test(NAME gate.format.c COMMAND ${CLANG_FORMAT_EXE} --dry-run -Werror
                                      ${CF_C_SOURCES})
endif()
if(CMAKE_FORMAT_EXE)
  add_test(NAME gate.format.cmake COMMAND ${CMAKE_FORMAT_EXE} --check
                                          ${CF_CMAKE_SOURCES})
endif()
if(CMAKE_LINT_EXE)
  add_test(NAME gate.cmake.lint COMMAND ${CMAKE_LINT_EXE} ${CF_CMAKE_SOURCES})
endif()

if(CLANG_TIDY_EXE AND CF_CLANG_TIDY)
  # Through CMAKE_C_CLANG_TIDY so every translation unit is checked with
  # the flags it is actually compiled with, rather than a guess at them.
  #
  # No PARENT_SCOPE: include() shares the caller's scope, so the first
  # version set the variable one directory up, where nothing read it,
  # and clang-tidy reported a clean run by never running.
  #
  # The extra argument is there because clang-tidy reads the compile
  # command recorded for the real compiler, so on a GCC build it meets
  # GCC-only warning flags and calls each one an error. The flags are
  # correct for the compiler that will use them.
  set(CMAKE_C_CLANG_TIDY "${CLANG_TIDY_EXE}"
                         "--extra-arg=-Wno-unknown-warning-option")
  message(STATUS "clang-tidy: ${CLANG_TIDY_EXE}")
endif()
