# A consumer of the packaged library, used by Conan's test_package and by
# the vcpkg check alike: find the package, link the target, run.
cmake_minimum_required(VERSION 3.15)
project(configforge_package_test LANGUAGES C)

find_package(ConfigForge REQUIRED)

add_executable(package_test package_test.c)
target_link_libraries(package_test PRIVATE ConfigForge::configforge)
set_target_properties(
  package_test
  PROPERTIES C_STANDARD 90
             C_STANDARD_REQUIRED ON
             C_EXTENSIONS OFF)
