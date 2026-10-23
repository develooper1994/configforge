# Every package or program the build looks for is on the dependency page.
#
# docs/Dependencies.md is the bill of materials for a person. A page like
# that goes stale the first time somebody adds a find_program and does
# not think of it, so the build reads its own files for what it looks
# for and fails when the page does not name it.

file(READ "${SRC}/docs/Dependencies.md" page)

file(
  GLOB_RECURSE
  build_files
  "${SRC}/CMakeLists.txt"
  "${SRC}/cmake/*.cmake"
  "${SRC}/scripts/*.cmake"
  "${SRC}/src/CMakeLists.txt"
  "${SRC}/src/*/CMakeLists.txt"
  "${SRC}/tests/CMakeLists.txt"
  "${SRC}/tests/*/CMakeLists.txt"
  "${SRC}/tools/CMakeLists.txt"
  "${SRC}/tools/*/CMakeLists.txt"
  "${SRC}/examples/CMakeLists.txt")

set(ws "[ \t\r\n]")
set(names "")
foreach(f ${build_files})
  # This file names the commands it looks for, in its own patterns.
  if(f MATCHES "GateDependencies.cmake$")
    continue()
  endif()
  file(READ "${f}" text)

  string(REGEX MATCHALL "find_package\\(${ws}*[A-Za-z0-9_]+" found "${text}")
  foreach(m ${found})
    string(REGEX REPLACE "find_package\\(${ws}*" "" n "${m}")
    list(APPEND names "${n}")
  endforeach()

  string(
    REGEX
      MATCHALL
      "find_program\\(${ws}*[A-Za-z0-9_]+${ws}+(NAMES${ws}+)?[A-Za-z0-9_.+-]+"
      found "${text}")
  foreach(m ${found})
    string(REGEX REPLACE ".*${ws}([A-Za-z0-9_.+-]+)$" "\\1" n "${m}")
    list(APPEND names "${n}")
  endforeach()

  set(pkg "pkg_check_modules\\(${ws}*[A-Za-z0-9_]+")
  string(APPEND pkg "(${ws}+REQUIRED)?${ws}+[A-Za-z0-9_.+-]+")
  string(REGEX MATCHALL "${pkg}" found "${text}")
  foreach(m ${found})
    string(REGEX REPLACE ".*${ws}([A-Za-z0-9_.+-]+)$" "\\1" n "${m}")
    list(APPEND names "${n}")
  endforeach()
endforeach()
list(REMOVE_DUPLICATES names)

set(bad 0)
foreach(n ${names})
  # As a word, so "nm" is not found inside "named"; and escaped, so the
  # "+" in clang++ is a character rather than a repetition.
  string(REPLACE "." "\\." esc "${n}")
  string(REPLACE "+" "\\+" esc "${esc}")
  if(NOT page MATCHES "(^|[^A-Za-z0-9_+-])${esc}([^A-Za-z0-9_+-]|$)")
    message("VIOLATION: the build looks for '${n}' and docs/Dependencies.md "
            "does not name it")
    set(bad 1)
  endif()
endforeach()
if(bad)
  message(FATAL_ERROR "dependency gate failed")
endif()
list(LENGTH names count)
message("dependency gate: clean, ${count} dependencies named")
