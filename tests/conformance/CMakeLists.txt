# The program the conformance run (scripts/conformance.py, verify's
# EXTRA=conformance) puts each suite document through. Built with the
# tests, so it compiles on every preset; it is run only by that step,
# which needs the suites and a network to fetch them.
add_executable(conform conform.c)
target_link_libraries(conform PRIVATE configforge)
set_target_properties(
  conform
  PROPERTIES C_STANDARD 90
             C_STANDARD_REQUIRED ON
             C_EXTENSIONS OFF)
configforge_apply_warnings(conform)
configforge_apply_analysis(conform)
