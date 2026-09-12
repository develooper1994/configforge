include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(
  TARGETS configforge
  EXPORT ConfigForgeTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(
  EXPORT ConfigForgeTargets
  FILE ConfigForgeTargets.cmake
  NAMESPACE ConfigForge::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ConfigForge)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/ConfigForgeConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)

configure_package_config_file(
  "${CMAKE_SOURCE_DIR}/cmake/ConfigForgeConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/ConfigForgeConfig.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ConfigForge)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/ConfigForgeConfig.cmake"
              "${CMAKE_CURRENT_BINARY_DIR}/ConfigForgeConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ConfigForge)
