# Format and lint what changed, the way the gates will check it.
#
#   cmake -P scripts/format.cmake               files changed since HEAD
#   cmake -DALL=ON -P scripts/format.cmake      every source the gates cover
#   cmake -DCHECK=ON -P scripts/format.cmake    report only; change nothing
#
# C goes through clang-format, CMake through cmake-format and then
# cmake-lint, each only where it is installed. Prints what needs
# attention and exits non-zero when anything does, so it can be chained
# in front of `cmake -P scripts/verify.cmake`.
#
# It touches exactly the files the gates check (CF_C_SOURCES in
# cmake/ConfigForgeFormat.cmake) and nothing else. The C++ wrapper and
# the private headers are laid out by hand; formatting them would undo
# that, and no gate would notice.
#
# One thing a formatter cannot settle on its own: CI installs
# clang-format 18, and versions disagree about a handful of layouts.
# This script uses whichever is installed. Where the two disagreed the
# code was rewritten so that neither touches it (see cf_error.h and
# src/bridge/dir.c), so a file this formats cleanly with any recent
# version is a file the CI gate accepts - and gate.format.c is the check
# that says so if not.
#
# Written in CMake for the reason verify.cmake is: it runs wherever the
# build does, with nothing else installed.

cmake_minimum_required(VERSION 3.10)

get_filename_component(REPO "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

find_program(GIT_EXE git)
find_program(CLANG_FORMAT_EXE clang-format)
find_program(CMAKE_FORMAT_EXE cmake-format)
find_program(CMAKE_LINT_EXE cmake-lint)

# The files: changed and untracked, or everything the gates read.
set(files "")
if(ALL)
  file(
    GLOB_RECURSE files
    RELATIVE "${REPO}"
    "${REPO}/src/*.c"
    "${REPO}/include/configforge/*.h"
    "${REPO}/tests/*.c"
    "${REPO}/tools/*.c"
    "${REPO}/examples/*.c"
    "${REPO}/CMakeLists.txt"
    "${REPO}/cmake/*.cmake"
    "${REPO}/scripts/*.cmake"
    "${REPO}/src/*CMakeLists.txt"
    "${REPO}/tests/*CMakeLists.txt"
    "${REPO}/tests/*.cmake"
    "${REPO}/tools/*CMakeLists.txt"
    "${REPO}/examples/*CMakeLists.txt")
else()
  if(NOT GIT_EXE)
    message(FATAL_ERROR "without git there is no 'changed'; use -DALL=ON")
  endif()
  execute_process(
    COMMAND "${GIT_EXE}" diff --name-only HEAD
    WORKING_DIRECTORY "${REPO}"
    OUTPUT_VARIABLE changed)
  execute_process(
    COMMAND "${GIT_EXE}" ls-files --others --exclude-standard
    WORKING_DIRECTORY "${REPO}"
    OUTPUT_VARIABLE untracked)
  string(REPLACE "\n" ";" files "${changed}${untracked}")
endif()

set(c_files "")
set(cmake_files "")
foreach(f ${files})
  if(NOT EXISTS "${REPO}/${f}" OR f MATCHES "^build")
    continue()
  endif()
  if(f MATCHES "^(src|tests|tools|examples)/.*\\.c$"
     OR f MATCHES "^include/configforge/.*\\.h$")
    list(APPEND c_files "${f}")
  elseif(f MATCHES "(^|/)CMakeLists\\.txt$" OR f MATCHES "\\.cmake$")
    list(APPEND cmake_files "${f}")
  endif()
endforeach()
list(REMOVE_DUPLICATES c_files)
list(REMOVE_DUPLICATES cmake_files)

set(bad 0)

if(c_files)
  if(NOT CLANG_FORMAT_EXE)
    message("clang-format not found: C files left as they are")
  elseif(CHECK)
    execute_process(
      COMMAND "${CLANG_FORMAT_EXE}" --dry-run -Werror ${c_files}
      WORKING_DIRECTORY "${REPO}"
      RESULT_VARIABLE rc
      ERROR_VARIABLE out)
    if(NOT rc EQUAL 0)
      message("clang-format would change:\n${out}")
      set(bad 1)
    endif()
  else()
    execute_process(COMMAND "${CLANG_FORMAT_EXE}" -i ${c_files}
                    WORKING_DIRECTORY "${REPO}")
  endif()
endif()

if(cmake_files)
  if(NOT CMAKE_FORMAT_EXE)
    message("cmake-format not found: CMake files left as they are")
  else()
    if(CHECK)
      set(mode --check)
    else()
      set(mode -i)
    endif()
    execute_process(
      COMMAND "${CMAKE_FORMAT_EXE}" ${mode} ${cmake_files}
      WORKING_DIRECTORY "${REPO}"
      RESULT_VARIABLE rc
      OUTPUT_VARIABLE out
      ERROR_VARIABLE out)
    if(NOT rc EQUAL 0)
      message("cmake-format:\n${out}")
      set(bad 1)
    endif()
  endif()
  if(CMAKE_LINT_EXE)
    execute_process(
      COMMAND "${CMAKE_LINT_EXE}" ${cmake_files}
      WORKING_DIRECTORY "${REPO}"
      RESULT_VARIABLE rc
      OUTPUT_VARIABLE out
      ERROR_VARIABLE out)
    if(NOT rc EQUAL 0)
      message("cmake-lint:\n${out}")
      set(bad 1)
    endif()
  endif()
endif()

list(LENGTH c_files nc)
list(LENGTH cmake_files ncm)
if(bad)
  message(FATAL_ERROR "format: needs attention (${nc} C, ${ncm} CMake)")
endif()
message("format: clean (${nc} C, ${ncm} CMake)")
