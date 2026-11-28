# The generators of docs/compliance and the like, each against a fixture.
add_test(
  NAME check.cert
  COMMAND
    ${CMAKE_COMMAND}
    -DGEN=${CMAKE_SOURCE_DIR}/scripts/generate/cert_matrix.cmake
    -DFIX=${CMAKE_CURRENT_SOURCE_DIR}/cert
    -DBIN=${CMAKE_CURRENT_BINARY_DIR}/cert -P
    ${CMAKE_CURRENT_SOURCE_DIR}/cert/check.cmake)
