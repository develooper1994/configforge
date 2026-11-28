# docs/compliance/Cert.md is the list docs/compliance/cert.txt, regenerated;
# and every clang-tidy cell of the list is a check the repository's
# .clang-tidy leaves enabled (D-236).
cmake_minimum_required(VERSION 3.10)

set(page "${SRC}/docs/compliance/Cert.md")
set(fresh "${BIN}/Cert.fresh.md")
string(
  CONCAT mismatch
         "docs/compliance/Cert.md does not match "
         "docs/compliance/cert.txt: run cmake "
         "-DWRITE=docs/compliance/Cert.md "
         "-P scripts/generate/cert_matrix.cmake")
execute_process(
  COMMAND "${CMAKE_COMMAND}" "-DOUT=${fresh}" -P
          "${SRC}/scripts/generate/cert_matrix.cmake" RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(
    FATAL_ERROR "scripts/generate/cert_matrix.cmake: cannot be run\n${mismatch}"
  )
endif()
file(READ "${page}" have)
file(READ "${fresh}" want)
string(REPLACE "\r" "" have "${have}")
string(REPLACE "\r" "" want "${want}")
if(NOT have STREQUAL want)
  message(FATAL_ERROR "${mismatch}")
endif()

# The checks clang-tidy knows and .clang-tidy leaves on, read from the source
# root so that the repository's own file decides.
find_program(CF_CERT_CLANG_TIDY NAMES clang-tidy)
# The names are those of the clang-tidy CI pins (22, the job `tidy`): an older
# one renamed some checks (the analyzer's ArrayBound was alpha before 21), so
# the column is held only by that version or a newer one.
set(cert_tidy_major 0)
if(CF_CERT_CLANG_TIDY)
  execute_process(
    COMMAND "${CF_CERT_CLANG_TIDY}" --version
    RESULT_VARIABLE vrc
    OUTPUT_VARIABLE vtext)
  if(vrc EQUAL 0 AND vtext MATCHES "version ([0-9]+)")
    set(cert_tidy_major "${CMAKE_MATCH_1}")
  endif()
endif()
if(NOT CF_CERT_CLANG_TIDY)
  message("gate.cert: clang-tidy column not checked, no clang-tidy")
elseif(cert_tidy_major LESS 22)
  message("gate.cert: clang-tidy column not checked, clang-tidy "
          "${cert_tidy_major} is older than the 22 the names are from")
else()
  execute_process(
    COMMAND "${CF_CERT_CLANG_TIDY}" --list-checks
    WORKING_DIRECTORY "${SRC}"
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE listing)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "gate.cert: clang-tidy --list-checks failed")
  endif()
  string(REPLACE "\r" "" listing "${listing}")
  string(REPLACE "\n" ";" listing "${listing}")
  set(enabled "")
  foreach(entry IN LISTS listing)
    string(STRIP "${entry}" entry)
    if(NOT entry STREQUAL "" AND NOT entry MATCHES ":$")
      list(APPEND enabled "${entry}")
    endif()
  endforeach()
  file(READ "${SRC}/docs/compliance/cert.txt" list_text)
  string(REPLACE "\r" "" list_text "${list_text}")
  string(REGEX REPLACE "(^|\n)#[^\n]*" "" list_text "${list_text}")
  string(REGEX MATCHALL "[ ]clang-tidy [A-Za-z0-9._-]+" cells "${list_text}")
  foreach(cell IN LISTS cells)
    string(REGEX REPLACE "^.*clang-tidy " "" check "${cell}")
    if(NOT check IN_LIST enabled)
      message(
        FATAL_ERROR
          "docs/compliance/cert.txt names the clang-tidy check ${check}, "
          "which clang-tidy does not list as enabled here: it has no such "
          "check, or .clang-tidy disables it")
    endif()
  endforeach()
endif()
message("cert gate: clean")
