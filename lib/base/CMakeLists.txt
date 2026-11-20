# What every component shares: alloc, error, value.
foreach(t configforge configforge_serialize)
  target_include_directories(
    ${t} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
endforeach()

target_sources(
  configforge
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/error.c
          ${CMAKE_CURRENT_SOURCE_DIR}/src/alloc.c
          ${CMAKE_CURRENT_SOURCE_DIR}/src/value.c)
