# gate.misra: cppcheck's MISRA C:2012 addon over the library sources the misra
# profile builds, against the record docs/compliance/deviations.txt (N-19,
# D-112, docs/Compliance.md "Deviation records").
#
# Run mode (no -DOUTPUT): -DSRC=<source root> -DBIN=<misra build directory>
# -DDEFINES=<-Da|-Db> (the compiler's own macros, joined with '|'), -DJOBS=<n>
# (default: the logical cores, at most 8), -DMISRA_CPPCHECK_EXTRA=<options>.
# Compare mode: -DOUTPUT=<file of cppcheck's output already made>; no cppcheck
# is run, the same code compares it with -DRECORD=<file> (the test
# misra.compare). -DVERSION=<x.y[.z]> stands for `cppcheck --version` there.
# -DWRITE=ON (or -DWRITE=<file>) writes the record for the reports found.
# -DCI=ON, or the environment variable CI=true (GitHub sets it), makes a
# missing or different cppcheck a failure instead of a skip.
cmake_minimum_required(VERSION 3.10)

# The keys of the record and of the reports name variables: rec_count_<key>.
# cmake-lint: disable=C0103

if(NOT DEFINED RECORD)
  set(RECORD "${SRC}/docs/compliance/deviations.txt")
endif()
set(required OFF)
if(CI OR "$ENV{CI}" STREQUAL "true")
  set(required ON)
endif()
set(write_to "")
if(WRITE)
  if(WRITE STREQUAL "ON"
     OR WRITE STREQUAL "1"
     OR WRITE STREQUAL "TRUE")
    set(write_to "${RECORD}")
  else()
    set(write_to "${WRITE}")
  endif()
endif()

set(problems "")
# Adds one problem, its arguments joined, to the list the gate reports.
macro(PROBLEM)
  string(CONCAT problem_text ${ARGV})
  list(APPEND problems "${problem_text}")
endmacro()

# The lines of a file as list items, each with a leading '|' so that an empty
# line keeps its place; ';', '[' and ']' are replaced so that the list holds.
function(read_lines path out)
  file(READ "${path}" text)
  string(REPLACE "\r" "" text "${text}")
  string(REPLACE ";" "<SEMI>" text "${text}")
  string(REPLACE "[" "<LB>" text "${text}")
  string(REPLACE "]" "<RB>" text "${text}")
  string(REPLACE "\n" ";|" text "|${text}")
  set(${out}
      "${text}"
      PARENT_SCOPE)
endfunction()

# Zero-padded number, for sorting rules numerically.
function(pad n out)
  string(LENGTH "000${n}" width)
  math(EXPR start "${width} - 4")
  string(SUBSTRING "000${n}" ${start} 4 padded)
  set(${out}
      "${padded}"
      PARENT_SCOPE)
endfunction()

# ---- The record --------------------------------------------------------
set(rec_keys "")
set(rec_ignored "")
set(rec_tool "")
set(tool_lines 0)
if(EXISTS "${RECORD}")
  read_lines("${RECORD}" lines)
  set(number 0)
  foreach(item IN LISTS lines)
    math(EXPR number "${number} + 1")
    string(SUBSTRING "${item}" 1 -1 line)
    string(STRIP "${line}" line)
    if(line STREQUAL "" OR line MATCHES "^#")
      continue()
    endif()
    string(REPLACE " | " ";" fields "${line}")
    list(LENGTH fields n)
    list(GET fields 0 first)
    if(first STREQUAL "tool")
      set(tool_ok OFF)
      if(n EQUAL 3)
        list(GET fields 1 second)
        list(GET fields 2 candidate)
        if(second STREQUAL "cppcheck" AND candidate MATCHES "^[0-9]+\\.[0-9]+$")
          set(tool_ok ON)
          set(rec_tool "${candidate}")
        endif()
      endif()
      if(NOT tool_ok)
        problem("line ${number}: expected `tool | cppcheck | <major.minor>`")
      endif()
      math(EXPR tool_lines "${tool_lines} + 1")
    elseif(first STREQUAL "ignore")
      set(ok OFF)
      if(n EQUAL 3)
        list(GET fields 1 rule)
        list(GET fields 2 reason)
        if(rule MATCHES "^[0-9]+\\.[0-9]+$" AND NOT reason STREQUAL "")
          set(ok ON)
          list(APPEND rec_ignored "${rule}")
        endif()
      endif()
      if(NOT ok)
        problem("line ${number}: expected `ignore | <rule> | <reason>`")
      endif()
    elseif(first MATCHES "^[0-9]+\\.[0-9]+$")
      if(NOT n EQUAL 5)
        problem("line ${number}: expected five fields: rule | file | count | "
                "kind | reason")
        continue()
      endif()
      list(GET fields 1 file)
      list(GET fields 2 count)
      list(GET fields 3 kind)
      list(GET fields 4 reason)
      set(rule "${first}")
      set(bad "")
      if(NOT count MATCHES "^[1-9][0-9]*$")
        string(APPEND bad " the count is not a positive number;")
      endif()
      if(file MATCHES "/$" OR file STREQUAL "")
        string(APPEND bad " the file is a directory or empty;")
      endif()
      if(NOT kind MATCHES "^(deviation|tool|backlog)$")
        string(APPEND bad " the kind is not deviation, tool or backlog;")
      endif()
      if(reason MATCHES "<SEMI>|<LB>|<RB>")
        string(APPEND bad " the reason holds ';', '[' or ']';")
      endif()
      string(LENGTH "${reason}" reason_length)
      if(reason_length LESS 12 OR reason STREQUAL "TODO")
        string(APPEND bad
               " the reason is missing, TODO or under 12 characters;")
      endif()
      if(kind STREQUAL "deviation" AND file MATCHES "/include/")
        string(APPEND bad " a public header takes no deviation;")
      endif()
      if(NOT bad STREQUAL "")
        problem("line ${number}: ${rule} ${file}:${bad}")
      endif()
      set(key "${rule}/${file}")
      if(DEFINED "rec_count_${key}")
        problem("line ${number}: ${rule} ${file} is recorded twice")
      endif()
      set("rec_count_${key}" "${count}")
      set("rec_kind_${key}" "${kind}")
      set("rec_reason_${key}" "${reason}")
      list(APPEND rec_keys "${key}")
    else()
      problem("line ${number}: not a record, `tool` or `ignore` line")
    endif()
  endforeach()
endif()
if(NOT tool_lines EQUAL 1 AND NOT write_to)
  problem("the record needs exactly one `tool | cppcheck | <major.minor>` line "
          "(found ${tool_lines})")
endif()

# ---- cppcheck's output -------------------------------------------------
set(version "")
if(DEFINED OUTPUT)
  if(DEFINED VERSION)
    string(REGEX MATCH "^[0-9]+\\.[0-9]+" version "${VERSION}")
  endif()
  set(output_file "${OUTPUT}")
else()
  find_program(CPPCHECK_EXE cppcheck)
  if(NOT CPPCHECK_EXE)
    if(required)
      message(FATAL_ERROR "gate.misra: cppcheck is not installed")
    endif()
    message("gate.misra: skipped, cppcheck is not installed")
    return()
  endif()
  execute_process(
    COMMAND "${CPPCHECK_EXE}" --version
    OUTPUT_VARIABLE found
    RESULT_VARIABLE rc)
  string(REGEX MATCH "[0-9]+\\.[0-9]+" version "${found}")
  if(NOT rc EQUAL 0 OR version STREQUAL "")
    message(FATAL_ERROR "gate.misra: cannot read `cppcheck --version`")
  endif()
endif()

set(different "")
if(NOT version STREQUAL ""
   AND NOT write_to
   AND NOT rec_tool STREQUAL ""
   AND NOT version STREQUAL rec_tool)
  set(different "the record is from cppcheck ${rec_tool}, this is ${version}: "
                "regenerate with -DWRITE=ON and review the difference")
  string(CONCAT different ${different})
  if(required)
    message(FATAL_ERROR "gate.misra: ${different}")
  endif()
  message("gate.misra: skipped, ${different}")
  return()
endif()

if(NOT DEFINED OUTPUT)
  # The addon must have run: a file that violates 15.1 must be reported.
  set(canary_dir "${BIN}/misra-canary")
  file(MAKE_DIRECTORY "${canary_dir}")
  file(WRITE "${canary_dir}/canary.c"
       "int main(void)\n{\n  goto x;\nx:\n  return 0;\n}\n")
  execute_process(
    COMMAND "${CPPCHECK_EXE}" --enable=style --addon=misra --quiet
            "--template={file}:{line}:{column}: {id}" canary.c
    WORKING_DIRECTORY "${canary_dir}"
    OUTPUT_VARIABLE canary_out
    ERROR_VARIABLE canary_out)
  if(NOT canary_out MATCHES "misra-c2012-15\\.1")
    message(
      FATAL_ERROR
        "gate.misra: the MISRA addon reported nothing for a file that "
        "violates 15.1: python3 or the addon is missing, or cppcheck changed "
        "its output:\n${canary_out}")
  endif()

  string(REPLACE "|" ";" defines "${DEFINES}")
  if(DEFINED JOBS)
    set(cores "${JOBS}")
  else()
    cmake_host_system_information(RESULT cores QUERY NUMBER_OF_LOGICAL_CORES)
    if(cores GREATER 8)
      set(cores 8)
    endif()
    if(cores LESS 1)
      set(cores 1)
    endif()
  endif()
  set(cache "${BIN}/misra-cppcheck")
  file(REMOVE_RECURSE "${cache}")
  file(MAKE_DIRECTORY "${cache}")
  execute_process(
    COMMAND
      "${CPPCHECK_EXE}" "--project=${BIN}/compile_commands.json"
      "--file-filter=*/lib/*/src/*" --std=c89 ${defines} --enable=style
      --addon=misra "--cppcheck-build-dir=${cache}" -j ${cores}
      "--template={file}:{line}:{column}: {id}" --suppress=missingIncludeSystem
      --suppress=unusedFunction --quiet ${MISRA_CPPCHECK_EXTRA}
    WORKING_DIRECTORY "${SRC}"
    OUTPUT_VARIABLE run_out
    ERROR_VARIABLE run_err
    RESULT_VARIABLE rc)
  set(output_file "${BIN}/misra-cppcheck.txt")
  file(WRITE "${output_file}" "${run_out}\n${run_err}")
  if(NOT rc EQUAL 0 OR run_err MATCHES
                       "(internalAstError|syntaxError|cppcheckError)")
    problem("cppcheck failed (exit ${rc}); see ${output_file}")
  endif()
endif()

# ---- The reports -------------------------------------------------------
set(rep_keys "")
set(ignored_dropped 0)
set(config_notes 0)
set(source_prefix "${SRC}/")
if(EXISTS "${output_file}")
  read_lines("${output_file}" lines)
else()
  set(lines "")
endif()
foreach(item IN LISTS lines)
  string(SUBSTRING "${item}" 1 -1 line)
  if(line MATCHES ": misra-config$")
    math(EXPR config_notes "${config_notes} + 1")
  elseif(line MATCHES "^(.*):([0-9]+):([0-9]+): misra-c2012-([0-9]+\\.[0-9]+)$")
    set(file "${CMAKE_MATCH_1}")
    set(report
        "${CMAKE_MATCH_1}:${CMAKE_MATCH_2}:${CMAKE_MATCH_3}:${CMAKE_MATCH_4}")
    set(rule "${CMAKE_MATCH_4}")
    if(DEFINED "seen|${report}")
      continue()
    endif()
    set("seen|${report}" ON)
    string(FIND "${file}" "${source_prefix}" at)
    if(at EQUAL 0)
      string(LENGTH "${source_prefix}" cut)
      string(SUBSTRING "${file}" ${cut} -1 file)
    endif()
    list(FIND rec_ignored "${rule}" ignored_at)
    if(NOT ignored_at EQUAL -1)
      math(EXPR ignored_dropped "${ignored_dropped} + 1")
      continue()
    endif()
    set(key "${rule}/${file}")
    if(NOT DEFINED "rep_count_${key}")
      set("rep_count_${key}" 0)
      list(APPEND rep_keys "${key}")
    endif()
    set(now "${rep_count_${key}}")
    math(EXPR now "${now} + 1")
    set("rep_count_${key}" "${now}")
  endif()
endforeach()

# ---- Write mode --------------------------------------------------------
if(write_to)
  set(sortable "")
  foreach(key IN LISTS rep_keys)
    string(FIND "${key}" "/" slash)
    string(SUBSTRING "${key}" 0 ${slash} rule)
    math(EXPR slash "${slash} + 1")
    string(SUBSTRING "${key}" ${slash} -1 file)
    string(REPLACE "." ";" numbers "${rule}")
    list(GET numbers 0 a)
    list(GET numbers 1 b)
    pad("${a}" pa)
    pad("${b}" pb)
    list(APPEND sortable "${pa}.${pb}|${file}")
  endforeach()
  list(SORT sortable)
  if(version STREQUAL "")
    set(version "${rec_tool}")
  endif()
  set(text "tool | cppcheck | ${version}\n")
  string(
    APPEND
    text
    "# MISRA C:2012 reports of cppcheck's addon, one line each (rule, file),\n"
    "# read by gate.misra. Format: rule | file | count | kind | reason.\n"
    "# Regenerate: cmake -DSRC=. -DBIN=<misra build>\n"
    "#   -DDEFINES=<compiler macros> -DWRITE=ON\n"
    "#   -P cmake/gates/GateMisra.cmake, then review the difference.\n"
    "# Kinds: deviation (decided), tool (the addon is wrong),\n"
    "#   backlog (undecided).\n"
    "# A reason holds no ; [ ]. No suppression comments: this file is\n"
    "#   the only place.\n")
  # ignore lines are kept with their reasons.
  if(EXISTS "${RECORD}")
    read_lines("${RECORD}" old)
    foreach(item IN LISTS old)
      string(SUBSTRING "${item}" 1 -1 line)
      if(line MATCHES "^ignore \\| ")
        string(REPLACE "<SEMI>" ";" line "${line}")
        string(APPEND text "${line}\n")
      endif()
    endforeach()
  endif()
  foreach(entry IN LISTS sortable)
    string(REPLACE "|" ";" parts "${entry}")
    list(GET parts 0 padded)
    list(GET parts 1 file)
    string(REPLACE "." ";" numbers "${padded}")
    list(GET numbers 0 a)
    list(GET numbers 1 b)
    string(REGEX REPLACE "^0+([0-9])" "\\1" a "${a}")
    string(REGEX REPLACE "^0+([0-9])" "\\1" b "${b}")
    set(rule "${a}.${b}")
    set(key "${rule}/${file}")
    set(kind "TODO")
    set(reason "TODO")
    if(DEFINED "rec_kind_${key}")
      set(kind "${rec_kind_${key}}")
      set(reason "${rec_reason_${key}}")
    else()
      foreach(other IN LISTS rec_keys)
        if(other MATCHES "^${a}\\.${b}/")
          set(kind "${rec_kind_${other}}")
          set(reason "${rec_reason_${other}}")
          break()
        endif()
      endforeach()
    endif()
    string(APPEND text
           "${rule} | ${file} | ${rep_count_${key}} | ${kind} | ${reason}\n")
  endforeach()
  file(WRITE "${write_to}" "${text}")
  list(LENGTH sortable written)
  message("gate.misra: wrote ${written} record lines to ${write_to}")
  return()
endif()

# ---- Compare -----------------------------------------------------------
foreach(key IN LISTS rep_keys)
  string(FIND "${key}" "/" slash)
  string(SUBSTRING "${key}" 0 ${slash} rule)
  math(EXPR slash "${slash} + 1")
  string(SUBSTRING "${key}" ${slash} -1 file)
  set(n "${rep_count_${key}}")
  if(NOT DEFINED "rec_count_${key}")
    problem("VIOLATION: ${file}: rule ${rule} reported ${n} time(s) and has no "
            "record")
  elseif(NOT "${rec_count_${key}}" EQUAL n)
    problem("VIOLATION: ${file}: rule ${rule} reported ${n}, the record says "
            "${rec_count_${key}}")
  endif()
endforeach()
foreach(key IN LISTS rec_keys)
  if(NOT DEFINED "rep_count_${key}")
    string(REGEX REPLACE "^([0-9]+\\.[0-9]+)/" "rule \\1 in " shown "${key}")
    problem("STALE: record line for ${shown} matches no report")
  endif()
endforeach()

# A comment that hides a report is a record the gate cannot count.
file(GLOB_RECURSE sources "${SRC}/lib/*.c" "${SRC}/lib/*.h")
foreach(path IN LISTS sources)
  file(READ "${path}" text)
  if(text MATCHES "cppcheck-suppress[ \t]+misra")
    problem("${path}: a cppcheck-suppress of a MISRA rule: the record is the "
            "only place")
  endif()
endforeach()

# ---- The summary -------------------------------------------------------
set(reports 0)
set(headers 0)
set(files "")
set(rules "")
foreach(key IN LISTS rep_keys)
  string(FIND "${key}" "/" slash)
  string(SUBSTRING "${key}" 0 ${slash} rule)
  math(EXPR slash "${slash} + 1")
  string(SUBSTRING "${key}" ${slash} -1 file)
  set(now "${rep_count_${key}}")
  math(EXPR reports "${reports} + ${now}")
  if(file MATCHES "/include/")
    math(EXPR headers "${headers} + ${now}")
  endif()
  list(APPEND files "${file}")
  list(APPEND rules "${rule}")
endforeach()
list(REMOVE_DUPLICATES files)
list(REMOVE_DUPLICATES rules)
list(LENGTH files file_count)
list(LENGTH rules rule_count)
list(LENGTH rec_keys record_count)
set(deviation 0)
set(tool 0)
set(backlog 0)
foreach(key IN LISTS rec_keys)
  set(kind_of "${rec_kind_${key}}")
  if(DEFINED "${kind_of}")
    math(EXPR "${kind_of}" "${${kind_of}} + 1")
  endif()
endforeach()
if(rec_ignored)
  string(REPLACE ";" " " ignored_text "${rec_ignored}")
else()
  set(ignored_text "none")
endif()
if(version STREQUAL "")
  set(version "${rec_tool}")
endif()
string(
  CONCAT summary
         "gate.misra: ${reports} reports in ${file_count} files and "
         "${rule_count} rules, ${record_count} record lines (deviation "
         "${deviation}, tool ${tool}, backlog ${backlog}); ${headers} reports "
         "in public headers; ${config_notes} misra-config notes; ignored "
         "rules: ${ignored_text} (${ignored_dropped} reports dropped); "
         "cppcheck ${version}")

if(problems)
  list(LENGTH problems total)
  set(shown 0)
  foreach(text IN LISTS problems)
    if(shown LESS 40)
      message("${text}")
    endif()
    math(EXPR shown "${shown} + 1")
  endforeach()
  if(total GREATER 40)
    math(EXPR more "${total} - 40")
    message("and ${more} more")
  endif()
  message(FATAL_ERROR "gate.misra: ${total} problem(s)\n${summary}")
endif()
message("${summary}")
