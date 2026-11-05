# The one clang-format the formatting gate accepts.
#
# Versions disagree about layouts no code can satisfy both ways - 18 and
# 22 space a pointer to member differently, `M S::*m` against `M S::* m`
# - so a file clean under one fails under the other, and the gate failed
# on one system only. One major version is the answer: setup.sh installs
# it from PyPI (pinned), Windows has it from LLVM, CI pip-installs it. A
# clang-format of another major is not used at all, by the gate or by
# scripts/format.cmake, and says so, rather than reformatting the tree
# into the other version's layout.
#
#   configforge_find_clang_format(<var>)
#
# sets <var> to the program, or to nothing with a STATUS message.

set(CF_CLANG_FORMAT_MAJOR 22)

# Sets <out> to clang-format of CF_CLANG_FORMAT_MAJOR, or to nothing.
function(configforge_find_clang_format out)
  # The names literally, for gate.dependencies, which reads them.
  find_program(CF_CLANG_FORMAT_PROGRAM NAMES clang-format-22 clang-format)
  set(program "${CF_CLANG_FORMAT_PROGRAM}")
  if(NOT program)
    set(${out}
        ""
        PARENT_SCOPE)
    return()
  endif()
  execute_process(
    COMMAND ${program} --version
    OUTPUT_VARIABLE version_text
    ERROR_QUIET)
  string(REGEX MATCH "version ([0-9]+)" _ "${version_text}")
  if(NOT CMAKE_MATCH_1 STREQUAL "${CF_CLANG_FORMAT_MAJOR}")
    message(
      STATUS "clang-format: ${program} is version "
             "'${CMAKE_MATCH_1}', not ${CF_CLANG_FORMAT_MAJOR}; formatting is "
             "not checked (scripts/buildenv/setup.sh installs it)")
    set(${out}
        ""
        PARENT_SCOPE)
    return()
  endif()
  set(${out}
      "${program}"
      PARENT_SCOPE)
endfunction()
