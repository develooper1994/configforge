# The comparison (P-1b, D-111): ConfigForge and the other libraries reading
# the same documents, built here under one set of flags. Its own CMake
# project: the top-level CMakeLists.txt never adds it, and nothing in the
# default build, the gates' build or scripts/verify.cmake can reach a library
# fetched for it.
#
#   cmake -S bench/compare -B build-cfb -DCMAKE_BUILD_TYPE=Release
#       ConfigForge alone: what the per-push job runs; nothing is fetched.
#   cmake -DLIBS=$HOME/cfb-libs -P scripts/bench/fetch.cmake
#   cmake -S bench/compare -B build-cfb -DCF_BENCH_LIBS=$HOME/cfb-libs
#       every library; it never fetches. A library missing from the
#       directory stops the configure.
#
# Every library's sources are compiled here, by this compiler, with
# CF_BENCH_FLAGS and nothing else: no -O of its own, no -march. ConfigForge
# is not add_subdirectory'd (its modules take CF_REPO_ROOT from
# CMAKE_SOURCE_DIR, which inside another project is this one): it is
# configured on its own, below, and its readers library linked.
cmake_minimum_required(VERSION 3.16)
project(cfb_compare C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CF_BENCH_LIBS
    ""
    CACHE PATH "directory scripts/bench/fetch.cmake filled")
set(CF_BENCH_FLAGSET
    "shipped"
    CACHE STRING "shipped, bare or native")
set_property(CACHE CF_BENCH_FLAGSET PROPERTY STRINGS shipped bare native)

get_filename_component(CF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
include("${CMAKE_CURRENT_SOURCE_DIR}/libraries.cmake")

# CMake's Release adds -O3 -DNDEBUG after CMAKE_C_FLAGS: emptied, so that
# -O2 is what every library gets.
set(CMAKE_C_FLAGS_RELEASE "")
set(CMAKE_CXX_FLAGS_RELEASE "")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "")

if(CF_BENCH_FLAGSET STREQUAL "shipped")
  set(CF_BENCH_FLAGS
      -O2
      -fstack-protector-strong
      -fstack-clash-protection
      -fcf-protection=full
      -U_FORTIFY_SOURCE
      -D_FORTIFY_SOURCE=3)
  set(cfb_harden ON)
elseif(CF_BENCH_FLAGSET STREQUAL "bare")
  set(CF_BENCH_FLAGS -O2)
  set(cfb_harden OFF)
elseif(CF_BENCH_FLAGSET STREQUAL "native")
  set(CF_BENCH_FLAGS -O3 -march=native)
  set(cfb_harden OFF)
else()
  message(
    FATAL_ERROR "bench/compare: CF_BENCH_FLAGSET is shipped, bare or native")
endif()
string(REPLACE ";" " " CF_BENCH_FLAGS_STR "${CF_BENCH_FLAGS}")

# ---- ConfigForge, configured and built on its own ------------------------
set(cf_bin "${CMAKE_BINARY_DIR}/configforge")
if(NOT EXISTS "${cf_bin}/libconfigforge_serialize.a")
  message(STATUS "bench/compare: configuring ConfigForge in ${cf_bin}")
  execute_process(
    COMMAND
      "${CMAKE_COMMAND}" -S "${CF_ROOT}" -B "${cf_bin}"
      -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
      -DCMAKE_C_FLAGS_RELEASE= "-DCMAKE_C_FLAGS=${CF_BENCH_FLAGS_STR}"
      -DCF_BUILD_TESTING=OFF -DCF_BUILD_EXAMPLES=OFF -DCF_BUILD_TOOLS=OFF
      -DCF_ENABLE_GATES=OFF -DCF_WITH_XML=ON -DCF_YAML_BACKEND=builtin
      -DCF_HARDEN=${cfb_harden}
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE out)
  if(NOT rc EQUAL 0)
    message(
      FATAL_ERROR "bench/compare: ConfigForge does not configure:\n${out}")
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${cf_bin}" --target
            configforge_serialize --parallel
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE out)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "bench/compare: ConfigForge does not build:\n${out}")
  endif()
endif()
include("${CF_ROOT}/cmake/ConfigForgeVersion.cmake")
configforge_read_version(CF_VERSION)
execute_process(
  COMMAND git -C "${CF_ROOT}" rev-parse --short=12 HEAD
  OUTPUT_VARIABLE cf_commit
  OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)

# ---- the targets ---------------------------------------------------------
# Every target compiled by us gets the flags and nothing else.
function(cfb_flags tgt)
  target_compile_options(${tgt} PRIVATE ${CF_BENCH_FLAGS})
endfunction()

# A library's sources become lib_<name> (what the speed rows use) and
# sizelib_<name> (the same, in sections the linker can drop, for the size
# rows). cfb_library(<name> SOURCES ... [INCLUDES ...] [DEFINITIONS ...]
# [CXX]) .
function(cfb_library name)
  cmake_parse_arguments(
    L
    "CXX"
    ""
    "SOURCES;INCLUDES;DEFINITIONS"
    ${ARGN})
  foreach(kind lib sizelib)
    add_library(${kind}_${name} STATIC ${L_SOURCES})
    target_include_directories(${kind}_${name} PUBLIC ${L_INCLUDES})
    target_compile_definitions(${kind}_${name} PUBLIC ${L_DEFINITIONS})
    cfb_flags(${kind}_${name})
    # a library's own warnings are its own business
    target_compile_options(${kind}_${name} PRIVATE -w)
    if(kind STREQUAL "sizelib")
      target_compile_options(${kind}_${name} PRIVATE -ffunction-sections
                                                     -fdata-sections)
    endif()
  endforeach()
endfunction()

set(CFB_ADAPTER_IDS "")

# cfb_adapter(<id> <source> [VERSIONOF <fetched library>] [LIBS <library
# name>...] [SYSTEM <link flag>...])
function(cfb_adapter id source)
  cmake_parse_arguments(
    A
    ""
    "VERSIONOF"
    "LIBS;SYSTEM"
    ${ARGN})
  set(ver "system")
  if(A_VERSIONOF)
    if(LIB_${A_VERSIONOF}_TAG)
      set(ver "${LIB_${A_VERSIONOF}_TAG}")
    else()
      string(SUBSTRING "${LIB_${A_VERSIONOF}_COMMIT}" 0 12 short)
      set(ver "${LIB_${A_VERSIONOF}_DATE}@${short}")
    endif()
  endif()
  foreach(kind ad sizead)
    add_library(${kind}_${id} OBJECT "${CMAKE_CURRENT_SOURCE_DIR}/${source}")
    cfb_flags(${kind}_${id})
    target_include_directories(${kind}_${id}
                               PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_definitions(${kind}_${id} PRIVATE CFB_ADAPTER_ID="${id}"
                                                     CFB_VERSION="${ver}")
    if(kind STREQUAL "sizead")
      target_compile_options(${kind}_${id} PRIVATE -ffunction-sections
                                                   -fdata-sections)
    endif()
    foreach(l ${A_LIBS})
      if(kind STREQUAL "ad")
        target_link_libraries(${kind}_${id} PRIVATE lib_${l})
      else()
        target_link_libraries(${kind}_${id} PRIVATE sizelib_${l})
      endif()
    endforeach()
  endforeach()
  set(CFB_ADAPTER_IDS
      ${CFB_ADAPTER_IDS} ${id}
      PARENT_SCOPE)
  set(CFB_ADAPTER_${id}_LIBS
      ${A_LIBS}
      PARENT_SCOPE)
  set(CFB_ADAPTER_${id}_SYSTEM
      ${A_SYSTEM}
      PARENT_SCOPE)
endfunction()

# ConfigForge's own readers: a library target made of the build's archive.
add_library(lib_configforge STATIC IMPORTED)
set_target_properties(
  lib_configforge
  PROPERTIES IMPORTED_LOCATION "${cf_bin}/libconfigforge_serialize.a"
             INTERFACE_INCLUDE_DIRECTORIES "${cf_bin}/staged/include"
             INTERFACE_LINK_LIBRARIES "expat;m")
add_library(sizelib_configforge INTERFACE)
target_link_libraries(sizelib_configforge INTERFACE lib_configforge)

cfb_adapter(configforge adapters/configforge.cpp LIBS configforge)
foreach(kind ad sizead)
  target_compile_definitions(
    ${kind}_configforge PRIVATE CFB_WITH_XML=1
                                CFB_CF_VERSION="${CF_VERSION}+${cf_commit}")
endforeach()

# ---- the other libraries: only when their directory is given -------------
if(CF_BENCH_LIBS)
  foreach(name ${CFB_LIB_NAMES})
    if(NOT EXISTS "${CF_BENCH_LIBS}/${name}")
      message(
        FATAL_ERROR
          "bench/compare: library ${name} is not in ${CF_BENCH_LIBS}: "
          "run cmake -DLIBS=${CF_BENCH_LIBS} -P scripts/bench/fetch.cmake")
    endif()
    set(LIB_DIR "${CF_BENCH_LIBS}/${name}")
    include("${CMAKE_CURRENT_SOURCE_DIR}/libs/${name}.cmake")
  endforeach()
  include("${CMAKE_CURRENT_SOURCE_DIR}/libs/expat.cmake")
endif()

# ---- the programs --------------------------------------------------------
add_executable(cfb_compare main.cpp registry.cpp)
cfb_flags(cfb_compare)
target_include_directories(cfb_compare PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
if(CFB_NO_NOINLINE)
  target_compile_definitions(cfb_compare PRIVATE CFB_NO_NOINLINE=1)
endif()
foreach(id ${CFB_ADAPTER_IDS})
  # the adapters' objects are linked directly: a static library would drop a
  # file whose only symbol is the Registrar
  target_sources(cfb_compare PRIVATE $<TARGET_OBJECTS:ad_${id}>)
  foreach(l ${CFB_ADAPTER_${id}_LIBS})
    target_link_libraries(cfb_compare PRIVATE lib_${l})
  endforeach()
  target_link_libraries(cfb_compare PRIVATE ${CFB_ADAPTER_${id}_SYSTEM})
endforeach()

# What a library adds to a program: one program a library, parsing one fixed
# document, and one with an empty adapter to subtract.
add_subdirectory(size)

# ---- the checks that need the fetched libraries ---------------------------
enable_testing()
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND AND CF_BENCH_LIBS)
  set(CF_BENCH_CFBENCH
      ""
      CACHE FILEPATH "cf_bench, to make the documents")
  if(CF_BENCH_CFBENCH)
    foreach(check touch probes)
      add_test(
        NAME bench.${check}
        COMMAND
          "${Python3_EXECUTABLE}" "${CF_ROOT}/scripts/bench/run.py" --check
          ${check} --compare "$<TARGET_FILE:cfb_compare>" --cfbench
          "${CF_BENCH_CFBENCH}" --work "${CMAKE_BINARY_DIR}/check")
    endforeach()
  endif()
endif()
