# The table of kinds in docs/Formats.md is generated, not written: it is
# what `doctor formats --format markdown` prints, between the markers
# <!-- formats:begin --> and <!-- formats:end -->. A codec whose flags
# change, or a format added, makes the page stale; this fails then and
# names the command that writes it again. The plain text form is held to
# name every codec too.

cmake_minimum_required(VERSION 3.10)

execute_process(
  COMMAND ${DOCTOR} formats --format markdown
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE table
  ERROR_VARIABLE errout)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "doctor formats --format markdown failed: ${errout}")
endif()

execute_process(
  COMMAND ${DOCTOR} formats
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE plain)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "doctor formats failed")
endif()
foreach(name json cbor ini)
  if(NOT plain MATCHES "(^|\n)${name} ")
    message(FATAL_ERROR "doctor formats does not list ${name}:\n${plain}")
  endif()
endforeach()

execute_process(
  COMMAND ${DOCTOR} formats --format sarif
  RESULT_VARIABLE rc
  OUTPUT_QUIET ERROR_QUIET)
if(rc EQUAL 0)
  message(FATAL_ERROR "doctor formats took a format it does not print")
endif()

# Without cf_emit no codec writes, and each says "read" where the page,
# made from a full build, says "read and written".
if(NOT EMIT)
  return()
endif()

file(READ ${PAGE} page)
string(FIND "${page}" "<!-- formats:begin -->\n" from)
string(FIND "${page}" "<!-- formats:end -->" to)
if(from EQUAL -1 OR to EQUAL -1)
  message(FATAL_ERROR "${PAGE} has no <!-- formats:begin --> and "
                      "<!-- formats:end --> markers")
endif()
string(LENGTH "<!-- formats:begin -->\n" skip)
math(EXPR start "${from} + ${skip}")
math(EXPR len "${to} - ${start}")
string(SUBSTRING "${page}" ${start} ${len} held)

set(regen
    "Regenerate it with a build that has every format (-DCF_WITH_XML=ON): "
    "doctor formats --format markdown, and put the output between the "
    "markers.")

# The page holds every format. A build that left some out prints fewer
# rows, each of which the page must have exactly; a build with all of
# them prints the page.
string(REPLACE "\n" ";" table_rows "${table}")
string(REPLACE "\n" ";" held_rows "${held}")
list(REMOVE_ITEM table_rows "")
list(REMOVE_ITEM held_rows "")
list(LENGTH table_rows n_table)
list(LENGTH held_rows n_held)
if(n_table EQUAL n_held)
  if(NOT held STREQUAL table)
    message(
      FATAL_ERROR
        "the table in docs/Formats.md is not what doctor prints.\n${regen}\n"
        "--- page ---\n${held}--- doctor ---\n${table}")
  endif()
elseif(n_table LESS n_held)
  foreach(row IN LISTS table_rows)
    list(FIND held_rows "${row}" at)
    if(at EQUAL -1)
      message(FATAL_ERROR "docs/Formats.md has no row\n${row}\n${regen}")
    endif()
  endforeach()
else()
  message(FATAL_ERROR "doctor prints more formats than docs/Formats.md holds. "
                      "${regen}")
endif()
