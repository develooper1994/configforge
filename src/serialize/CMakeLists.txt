# The serialization layer. It may use the leaf types - values, errors,
# arenas - and may not touch the key space, the definitions or the
# sources. The boundary is checked in CI so that extracting this layer
# later stays a directory move and a build file.
target_sources(configforge PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/blob.c
                                   ${CMAKE_CURRENT_SOURCE_DIR}/codec.c)

if(CF_WITH_INI)
  target_sources(configforge PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/ini.c)
endif()
if(CF_WITH_JSON)
  target_sources(configforge PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/json.c)
endif()
