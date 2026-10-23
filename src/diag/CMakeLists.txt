# Diagnostics read the key space and never modify it.
target_sources(
  configforge
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/explain.c
          ${CMAKE_CURRENT_SOURCE_DIR}/report.c
          ${CMAKE_CURRENT_SOURCE_DIR}/diff.c)
