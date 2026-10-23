include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(
  TARGETS configforge
  EXPORT ConfigForgeTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# The source tree's cf_build_options.h holds defaults; the installed one
# has to hold this build's actual values. Installing the defaults would
# let a consumer compiled with a plain -I disagree with the library
# about CF_WITH_REAL, which changes the size of cf_value.
#
# The explicit install below would win on declaration order anyway. The
# exclusion is here so that it does not have to: a correctness that
# depends on the order of two install() calls is a correctness nobody
# will preserve through the next edit.
install(
  DIRECTORY include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  PATTERN "cf_build_options.h" EXCLUDE)

# The licence travels with the headers, which are a source distribution.
install(FILES "${CMAKE_SOURCE_DIR}/LICENSE"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/configforge)
install(FILES "${CMAKE_BINARY_DIR}/generated/configforge/cf_build_options.h"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/configforge)

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
