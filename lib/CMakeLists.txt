# One directory per component (D-100). Each declares itself with
# cf_component, which adds its sources to configforge, and the ones the
# serialization library is made of to configforge_serialize too, as the
# order here says: a component uses only the ones before it.
add_subdirectory(port)
add_subdirectory(numeric)
add_subdirectory(base)
add_subdirectory(platform)
add_subdirectory(serialize)
add_subdirectory(config)
add_subdirectory(cxx)
