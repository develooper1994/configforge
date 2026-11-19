# configforge-codec: the serialization library's program. It reads
# documents and never a configuration, so each test writes its own
# inputs and needs no fixture.

# `configforge codec formats`, and docs/Formats.md's generated table held
# to it.
add_test(
  NAME codec.formats
  COMMAND
    ${CMAKE_COMMAND} -DCODEC=$<TARGET_FILE:configforge_codec>
    -DPAGE=${CMAKE_SOURCE_DIR}/docs/Formats.md -DEMIT=${CF_WITH_EMIT} -P
    ${CMAKE_CURRENT_SOURCE_DIR}/formats.cmake)

# `detect`, `check` and `dump`.
foreach(script detect check dump)
  add_test(
    NAME codec.${script}
    COMMAND
      ${CMAKE_COMMAND} -DCODEC=$<TARGET_FILE:configforge_codec>
      -DWORK=${CMAKE_CURRENT_BINARY_DIR}/${script} -DCBOR=${CF_WITH_CBOR} -P
      ${CMAKE_CURRENT_SOURCE_DIR}/${script}.cmake)
endforeach()

# The version it reports is the serialization library's.
add_test(NAME codec.version COMMAND configforge_codec --version)
set_tests_properties(
  codec.version PROPERTIES PASS_REGULAR_EXPRESSION
                           "^ConfigForge codec [0-9]+\\.[0-9]+\\.[0-9]+")

# Anything else is the usage, on stderr, exit 2.
add_test(NAME codec.usage
         COMMAND ${CMAKE_COMMAND} -DCODEC=$<TARGET_FILE:configforge_codec> -P
                 ${CMAKE_CURRENT_SOURCE_DIR}/usage.cmake)
