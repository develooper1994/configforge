# The core knows nothing about formats, sources or platforms.
target_sources(
  configforge
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/version.c
          ${CMAKE_CURRENT_SOURCE_DIR}/keyspace.c
          ${CMAKE_CURRENT_SOURCE_DIR}/define.c
          ${CMAKE_CURRENT_SOURCE_DIR}/validate.c
          ${CMAKE_CURRENT_SOURCE_DIR}/config.c
          ${CMAKE_CURRENT_SOURCE_DIR}/iterate.c
          ${CMAKE_CURRENT_SOURCE_DIR}/live.c
          ${CMAKE_CURRENT_SOURCE_DIR}/edit.c)
