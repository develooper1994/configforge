cf_unit_test_executable(test_profile configforge)

add_test(NAME profile.${CF_BUILD_PROFILE_NAME}
         COMMAND test_profile "${CF_BUILD_PROFILE_NAME}")

function(cf_cversion_case name pattern)
  add_test(
    NAME profile.cversion.${name}
    COMMAND
      "${CMAKE_COMMAND}" -S "${CMAKE_CURRENT_SOURCE_DIR}/cversion" -B
      "${CMAKE_CURRENT_BINARY_DIR}/${name}" -G "${CMAKE_GENERATOR}"
      "-DSRC=${CMAKE_SOURCE_DIR}" "-DCASE=${name}"
      "-DCF_C_ROOT=${CMAKE_CURRENT_BINARY_DIR}/${name}" ${ARGN})
  set_tests_properties(profile.cversion.${name}
                       PROPERTIES PASS_REGULAR_EXPRESSION "${pattern}")
endfunction()

cf_cversion_case(
  gnu
  "a.c[ \n]+is[ \n]+compiled[ \n]+as[ \n]+C11[ \n]+\\(-std=c11\\),[ \n]+not[ \n]+C89:[ \n]+libfyaml[ \n]+needs[ \n]+it[ \n]+here.*options:[ \n]+-std=c11"
  -DCMAKE_C_COMPILER_ID=GNU)
cf_cversion_case(
  other
  "a.c[ \n]+needs[ \n]+C11,[ \n]+and[ \n]+TinyCC[ \n]+is[ \n]+given[ \n]+no[ \n]+flag"
  -DCMAKE_C_COMPILER_ID=TinyCC)
cf_cversion_case(msvc-old "MSVC[ \n]+19\\.28" -DCMAKE_C_COMPILER_ID=MSVC
                 -DMSVC_VERSION=1920)
cf_cversion_case(
  refused
  "a.c[ \n]+needs[ \n]+C11[ \n]+\\(libfyaml[ \n]+needs[ \n]+it[ \n]+here\\),[ \n]+which[ \n]+profile[ \n]+misra[ \n]+does[ \n]+not[ \n]+allow"
  -DCMAKE_C_COMPILER_ID=GNU
  -DCF_C89_ONLY=ON
  -DCF_BUILD_PROFILE_NAME=misra)
cf_cversion_case(noreason "REASON[ \n]+is[ \n]+required"
                 -DCMAKE_C_COMPILER_ID=GNU)
cf_cversion_case(
  badstd "standard[ \n]+'98'[ \n]+is[ \n]+not[ \n]+99[ \n]+or[ \n]+11"
  -DCMAKE_C_COMPILER_ID=GNU)

add_test(
  NAME profile.gate_cversion
  COMMAND
    "${CMAKE_COMMAND}" "-DSRC=${CMAKE_SOURCE_DIR}"
    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}" -P
    "${CMAKE_CURRENT_SOURCE_DIR}/gate_cversion.cmake")
