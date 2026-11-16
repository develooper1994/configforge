# The symbol lists: the library's exported interface, written down.
#
#   cmake -DSRC=<repo> [-DLIB=<library>] [-DWITH_REAL=ON] -P GateAbi.cmake
#   cmake -DSRC=<repo> -DWRITE=ON -P GateAbi.cmake      rewrite the lists
#
# cmake/configforge.symbols names every promised CF_API function, one a
# line, and "name real" for one that exists only with CF_WITH_REAL. The
# list is what a program built against this library may call, so a change
# to it is a change to the ABI, and it shows in a diff as one - which a
# header edit does not. cmake/configforge.internal.symbols, in the same
# format, names the internal tier (D-51, D-88): functions exported from
# the library and declared only under CF_INTERNAL_API, outside the 1.x
# promise. A line added or removed there is no version at all.
# cmake/configforge.experimental.symbols names the experimental tier (D-89):
# functions declared CF_EXPERIMENTAL(major, minor), exported, outside the
# promise for the minor release they came in, one line "name major.minor"
# ("name major.minor real" under CF_WITH_REAL). Three checks:
#
#   headers   every CF_API declaration is listed in the list its place in
#             the header says - promised, or inside #if CF_INTERNAL_API,
#             internal - and every listed name is declared: adding a
#             promised one is a minor version, removing one a major
#             version, and neither happens by accident. A name in both
#             lists is a violation. Every CF_EXPERIMENTAL declaration is
#             listed with its release, and a release below the current
#             major and minor is a violation: the next minor makes the
#             function promised or removes it.
#   library   every listed name, of all three lists, is defined in the
#             built library.
#   shared    a shared library exports nothing else: -fvisibility=hidden
#             is what keeps the internals internal, and this checks it.
#
# Doctor's CF_DOCTOR_API is not here: libconfigforge_doctor is static
# (D-41), so there is one ABI to keep, and this is it.
cmake_minimum_required(VERSION 3.10)

# Refused rather than guessed: a shell that splits -DSRC=. from its value
# (PowerShell does) left SRC empty, and the list was written to
# /cmake/configforge.symbols at the root of the drive, twice.
if(NOT DEFINED SRC OR NOT EXISTS "${SRC}/include/configforge/configforge.h")
  message(FATAL_ERROR "gate.abi: SRC must name the repository (-DSRC=<repo>), "
                      "not \"${SRC}\"")
endif()

set(list_file "${SRC}/cmake/configforge.symbols")
set(internal_file "${SRC}/cmake/configforge.internal.symbols")
set(experimental_file "${SRC}/cmake/configforge.experimental.symbols")
include("${SRC}/cmake/ConfigForgeVersion.cmake")

# Every CF_API name in the headers, "name" or "name real": the promised
# ones in `declared`, the ones inside #if CF_INTERNAL_API in
# `declared_internal`.
file(GLOB headers "${SRC}/include/configforge/*.h")
list(SORT headers)
set(declared "")
set(declared_internal "")
set(declared_experimental "")
foreach(h ${headers})
  # Comments can contain unmatched brackets, which join CMake list
  # elements. Only declarations and the relevant directives are tokens.
  file(
    STRINGS "${h}" lines
    REGEX
      "^(#[ \t]*(if|ifdef|ifndef|endif)|(CF_NODISCARD[ \t]+)?(CF_API[ \t]|CF_EXPERIMENTAL\\())"
  )
  # What the open conditionals are: "real" for #if CF_WITH_REAL,
  # "internal" for #if CF_INTERNAL_API, "other" for any other. #else and
  # #elif are not read: a fenced block holds neither.
  set(stack "")
  foreach(l IN LISTS lines)
    if(l MATCHES "^#[ \t]*endif")
      list(LENGTH stack depth)
      if(depth GREATER 0)
        math(EXPR last "${depth} - 1")
        list(REMOVE_AT stack ${last})
      endif()
    elseif(l MATCHES "^#[ \t]*if[ \t]+CF_WITH_REAL([^A-Za-z0-9_]|$)")
      list(APPEND stack "real")
    elseif(l MATCHES "^#[ \t]*if[ \t]+CF_INTERNAL_API([^A-Za-z0-9_]|$)")
      list(APPEND stack "internal")
    elseif(l MATCHES "^#[ \t]*if")
      list(APPEND stack "other")
    elseif(
      l MATCHES
      "^(CF_NODISCARD[ \t]+)?CF_EXPERIMENTAL\\(([0-9]+), *([0-9]+)\\)[ \t].*[ \t*]([a-z_][a-z0-9_]*)\\("
    )
      set(name "${CMAKE_MATCH_4} ${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")
      if("real" IN_LIST stack)
        set(name "${name} real")
      endif()
      list(APPEND declared_experimental "${name}")
    elseif(l MATCHES
           "^(CF_NODISCARD[ \t]+)?CF_API[ \t].*[ \t*]([a-z_][a-z0-9_]*)\\(")
      set(name "${CMAKE_MATCH_2}")
      if("real" IN_LIST stack)
        set(name "${name} real")
      endif()
      if("internal" IN_LIST stack)
        list(APPEND declared_internal "${name}")
      else()
        list(APPEND declared "${name}")
      endif()
    endif()
  endforeach()
endforeach()
list(SORT declared)
list(SORT declared_internal)
list(SORT declared_experimental)

if(WRITE)

  string(REPLACE ";" "\n" body "${declared}")
  set(text "# The public interface of libconfigforge: every CF_API function.\n")
  string(APPEND text
         "# Checked by gate.abi against the headers and the built library;\n")
  string(APPEND text "# rewrite with cmake -DSRC=. -DWRITE=ON -P "
         "cmake/gates/GateAbi.cmake.\n")
  string(
    APPEND
    text
    "# A line added is a minor version, a line removed a major "
    "one; a name moved to configforge.internal.symbols is neither "
    "(D-51).\n")
  string(APPEND text "${body}\n")
  # LF on every system: file(WRITE) writes CRLF on Windows.
  configforge_write_lf("${list_file}" "${text}")
  list(LENGTH declared n)
  message("abi: ${n} symbols written to ${list_file}")

  string(REPLACE ";" "\n" body "${declared_internal}")
  set(text "# The internal tier of libconfigforge: exported, outside the 1.x ")
  string(APPEND text "promise (D-51).\n")
  string(APPEND text
         "# Checked by gate.abi against the headers and the built library;\n")
  string(APPEND text "# rewrite with cmake -DSRC=. -DWRITE=ON -P "
         "cmake/gates/GateAbi.cmake.\n")
  string(APPEND text "# A line added or removed here is no version at all.\n")
  string(APPEND text "${body}\n")
  configforge_write_lf("${internal_file}" "${text}")
  list(LENGTH declared_internal n)
  message("abi: ${n} internal symbols written to ${internal_file}")

  string(REPLACE ";" "\n" body "${declared_experimental}")
  set(text "# Experimental: exported, outside the 1.x promise until promoted ")
  string(APPEND text "or removed.\n")
  string(APPEND text
         "# Checked by gate.abi against the headers and the built library;\n")
  string(APPEND text "# rewrite with cmake -DSRC=. -DWRITE=ON -P "
         "cmake/gates/GateAbi.cmake.\n")
  string(APPEND text "# One line a name, \"name major.minor\": the release it "
         "came in, which the next minor makes promised or removes (D-89).\n")
  string(APPEND text "${body}\n")
  configforge_write_lf("${experimental_file}" "${text}")
  list(LENGTH declared_experimental n)
  message("abi: ${n} experimental symbols written to ${experimental_file}")
  return()
endif()

file(STRINGS "${list_file}" listed REGEX "^[a-z_]")
file(STRINGS "${internal_file}" listed_internal REGEX "^[a-z_]")
file(STRINGS "${experimental_file}" listed_experimental REGEX "^[a-z_]")
set(bad 0)
foreach(d ${declared})
  if(NOT d IN_LIST listed)
    message("VIOLATION: '${d}' is declared CF_API and not in "
            "cmake/configforge.symbols: add it, as a new public symbol")
    set(bad 1)
  endif()
endforeach()
foreach(s ${listed})
  if(NOT s IN_LIST declared)
    message("VIOLATION: '${s}' is in cmake/configforge.symbols and not "
            "declared as a promised function: removing a public function "
            "breaks every program built against it")
    set(bad 1)
  endif()
endforeach()
foreach(d ${declared_internal})
  if(NOT d IN_LIST listed_internal)
    message("VIOLATION: '${d}' is declared CF_API under CF_INTERNAL_API and "
            "not in cmake/configforge.internal.symbols: add it")
    set(bad 1)
  endif()
endforeach()
foreach(s ${listed_internal})
  if(NOT s IN_LIST declared_internal)
    message("VIOLATION: '${s}' is in cmake/configforge.internal.symbols and "
            "not declared under CF_INTERNAL_API")
    set(bad 1)
  endif()
endforeach()
foreach(s ${listed})
  if(s IN_LIST listed_internal)
    message("VIOLATION: '${s}' is in both lists")
    set(bad 1)
  endif()
endforeach()

# The experimental tier: the list equals the declarations, no name is in
# another list, and none is older than the current minor release.
configforge_read_version(current)
string(REGEX MATCH "^([0-9]+)\\.([0-9]+)" ignored "${current}")
set(current_major "${CMAKE_MATCH_1}")
set(current_minor "${CMAKE_MATCH_2}")
# The same names as "name" or "name real", for the library check.
set(listed_experimental_plain "")
foreach(d ${declared_experimental})
  if(NOT d IN_LIST listed_experimental)
    message("VIOLATION: '${d}' is declared CF_EXPERIMENTAL and not in "
            "cmake/configforge.experimental.symbols: add it, as "
            "\"name major.minor\"")
    set(bad 1)
  endif()
endforeach()
foreach(s ${listed_experimental})
  if(NOT s MATCHES "^([a-z_][a-z0-9_]*) ([0-9]+)\\.([0-9]+)( real)?$")
    message("VIOLATION: '${s}' in cmake/configforge.experimental.symbols is "
            "not \"name major.minor\"")
    set(bad 1)
    continue()
  endif()
  set(name "${CMAKE_MATCH_1}")
  set(since_major "${CMAKE_MATCH_2}")
  set(since_minor "${CMAKE_MATCH_3}")
  list(APPEND listed_experimental_plain "${name}${CMAKE_MATCH_4}")
  if(NOT s IN_LIST declared_experimental)
    message("VIOLATION: '${s}' is in cmake/configforge.experimental.symbols "
            "and not declared CF_EXPERIMENTAL with that release")
    set(bad 1)
  endif()
  if("${name}" IN_LIST listed OR "${name}" IN_LIST listed_internal)
    message("VIOLATION: '${name}' is experimental and in another list")
    set(bad 1)
  endif()
  if(since_major LESS current_major
     OR (since_major EQUAL current_major AND since_minor LESS current_minor))
    message("VIOLATION: '${name}' has been experimental since "
            "${since_major}.${since_minor}: make it CF_API or remove it "
            "(D-89)")
    set(bad 1)
  endif()
endforeach()

# The library, where there is an nm to read it with.
find_program(NM_EXE nm)
if(LIB AND NM_EXE)
  execute_process(
    COMMAND ${NM_EXE} -g --defined-only "${LIB}"
    OUTPUT_VARIABLE defs
    ERROR_QUIET)
  string(REPLACE "\n" ";" deflines "${defs}")
  set(defined "")
  foreach(line ${deflines})
    if(line MATCHES "[0-9a-fA-F]+[ \t]+[TDRB][ \t]+_?(cf_[A-Za-z0-9_]*)$")
      list(APPEND defined "${CMAKE_MATCH_1}")
    endif()
  endforeach()
  set(names "")
  foreach(s ${listed} ${listed_internal} ${listed_experimental_plain})
    string(REGEX REPLACE " real$" "" name "${s}")
    if(s MATCHES " real$" AND NOT WITH_REAL)
      continue()
    endif()
    list(APPEND names "${name}")
    if(NOT name IN_LIST defined)
      message("VIOLATION: '${name}' is listed and not defined in ${LIB}")
      set(bad 1)
    endif()
  endforeach()
  if(LIB MATCHES "\\.(so|dylib)(\\.[0-9.]+)?$")
    foreach(d ${defined})
      if(NOT d IN_LIST names)
        message("VIOLATION: '${d}' is exported by the shared library and is "
                "in no list: give it CF_API and list it, or keep it "
                "hidden")
        set(bad 1)
      endif()
    endforeach()
  endif()
endif()

if(bad)
  message(FATAL_ERROR "the symbol lists and the interface disagree")
endif()
