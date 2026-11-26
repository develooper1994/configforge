# Frama-C EVA over the bottom components (card L-3b).
#
#   cmake -DSRC=<repository root> [-DSTAGED=<dir with staged headers>]
#         [-DFRAMAC=<frama-c>] [-DOUT=<dir>] -P scripts/check/eva.cmake
#
# One harness per component (tests/eva/eva_<name>.c), each run by itself.
# A property whose status is not Valid, Considered valid, Dead or Unreachable
# is an alarm. Every alarm must be listed in .eva-accepted.txt
# ("<file> <function> <kind>", kind with '_' for spaces) and every listed
# entry must match an alarm: both failures exit 1. The CSV of a run is
# deleted first and a failing frama-c fails the step.

if(NOT SRC)
  message(FATAL_ERROR "eva: -DSRC=<repository root> is required")
endif()
if(NOT FRAMAC)
  find_program(FRAMAC frama-c HINTS /opt/cf-tools/bin)
endif()
if(NOT FRAMAC)
  message(STATUS "eva                   skipped: no frama-c")
  return()
endif()
if(NOT OUT)
  set(OUT "${SRC}/build/eva")
endif()
file(MAKE_DIRECTORY "${OUT}")

set(common_flags
    -machdep
    gcc_x86_64
    -eva
    -eva-precision
    5
    -eva-warn-copy-indeterminate=@all
    -no-unicode)
set(split_flags
    "-eva-slevel-function=cf_u64_to_text:500,main:200"
    "-eva-split-return-function=cf_u64_to_text:full,cf_i64_to_text:full"
    -eva-split-limit 200)

# name | sources (relative to SRC) | extra flags
set(harnesses
    port
    numeric
    base
    alloc
    arena_fixed)
set(src_port lib/port/src/port.c)
set(src_numeric lib/numeric/src/numeric.c lib/port/src/port.c)
set(src_base
    lib/base/src/error.c
    lib/base/src/key.c
    lib/base/src/value.c
    lib/numeric/src/numeric.c
    lib/port/src/port.c)
set(src_alloc lib/base/src/alloc.c lib/port/src/port.c)
set(src_arena_fixed lib/base/src/alloc.c lib/port/src/port.c)
set(flags_arena_fixed)
set(flags_port ${split_flags})
set(flags_numeric)
set(flags_base ${split_flags})
set(flags_alloc)

set(inc "-I${SRC}/lib/port/include -I${SRC}/lib/numeric/include")
set(inc "${inc} -I${SRC}/lib/base/include")
if(STAGED)
  set(inc "${inc} -I${STAGED}")
endif()

set(ok_status "Valid" "Considered valid" "Dead" "Unreachable")
set(alarms_all "")
set(failed FALSE)

foreach(h IN LISTS harnesses)
  set(csv "${OUT}/eva_${h}.csv")
  file(REMOVE "${csv}")
  set(srcs "")
  foreach(f IN LISTS src_${h})
    list(APPEND srcs "${SRC}/${f}")
  endforeach()
  execute_process(
    COMMAND
      "${FRAMAC}" "-cpp-extra-args=${inc} -DCF_INTERNAL_API=1" ${common_flags}
      ${flags_${h}} "${SRC}/tests/eva/eva_${h}.c" ${srcs} -then -report
      -report-csv "${csv}"
    WORKING_DIRECTORY "${SRC}"
    RESULT_VARIABLE rc
    OUTPUT_FILE "${OUT}/eva_${h}.log"
    ERROR_FILE "${OUT}/eva_${h}.log")
  if(NOT rc EQUAL 0 OR NOT EXISTS "${csv}")
    message(
      SEND_ERROR "eva: frama-c failed on eva_${h}.c (see ${OUT}/eva_${h}.log)")
    set(failed TRUE)
    continue()
  endif()
  file(READ "${csv}" content)
  string(REPLACE ";" "," content "${content}")
  string(REGEX MATCHALL "[^\n]+" lines "${content}")
  list(POP_FRONT lines) # header
  set(count 0)
  foreach(line IN LISTS lines)
    string(REPLACE "\t" ";" f "${line}")
    list(LENGTH f n)
    if(n LESS 6)
      continue()
    endif()
    list(GET f 0 dir)
    list(GET f 1 file)
    list(GET f 3 fn)
    list(GET f 4 kind)
    list(GET f 5 status)
    if(dir MATCHES "^FRAMAC_SHARE" OR dir MATCHES "^tests")
      continue()
    endif()
    if(status IN_LIST ok_status)
      continue()
    endif()
    string(REPLACE " " "_" kind "${kind}")
    set(key "${dir}/${file} ${fn} ${kind}")
    list(APPEND alarms_all "${key}")
    math(EXPR count "${count}+1")
  endforeach()
  message(STATUS "eva_${h}: ${count} alarm lines")
endforeach()
list(REMOVE_DUPLICATES alarms_all)

# the accepted list
set(accepted "")
file(STRINGS "${SRC}/.eva-accepted.txt" acc_lines)
foreach(l IN LISTS acc_lines)
  if(l MATCHES "^[ \t]*#" OR l MATCHES "^[ \t]*$")
    continue()
  endif()
  string(STRIP "${l}" l)
  list(APPEND accepted "${l}")
endforeach()

foreach(a IN LISTS alarms_all)
  if(NOT a IN_LIST accepted)
    message(SEND_ERROR "eva: alarm not in .eva-accepted.txt: ${a}")
    set(failed TRUE)
  endif()
endforeach()
foreach(a IN LISTS accepted)
  if(NOT a IN_LIST alarms_all)
    message(SEND_ERROR "eva: .eva-accepted.txt entry matches no alarm: ${a}")
    set(failed TRUE)
  endif()
endforeach()

list(LENGTH alarms_all total)
if(failed)
  message(FATAL_ERROR "eva: ${total} distinct alarms; not all accounted for")
endif()
message(STATUS "eva: ${total} alarms, all accepted")
