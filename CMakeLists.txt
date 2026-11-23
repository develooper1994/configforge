# ConfigForge - top level.
#
# Kept thin on purpose: options, policy and gates live in cmake/, and
# every source directory owns its own CMakeLists.txt. The build tree
# mirrors the source tree so that an architectural boundary is visible
# in the build as well as in the file system.

cmake_minimum_required(VERSION 3.10)

# Not in the source tree: a build there writes CMakeFiles/ and a cache
# among the sources, which is how one was found at the root. Refused
# before project(), so that nothing is written.
get_filename_component(cf_src "${CMAKE_SOURCE_DIR}" REALPATH)
get_filename_component(cf_bin "${CMAKE_BINARY_DIR}" REALPATH)
if(cf_src STREQUAL cf_bin)
  message(FATAL_ERROR "ConfigForge is built outside its source tree: "
                      "cmake -S . -B build (or cmake --preset default)")
endif()

# What the modules name a file of the repository by. A component built alone
# sets it to its own repository (cmake/ConfigForgeComponent.cmake).
set(CF_REPO_ROOT "${CMAKE_SOURCE_DIR}")
# The sources of each component go into the two libraries below, not into
# a library of its own.
set(CF_WHOLE_PROJECT ON)

# The version is written once, in lib/config/include/configforge/cf_version.h.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ConfigForgeVersion.cmake)
configforge_read_version(CF_VERSION)

project(
  ConfigForge
  VERSION ${CF_VERSION}
  DESCRIPTION "Configuration resolution that explains itself"
  LANGUAGES C)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(ConfigForgeOptions)
include(ConfigForgeCVersion)
include(ConfigForgeComponent)

# After the options, not before: CF_WITH_CXX does not exist until
# ConfigForgeOptions has declared it, and an if() on an undefined
# variable is silently false.
if(CF_WITH_CXX)
  enable_language(CXX)
endif()
include(ConfigForgeCompiler)
include(ConfigForgeSize)
include(ConfigForgeAnalysis)
include(ConfigForgeGates)
include(ConfigForgeFormat)

set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

target_include_directories(configforge PUBLIC $<INSTALL_INTERFACE:include>)

configforge_setup_cppcheck()
configforge_apply_warnings(configforge)
configforge_apply_hardening(configforge)
cf_profile_report()
message(STATUS "limits: keys ${CF_KEY_MAX}, messages ${CF_MESSAGE_MAX}, "
               "paths ${CF_PATH_MAX}, nesting ${CF_MAX_DEPTH}; stack budget "
               "${CF_STACK_BUDGET} bytes per function")
configforge_generate_options(configforge)
configforge_apply_options(configforge)
configforge_apply_analysis(configforge)
configforge_build_id(configforge)

# The serialization library (D-27, D-90): the readers and writers and what
# they need, a library of its own. libconfigforge compiles the same sources,
# so it carries every function of it.
include(ConfigForgeSerialize)
add_library(configforge_serialize)
add_library(ConfigForge::configforge_serialize ALIAS configforge_serialize)
target_include_directories(configforge_serialize
                           PUBLIC $<INSTALL_INTERFACE:include>)
configforge_apply_warnings(configforge_serialize)
configforge_apply_hardening(configforge_serialize)
configforge_generate_options(configforge_serialize)
configforge_apply_options(configforge_serialize)
configforge_apply_analysis(configforge_serialize)
target_compile_definitions(configforge_serialize PRIVATE CF_BUILDING_LIBRARY=1
                                                         CF_INTERNAL_API=1)
# configforge is made before configforge_setup_cppcheck(), so its sources
# have never run through cppcheck; the same sources here are not either,
# until both are (Roadmap, "Critical-system standards").
set_target_properties(configforge_serialize PROPERTIES C_CPPCHECK "")

add_subdirectory(lib)
# What the components declared, for gate.layering, which runs with -P.
cf_write_layers("${CMAKE_BINARY_DIR}/layers.cmake")
include(ConfigForgeStaged)
if(CF_WITH_YAML)
  target_link_libraries(configforge PRIVATE ${CF_YAML_LIBRARY})
  target_link_libraries(configforge_serialize PRIVATE ${CF_YAML_LIBRARY})
  # libfyaml's header needs C11 (0.9.x's pre-C11 fallback for atomics does
  # not compile; fixed after 0.9.6): the one file that includes it is
  # compiled as C11, after the C89 flag so that it wins, and the rest of
  # the library stays C89. libfyaml builds with neither MSVC nor a C89-only
  # compiler, and the libyaml backend is the one for those (D-62).
  if(_CF_YAML_LIBFYAML AND NOT MSVC)
    cf_source_needs_c(
      lib/serialize/src/yaml.c 11 REASON
      "libfyaml's header needs C11 (0.9.x's pre-C11 fallback for \
atomics does not compile)")
  endif()
endif()
if(CF_WITH_XML)
  target_link_libraries(configforge PRIVATE ${CF_EXPAT_LIBRARY})
  target_link_libraries(configforge_serialize PRIVATE ${CF_EXPAT_LIBRARY})
endif()
if(CF_WITH_SQLITE)
  target_link_libraries(configforge PRIVATE ${CF_SQLITE_LIBRARY})
endif()
# macOS's user defaults are read through CoreFoundation, whose headers
# are not C89 (SInt64 is a long long): posix.c, the one file that
# includes them, is compiled as C99 there, after the C89 flag so that it
# wins, as yaml.c is for libfyaml. PUBLIC: a static library's consumer
# links the framework too. Elsewhere nothing changes.
if(CF_WITH_PREFERENCES AND APPLE)
  cf_source_needs_c(lib/platform/src/posix/posix.c 99 REASON
                    "CoreFoundation's headers use long long (SInt64)")
  target_link_libraries(configforge PUBLIC "-framework CoreFoundation")
endif()

# Only what CF_API marks is visible from a shared library, and the
# library's own sources are the ones that export rather than import.
target_compile_definitions(configforge PRIVATE CF_BUILDING_LIBRARY=1
                                               CF_INTERNAL_API=1)

set_target_properties(
  configforge configforge_serialize
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

# Timings to start an optimisation from (bench/): needs Doctor.
option(CF_BUILD_BENCH "Build the benchmark" OFF)
if(CF_BUILD_BENCH AND TARGET configforge_doctor)
  add_subdirectory(bench)
endif()

cf_cversion_write()
configforge_add_gates(configforge)

include(ConfigForgeInstall)
include(ConfigForgeSbom)
configforge_generate_sbom()
