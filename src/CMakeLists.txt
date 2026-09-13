# Each layer owns its own list. The core knows nothing about formats;
# the serialization layer knows nothing about configuration semantics.
add_subdirectory(platform)
add_subdirectory(core)
add_subdirectory(sources)
add_subdirectory(diag)
add_subdirectory(serialize)
add_subdirectory(bridge)
