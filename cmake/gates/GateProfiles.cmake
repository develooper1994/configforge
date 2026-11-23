cmake_minimum_required(VERSION 3.10)

include("${SRC}/cmake/ConfigForgeProfile.cmake")

macro(violation name detail)
  message(STATUS "VIOLATION: profile ${name}: ${detail}")
  set(failed ON)
endmacro()

set(directory "${SRC}/cmake/profiles")
file(
  GLOB actual
  RELATIVE "${directory}"
  "${directory}/*")
set(names "")
foreach(entry IN LISTS CF_PROFILE_OPTIONS)
  string(REPLACE ":" ";" pair "${entry}")
  list(GET pair 0 name)
  list(GET pair 1 type)
  list(APPEND names "${name}")
  set("type_${name}" "${type}")
endforeach()

if(CMAKE_VERSION VERSION_LESS 3.19)
  message(
    STATUS
      "profile gate: CMake ${CMAKE_VERSION} cannot read JSON; presets skipped")
else()
  file(READ "${SRC}/CMakePresets.json" presets)
  string(JSON count LENGTH "${presets}" configurePresets)
  set(i 0)
  while(i LESS count)
    string(
      JSON
      preset
      GET
      "${presets}"
      configurePresets
      ${i}
      name)
    string(
      JSON
      selection
      ERROR_VARIABLE
      error
      GET
      "${presets}"
      configurePresets
      ${i}
      cacheVariables
      CF_PROFILE)
    if(error STREQUAL "NOTFOUND")
      set("selection_${preset}" "${selection}")
    endif()
    math(EXPR i "${i} + 1")
  endwhile()
endif()

cf_option_defaults()
foreach(profile IN LISTS CF_PROFILES)
  set(file "${directory}/${profile}.cmake")
  list(REMOVE_ITEM actual "${profile}.cmake")
  if(NOT EXISTS "${file}")
    violation("${profile}" "missing profile file")
    continue()
  endif()

  file(READ "${file}" contents)
  if(NOT contents MATCHES "^# ")
    violation("${profile}" "first line must start with '# '")
  endif()
  cf_profile_parse("${file}" parsed)
  foreach(name IN LISTS names)
    if(NOT name IN_LIST parsed_NAMES)
      violation("${profile}" "${name} is missing")
      continue()
    endif()
    if(NOT contents MATCHES
       "(^|\n)set\\(${name} [^ \n]+ CACHE ${type_${name}} \"profile\"\\)")
      violation("${profile}" "${name} must have type ${type_${name}}")
    endif()
    if(profile STREQUAL "default")
      if(NOT DEFINED _cf_default_${name})
        violation("${profile}" "${name} has no option default declaration")
      elseif(NOT "${parsed_${name}}" STREQUAL "${_cf_default_${name}}")
        violation(
          "${profile}"
          "${name} is ${parsed_${name}}, its default is ${_cf_default_${name}}")
      endif()
    endif()
  endforeach()
  if(NOT "${parsed_NAMES}" STREQUAL "${names}")
    violation("${profile}"
              "options must appear exactly once in CF_PROFILE_OPTIONS order")
  endif()
  if(parsed_CF_FORCE_PORTABLE AND NOT parsed_CF_SINGLE_THREADED)
    violation("${profile}" "CF_FORCE_PORTABLE requires CF_SINGLE_THREADED")
  endif()
  foreach(limit CF_KEY_MAX:32:65535 CF_MESSAGE_MAX:64:65535
                CF_PATH_MAX:64:65535 CF_MAX_DEPTH:4:1024)
    string(REPLACE ":" ";" range "${limit}")
    list(GET range 0 name)
    list(GET range 1 low)
    list(GET range 2 high)
    set(value "${parsed_${name}}")
    if(NOT value MATCHES "^[0-9]+$"
       OR value LESS low
       OR value GREATER high)
      violation("${profile}" "${name} must be a number from ${low} to ${high}")
    endif()
  endforeach()
  if(NOT CMAKE_VERSION VERSION_LESS 3.19 AND NOT "${selection_${profile}}"
                                             STREQUAL "${profile}")
    violation("${profile}" "configure preset must set CF_PROFILE to ${profile}")
  endif()
endforeach()

foreach(file IN LISTS actual)
  violation("${file}" "unexpected file in cmake/profiles")
endforeach()

file(READ "${SRC}/README.md" readme)
string(REGEX MATCHALL
             "[|] `CF_WITH_[A-Z_]+` [|] [^|\n]+ [|] [0-9]+\\.[0-9]+ KB [|]"
             rows "${readme}")
list(LENGTH rows count)
if(count LESS 8 OR NOT readme MATCHES "from [0-9]+ KB to")
  violation("README" "library size table needs eight rows and a starting size")
endif()

if(failed)
  message(FATAL_ERROR "profile gate failed")
endif()
list(LENGTH CF_PROFILES count)
message(STATUS "profile gate: clean, ${count} profiles")
