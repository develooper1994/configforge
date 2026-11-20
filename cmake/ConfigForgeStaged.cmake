# Every public header in one directory, for the tools that want one
# (abidiff, the CBMC proofs, check_include_file, the gates' probes):
# ${CMAKE_BINARY_DIR}/staged/include/configforge/. The headers are in the
# component that owns each (lib/<c>/include/configforge/, D-100), and
# cf_doctor.h is Doctor's. cf_build_options.h is this build's generated
# one, not the source tree's defaults, as the build's own include order
# (cmake/ConfigForgeOptions.cmake) and an installed copy have it.
set(CF_STAGED_DIR "${CMAKE_BINARY_DIR}/staged/include")
file(GLOB cf_staged_headers "${CF_REPO_ROOT}/lib/*/include/configforge/*"
     "${CF_REPO_ROOT}/tools/doctor/include/configforge/*")
foreach(h ${cf_staged_headers})
  get_filename_component(name "${h}" NAME)
  if(NOT name STREQUAL "cf_build_options.h")
    configure_file("${h}" "${CF_STAGED_DIR}/configforge/${name}" COPYONLY)
  endif()
endforeach()
configure_file("${CMAKE_BINARY_DIR}/generated/configforge/cf_build_options.h"
               "${CF_STAGED_DIR}/configforge/cf_build_options.h" COPYONLY)
