# What the library's stack frames cost, and whether they fit the budget.
#
#   cmake -DOBJDIR=build/CMakeFiles/configforge.dir -P scripts/stack.cmake
#   cmake -DOBJDIR=... -DBUDGET=1024 -P scripts/stack.cmake     would it fit?
#
# The build runs this after the library links (CF_STACK_BUDGET), and it
# can be run by hand on any build directory to ask "what if": the same
# measurements against another budget, without rebuilding.
#
# It reads the .su files the compiler writes with -fstack-usage - one
# line per function, the bytes its frame takes - so the figures are the
# compiler's, for this compiler, these flags and these limits, not an
# estimate kept in a document. And it reads each source's #if CF_WITH_*
# guards, so it can say which feature a frame belongs to: the stack a
# feature brings is the case for turning it off, and the answer to "what
# fits in a kilobyte" is a list of options rather than a guess.
#
# Inputs:
#   OBJDIR   where the .su files are (the library target's object dir)
#   BUDGET   bytes per function; 0 reports and checks nothing (default 0)
#   OUT      also write the full table here
#   LIB      the built library, removed when the budget is broken so the
#            next build links it again and checks again
#   TOP      how many of the largest frames to print (default 8)
#
# A frame is not the whole story. A thread's stack has to hold the
# deepest chain of calls, and the decoders recurse once per level of
# nesting, up to CF_MAX_DEPTH. The budget bounds each link of the chain;
# the length of the chain is the nesting limit's to bound.

cmake_minimum_required(VERSION 3.10)

if(NOT DEFINED OBJDIR)
  message(FATAL_ERROR "stack: OBJDIR is the library's object directory")
endif()
if(NOT DEFINED BUDGET)
  set(BUDGET 0)
endif()
if(NOT DEFINED TOP)
  set(TOP 8)
endif()

file(GLOB_RECURSE sus "${OBJDIR}/*.su")
if(NOT sus)
  message("stack: nothing measured (no .su files under ${OBJDIR})")
  return()
endif()

# The positive CF_WITH_* guards in force at each line of a file, as a
# list of "line:GUARDS" transitions. Brackets and semicolons are taken
# out first, because CMake would read them as list syntax.
function(guards_of file out)
  file(READ "${file}" text)
  string(REPLACE ";" " " text "${text}")
  string(REPLACE "[" "(" text "${text}")
  string(REPLACE "]" ")" text "${text}")
  string(REPLACE "\n" ";" lines "${text}")
  set(stack "")
  set(trans "")
  set(n 0)
  foreach(l IN LISTS lines)
    math(EXPR n "${n} + 1")
    if(NOT l MATCHES
       "^[ \t]*#[ \t]*(if|ifdef|ifndef|elif|else|endif)([^a-z_].*)?$")
      continue()
    endif()
    set(dir "${CMAKE_MATCH_1}")
    set(cond "${CMAKE_MATCH_2}")
    if(dir STREQUAL "if"
       OR dir STREQUAL "ifdef"
       OR dir STREQUAL "ifndef")
      if(dir STREQUAL "if" AND cond MATCHES
                               "^[ \t]+(CF_WITH_[A-Z0-9_]+)[ \t]*(/[*].*)?$")
        list(APPEND stack "${CMAKE_MATCH_1}")
      else()
        list(APPEND stack "-")
      endif()
    elseif(dir STREQUAL "elif" OR dir STREQUAL "else")
      list(LENGTH stack depth)
      if(depth GREATER 0)
        list(REMOVE_AT stack -1)
        list(APPEND stack "-")
      endif()
    else()
      list(LENGTH stack depth)
      if(depth GREATER 0)
        list(REMOVE_AT stack -1)
      endif()
    endif()
    set(g "")
    foreach(s IN LISTS stack)
      if(NOT s STREQUAL "-")
        if(g)
          string(APPEND g "+")
        endif()
        string(APPEND g "${s}")
      endif()
    endforeach()
    list(APPEND trans "${n}:${g}")
  endforeach()
  set(${out}
      "${trans}"
      PARENT_SCOPE)
endfunction()

# Each entry as "BYTES|function|file|line|qualifier", bytes zero-padded
# so that a string sort is a numeric one on every CMake version.
set(entries "")
set(unbounded "")
foreach(su IN LISTS sus)
  file(STRINGS "${su}" rows)
  foreach(r IN LISTS rows)
    # GCC and most clangs write file:line:column:function; FreeBSD's
    # clang 19 writes file:line:function, and reading only the first
    # broke the build there. Tried in that order, so a column is never
    # taken for a line.
    if(r MATCHES "^(.*):([0-9]+):([0-9]+):([^\t]+)\t([0-9]+)\t(.*)$")
      set(file "${CMAKE_MATCH_1}")
      set(line "${CMAKE_MATCH_2}")
      set(fn "${CMAKE_MATCH_4}")
      set(bytes "${CMAKE_MATCH_5}")
      set(qual "${CMAKE_MATCH_6}")
    elseif(r MATCHES "^(.*):([0-9]+):([^\t]+)\t([0-9]+)\t(.*)$")
      set(file "${CMAKE_MATCH_1}")
      set(line "${CMAKE_MATCH_2}")
      set(fn "${CMAKE_MATCH_3}")
      set(bytes "${CMAKE_MATCH_4}")
      set(qual "${CMAKE_MATCH_5}")
    else()
      continue()
    endif()
    string(LENGTH "${bytes}" len)
    set(padded "${bytes}")
    while(len LESS 8)
      set(padded "0${padded}")
      math(EXPR len "${len} + 1")
    endwhile()
    list(APPEND entries "${padded}|${fn}|${file}|${line}|${qual}")
    if(qual STREQUAL "dynamic")
      list(APPEND unbounded "${fn} (${file}:${line})")
    endif()
  endforeach()
endforeach()
list(SORT entries)
list(REVERSE entries)
list(LENGTH entries count)
if(count EQUAL 0)
  # Silence here would pass a build whose frames nobody measured.
  list(GET sus 0 first_su)
  file(STRINGS "${first_su}" first_rows LIMIT_COUNT 1)
  message(FATAL_ERROR "stack: .su files found, but no line in a format this "
                      "script reads; the first is: ${first_rows}")
endif()

# The guard of every entry, files read once each.
set(seen_files "")
set(table "")
set(by_guard "")
set(over "")
foreach(e IN LISTS entries)
  string(REPLACE "|" ";" f "${e}")
  list(GET f 0 padded)
  list(GET f 1 fn)
  list(GET f 2 file)
  list(GET f 3 line)
  math(EXPR bytes "${padded} + 0")
  string(MAKE_C_IDENTIFIER "${file}" id)
  if(NOT DEFINED trans_${id})
    if(EXISTS "${file}")
      guards_of("${file}" trans_${id})
    else()
      set(trans_${id} "")
    endif()
  endif()
  set(guard "")
  foreach(t IN LISTS trans_${id})
    string(REGEX MATCH "^([0-9]+):(.*)$" _ "${t}")
    if(CMAKE_MATCH_1 GREATER line)
      break()
    endif()
    set(guard "${CMAKE_MATCH_2}")
  endforeach()
  if(guard STREQUAL "")
    set(key "always built")
  else()
    set(key "${guard}")
  endif()
  # The first entry seen for a guard is its largest, entries being in
  # descending order.
  string(MAKE_C_IDENTIFIER "${key}" gid)
  if(NOT DEFINED guard_${gid})
    set(guard_${gid} "${bytes}")
    list(APPEND by_guard "${key}|${bytes}|${fn}")
  endif()
  get_filename_component(short "${file}" NAME)
  list(APPEND table "${bytes}|${fn}|${short}:${line}|${guard}")
  if(BUDGET GREATER 0 AND bytes GREATER BUDGET)
    if(guard STREQUAL "")
      set(cure "")
    else()
      set(cure " - gone with ${guard}=OFF")
    endif()
    list(APPEND over "${bytes} bytes  ${fn} (${short}:${line})${cure}")
  endif()
endforeach()

# Columns, for people.
function(pad value width out)
  set(v "${value}")
  string(LENGTH "${v}" len)
  while(len LESS width)
    string(APPEND v " ")
    math(EXPR len "${len} + 1")
  endwhile()
  set(${out}
      "${v}"
      PARENT_SCOPE)
endfunction()

# The same, padded on the left, for numbers.
function(lpad value width out)
  set(v "${value}")
  string(LENGTH "${v}" len)
  while(len LESS width)
    set(v " ${v}")
    math(EXPR len "${len} + 1")
  endwhile()
  set(${out}
      "${v}"
      PARENT_SCOPE)
endfunction()

if(BUDGET GREATER 0)
  set(head "stack: ${count} functions, budget ${BUDGET} bytes per function")
else()
  set(head "stack: ${count} functions, no budget set")
endif()
set(report "${head}\n\nlargest frames:\n")
set(i 0)
foreach(t IN LISTS table)
  if(NOT i LESS TOP)
    break()
  endif()
  string(REPLACE "|" ";" f "${t}")
  list(GET f 0 b)
  list(GET f 1 fn)
  list(GET f 2 at)
  list(GET f 3 g)
  lpad("${b}" 7 b)
  pad("${fn}" 28 fn)
  pad("${at}" 22 at)
  string(APPEND report "${b}  ${fn}${at}${g}\n")
  math(EXPR i "${i} + 1")
endforeach()
string(APPEND report "\nthe largest frame each feature brings:\n")
foreach(g IN LISTS by_guard)
  string(REPLACE "|" ";" f "${g}")
  list(GET f 0 k)
  list(GET f 1 b)
  list(GET f 2 fn)
  lpad("${b}" 7 b)
  pad("${k}" 24 k)
  string(APPEND report "${b}  ${k}${fn}\n")
endforeach()
string(
  APPEND report
  "\nFrames grow with CF_KEY_MAX, CF_MESSAGE_MAX and CF_PATH_MAX; a thread\n"
  "also needs room for the decoders' recursion, one level per CF_MAX_DEPTH.\n")

if(DEFINED OUT)
  set(full "${report}\nevery frame:\n")
  foreach(t IN LISTS table)
    string(REPLACE "|" ";" f "${t}")
    list(GET f 0 b)
    list(GET f 1 fn)
    list(GET f 2 at)
    list(GET f 3 g)
    lpad("${b}" 7 b)
    string(APPEND full "${b}  ${fn}  ${at}  ${g}\n")
  endforeach()
  file(WRITE "${OUT}" "${full}")
endif()

if(unbounded)
  string(REPLACE ";" "\n  " u "${unbounded}")
  message("${report}")
  if(DEFINED LIB)
    file(REMOVE "${LIB}")
  endif()
  message(FATAL_ERROR "stack: frames with no bound (a VLA or alloca):\n  ${u}")
endif()

if(over)
  list(LENGTH over nover)
  string(REPLACE ";" "\n  " o "${over}")
  message("${report}")
  if(DEFINED LIB)
    file(REMOVE "${LIB}")
  endif()
  message(
    FATAL_ERROR
      "stack: ${nover} function(s) over the budget of ${BUDGET} bytes:\n"
      "  ${o}\n"
      "The choices: raise CF_STACK_BUDGET, lower CF_KEY_MAX, CF_MESSAGE_MAX or "
      "CF_PATH_MAX, or turn off a feature named above. Another budget can be "
      "tried without building: "
      "cmake -DOBJDIR=<dir> -DBUDGET=<n> -P scripts/stack.cmake")
endif()

list(GET table 0 first)
string(REPLACE "|" ";" f "${first}")
list(GET f 0 b)
list(GET f 1 fn)
if(BUDGET GREATER 0)
  message("stack: largest frame ${b} bytes (${fn}), within ${BUDGET}")
else()
  message("${report}")
endif()
