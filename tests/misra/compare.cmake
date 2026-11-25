# The comparator of gate.misra (cmake/gates/GateMisra.cmake), driven with
# hand-written cppcheck output and records: no cppcheck is run, so this test
# is CMake only and runs in every profile and on every platform. The cppcheck
# run itself is the real gate.misra (the misra profile).
#
# -DSRC=<source root> -DWORK=<scratch directory>

cmake_minimum_required(VERSION 3.10)

# The environment of a CI machine must not turn a skip into a failure here.
unset(ENV{CI})

set(gate "${SRC}/cmake/gates/GateMisra.cmake")
set(failed 0)

# The source root the gate scans for suppressions: a tree of this test's own.
set(clean_tree "${WORK}/tree-clean")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${clean_tree}/lib/a/src")
file(WRITE "${clean_tree}/lib/a/src/x.c" "int x(void)\n{\n  return 0;\n}\n")
set(tree "${clean_tree}")

set(TOOL "tool | cppcheck | 2.19\n")
set(REASON "early return is the house style")
set(R155 "15.5 | lib/a/src/x.c | 2 | deviation | ${REASON}\n")
set(O155
    "lib/a/src/x.c:3:5: misra-c2012-15.5\nlib/a/src/x.c:9:5: misra-c2012-15.5\n"
)

# case(<name> <rc> <pattern> <output> <record> [extra cmake arguments])
# Runs the gate over the output and the record and checks the exit status and
# a regular expression on everything it printed. Leaves `out` set.
function(
  case
  name
  want
  pattern
  output
  record)
  set(dir "${WORK}/${name}")
  file(MAKE_DIRECTORY "${dir}")
  file(WRITE "${dir}/out.txt" "${output}")
  file(WRITE "${dir}/rec.txt" "${record}")
  execute_process(
    COMMAND ${CMAKE_COMMAND} "-DSRC=${tree}" "-DOUTPUT=${dir}/out.txt"
            "-DRECORD=${dir}/rec.txt" ${ARGN} -P "${gate}"
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err)
  set(all "${out}${err}")
  set(why "")
  if(NOT rc STREQUAL "${want}")
    set(why "exit ${rc}, wanted ${want}")
  elseif(NOT all MATCHES "${pattern}")
    set(why "no match for `${pattern}`")
  endif()
  if(why)
    message(SEND_ERROR "case ${name}: ${why}\n${all}")
    math(EXPR failed "${failed} + 1")
    set(failed
        "${failed}"
        PARENT_SCOPE)
  endif()
  set(out
      "${all}"
      PARENT_SCOPE)
endfunction()

# 1. The good case: two reports of 15.5, one line that says so.
case(
  good
  0
  "2 reports in 1 files and 1 rules, 1 record lines \\(deviation 1, tool 0, backlog 0\\)"
  "${O155}"
  "${TOOL}${R155}")

# 2. A report with no record line.
case(
  no-record
  1
  "VIOLATION: lib/a/src/x.c: rule 12\\.1 reported 1 time"
  "${O155}lib/a/src/x.c:12:7: misra-c2012-12.1\n"
  "${TOOL}${R155}")

# 3. A record line that matches no report.
case(
  stale
  1
  "STALE"
  "${O155}"
  "${TOOL}${R155}10.4 | lib/a/src/y.c | 1 | backlog | not yet examined here\n")

# 4. A count that is too high or too low in the record.
case(
  count-up
  1
  "reported 3, the record says 2"
  "${O155}lib/a/src/x.c:20:5: misra-c2012-15.5\n"
  "${TOOL}${R155}")
case(
  count-down
  1
  "reported 1, the record says 2"
  "lib/a/src/x.c:3:5: misra-c2012-15.5\n"
  "${TOOL}${R155}")

# 5. One file, line, column and rule from two sources is one report.
case(
  twice
  0
  "1 reports in 1 files"
  "lib/a/src/x.c:3:5: misra-c2012-15.5\nlib/a/src/x.c:3:5: misra-c2012-15.5\n"
  "${TOOL}15.5 | lib/a/src/x.c | 1 | deviation | ${REASON}\n")

# 6. A reason that is TODO, short, or holds a ';'.
foreach(bad "TODO" "short" "a reason; with a semicolon")
  string(MAKE_C_IDENTIFIER "${bad}" id)
  case(
    reason-${id}
    1
    "line 2: 15\\.5 lib/a/src/x\\.c: the reason"
    "${O155}"
    "${TOOL}15.5 | lib/a/src/x.c | 2 | deviation | ${bad}\n")
endforeach()

# 7. A deviation in a public header is refused; a backlog line there is not.
case(
  header-deviation
  1
  "public header"
  "lib/a/include/h.h:1:1: misra-c2012-8.4\n"
  "${TOOL}8.4 | lib/a/include/h.h | 1 | deviation | ${REASON}\n")
case(
  header-backlog
  0
  "1 reports in public headers"
  "lib/a/include/h.h:1:1: misra-c2012-8.4\n"
  "${TOOL}8.4 | lib/a/include/h.h | 1 | backlog | a later card fixes it\n")

# 8. The gate holds no class of a rule: 17.3 takes each kind.
foreach(kind deviation tool backlog)
  case(
    class-${kind}
    0
    "1 reports"
    "lib/a/src/x.c:3:5: misra-c2012-17.3\n"
    "${TOOL}17.3 | lib/a/src/x.c | 1 | ${kind} | ${REASON}\n")
endforeach()

# 9. An ignored rule is dropped before the count; an ignore line needs no
# report.
set(IGNORE "ignore | 2.5 | whole-program rule, for the integrator\n")
case(
  ignored
  0
  "0 reports in 0 files and 0 rules.*ignored rules: 2\\.5 \\(1 reports dropped\\)"
  "lib/a/src/x.c:3:5: misra-c2012-2.5\n"
  "${TOOL}${IGNORE}")
case(
  ignore-unused
  0
  "0 reports"
  ""
  "${TOOL}${IGNORE}")

# 10. Malformed lines name their line number.
case(
  four-fields
  1
  "line 3"
  ""
  "${TOOL}# a comment\n15.5 | lib/a/src/x.c | 2 | deviation\n")
foreach(field "2 | maybe" "0 | deviation" "x | deviation")
  string(MAKE_C_IDENTIFIER "${field}" id)
  case(
    field-${id}
    1
    "line 2"
    "${O155}"
    "${TOOL}15.5 | lib/a/src/x.c | ${field} | ${REASON}\n")
endforeach()
case(
  two-tool-lines
  1
  "exactly one `tool"
  ""
  "${TOOL}${TOOL}")
case(
  no-tool-line
  1
  "exactly one `tool"
  ""
  "")

# 11. A misra-config line is counted, never a violation; nothing at all is a
# clean run.
case(
  config
  0
  "1 misra-config notes"
  "lib/a/src/x.c:7:3: misra-config\n"
  "${TOOL}")
case(
  empty
  0
  "0 reports"
  ""
  "${TOOL}")

# 12. The version: a record is from one cppcheck. Locally another is a status
# line; in CI (-DCI=ON) it is a failure; a patch release is the same version.
case(
  version-other
  0
  "skipped.*cppcheck 2\\.19.*2\\.20"
  "${O155}"
  "${TOOL}${R155}"
  -DVERSION=2.20.0)
case(
  version-other-ci
  1
  "cppcheck 2\\.19.*2\\.20.*regenerate"
  "${O155}"
  "${TOOL}${R155}"
  -DVERSION=2.20.0
  -DCI=ON)
case(
  version-patch
  0
  "2 reports.*cppcheck 2\\.19"
  "${O155}"
  "${TOOL}${R155}"
  -DVERSION=2.19.4)

# 13. A comment that hides a report is refused: the record is the only place.
set(hidden_tree "${WORK}/tree-hidden")
file(MAKE_DIRECTORY "${hidden_tree}/lib/x/src")
file(WRITE "${hidden_tree}/lib/x/src/a.c"
     "/* cppcheck-suppress misra-c2012-15.5 */\nint a;\n")
set(tree "${hidden_tree}")
case(
  suppress
  1
  "cppcheck-suppress"
  ""
  "${TOOL}")
set(tree "${clean_tree}")

# 14. The write mode: the record for the reports found. A line that exists
# keeps its kind and reason; a new one is TODO; a line whose reports are gone
# is dropped; the tool line takes the version that ran.
set(written "${WORK}/written.txt")
case(
  write
  0
  "wrote 2 record lines"
  "${O155}lib/a/src/x.c:12:7: misra-c2012-12.1\n"
  "${TOOL}${IGNORE}${R155}10.4 | lib/a/src/y.c | 1 | backlog | not yet examined here\n"
  "-DWRITE=${written}"
  -DVERSION=2.19.3)
file(READ "${written}" text)
foreach(
  want
  "^tool \\| cppcheck \\| 2\\.19\n"
  "\nignore \\| 2\\.5 \\| whole-program rule, for the integrator\n"
  "\n12\\.1 \\| lib/a/src/x\\.c \\| 1 \\| TODO \\| TODO\n"
  "\n15\\.5 \\| lib/a/src/x\\.c \\| 2 \\| deviation \\| ${REASON}\n")
  if(NOT text MATCHES "${want}")
    message(
      SEND_ERROR "case write: the written record lacks `${want}`:\n${text}")
    math(EXPR failed "${failed} + 1")
  endif()
endforeach()
if(text MATCHES "10\\.4")
  message(SEND_ERROR "case write: the line whose reports are gone was kept")
  math(EXPR failed "${failed} + 1")
endif()
# 12.1's kind and reason are unknown (no other line of the rule): the written
# record fails the compare until they are filled in, and passes after.
file(READ "${written}" record)
case(
  written-todo
  1
  "line [0-9]+: 12\\.1 lib/a/src/x\\.c"
  "${O155}lib/a/src/x.c:12:7: misra-c2012-12.1\n"
  "${record}")
string(REPLACE "| TODO | TODO" "| backlog | not yet examined here" record
               "${record}")
case(
  written-filled
  0
  "3 reports"
  "${O155}lib/a/src/x.c:12:7: misra-c2012-12.1\n"
  "${record}")

if(failed)
  message(FATAL_ERROR "misra.compare: ${failed} failed")
endif()
message("misra.compare: all cases passed")
