# The front door, `configforge <command>`: its own command line, and the
# programs it runs - Doctor, and one written here that only the PATH
# finds. front.cmake holds the cases.
set(have_doctor OFF)
set(have_gen OFF)
set(have_codec OFF)
set(front_cwd ${CMAKE_CURRENT_BINARY_DIR})
if(TARGET configforge_codec)
  set(have_codec ON)
endif()
if(TARGET doctor)
  set(have_doctor ON)
  set(have_gen ON)
  # Where Doctor's fixtures are written.
  set(front_cwd ${CMAKE_BINARY_DIR}/tests/doctor)
endif()
add_test(
  NAME front.run
  COMMAND
    ${CMAKE_COMMAND} -DFRONT=$<TARGET_FILE:configforge_front>
    -DHAVE_DOCTOR=${have_doctor} -DHAVE_GEN=${have_gen}
    -DHAVE_CODEC=${have_codec} -DUNIX_HOST=${CMAKE_HOST_UNIX}
    -DWORK=${CMAKE_CURRENT_BINARY_DIR}/work -P
    ${CMAKE_CURRENT_SOURCE_DIR}/front.cmake
  WORKING_DIRECTORY ${front_cwd})
if(have_doctor)
  set_tests_properties(front.run PROPERTIES FIXTURES_REQUIRED DOCTOR_REPORTS)
endif()

if(TARGET configforge_front AND CF_WITH_JSON)
  add_test(
    NAME front.diff
    COMMAND
      ${CMAKE_COMMAND} -DFRONT=$<TARGET_FILE:configforge_front>
      -DHAVE_YAML=${CF_WITH_YAML} -DWORK=${CMAKE_CURRENT_BINARY_DIR}/diff -P
      ${CMAKE_CURRENT_SOURCE_DIR}/diff.cmake)
endif()
