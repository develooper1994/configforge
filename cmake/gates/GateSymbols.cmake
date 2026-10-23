# Two checks over the built library:
#   forbidden imports  - no locale-sensitive C library function
#   symbol prefixes    - every export begins with cf_ or CF_
find_program(NM_EXE nm)
if(NOT NM_EXE)
  message("symbol gate: nm not found, skipped")
  return()
endif()

set(FORBIDDEN
    strtod
    atof
    atoi
    atol
    strtol
    strtoul
    tolower
    toupper
    isalpha
    isdigit
    isspace
    isalnum
    isupper
    islower
    strcoll
    strxfrm
    localeconv
    setlocale
    sprintf
    snprintf
    vsprintf
    vsnprintf
    printf
    fprintf)

execute_process(
  COMMAND ${NM_EXE} -u "${LIB}"
  OUTPUT_VARIABLE undef
  ERROR_QUIET)
set(bad 0)
foreach(f ${FORBIDDEN})
  if(undef MATCHES "[ \t]U[ \t]+${f}\n")
    message("VIOLATION: library references ${f}")
    set(bad 1)
  endif()
endforeach()

execute_process(
  COMMAND ${NM_EXE} -g --defined-only "${LIB}"
  OUTPUT_VARIABLE defs
  ERROR_QUIET)
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

# No mutable global state.
#
# A static pool inside a source file is global mutable state: not
# reentrant, not thread-safe, and a direct breach of a stated
# principle - which is exactly the mistake the memory source made
# before anything checked.
#
# The test is the section, not nm's letter. Under PIE a `const` struct
# holding pointers lands in .data.rel.ro and nm calls it `d`, so a gate
# reading the letter would fail on correct code and teach everyone to
# ignore it. .data and .bss are the mutable ones.
#
# Not under a sanitiser. AddressSanitizer instruments globals, which
# moves `const` data out of .data.rel.ro into .data and adds its own
# __odr_asan descriptors - so the measurement stops meaning what it
# means everywhere else. A gate that fires on correct code teaches
# people to ignore it, which is worse than not having the gate.
#
# Not under the Tiny C Compiler either, which puts `const` data - string
# literals, constant tables - in .data rather than .rodata, so every one
# of them would read as mutable. That is where tcc puts things, not what
# the code is, and the same sources pass this check under GCC and Clang.
find_program(OBJDUMP_EXE objdump)
if(COMPILER STREQUAL "TinyCC")
  message("symbol gate: mutable-global check skipped: tcc places const "
          "data in .data")
elseif(OBJDUMP_EXE AND NOT SANITIZE)
  execute_process(
    COMMAND ${OBJDUMP_EXE} -t "${LIB}"
    OUTPUT_VARIABLE syms
    ERROR_QUIET)
  string(REPLACE "\n" ";" symlines "${syms}")
  foreach(line ${symlines})
    if(line MATCHES "[ \t]O[ \t]+(\\.data|\\.bss)[ \t]")
      string(REGEX MATCH "[^ \t]+$" sym "${line}")
      message("VIOLATION: mutable global '${sym}'")
      set(bad 1)
    endif()
  endforeach()
endif()

# Only the allocator may call the allocator.
#
# Every allocation goes through cf_allocator so that a caller can supply
# their own and the fixed-capacity profile can refuse to allocate at
# all. One file reaching past it would make both untrue quietly.
execute_process(
  COMMAND ${NM_EXE} -u "${LIB}"
  OUTPUT_VARIABLE undefs
  ERROR_QUIET)
string(REPLACE "\n" ";" ulines "${undefs}")
set(member "")
foreach(line ${ulines})
  if(line MATCHES "^(.+\\.o):$")
    set(member "${CMAKE_MATCH_1}")
  elseif(line MATCHES "[ \t]U[ \t]+(malloc|free|calloc|realloc)$")
    if(NOT member STREQUAL "alloc.c.o")
      message("VIOLATION: ${member} calls ${CMAKE_MATCH_1} directly")
      set(bad 1)
    endif()
  endif()
endforeach()

if(bad)
  message(FATAL_ERROR "symbol gate failed")
endif()
message("symbol gate: clean")
