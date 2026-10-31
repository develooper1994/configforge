# The bounded proofs (docs/Safety.md): each tests/proofs/proof_*.c
# under CBMC, with the checks a memory or arithmetic error would fail -
# bounds, pointers, signed overflow, every loop unwound far enough that
# no execution is left out (an assertion says so, or the proof fails).
#
#   cmake -DSRC=<repo> [-DUNWIND=48] -P scripts/proofs.cmake
#
# Each harness says which library sources it proves, on a line
#   cbmc-sources: src/core/numeric.c src/core/port.c
cmake_minimum_required(VERSION 3.10)

if(NOT DEFINED SRC OR NOT EXISTS "${SRC}/include/configforge/configforge.h")
  message(FATAL_ERROR "proofs: SRC must name the repository")
endif()
if(NOT DEFINED UNWIND)
  set(UNWIND 48)
endif()
find_program(CBMC_EXE cbmc)
if(NOT CBMC_EXE)
  message(FATAL_ERROR "proofs: no cbmc")
endif()

file(GLOB harnesses "${SRC}/tests/proofs/proof_*.c")
set(bad 0)
foreach(h ${harnesses})
  get_filename_component(name "${h}" NAME_WE)
  file(STRINGS "${h}" line REGEX "cbmc-sources:")
  string(REGEX REPLACE ".*cbmc-sources:[ \t]*" "" line "${line}")
  string(REGEX REPLACE "[ \t]*\\*/.*" "" line "${line}")
  separate_arguments(rel UNIX_COMMAND "${line}")
  set(sources "")
  foreach(r ${rel})
    list(APPEND sources "${SRC}/${r}")
  endforeach()
  execute_process(
    COMMAND
      ${CBMC_EXE} "${h}" ${sources} -I "${SRC}/include" --bounds-check
      --pointer-check --signed-overflow-check --div-by-zero-check
      --undefined-shift-check --unwind ${UNWIND} --unwinding-assertions
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE out)
  if(rc EQUAL 0)
    message("proof ${name}: VERIFICATION SUCCESSFUL")
  else()
    string(REGEX MATCHALL "\\[[^\n]*\\] [^\n]*FAILURE" failures "${out}")
    message("VIOLATION: ${name} is not proven")
    foreach(f ${failures})
      message("  ${f}")
    endforeach()
    if(NOT failures)
      message("${out}")
    endif()
    set(bad 1)
  endif()
endforeach()
if(bad)
  message(FATAL_ERROR "proofs failed")
endif()
