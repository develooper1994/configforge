# The core knows nothing about formats, sources or platforms.
target_sources(
  configforge
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/version.c
          ${CMAKE_CURRENT_SOURCE_DIR}/port.c
          ${CMAKE_CURRENT_SOURCE_DIR}/error.c
          ${CMAKE_CURRENT_SOURCE_DIR}/numeric.c
          ${CMAKE_CURRENT_SOURCE_DIR}/alloc.c
          ${CMAKE_CURRENT_SOURCE_DIR}/value.c
          ${CMAKE_CURRENT_SOURCE_DIR}/keyspace.c
          ${CMAKE_CURRENT_SOURCE_DIR}/define.c
          ${CMAKE_CURRENT_SOURCE_DIR}/validate.c
          ${CMAKE_CURRENT_SOURCE_DIR}/config.c)
