# ConfigForge - top level.
#
# Kept thin on purpose: options, policy and gates live in cmake/, and
# every source directory owns its own CMakeLists.txt. The build tree
# mirrors the source tree so that an architectural boundary is visible
# in the build as well as in the file system.

cmake_minimum_required(VERSION 3.10)

project(ConfigForge
        VERSION 0.2.0
        DESCRIPTION "Configuration resolution that explains itself"
        LANGUAGES C)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(ConfigForgeOptions)

# After the options, not before: CF_WITH_CXX does not exist until
# ConfigForgeOptions has declared it, and an if() on an undefined
# variable is silently false.
if(CF_WITH_CXX)
  enable_language(CXX)
endif()
include(ConfigForgeCompiler)
include(ConfigForgeAnalysis)
include(ConfigForgeGates)
include(ConfigForgeFormat)

set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(configforge)
add_library(ConfigForge::configforge ALIAS configforge)

target_include_directories(configforge
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

configforge_setup_cppcheck()
configforge_apply_warnings(configforge)
configforge_apply_options(configforge)
configforge_apply_analysis(configforge)

add_subdirectory(src)

set_target_properties(configforge PROPERTIES
  VERSION   ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
  C_STANDARD 90
  C_STANDARD_REQUIRED ON
  C_EXTENSIONS OFF)

option(CF_BUILD_TOOLS "Build ConfigForge Doctor" ON)
if(CF_BUILD_TOOLS)
  add_subdirectory(tools)
endif()

if(CF_BUILD_TESTING)
  enable_testing()
  add_subdirectory(tests)
endif()

if(CF_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

configforge_add_gates(configforge)

include(ConfigForgeInstall)
