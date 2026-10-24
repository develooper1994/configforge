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

# A clean report exits 0, and one with a finding at the --fail-on level 1.
# The exit code is the interface a CI job uses, so it is what gets
# asserted.
doctor_case(lint_clean 0 lint|cf_fx_staging.json)
# A dead source is a warning by default: a system file wholly overridden
# by a user's is often intended. It fails a run that asks for warnings.
doctor_case(lint_dead 0 lint|cf_fx_dead.json)
doctor_case(lint_dead_strict 1 lint|cf_fx_dead.json|--fail-on|warning)
doctor_case(dump 0 dump|cf_fx_prod.json)
doctor_case(explain 0 explain|cf_fx_prod.json|server.port)
doctor_case(explain_missing 2 explain|cf_fx_prod.json|no.such.key)
doctor_case(diff 0 diff|cf_fx_prod.json|cf_fx_staging.json)
doctor_case(lint_policy 1 lint|cf_fx_leak.json)
doctor_case(lint_rules 0 lint|cf_fx_rules.json)
doctor_case(lint_twice 1 lint|cf_fx_twice.json)
doctor_case(lint_sources 1 lint|cf_fx_sources.json)
doctor_case(lint_files_secret 0 lint|cf_fx_files.json|--fail-on|never)
doctor_case(not_a_report 2 dump|cf_fx_staging.ini)

# The version Doctor reports is the one cf_version.h says, with the
# commit after it when the build knew one.
add_test(NAME doctor.version COMMAND doctor --version)
set_tests_properties(
  doctor.version
  PROPERTIES
    PASS_REGULAR_EXPRESSION
    "^ConfigForge Doctor ${PROJECT_VERSION}( \\([0-9a-f]+(-dirty)?\\))?\n$")

# A baseline turns a failing run into a passing one and reports what no
# longer matches. Written by the test rather than checked in, so it
# cannot drift away from the fixture it refers to.
add_test(NAME doctor.baseline
         COMMAND ${CMAKE_COMMAND} -DDOCTOR=$<TARGET_FILE:doctor> -P
                 ${CMAKE_SOURCE_DIR}/tests/doctor/baseline.cmake)
set_tests_properties(doctor.baseline PROPERTIES FIXTURES_REQUIRED
                                                DOCTOR_REPORTS)

# The git config adapter, the first of the foreign ones. It writes its
# own input, so it needs no fixture.
add_test(NAME doctor.git
         COMMAND ${CMAKE_COMMAND} -DDOCTOR=$<TARGET_FILE:doctor> -P
                 ${CMAKE_SOURCE_DIR}/tests/doctor/git.cmake)

# Spring Boot's /actuator/env and .NET's GetDebugView(), the same way.
foreach(adapter spring dotnet)
  add_test(NAME doctor.${adapter}
           COMMAND ${CMAKE_COMMAND} -DDOCTOR=$<TARGET_FILE:doctor> -P
                   ${CMAKE_SOURCE_DIR}/tests/doctor/${adapter}.cmake)
endforeach()

# Severity a project chose, severity a profile implies, and SARIF.
add_test(NAME doctor.severity
         COMMAND ${CMAKE_COMMAND} -DDOCTOR=$<TARGET_FILE:doctor> -P
                 ${CMAKE_SOURCE_DIR}/tests/doctor/severity.cmake)
set_tests_properties(doctor.severity PROPERTIES FIXTURES_REQUIRED
                                                DOCTOR_REPORTS)

# Doctor as a library (cf_doctor.h): a program linting itself, in
# process, with no report on disk.
add_executable(doctor_api test_api.c)
target_link_libraries(doctor_api PRIVATE configforge_doctor)
set_target_properties(
  doctor_api
  PROPERTIES C_STANDARD 90
             C_STANDARD_REQUIRED ON
             C_EXTENSIONS OFF)
configforge_apply_warnings(doctor_api)
configforge_apply_analysis(doctor_api)
add_test(NAME doctor.api COMMAND doctor_api)
