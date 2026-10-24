# The vcpkg port, as an overlay of this repository:
#
#   vcpkg install configforge --overlay-ports=<repo>/packaging/vcpkg
#
# Built from the checkout it sits in, so the port is tested against the
# code beside it rather than a release that came before. A port in the
# vcpkg registry would take the same body with vcpkg_from_github at a
# release tag in place of the first line.
get_filename_component(SOURCE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../.."
                       ABSOLUTE)

vcpkg_check_features(
  OUT_FEATURE_OPTIONS
  FEATURE_OPTIONS
  FEATURES
  yaml
  CF_WITH_YAML
  xml
  CF_WITH_XML)

vcpkg_cmake_configure(
  SOURCE_PATH
  "${SOURCE_PATH}"
  OPTIONS
  ${FEATURE_OPTIONS}
  -DCF_BUILD_TESTING=OFF
  -DCF_BUILD_TOOLS=OFF
  -DCF_BUILD_EXAMPLES=OFF
  -DCF_ENABLE_GATES=OFF)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME ConfigForge CONFIG_PATH
                         lib/cmake/ConfigForge)
vcpkg_fixup_pkgconfig()

# The headers once, the licence and the SBOM once: the debug tree keeps
# only its libraries.
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include"
     "${CURRENT_PACKAGES_DIR}/debug/share")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
