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

# `configforge codec pairs`, and docs/Formats.md's generated tables held to
# it. The table is the same in every build.
add_test(
  NAME codec.pairs_page
  COMMAND
    ${CMAKE_COMMAND} -DCODEC=$<TARGET_FILE:configforge_codec>
    -DPAGE=${CMAKE_SOURCE_DIR}/docs/Formats.md -P
    ${CMAKE_CURRENT_SOURCE_DIR}/pairs_page.cmake)

# `detect`, `check`, `dump` and `convert`; `convert` writes, so it needs
# the encoders (CF_WITH_EMIT, off in the minimal preset).
set(cf_codec_scripts detect check dump)
if(CF_WITH_EMIT)
  list(APPEND cf_codec_scripts convert)
endif()
foreach(script ${cf_codec_scripts})
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

# D-109: every document through every ordered pair of the six writing
# formats, held to the table of what each can hold. It runs the
# converter's own code (convert_core.c) without a process, which is why it
# is a program and not a script; it needs every format the pairs name.
if(CF_WITH_EMIT
   AND CF_WITH_REAL
   AND CF_WITH_INI
   AND CF_WITH_JSON
   AND CF_WITH_TOML
   AND CF_WITH_YAML
   AND CF_WITH_CBOR)
  add_executable(codec_pairs pairs.c
                             ${CMAKE_SOURCE_DIR}/tools/codec/convert_core.c)
  target_include_directories(codec_pairs
                             PRIVATE ${CMAKE_SOURCE_DIR}/tools/codec)
  target_link_libraries(codec_pairs PRIVATE configforge_serialize)
  set_target_properties(
    codec_pairs
    PROPERTIES C_STANDARD 90
               C_STANDARD_REQUIRED ON
               C_EXTENSIONS OFF)
  configforge_apply_warnings(codec_pairs)
  configforge_apply_analysis(codec_pairs)
  add_test(NAME codec.pairs
           COMMAND codec_pairs --root ${CMAKE_SOURCE_DIR} --docs
                   ${CMAKE_CURRENT_SOURCE_DIR}/pairs.manifest)
endif()
