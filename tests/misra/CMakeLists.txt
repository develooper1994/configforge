# The comparator of gate.misra, driven with hand-written cppcheck output and
# records: CMake only, in every profile and on every platform.
add_test(
  NAME misra.compare
  COMMAND
    ${CMAKE_COMMAND} "-DSRC=${CMAKE_SOURCE_DIR}"
    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/work" -P
    ${CMAKE_CURRENT_SOURCE_DIR}/compare.cmake)
