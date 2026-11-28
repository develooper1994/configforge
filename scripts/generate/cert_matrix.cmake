# docs/compliance/Cert.md: the SEI CERT C matrix, written from one list.
#
#   cmake -DWRITE=docs/compliance/Cert.md -P scripts/generate/cert_matrix.cmake
#                                  replaces the whole page
#   cmake -DOUT=<file> -P scripts/generate/cert_matrix.cmake
#                                  writes the page elsewhere
#   -DIN=<list>                    the list read, docs/compliance/cert.txt by
#                                  default
#
# One line a rule in the list: <rule id> | <checked by> | <note>. The rule ids
# only, never the standard's titles (D-236). Checked by is one or more of,
# separated by "; ":
#
#   clang-tidy <check>   a check the repository's .clang-tidy enables
#   cppcheck <id>        a cppcheck message id the analyze run fails on
#   compiler <flag>      a -W flag the build makes an error
#   cbmc, eva            a proof row of docs/Safety.md
#   design D-<n>         the library never does the thing, by a decision
#   review               a person reads for it at a release (the note says
#                        what to read)
#   n/a                  the library uses no such facility (the note starts
#                        with the grep that shows it)
#
# A line starting with # is a comment. The script fails on an empty field, an
# unknown kind of checking, a duplicate rule id, a rule id not ending in -C,
# and an n/a row whose note does not start with "grep". gate.cert regenerates
# the page and compares, and checks the clang-tidy cells against
# `clang-tidy --list-checks`.
#
# Written in CMake for the reason the other scripts are: it runs wherever
# the build does.

cmake_minimum_required(VERSION 3.10)

get_filename_component(REPO "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
if(NOT DEFINED WRITE AND NOT DEFINED OUT)
  message(
    FATAL_ERROR
      "cert_matrix.cmake: give -DWRITE=<page> or -DOUT=<file> (see the top of "
      "the script)")
endif()
if(NOT DEFINED IN)
  set(IN "${REPO}/docs/compliance/cert.txt")
endif()
if(NOT EXISTS "${IN}")
  message(FATAL_ERROR "cert_matrix.cmake: no list at ${IN}")
endif()

set(CF_CERT_KINDS
    clang-tidy
    cppcheck
    compiler
    cbmc
    eva
    design
    review
    n/a)
set(CF_CERT_WITH_ARGUMENT clang-tidy cppcheck compiler design)

# Semicolons and brackets are protected while the text is a CMake list.
file(READ "${IN}" text)
string(REPLACE "\r" "" text "${text}")
string(REPLACE ";" "@SEMI@" text "${text}")
string(REPLACE "[" "@LB@" text "${text}")
string(REPLACE "]" "@RB@" text "${text}")
string(REPLACE "\n" ";" lines "${text}")

set(seen "")
set(sections "")
set(total 0)
set(unchecked 0)
foreach(kind IN LISTS CF_CERT_KINDS)
  set(count_${kind} 0)
endforeach()

set(lineno 0)
foreach(line IN LISTS lines)
  math(EXPR lineno "${lineno} + 1")
  string(STRIP "${line}" line)
  if(line STREQUAL "" OR line MATCHES "^#")
    continue()
  endif()
  set(where "${IN}:${lineno}")
  if(NOT line MATCHES "^([^|]*)[|]([^|]*)[|]([^|]*)$")
    message(
      FATAL_ERROR
        "${where}: want '<rule id> | <checked by> | <note>' (the note holds "
        "no |)")
  endif()
  string(STRIP "${CMAKE_MATCH_1}" id)
  string(STRIP "${CMAKE_MATCH_2}" by)
  string(STRIP "${CMAKE_MATCH_3}" note)
  if(id STREQUAL ""
     OR by STREQUAL ""
     OR note STREQUAL "")
    message(FATAL_ERROR "${where}: an empty field")
  endif()
  if(NOT id MATCHES "^([A-Z][A-Z][A-Z])[0-9][0-9]-C$")
    message(FATAL_ERROR "${where}: '${id}' is not a rule id like ARR30-C")
  endif()
  set(section "${CMAKE_MATCH_1}")
  if(id IN_LIST seen)
    message(FATAL_ERROR "${where}: ${id} appears twice")
  endif()
  list(APPEND seen "${id}")
  if(NOT section IN_LIST sections)
    list(APPEND sections "${section}")
  endif()

  string(REPLACE "@SEMI@" ";" items "${by}")
  set(cell "")
  set(row_kinds "")
  foreach(
    item IN
    LISTS
    items)
    string(STRIP "${item}" item)
    if(item STREQUAL "")
      message(FATAL_ERROR "${where}: an empty kind of checking in '${by}'")
    endif()
    string(REGEX MATCH "^[^ ]+" kind "${item}")
    if(NOT kind IN_LIST CF_CERT_KINDS)
      message(FATAL_ERROR "${where}: unknown kind of checking '${kind}'")
    endif()
    string(LENGTH "${kind}" klen)
    string(SUBSTRING "${item}" ${klen} -1 arg)
    string(STRIP "${arg}" arg)
    if(kind IN_LIST CF_CERT_WITH_ARGUMENT)
      if(arg STREQUAL "" OR arg MATCHES " ")
        message(FATAL_ERROR "${where}: '${kind}' takes one argument: '${item}'")
      endif()
    elseif(NOT arg STREQUAL "")
      message(FATAL_ERROR "${where}: '${kind}' takes no argument: '${item}'")
    endif()
    if(kind STREQUAL "design" AND NOT arg MATCHES "^D-[0-9]+$")
      message(FATAL_ERROR "${where}: design takes D-<n>: '${item}'")
    endif()
    if(kind STREQUAL "n/a" AND NOT note MATCHES "^`?grep")
      message(FATAL_ERROR "${where}: an n/a row's note starts with grep")
    endif()
    if(NOT kind IN_LIST row_kinds)
      list(APPEND row_kinds "${kind}")
      math(EXPR count_${kind} "${count_${kind}} + 1")
    endif()
    if(cell STREQUAL "")
      set(cell "`${item}`")
    else()
      set(cell "${cell}; `${item}`")
    endif()
  endforeach()
  set(tool_kinds ${row_kinds})
  list(REMOVE_ITEM tool_kinds review n/a)
  if(NOT tool_kinds)
    math(EXPR unchecked "${unchecked} + 1")
  endif()
  math(EXPR total "${total} + 1")
  set(rows_${section} "${rows_${section}}| ${id} | ${cell} | ${note} |\n")
endforeach()

if(total EQUAL 0)
  message(FATAL_ERROR "${IN}: no rules")
endif()

string(
  CONCAT
    result
    "<!-- generated by `scripts/generate/cert_matrix.cmake`; edit "
    "`docs/compliance/cert.txt` -->\n"
    "# SEI CERT C: how each rule is checked\n\n"
    "Every rule of the SEI CERT C Coding Standard (rules, not "
    "recommendations), by id, with what checks this library against it "
    "(D-236). A rule no tool sees says `review` and what a person reads for "
    "it at a release; a rule about a facility the library does not use says "
    "`n/a` and the `grep` that shows it.\n\n"
    "Rules: ${total}, of which ${unchecked} no tool, proof or decision holds: "
    "a person reads for them, or the library does not use the facility.\n\n"
    "| Checked by | Rules |\n| ---------- | ----- |\n")
foreach(kind IN LISTS CF_CERT_KINDS)
  string(APPEND result "| `${kind}` | ${count_${kind}} |\n")
endforeach()
string(APPEND result "\nA rule with several kinds counts under each.\n")
foreach(section IN LISTS sections)
  string(APPEND result "\n## ${section}\n\n| Rule | Checked by | Note |\n"
         "| ---- | ---------- | ---- |\n${rows_${section}}")
endforeach()
string(REPLACE "@SEMI@" ";" result "${result}")
string(REPLACE "@LB@" "[" result "${result}")
string(REPLACE "@RB@" "]" result "${result}")

set(target "${OUT}")
if(DEFINED WRITE)
  set(target "${WRITE}")
endif()
include("${REPO}/cmake/ConfigForgeVersion.cmake")
configforge_write_lf("${target}" "${result}")
