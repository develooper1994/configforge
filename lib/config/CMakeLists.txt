# The configuration library: core, sources, diag and bridge.
#
# core knows nothing about formats, sources or platforms. Sources produce
# candidates: none of them resolves anything, and none knows what other
# sources exist. Diagnostics read the key space and never modify it. The
# bridge (key space <-> bytes) holds the only files that use both the
# configuration model and the serialization layer, because bridging them
# is what they are; nothing below may depend on it, which is the rule
# gate.layering checks.
cmake_minimum_required(VERSION 3.10)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(cf_config C)
  include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/ConfigForgeOptions.cmake)
endif()
include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/ConfigForgeComponent.cmake)

cf_component(
  config
  DEPENDS
  port
  numeric
  base
  text
  lanes
  platform
  serialize
  SOURCES
  src/core/version.c
  src/core/keyspace.c
  src/core/define.c
  src/core/validate.c
  src/core/config.c
  src/core/iterate.c
  src/core/live.c
  src/core/edit.c
  src/sources/entry.c
  src/sources/memory.c
  src/sources/defaults.c
  src/sources/env.c
  src/sources/cli.c
  src/sources/compat.c
  src/sources/snapshot.c
  src/diag/explain.c
  src/diag/report.c
  src/diag/diff.c
  src/diag/usage.c
  src/bridge/flatten.c
  src/bridge/image.c
  src/bridge/file.c
  src/bridge/dotenv.c
  src/bridge/dir.c
  src/bridge/emit.c
  src/bridge/store.c
  src/bridge/secrets.c
  src/bridge/registry.c
  src/bridge/preferences.c
  src/bridge/schema.c)

# The store through sqlite3, found as a header and a library like the
# other dependencies. Without it the source still exists and refuses
# by name.
if(CF_WITH_SQLITE)
  find_path(CF_SQLITE_INCLUDE_DIR sqlite3.h)
  find_library(CF_SQLITE_LIBRARY NAMES sqlite3 libsqlite3)
  if(NOT CF_SQLITE_INCLUDE_DIR OR NOT CF_SQLITE_LIBRARY)
    message(
      FATAL_ERROR
        "CF_WITH_SQLITE needs sqlite3: install it (libsqlite3-dev, brew "
        "install sqlite, vcpkg install sqlite3) or point "
        "CMAKE_PREFIX_PATH at it")
  endif()
  target_include_directories(${CF_FULL_TARGET} PRIVATE ${CF_SQLITE_INCLUDE_DIR})
  # Linked where the target is made: here alone, the top-level
  # CMakeLists.txt in the whole project.
  if(NOT CF_WHOLE_PROJECT)
    target_link_libraries(cf_config PRIVATE ${CF_SQLITE_LIBRARY})
  endif()
endif()
