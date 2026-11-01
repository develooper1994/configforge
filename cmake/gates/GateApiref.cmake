# docs/API.md says what the headers say.
#
# The reference is generated from the header comments by
# scripts/apiref.cmake. This regenerates it into the build directory and
# compares: a function added, removed or re-documented without running
# the script is a reference that has started to lie, and this is where
# it stops. Line endings are ignored, because a Windows checkout may
# convert them and the words are what matter.

cmake_minimum_required(VERSION 3.10)

set(fresh "${BIN}/API.md")
execute_process(
  COMMAND ${CMAKE_COMMAND} -DOUT=${fresh} -P ${SRC}/scripts/apiref.cmake
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE out)
if(NOT rc EQUAL 0)
  message("${out}")
  message(FATAL_ERROR "the API reference could not be generated")
endif()

file(READ "${fresh}" want)
file(READ "${SRC}/docs/API.md" have)
string(REPLACE "\r" "" want "${want}")
string(REPLACE "\r" "" have "${have}")
if(NOT want STREQUAL have)
  message("VIOLATION: docs/API.md does not match the headers; "
          "run `cmake -P scripts/apiref.cmake` and commit the result")
  message(FATAL_ERROR "API reference gate failed")
endif()

# A link must open the named implementation, not merely an existing file.
set(link_pattern "\\[`cf_[A-Za-z0-9_]+` \\([^)]*\\)\\]\\(")
string(APPEND link_pattern "https://github.com/develooper1994/configforge/"
       "blob/main/[^#]+#L[0-9]+\\)")
string(REGEX MATCHALL "${link_pattern}" links "${want}")
set(linked "")
foreach(link IN LISTS links)
  string(REGEX MATCH "\\[`(cf_[A-Za-z0-9_]+)` \\(([^)]*)\\)\\].*#L([0-9]+)"
               parts "${link}")
  set(symbol "${CMAKE_MATCH_1}")
  list(APPEND linked "${symbol}")
  set(source "${CMAKE_MATCH_2}")
  math(EXPR index "${CMAKE_MATCH_3} - 1")
  string(MD5 id "${source}")
  if(NOT DEFINED source_lines_${id})
    file(READ "${SRC}/${source}" text)
    string(REPLACE "\\" "<BACKSLASH>" text "${text}")
    string(REPLACE ";" "<SEMI>" text "${text}")
    string(REPLACE "[" "<LB>" text "${text}")
    string(REPLACE "]" "<RB>" text "${text}")
    string(REPLACE "\n" ";" source_lines_${id} "${text}")
  endif()
  list(LENGTH source_lines_${id} count)
  if(index LESS 0 OR index GREATER_EQUAL count)
    message(FATAL_ERROR "invalid implementation line in ${link}")
  endif()
  list(GET source_lines_${id} ${index} line)
  if(NOT line MATCHES "(^|[ \t*])${symbol}[ \t]*\\(")
    message(FATAL_ERROR "implementation link does not name ${symbol}: ${link}\n"
                        "target line: ${line}")
  endif()
endforeach()
string(REGEX MATCHALL "CF_(DOCTOR_)?API[^\n;]*cf_[A-Za-z0-9_]+[ \t]*\\("
             declarations "${want}")
foreach(declaration IN LISTS declarations)
  string(REGEX MATCH "(cf_[A-Za-z0-9_]+)[ \t]*\\(" match "${declaration}")
  set(symbol "${CMAKE_MATCH_1}")
  if(NOT symbol IN_LIST linked)
    message(FATAL_ERROR "API declaration has no implementation link: ${symbol}")
  endif()
endforeach()
list(LENGTH links count)
message("API implementation links: ${count} checked")
message("API reference gate: clean")
