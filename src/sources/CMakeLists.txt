# Sources produce candidates. None of them resolves anything, and none
# knows what other sources exist.
target_sources(
  configforge
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/entry.c
          ${CMAKE_CURRENT_SOURCE_DIR}/memory.c
          ${CMAKE_CURRENT_SOURCE_DIR}/defaults.c
          ${CMAKE_CURRENT_SOURCE_DIR}/env.c
          ${CMAKE_CURRENT_SOURCE_DIR}/environ.c
          ${CMAKE_CURRENT_SOURCE_DIR}/cliargs.c
          ${CMAKE_CURRENT_SOURCE_DIR}/cli.c)
