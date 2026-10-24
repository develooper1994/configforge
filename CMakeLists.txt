# ConfigForge - top level.
#
# Kept thin on purpose: options, policy and gates live in cmake/, and
# every source directory owns its own CMakeLists.txt. The build tree
# mirrors the source tree so that an architectural boundary is visible
# in the build as well as in the file system.

cmake_minimum_required(VERSION 3.10)

# The version is written once, in include/configforge/cf_version.h.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ConfigForgeVersion.cmake)
configforge_read_version(CF_VERSION)

project(
  ConfigForge
  VERSION ${CF_VERSION}
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

# Windows finds a DLL beside the executable or on PATH and nowhere else.
# With every binary of a shared build in one directory, every test and
# tool finds the library it was linked against.
if(WIN32
   AND BUILD_SHARED_LIBS
   AND NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()

add_library(configforge)
add_library(ConfigForge::configforge ALIAS configforge)

target_include_directories(
  configforge PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                     $<INSTALL_INTERFACE:include>)

configforge_setup_cppcheck()
configforge_apply_warnings(configforge)
message(STATUS "limits: keys ${CF_KEY_MAX}, messages ${CF_MESSAGE_MAX}, "
               "paths ${CF_PATH_MAX}, nesting ${CF_MAX_DEPTH}; stack budget "
               "${CF_STACK_BUDGET} bytes per function")
configforge_generate_options(configforge)
configforge_apply_options(configforge)
configforge_apply_analysis(configforge)
configforge_build_id(configforge)

add_subdirectory(src)
if(CF_WITH_YAML)
  target_link_libraries(configforge PRIVATE ${CF_YAML_LIBRARY})
endif()
if(CF_WITH_XML)
  target_link_libraries(configforge PRIVATE ${CF_EXPAT_LIBRARY})
endif()
if(CF_WITH_SQLITE)
  target_link_libraries(configforge PRIVATE ${CF_SQLITE_LIBRARY})
endif()

# Only what CF_API marks is visible from a shared library, and the
# library's own sources are the ones that export rather than import.
target_compile_definitions(configforge PRIVATE CF_BUILDING_LIBRARY=1)

set_target_properties(
  configforge
  PROPERTIES VERSION ${PROJECT_VERSION}
             SOVERSION ${PROJECT_VERSION_MAJOR}
             C_VISIBILITY_PRESET hidden
             CXX_VISIBILITY_PRESET hidden
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
include(ConfigForgeSbom)
configforge_generate_sbom()
