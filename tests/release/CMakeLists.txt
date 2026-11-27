# The record of how a release archive was built (L-23): the script runs on
# this build's own directory.
add_test(
  NAME release.buildinfo
  COMMAND
    ${CMAKE_COMMAND} "-DSRC=${CMAKE_SOURCE_DIR}" "-DBUILD=${CMAKE_BINARY_DIR}"
    "-DOUT=${CMAKE_BINARY_DIR}/buildinfo-test.md" -P
    "${CMAKE_CURRENT_SOURCE_DIR}/buildinfo.cmake")
