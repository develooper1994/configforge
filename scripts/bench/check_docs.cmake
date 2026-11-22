# bench.docs (P-1b): cf_bench --emit-docs writes the documents the comparison
# reads, and prints a line a file: name, bytes, FNV-1a. The lines of the 1 MiB
# documents are in bench/docs-1mib.fnv; this regenerates them and fails when one
# differs, so that a generator that drifts, or that differs between compilers,
# is seen.
#
#   cmake -DBENCH=<cf_bench> -DEXPECT=<fnv> -DWORK=<dir> -P check_docs.cmake
#   the same with -DWRITE=ON writes the file from a run whose output was read
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
execute_process(
  COMMAND "${BENCH}" --emit-docs "${WORK}" --size 1
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE have
  ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "bench.docs: cf_bench --emit-docs failed (${rc}): ${err}")
endif()
file(REMOVE_RECURSE "${WORK}")
string(REPLACE "\r\n" "\n" have "${have}")
if(WRITE)
  file(WRITE "${EXPECT}" "${have}")
  message(STATUS "bench.docs: wrote ${EXPECT}")
  return()
endif()
file(READ "${EXPECT}" want)
string(REPLACE "\r\n" "\n" want "${want}")
if(have STREQUAL want)
  message("bench.docs: the documents are the ones in bench/docs-1mib.fnv")
  return()
endif()
string(REPLACE "\n" ";" have_lines "${have}")
string(REPLACE "\n" ";" want_lines "${want}")
list(LENGTH want_lines n)
list(LENGTH have_lines m)
set(i 0)
foreach(line ${have_lines})
  list(GET want_lines ${i} expect_line)
  if(NOT line STREQUAL expect_line)
    message(
      FATAL_ERROR
        "bench.docs: a document is not the committed one:\n"
        "  now:       ${line}\n  committed: ${expect_line}\n"
        "If a generator was meant to change, rewrite the file with "
        "-DWRITE=ON and commit it with the reason.")
  endif()
  math(EXPR i "${i} + 1")
  if(i GREATER_EQUAL n)
    break()
  endif()
endforeach()
message(FATAL_ERROR "bench.docs: ${m} documents now, ${n} committed")
