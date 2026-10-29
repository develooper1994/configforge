# The symbol list: the library's public interface, written down.
#
#   cmake -DSRC=<repo> [-DLIB=<library>] [-DWITH_REAL=ON] -P GateAbi.cmake
#   cmake -DSRC=<repo> -DWRITE=ON -P GateAbi.cmake      rewrite the list
#
# cmake/configforge.symbols names every CF_API function, one a line, and
# "name real" for one that exists only with CF_WITH_REAL. The list is
# what a program built against this library may call, so a change to it
# is a change to the ABI, and it shows in a diff as one - which a header
# edit does not. Three checks:
#
#   headers   every CF_API declaration is listed, and every listed name
#             is declared: adding one is a minor version, removing one a
#             major version, and neither happens by accident.
#   library   every listed name is defined in the built library.
#   shared    a shared library exports nothing else: -fvisibility=hidden
#             is what keeps the internals internal, and this checks it.
#
# Doctor's CF_DOCTOR_API is not here: libconfigforge_doctor is static
# (D-41), so there is one ABI to keep, and this is it.
cmake_minimum_required(VERSION 3.10)

set(list_file "${SRC}/cmake/configforge.symbols")

# Every CF_API name in the headers, "name" or "name real".
file(GLOB headers "${SRC}/include/configforge/*.h")
list(SORT headers)
set(declared "")
foreach(h ${headers})
  file(STRINGS "${h}" lines)
  set(real FALSE)
  foreach(l IN LISTS lines)
    if(l MATCHES "^#if CF_WITH_REAL")
      set(real TRUE)
    elseif(l MATCHES "^#endif")
      set(real FALSE)
    elseif(l MATCHES "^CF_API[ \t].*[ \t*]([a-z_][a-z0-9_]*)\\(")
      if(real)
        list(APPEND declared "${CMAKE_MATCH_1} real")
      else()
        list(APPEND declared "${CMAKE_MATCH_1}")
      endif()
    endif()
  endforeach()
endforeach()
list(SORT declared)

if(WRITE)
  string(REPLACE ";" "\n" body "${declared}")
  file(
    WRITE "${list_file}"
    "# The public interface of libconfigforge: every CF_API function.\n"
    "# Checked by gate.abi against the headers and the built library;\n"
    "# rewrite with cmake -DSRC=. -DWRITE=ON -P cmake/gates/GateAbi.cmake.\n"
    "# A line added is a minor version, a line removed a major one.\n"
    "${body}\n")
  list(LENGTH declared n)
  message("abi: ${n} symbols written to ${list_file}")
  return()
endif()

file(STRINGS "${list_file}" listed REGEX "^[a-z_]")
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
    message("VIOLATION: '${s}' is in cmake/configforge.symbols and declared "
            "nowhere: removing a public function breaks every program "
            "built against it")
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
  foreach(s ${listed})
    string(REGEX REPLACE " real$" "" name "${s}")
    if(s MATCHES " real$" AND NOT WITH_REAL)
      continue()
    endif()
    list(APPEND names "${name}")
    if(NOT name IN_LIST defined)
      message("VIOLATION: '${name}' is public and not defined in ${LIB}")
      set(bad 1)
    endif()
  endforeach()
  if(LIB MATCHES "\\.(so|dylib)(\\.[0-9.]+)?$")
    foreach(d ${defined})
      if(NOT d IN_LIST names)
        message("VIOLATION: '${d}' is exported by the shared library and is "
                "not public: give it CF_API and list it, or keep it hidden")
        set(bad 1)
      endif()
    endforeach()
  endif()
endif()

if(bad)
  message(FATAL_ERROR "the symbol list and the interface disagree")
endif()
