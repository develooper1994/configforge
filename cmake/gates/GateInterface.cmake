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

# What the public headers include - stddef, stdarg and stdint, and nothing
# else - is gate.semgrep's rule cf-public-header-include: a rule in source,
# with a fixture, that names the line.

# And the whole interface, measured rather than assumed.
file(WRITE "${BIN}/iface.c"
     "#include <configforge/configforge.h>\nint main(void){return 0;}\n")
execute_process(
  COMMAND ${CC} -std=c89 -I "${BIN}/staged/include" -E "${BIN}/iface.c"
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
