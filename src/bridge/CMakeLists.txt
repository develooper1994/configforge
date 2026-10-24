# The bridge: key space <-> bytes.
#
# These are the only files that legitimately use both the configuration
# model and the serialization layer, because bridging them is what they
# are. Nothing below may depend on this directory, which is the rule the
# layering gate checks.
target_sources(
  configforge
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/flatten.c
          ${CMAKE_CURRENT_SOURCE_DIR}/image.c
          ${CMAKE_CURRENT_SOURCE_DIR}/file.c
          ${CMAKE_CURRENT_SOURCE_DIR}/dotenv.c
          ${CMAKE_CURRENT_SOURCE_DIR}/dir.c
          ${CMAKE_CURRENT_SOURCE_DIR}/emit.c
          ${CMAKE_CURRENT_SOURCE_DIR}/store.c
          ${CMAKE_CURRENT_SOURCE_DIR}/secrets.c)

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
  target_include_directories(configforge PRIVATE ${CF_SQLITE_INCLUDE_DIR})
endif()
