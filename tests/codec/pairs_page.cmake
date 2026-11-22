# The tables in docs/Formats.md of what each format holds and what each pair
# of formats loses are generated, not written: they are what
# `configforge codec pairs --format markdown` prints, between the markers
# <!-- pairs:begin --> and <!-- pairs:end -->. A bit added, a format
# whose row changes or a writer added makes the page stale; this fails then
# and names the command that writes it again. The table is the same in
# every build (it reads cf_format_holds only), so there is no guard.
#
#   -DCODEC=<the program>  -DPAGE=<docs/Formats.md>

cmake_minimum_required(VERSION 3.10)

execute_process(
  COMMAND ${CODEC} pairs --format markdown
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE table
  ERROR_VARIABLE errout)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "codec pairs --format markdown failed: ${errout}")
endif()

# A format it prints nothing for.
execute_process(
  COMMAND ${CODEC} pairs --format sarif
  RESULT_VARIABLE rc
  OUTPUT_QUIET ERROR_QUIET)
if(NOT rc EQUAL 2)
  message(FATAL_ERROR "codec pairs --format sarif exited ${rc}, not 2")
endif()

# The text form: the second table has a line for each ordered pair of
# different formats, 6 * 5.
execute_process(
  COMMAND ${CODEC} pairs --format text
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE plain)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "codec pairs --format text failed")
endif()
string(FIND "${plain}" "\n\n" blank)
if(blank EQUAL -1)
  message(FATAL_ERROR "codec pairs --format text has no blank line:\n${plain}")
endif()
math(EXPR from "${blank} + 2")
string(SUBSTRING "${plain}" ${from} -1 second)
string(REPLACE "\n" ";" lines "${second}")
list(REMOVE_ITEM lines "")
list(LENGTH lines n_lines)
if(NOT n_lines EQUAL 30)
  message(
    FATAL_ERROR "the second table has ${n_lines} lines, not 30:\n${second}")
endif()

file(READ ${PAGE} page)
string(FIND "${page}" "<!-- pairs:begin -->\n" from)
string(FIND "${page}" "<!-- pairs:end -->" to)
if(from EQUAL -1 OR to EQUAL -1)
  message(FATAL_ERROR "${PAGE} has no <!-- pairs:begin --> and "
                      "<!-- pairs:end --> markers")
endif()
string(LENGTH "<!-- pairs:begin -->\n" skip)
math(EXPR start "${from} + ${skip}")
math(EXPR len "${to} - ${start}")
string(SUBSTRING "${page}" ${start} ${len} held)

if(NOT held STREQUAL table)
  message(
    FATAL_ERROR
      "the tables in docs/Formats.md are not what codec prints.\n"
      "regenerate it with configforge codec pairs --format markdown, and put the output between the markers\n"
      "--- page ---\n${held}--- codec ---\n${table}")
endif()
