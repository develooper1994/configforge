cmake_minimum_required(VERSION 3.10)

get_filename_component(root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
include("${root}/cmake/ConfigForgeProfile.cmake")

# The two input files can come from different commits and compiler patches.
function(read_sizes path prefix)
  file(STRINGS "${path}" lines)
  set(keys "")
  set(compiler "")
  foreach(line IN LISTS lines)
    if(line STREQUAL "" OR line MATCHES "^#")
      continue()
    elseif(
      line MATCHES
      "^tool[|][^|]+[|]([^| ]+ ([0-9]+)([.][^|]+)?)[|][^|]+[|][^|]+[|][^|]+$")
      if(compiler)
        message(FATAL_ERROR "${path}: duplicate tool line")
      endif()
      set(compiler "${CMAKE_MATCH_1}")
      string(REGEX REPLACE "^([^ ]+ [0-9]+).*" "\\1" family "${compiler}")
    elseif(line MATCHES "^(base|off|all|profile)[|]([^|]+)[|](-?[0-9]+|-)$")
      set(key "${CMAKE_MATCH_1}|${CMAKE_MATCH_2}")
      if(key IN_LIST keys)
        message(FATAL_ERROR "${path}: duplicate ${key}")
      endif()
      list(APPEND keys "${key}")
      # cmake-lint: disable=C0103
      set("${prefix}_${key}"
          "${CMAKE_MATCH_3}"
          PARENT_SCOPE)
    else()
      message(FATAL_ERROR "${path}: invalid size record '${line}'")
    endif()
  endforeach()
  if(NOT compiler OR NOT keys)
    message(FATAL_ERROR "${path}: missing tool or size records")
  endif()
  set(${prefix}_keys
      "${keys}"
      PARENT_SCOPE)
  set(${prefix}_compiler
      "${compiler}"
      PARENT_SCOPE)
  set(${prefix}_family
      "${family}"
      PARENT_SCOPE)
endfunction()

# Compare size records made by compatible compilers.
function(compare_sizes recorded measured)
  read_sizes("${recorded}" old)
  read_sizes("${measured}" new)
  if(NOT old_family STREQUAL new_family)
    message(
      STATUS
        "sizes: recorded with ${old_compiler}, this is ${new_compiler}; not "
        "compared")
    return()
  endif()
  foreach(key IN LISTS old_keys new_keys)
    if(NOT key IN_LIST new_keys OR NOT key IN_LIST old_keys)
      message(FATAL_ERROR "sizes: ${key}: present in only one file")
    endif()
  endforeach()
  foreach(key IN LISTS old_keys)
    set(a "${old_${key}}")
    set(b "${new_${key}}")
    if(a STREQUAL b)
      continue()
    endif()
    if(NOT a STREQUAL "-" AND NOT b STREQUAL "-")
      math(EXPR delta "(${b} - (${a})) * 100")
      math(EXPR limit "${a} * 3")
      if(limit LESS 0)
        math(EXPR limit "0 - (${limit})")
      endif()
      if(delta LESS 0)
        math(EXPR delta "0 - (${delta})")
      endif()
      if(NOT delta GREATER limit)
        continue()
      endif()
    endif()
    message(FATAL_ERROR "sizes: ${key}: recorded ${a}, measured ${b}")
  endforeach()
endfunction()

# Build one profile and measure its library code.
function(build_size name profile out)
  set(dir "${WORK}/${name}")
  # Reuse objects between runs, but read project defaults and profiles anew.
  execute_process(
    COMMAND
      "${CMAKE_COMMAND}" -S "${root}" -B "${dir}" "-UCF_*"
      -DCMAKE_BUILD_TYPE=MinSizeRel -DCF_BUILD_TESTING=OFF
      -DCF_BUILD_EXAMPLES=OFF -DCF_BUILD_TOOLS=OFF -DCF_ENABLE_GATES=OFF
      "-DCF_PROFILE=${profile}" ${ARGN}
    RESULT_VARIABLE result)
  if(result EQUAL 0)
    execute_process(
      COMMAND "${CMAKE_COMMAND}" --build "${dir}" --config MinSizeRel --target
              configforge RESULT_VARIABLE result)
  endif()
  if(NOT result EQUAL 0)
    message(STATUS "sizes: could not build ${name}")
    set(${out}
        "-"
        PARENT_SCOPE)
    return()
  endif()
  execute_process(
    COMMAND "${size_program}" -t "${dir}/libconfigforge.a"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output)
  if(NOT result EQUAL 0 OR NOT output MATCHES
                           "\n[ \t]*([0-9]+)[^\n]*[(]TOTALS[)]")
    message(FATAL_ERROR "sizes: cannot read the text total for ${name}")
  endif()
  set(${out}
      "${CMAKE_MATCH_1}"
      PARENT_SCOPE)
endfunction()

# Measure the default build, the profiles and the optional parts.
function(measure_sizes out)
  find_program(size_program NAMES size)
  if(NOT size_program)
    message(FATAL_ERROR "sizes: GNU binutils size is required")
  endif()
  build_size(base default base)
  if(base STREQUAL "-")
    message(FATAL_ERROR "sizes: could not build the default library")
  endif()
  # A -P script has no project of its own. Read the base build's system
  # and compiler data, including the target processor for cross builds.
  set(info "${WORK}/base/CMakeFiles/${CMAKE_VERSION}")
  include("${info}/CMakeSystem.cmake")
  include("${info}/CMakeCCompiler.cmake")
  if(NOT CMAKE_C_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
    message(
      FATAL_ERROR "sizes: requires GCC or Clang, got ${CMAKE_C_COMPILER_ID}")
  endif()
  execute_process(
    COMMAND git rev-parse --short=7 HEAD
    WORKING_DIRECTORY "${root}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE commit
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "sizes: cannot identify the source commit")
  endif()
  string(TIMESTAMP date "%Y-%m-%d" UTC)
  set(lines "# Written by scripts/check/sizes.cmake -DWRITE=ON; do not edit.\n")
  string(
    APPEND
    lines
    "tool|${CMAKE_SYSTEM_PROCESSOR}|${CMAKE_C_COMPILER_ID} "
    "${CMAKE_C_COMPILER_VERSION}|MinSizeRel|${commit}|${date}\n"
    "base|default|${base}\n")
  set(unavailable "")
  foreach(part IN LISTS CF_SIZE_PARTS)
    build_size("${part}" default bytes "-D${part}=OFF")
    if(bytes STREQUAL "-")
      set(saving "-")
      list(APPEND unavailable "${part}")
    else()
      math(EXPR saving "${base} - ${bytes}")
    endif()
    string(APPEND lines "off|${part}|${saving}\n")
  endforeach()
  list(LENGTH unavailable count)
  if(count GREATER 3)
    message(FATAL_ERROR "sizes: off builds failed for ${unavailable}")
  endif()
  build_size(
    all
    default
    bytes
    -DCF_WITH_TOML=OFF
    -DCF_WITH_EMIT=OFF
    -DCF_WITH_REAL=OFF
    -DCF_WITH_REPORT=OFF
    -DCF_WITH_IMAGE=OFF
    -DCF_WITH_INI=OFF
    -DCF_WITH_DOTENV=OFF
    -DCF_WITH_DIR=OFF)
  if(bytes STREQUAL "-")
    message(FATAL_ERROR "sizes: could not build with the eight parts disabled")
  endif()
  string(APPEND lines "all|parts|${bytes}\n")
  foreach(profile IN LISTS CF_PROFILES)
    build_size("profile-${profile}" "${profile}" bytes)
    string(APPEND lines "profile|${profile}|${bytes}\n")
  endforeach()
  include("${root}/cmake/ConfigForgeVersion.cmake")
  configforge_write_lf("${out}" "${lines}")
endfunction()

if((WRITE AND CHECK) OR (NOT WRITE AND NOT CHECK))
  message(FATAL_ERROR "sizes: choose -DWRITE=ON or -DCHECK=ON")
endif()
if(NOT DEFINED WORK)
  set(WORK "${root}/build-sizes")
endif()
get_filename_component(WORK "${WORK}" ABSOLUTE)
if(WRITE)
  if(NOT DEFINED OUT)
    set(OUT "${root}/docs/sizes.txt")
  endif()
  measure_sizes("${OUT}")
else()
  if(NOT DEFINED FILE)
    set(FILE "${root}/docs/sizes.txt")
    if(NOT EXISTS "${FILE}")
      message(
        STATUS "sizes: the first CI artifact will supply docs/sizes.txt; not "
               "compared")
      return()
    endif()
  endif()
  if(NOT DEFINED MEASURED)
    set(MEASURED "${WORK}/measured.txt")
    measure_sizes("${MEASURED}")
  endif()
  compare_sizes("${FILE}" "${MEASURED}")
endif()
