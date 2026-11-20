# The configuration library: core, sources, diag and bridge.
target_include_directories(
  configforge PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

add_subdirectory(src/core)
add_subdirectory(src/sources)
add_subdirectory(src/diag)
add_subdirectory(src/bridge)
