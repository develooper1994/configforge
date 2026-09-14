add_executable(doctor_fixtures fixtures.c)
target_link_libraries(doctor_fixtures PRIVATE configforge)
set_target_properties(
  doctor_fixtures
  PROPERTIES C_STANDARD 90
             C_STANDARD_REQUIRED ON
             C_EXTENSIONS OFF)
configforge_apply_warnings(doctor_fixtures)
# Without this the target links a sanitised library without the
# sanitiser runtime, which fails at link time rather than at run time -
# confusingly, because the message names the library and not the
# target that forgot.
configforge_apply_analysis(doctor_fixtures)

# The fixtures are generated first; every later test depends on them
# through a fixture dependency so the order is a statement rather than
# a hope about how ctest happens to schedule.
add_test(NAME doctor.fixtures COMMAND doctor_fixtures)
set_tests_properties(doctor.fixtures PROPERTIES FIXTURES_SETUP DOCTOR_REPORTS)

# Register one Doctor invocation as a test.
#
# ~~~
# doctor_case(<name> <expected-exit-code> <arg>...)
# ~~~
function(doctor_case name expect_fail)
  add_test(
    NAME doctor.${name}
    COMMAND
      ${CMAKE_COMMAND} -DDOCTOR=$<TARGET_FILE:doctor> -DARGS=${ARGN}
      -DEXPECT_FAIL=${expect_fail} -P
      ${CMAKE_SOURCE_DIR}/tests/doctor/run.cmake)
  set_tests_properties(doctor.${name} PROPERTIES FIXTURES_REQUIRED
                                                 DOCTOR_REPORTS)
endfunction()

# A clean report exits 0; a source that wins nothing exits 1. The exit
# code is the interface a CI job uses, so it is what gets asserted.
doctor_case(lint_clean 0 lint|cf_fx_staging.json)
doctor_case(lint_dead 1 lint|cf_fx_dead.json)
doctor_case(dump 0 dump|cf_fx_prod.json)
doctor_case(explain 0 explain|cf_fx_prod.json|server.port)
doctor_case(explain_missing 2 explain|cf_fx_prod.json|no.such.key)
doctor_case(diff 0 diff|cf_fx_prod.json|cf_fx_staging.json)
doctor_case(lint_policy 1 lint|cf_fx_leak.json)
doctor_case(lint_rules 0 lint|cf_fx_rules.json)
doctor_case(not_a_report 2 dump|cf_fx_staging.ini)

# A baseline turns a failing run into a passing one and reports what no
# longer matches. Written by the test rather than checked in, so it
# cannot drift away from the fixture it refers to.
add_test(NAME doctor.baseline
         COMMAND ${CMAKE_COMMAND} -DDOCTOR=$<TARGET_FILE:doctor> -P
                 ${CMAKE_SOURCE_DIR}/tests/doctor/baseline.cmake)
set_tests_properties(doctor.baseline PROPERTIES FIXTURES_REQUIRED
                                                DOCTOR_REPORTS)
