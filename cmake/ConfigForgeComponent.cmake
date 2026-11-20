# A component of the library, declared (D-100).
#
# Every lib/<name>/CMakeLists.txt calls cf_component once, and that call is
# the one place its dependencies and its sources are written: the build
# reads it, gate.layering reads it, and `cmake -S lib/<name>` builds the
# component alone from it.
#
# Inside the whole project (CF_WHOLE_PROJECT, set by the top-level
# CMakeLists.txt) the sources go into the two libraries, configforge and
# configforge_serialize, since CMake 3.10 cannot link object libraries.
# Alone, a component is a static library cf_<name>, linked to the cf_<dep>
# of each dependency, which are built from their own directories beside it.

include_guard(GLOBAL)

# A component built alone, as the top project, needs what the top-level
# CMakeLists.txt gives the whole project: the compiler's warnings, the
# analysis helpers, ctest, and the repository's cmake/ on the module path.
# Done here and not in cf_component, since a function's variables end with
# it and the helpers read them later.
if(NOT CF_WHOLE_PROJECT AND CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  get_filename_component(CF_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." REALPATH)
  list(APPEND CMAKE_MODULE_PATH "${CF_REPO_ROOT}/cmake")
  include(ConfigForgeCompiler)
  include(ConfigForgeAnalysis)
  include(CTest)
endif()

# Every component declared in this configure, in declaration order, for the
# layering gate (cf_write_layers).
set_property(GLOBAL PROPERTY CF_COMPONENTS "")

# What cf_component does inside the whole project: the sources go into
# configforge, and SERIALIZE_SOURCES into configforge_serialize too, each
# with the component's include/ as a public include directory. Sets
# CF_TARGETS and CF_FULL_TARGET for cf_component.
function(cf_component_whole dir sources serialize_sources)
  set(targets configforge)
  target_sources(configforge PRIVATE ${sources})
  if(serialize_sources)
    list(APPEND targets configforge_serialize)
    target_sources(configforge_serialize PRIVATE ${serialize_sources})
  endif()
  foreach(t ${targets})
    target_include_directories(${t} PUBLIC $<BUILD_INTERFACE:${dir}/include>)
  endforeach()
  set(CF_TARGETS
      "${targets}"
      PARENT_SCOPE)
  set(CF_FULL_TARGET
      configforge
      PARENT_SCOPE)
endfunction()

# lanes is not ConfigForge's (D-101): its own CMakeLists.txt reads nothing
# of ours, so a component built alone that depends on it compiles its
# LANES_SOURCES under ConfigForge's prefix, as the static library cf_lanes,
# whose symbols are cf_lanes_*. Only `cmake -S lib/lanes` builds the
# unprefixed library `lanes`.
function(cf_component_lanes lanes_dir)
  get_filename_component(lanes_dir "${lanes_dir}" REALPATH)
  include("${lanes_dir}/lanes_sources.cmake")
  add_library(cf_lanes STATIC ${LANES_SOURCES})
  target_include_directories(cf_lanes
                             PUBLIC $<BUILD_INTERFACE:${lanes_dir}/include>)
  # The prefix is PUBLIC: a component built against cf_lanes must call the
  # names cf_lanes defines (text's utf8.c called lanes_scalar, unprefixed).
  target_compile_definitions(
    cf_lanes
    PUBLIC LANES_PREFIX_CF=1
    PRIVATE LANES_BUILDING_LIBRARY=1)
  configforge_apply_warnings(cf_lanes)
  configforge_apply_hardening(cf_lanes)
  configforge_apply_analysis(cf_lanes)
  set_target_properties(
    cf_lanes
    PROPERTIES C_STANDARD 90
               C_STANDARD_REQUIRED ON
               C_EXTENSIONS OFF)
endfunction()

# What cf_component does alone: the components it depends on, from the
# directories beside it, and a library cf_<name> of its own, static, or an
# interface library where there is nothing to compile (cxx). Sets
# CF_TARGETS and CF_FULL_TARGET for cf_component.
function(
  cf_component_alone
  name
  dir
  deps
  sources)
  foreach(d ${deps})
    if(NOT TARGET cf_${d})
      if(d STREQUAL "lanes")
        cf_component_lanes("${dir}/../lanes")
      else()
        add_subdirectory("${dir}/../${d}" "${CMAKE_BINARY_DIR}/${d}")
      endif()
    endif()
  endforeach()

  set(tgt cf_${name})
  if(sources)
    add_library(${tgt} STATIC ${sources})
    target_include_directories(${tgt} PUBLIC $<BUILD_INTERFACE:${dir}/include>)
    foreach(d ${deps})
      target_link_libraries(${tgt} PUBLIC cf_${d})
    endforeach()
    configforge_apply_warnings(${tgt})
    configforge_apply_hardening(${tgt})
    configforge_generate_options(${tgt})
    configforge_apply_options(${tgt})
    configforge_apply_analysis(${tgt})
    target_compile_definitions(${tgt} PRIVATE CF_BUILDING_LIBRARY=1
                                              CF_INTERNAL_API=1)
    set_target_properties(
      ${tgt}
      PROPERTIES C_STANDARD 90
                 C_STANDARD_REQUIRED ON
                 C_EXTENSIONS OFF)
  else()
    # Headers only (cxx): nothing to compile.
    add_library(${tgt} INTERFACE)
    target_include_directories(${tgt}
                               INTERFACE $<BUILD_INTERFACE:${dir}/include>)
    foreach(d ${deps})
      target_link_libraries(${tgt} INTERFACE cf_${d})
    endforeach()
  endif()
  set(CF_TARGETS
      "${tgt}"
      PARENT_SCOPE)
  set(CF_FULL_TARGET
      "${tgt}"
      PARENT_SCOPE)
endfunction()

# What the layering gate is told of a component (cf_write_layers writes
# it): its name, what it depends on, and which of its files, as
# <name>/<file>, the serialization library is made of. cf_component calls
# it; a directory that cannot call cf_component, because it reads none of
# ConfigForge's variables (lib/lanes), is recorded by lib/CMakeLists.txt.
#
# ~~~
# cf_component_record(<name> [DEPENDS <dep>...] [LEAVES <file>...])
# ~~~
function(cf_component_record name)
  cmake_parse_arguments(
    R
    ""
    ""
    "DEPENDS;LEAVES"
    ${ARGN})
  set_property(GLOBAL APPEND PROPERTY CF_COMPONENTS ${name})
  set_property(GLOBAL PROPERTY CF_COMPONENT_${name}_DEPENDS "${R_DEPENDS}")
  set_property(GLOBAL PROPERTY CF_COMPONENT_${name}_LEAVES "${R_LEAVES}")
endfunction()

# Declare a component.
#
# ~~~
# cf_component(<name>
#              DEPENDS <component>[:<header>]...
#              SOURCES <file>...
#              [SERIALIZE_SOURCES <file>...])
# ~~~
#
# The files are relative to the component's directory. SERIALIZE_SOURCES is
# the subset of SOURCES that configforge_serialize is made of too. A
# dependency spelled <component>:<header> is one the build needs and the
# source may include that header of and no other (gate.layering).
#
# Sets, in the caller's scope:
#   CF_REPO_ROOT     the repository, for a path outside the component
#   CF_TARGETS       every target that compiles this component's sources
#   CF_FULL_TARGET   the one that compiles all of them: configforge, or
#                    cf_<name> alone
function(cf_component name)
  cmake_parse_arguments(
    A
    ""
    ""
    "DEPENDS;SOURCES;SERIALIZE_SOURCES"
    ${ARGN})
  set(dir "${CMAKE_CURRENT_LIST_DIR}")

  set(deps "")
  foreach(d ${A_DEPENDS})
    string(REGEX REPLACE ":.*$" "" bare "${d}")
    list(APPEND deps ${bare})
  endforeach()

  # For the layering gate: what it declared, and which of its files are the
  # serialization library's.
  set(leaves "")
  foreach(s ${A_SERIALIZE_SOURCES})
    list(APPEND leaves "${name}/${s}")
  endforeach()
  cf_component_record(
    ${name}
    DEPENDS
    ${A_DEPENDS}
    LEAVES
    ${leaves})

  set(sources "")
  foreach(s ${A_SOURCES})
    list(APPEND sources "${dir}/${s}")
  endforeach()
  set(serialize_sources "")
  foreach(s ${A_SERIALIZE_SOURCES})
    list(APPEND serialize_sources "${dir}/${s}")
  endforeach()

  if(CF_WHOLE_PROJECT)
    cf_component_whole("${dir}" "${sources}" "${serialize_sources}")
    set(repo_root "${CMAKE_SOURCE_DIR}")
  else()
    get_filename_component(repo_root "${dir}/../.." REALPATH)
    set(CF_REPO_ROOT "${repo_root}")
    cf_component_alone(${name} "${dir}" "${deps}" "${sources}")
  endif()
  set(CF_REPO_ROOT
      "${repo_root}"
      PARENT_SCOPE)
  set(CF_TARGETS
      "${CF_TARGETS}"
      PARENT_SCOPE)
  set(CF_FULL_TARGET
      "${CF_FULL_TARGET}"
      PARENT_SCOPE)

  # Alone, the top component's tests run with it; a dependency's are its
  # own build's.
  get_filename_component(here "${CMAKE_SOURCE_DIR}" REALPATH)
  get_filename_component(mine "${dir}" REALPATH)
  if(NOT CF_WHOLE_PROJECT
     AND here STREQUAL mine
     AND BUILD_TESTING
     AND EXISTS "${dir}/tests/CMakeLists.txt")
    add_subdirectory(tests)
  endif()
endfunction()

# What a component's tests/CMakeLists.txt begins with: what
# tests/CMakeLists.txt does for the tests of the whole project (the
# library's own clang-tidy does not read them), the shared helpers on the
# include path, and CF_TEST_LIB, the library a test links: configforge in the
# whole project (or <whole-project library>), cf_<name> alone.
#
# ~~~
# cf_component_tests(<name> [<whole-project library>])
# ~~~
function(cf_component_tests name)
  add_definitions(-DCF_INTERNAL_API=1)
  set(CMAKE_C_CLANG_TIDY
      ""
      PARENT_SCOPE)
  include_directories("${CF_REPO_ROOT}/tests/support")
  set(lib cf_${name})
  if(CF_WHOLE_PROJECT)
    set(lib configforge)
    if(ARGV1)
      set(lib ${ARGV1})
    endif()
  else()
    # The headers of every component in one directory (the staged copy),
    # for the tests that read a header by the compiler's own command line.
    include(ConfigForgeStaged)
    set(CF_STAGED_DIR
        "${CF_STAGED_DIR}"
        PARENT_SCOPE)
  endif()
  set(CF_TEST_LIB
      "${lib}"
      PARENT_SCOPE)
endfunction()

# Every unit test in the list is a plain C89 program returning non-zero on
# failure: one source <test>.c, linked to the component's library, a ctest
# named unit.<test>.
#
# ~~~
# cf_unit_tests(<test>...)
# ~~~
function(cf_unit_tests)
  foreach(t ${ARGN})
    add_executable(${t} ${t}.c)
    target_link_libraries(${t} PRIVATE ${CF_TEST_LIB})
    set_target_properties(
      ${t}
      PROPERTIES C_STANDARD 90
                 C_STANDARD_REQUIRED ON
                 C_EXTENSIONS OFF)
    configforge_apply_warnings(${t})
    configforge_apply_analysis(${t})
    if(CF_CPPCHECK_TESTS)
      set_target_properties(${t} PROPERTIES C_CPPCHECK "${CF_CPPCHECK_TESTS}")
    endif()
    add_test(NAME unit.${t} COMMAND ${t})
  endforeach()
endfunction()

# The declarations, as the layering gate reads them: written at configure
# time, since the gate runs with -P and sees none of this build.
#
# ~~~
# cf_write_layers(<file>)
# ~~~
function(cf_write_layers file)
  get_property(components GLOBAL PROPERTY CF_COMPONENTS)
  set(text "set(CF_LAYER_COMPONENTS ${components})\n")
  foreach(c ${components})
    get_property(deps GLOBAL PROPERTY CF_COMPONENT_${c}_DEPENDS)
    get_property(leaves GLOBAL PROPERTY CF_COMPONENT_${c}_LEAVES)
    # Quoted: set(<name> ) with nothing after it unsets the name.
    string(APPEND text "set(CF_DEPENDS_${c} \"${deps}\")\n")
    string(APPEND text "set(CF_LEAVES_${c} \"${leaves}\")\n")
  endforeach()
  file(WRITE "${file}" "${text}")
endfunction()
