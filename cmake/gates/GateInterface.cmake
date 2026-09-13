# The cost of including the library.
#
# The entire public interface currently adds about three milliseconds
# over an empty translation unit and preprocesses to roughly eleven
# hundred lines - about what one standard header costs. That is not an
# accident: the public headers reach for stddef, stdarg and stdint and
# nothing else.
#
# It is also one careless #include from being untrue, and the person who
# adds <stdio.h> to a public header will never see the cost. Every
# consumer of the library will. So it is a gate.

set(bad 0)

file(GLOB headers "${SRC}/include/configforge/*.h")
foreach(h ${headers})
  file(READ "${h}" text)
  string(REGEX MATCHALL "#[ \t]*include[ \t]*<[^>]+>" incs "${text}")
  foreach(inc ${incs})
    string(REGEX MATCH "<([^>]+)>" _m "${inc}")
    set(name "${CMAKE_MATCH_1}")
    # The allowed set, and the reason each one is in it:
    #   stddef  size_t, which is in every signature
    #   stdarg  the formatter's va_list
    #   stdint  exact-width types, where the toolchain has them
    if(NOT name MATCHES "^(stddef|stdarg|stdint)\\.h$")
      get_filename_component(basename "${h}" NAME)
      message("VIOLATION: public header ${basename} includes <${name}>")
      message("  every consumer of this library pays for that include")
      set(bad 1)
    endif()
  endforeach()
endforeach()

# And the whole interface, measured rather than assumed.
file(WRITE "${BIN}/iface.c"
     "#include <configforge/configforge.h>\nint main(void){return 0;}\n")
execute_process(
  COMMAND ${CC} -std=c89 -I "${SRC}/include" -E "${BIN}/iface.c"
  OUTPUT_VARIABLE pp
  ERROR_QUIET
  RESULT_VARIABLE rc)
if(rc EQUAL 0)
  string(REGEX MATCHALL "\n" nl "${pp}")
  list(LENGTH nl lines)
  # Generous: the point is to catch a jump, not to police a number.
  if(lines GREATER 4000)
    message("VIOLATION: the public interface preprocesses to ${lines} lines")
    message("  it was about 1100; something large was pulled in")
    set(bad 1)
  else()
    message("interface gate: ${lines} preprocessed lines")
  endif()
endif()

if(bad)
  message(FATAL_ERROR "interface gate failed")
endif()
