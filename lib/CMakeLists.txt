# One directory per component (D-100). Each declares itself with
# cf_component, which adds its sources to configforge, and the ones the
# serialization library is made of to configforge_serialize too, as the
# order here says: a component uses only the ones before it.
#
# lanes is first and is not ConfigForge's (D-101): its own build file reads
# nothing of ours and so cannot call cf_component. What ConfigForge adds to
# the targets it makes - the warnings, the sanitizers and the rest of the
# analysis builds - is added by the function it calls for each, and the
# layering gate is told of it here, with no dependency.
function(lanes_configure_target tgt)
  configforge_apply_warnings(${tgt})
  configforge_apply_analysis(${tgt})
  # Its test programs are checked as ConfigForge's are (cf_unit_tests).
  get_target_property(kind ${tgt} TYPE)
  if(kind STREQUAL "EXECUTABLE" AND CF_CPPCHECK_TESTS)
    set_target_properties(${tgt} PROPERTIES C_CPPCHECK "${CF_CPPCHECK_TESTS}")
  endif()
endfunction()
add_subdirectory(lanes)
cf_component_record(lanes)

add_subdirectory(port)
add_subdirectory(numeric)
add_subdirectory(base)
add_subdirectory(platform)
add_subdirectory(serialize)
add_subdirectory(config)
add_subdirectory(cxx)
