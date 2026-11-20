# The operating system: posix, win32, none.
foreach(t configforge configforge_serialize)
  target_include_directories(
    ${t} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
endforeach()

add_subdirectory(src)
