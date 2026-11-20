# The portability layer: compiler, limits, bounded formatting. It uses
# nothing.
foreach(t configforge configforge_serialize)
  target_include_directories(
    ${t} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
endforeach()

target_sources(configforge PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/port.c)
