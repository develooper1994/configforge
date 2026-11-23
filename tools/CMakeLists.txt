# Every tool is built into one directory, so that the front door finds
# the tools beside itself in the build tree as it does in an install
# (a multi-config generator adds its Debug/ or Release/ below it).
set(CF_TOOLS_DIR ${CMAKE_BINARY_DIR}/tools/bin)
include(GNUInstallDirs)

# `configforge <command>` runs `configforge-<command>`.
add_subdirectory(configforge)

# The serialization library's program needs neither the JSON option nor
# the report: it reads whatever formats the build has.
add_subdirectory(codec)
add_subdirectory(diff)

# Doctor reads reports (JSON) and its adapters write them.
if(CF_WITH_JSON AND CF_WITH_REPORT)
  add_subdirectory(doctor)
else()
  message(STATUS "doctor needs CF_WITH_JSON and CF_WITH_REPORT; not built")
endif()

# The contract's program reads the contract with the JSON reader and
# writes it through cf_schema_write: built where Doctor is.
if(CF_WITH_JSON AND CF_WITH_REPORT)
  add_subdirectory(gen)
endif()

# The programs, for `cmake --install`: the front door and every tool
# it runs, in one directory (D-99).
foreach(
  t
  configforge_front
  doctor
  configforge_gen
  configforge_codec
  configforge_diff)
  if(TARGET ${t})
    install(TARGETS ${t} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
                                 COMPONENT tools)
  endif()
endforeach()
