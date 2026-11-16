# Every tool is built into one directory, so that the front door finds
# the tools beside itself in the build tree as it does in an install
# (a multi-config generator adds its Debug/ or Release/ below it).
set(CF_TOOLS_DIR ${CMAKE_BINARY_DIR}/tools/bin)
include(GNUInstallDirs)

# `configforge <command>` runs `configforge-<command>`.
add_subdirectory(configforge)

# Doctor reads reports (JSON) and its adapters write them.
if(CF_WITH_JSON AND CF_WITH_REPORT)
  add_subdirectory(doctor)
else()
  message(STATUS "doctor needs CF_WITH_JSON and CF_WITH_REPORT; not built")
endif()

# The programs, for `cmake --install`: the front door and every tool
# it runs, in one directory (D-99).
foreach(t configforge_front doctor)
  if(TARGET ${t})
    install(
      TARGETS ${t}
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
      COMPONENT tools)
  endif()
endforeach()
if(TARGET doctor)
  install(
    PROGRAMS $<TARGET_FILE_DIR:doctor>/doctor${CMAKE_EXECUTABLE_SUFFIX}
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT tools)
endif()
