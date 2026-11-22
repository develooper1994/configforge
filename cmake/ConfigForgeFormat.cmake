# Formatting and static checking as build targets, so they are one
# command rather than a paragraph in a contributing guide nobody reads.

include(${CMAKE_CURRENT_LIST_DIR}/ConfigForgeClangFormat.cmake)
configforge_find_clang_format(CLANG_FORMAT_EXE)
find_program(CLANG_TIDY_EXE NAMES clang-tidy)
find_program(CMAKE_FORMAT_EXE NAMES cmake-format)
find_program(CMAKE_LINT_EXE NAMES cmake-lint)

file(
  GLOB_RECURSE
  CF_C_SOURCES
  "${CMAKE_SOURCE_DIR}/lib/*.c"
  "${CMAKE_SOURCE_DIR}/lib/*.h"
  "${CMAKE_SOURCE_DIR}/tools/doctor/include/configforge/*.h"
  "${CMAKE_SOURCE_DIR}/lib/*.hpp"
  "${CMAKE_SOURCE_DIR}/tests/*.c"
  "${CMAKE_SOURCE_DIR}/tests/*.cpp"
  "${CMAKE_SOURCE_DIR}/tools/*.c"
  "${CMAKE_SOURCE_DIR}/examples/*.c"
  "${CMAKE_SOURCE_DIR}/bench/*.c"
  "${CMAKE_SOURCE_DIR}/bench/compare/*.cpp"
  "${CMAKE_SOURCE_DIR}/bench/compare/*.h"
  "${CMAKE_SOURCE_DIR}/bench/compare/adapters/*.cpp"
  "${CMAKE_SOURCE_DIR}/bench/compare/size/*.cpp"
  "${CMAKE_SOURCE_DIR}/examples/*.cpp")

# The build files are source code too, and they had drifted into three
# styles before anything checked them.
file(
  GLOB_RECURSE
  CF_CMAKE_SOURCES
  "${CMAKE_SOURCE_DIR}/CMakeLists.txt"
  "${CMAKE_SOURCE_DIR}/cmake/*.cmake"
  "${CMAKE_SOURCE_DIR}/cmake/gates/*.cmake"
  "${CMAKE_SOURCE_DIR}/scripts/*.cmake"
  "${CMAKE_SOURCE_DIR}/lib/CMakeLists.txt"
  "${CMAKE_SOURCE_DIR}/lib/*/CMakeLists.txt"
  "${CMAKE_SOURCE_DIR}/lib/*.cmake"
  "${CMAKE_SOURCE_DIR}/tests/CMakeLists.txt"
  "${CMAKE_SOURCE_DIR}/tests/*/CMakeLists.txt"
  "${CMAKE_SOURCE_DIR}/tools/CMakeLists.txt"
  "${CMAKE_SOURCE_DIR}/tools/*/CMakeLists.txt"
  "${CMAKE_SOURCE_DIR}/examples/CMakeLists.txt"
  "${CMAKE_SOURCE_DIR}/examples/*/CMakeLists.txt"
  "${CMAKE_SOURCE_DIR}/bench/CMakeLists.txt")

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

# The other languages' checkers. Each is a gate test like the ones above,
# skipped with a message when its tool is absent, so a machine without it
# still builds and the CI job that has it still fails.
find_program(SHELLCHECK_EXE NAMES shellcheck)
find_program(RUFF_EXE NAMES ruff)
find_program(ACTIONLINT_EXE NAMES actionlint)
find_program(CODESPELL_EXE NAMES codespell)
find_program(LYCHEE_EXE NAMES lychee)

file(
  GLOB_RECURSE
  CF_SH_SOURCES
  "${CMAKE_SOURCE_DIR}/.clusterfuzzlite/*.sh"
  "${CMAKE_SOURCE_DIR}/actions/*.sh"
  "${CMAKE_SOURCE_DIR}/bindings/*.sh"
  "${CMAKE_SOURCE_DIR}/examples/*.sh"
  "${CMAKE_SOURCE_DIR}/packaging/*.sh"
  "${CMAKE_SOURCE_DIR}/scripts/*.sh"
  "${CMAKE_SOURCE_DIR}/tests/*.sh"
  "${CMAKE_SOURCE_DIR}/tools/*.sh")
file(GLOB CF_WORKFLOWS "${CMAKE_SOURCE_DIR}/.github/workflows/*.yml")

if(SHELLCHECK_EXE)
  add_test(NAME gate.shellcheck COMMAND ${SHELLCHECK_EXE} ${CF_SH_SOURCES})
else()
  message(STATUS "shellcheck not found: gate.shellcheck skipped")
endif()
if(RUFF_EXE)
  add_test(
    NAME gate.ruff
    COMMAND ${RUFF_EXE} check --no-cache bindings conanfile.py examples scripts
            tests
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
else()
  message(STATUS "ruff not found: gate.ruff skipped")
endif()
if(ACTIONLINT_EXE)
  add_test(NAME gate.actionlint
           COMMAND ${ACTIONLINT_EXE} -config-file
                   ${CMAKE_SOURCE_DIR}/.github/actionlint.yaml ${CF_WORKFLOWS})
else()
  message(STATUS "actionlint not found: gate.actionlint skipped")
endif()
if(CODESPELL_EXE)
  add_test(
    NAME gate.codespell
    COMMAND ${CODESPELL_EXE} --skip=lib/*/tests docs lib tools README.md
            CHANGELOG.md CONTRIBUTING.md SECURITY.md CODE_OF_CONDUCT.md
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
else()
  message(STATUS "codespell not found: gate.codespell skipped")
endif()
if(LYCHEE_EXE)
  # Offline: every link inside the repository and every anchor. The links
  # to other sites are checked weekly by the docs workflow's job `links`.
  add_test(
    NAME gate.links
    COMMAND ${LYCHEE_EXE} --offline --no-progress docs README.md CONTRIBUTING.md
            CHANGELOG.md scripts/README.md
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
else()
  message(STATUS "lychee not found: gate.links skipped")
endif()
