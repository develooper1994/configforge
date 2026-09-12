# Two checks over the built library:
#   forbidden imports  - no locale-sensitive C library function
#   symbol prefixes    - every export begins with cf_ or CF_
find_program(NM_EXE nm)
if(NOT NM_EXE)
  message("symbol gate: nm not found, skipped")
  return()
endif()

set(FORBIDDEN strtod atof atoi atol strtol strtoul
              tolower toupper isalpha isdigit isspace isalnum isupper islower
              strcoll strxfrm localeconv setlocale
              sprintf snprintf vsprintf vsnprintf printf fprintf)

execute_process(COMMAND ${NM_EXE} -u "${LIB}" OUTPUT_VARIABLE undef ERROR_QUIET)
set(bad 0)
foreach(f ${FORBIDDEN})
  if(undef MATCHES "[ \t]U[ \t]+${f}\n")
    message("VIOLATION: library references ${f}")
    set(bad 1)
  endif()
endforeach()

execute_process(COMMAND ${NM_EXE} -g --defined-only "${LIB}"
                OUTPUT_VARIABLE defs ERROR_QUIET)
string(REPLACE "\n" ";" lines "${defs}")
foreach(line ${lines})
  if(line MATCHES "[0-9a-fA-F]+[ \t]+[TDBRW][ \t]+([A-Za-z_][A-Za-z0-9_]*)")
    set(sym "${CMAKE_MATCH_1}")
    if(NOT sym MATCHES "^(cf_|CF_|_)")
      message("VIOLATION: unprefixed export ${sym}")
      set(bad 1)
    endif()
  endif()
endforeach()

if(bad)
  message(FATAL_ERROR "symbol gate failed")
endif()
message("symbol gate: clean")
