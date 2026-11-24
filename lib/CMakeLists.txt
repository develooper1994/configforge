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

# lanes is compiled into both libraries, under ConfigForge's prefix, so that
# nothing new is installed or linked and its symbols are cf_lanes_* (D-101,
# D-102). The prefix is the target's, not the source files': a source file's
# properties belong to the directory of the target, which is not this one,
# and no other file of ours uses a name of lanes' own.
include(${CMAKE_CURRENT_LIST_DIR}/lanes/lanes_sources.cmake)
foreach(t configforge configforge_serialize)
  target_sources(${t} PRIVATE ${LANES_SOURCES})
  target_include_directories(
    ${t} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/lanes/include>)
  target_compile_definitions(
    ${t} PRIVATE LANES_PREFIX_CF=1 LANES_BUILDING_LIBRARY=1
                 ${LANES_DEFINITIONS})
endforeach()

add_subdirectory(port)
add_subdirectory(numeric)
add_subdirectory(base)
add_subdirectory(text)
add_subdirectory(platform)
add_subdirectory(serialize)
add_subdirectory(config)
add_subdirectory(cxx)
