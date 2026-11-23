# Build profiles are read as data before any option acquires a default.
set(CF_PROFILES
    full
    default
    minimal
    misra
    embedded-json
    embedded-cbor
    server)
set(CF_PROFILE_OPTIONS
    CF_WITH_INI:BOOL
    CF_WITH_JSON:BOOL
    CF_WITH_TOML:BOOL
    CF_WITH_PROPERTIES:BOOL
    CF_WITH_CBOR:BOOL
    CF_WITH_TLV:BOOL
    CF_WITH_COMPOSE:BOOL
    CF_WITH_XML:BOOL
    CF_WITH_YAML:BOOL
    CF_WITH_SQLITE:BOOL
    CF_WITH_PREFERENCES:BOOL
    CF_WITH_DIR:BOOL
    CF_WITH_DOTENV:BOOL
    CF_WITH_EMIT:BOOL
    CF_WITH_IMAGE:BOOL
    CF_WITH_REPORT:BOOL
    CF_WITH_REAL:BOOL
    CF_WITH_CXX:BOOL
    CF_YAML_BACKEND:STRING
    CF_PLATFORM:STRING
    CF_SINGLE_THREADED:BOOL
    CF_FORCE_PORTABLE:BOOL
    CF_GC_SECTIONS:BOOL
    CF_LTO:BOOL
    CF_C89_ONLY:BOOL
    CF_LANES_BACKENDS:STRING
    CF_KEY_MAX:STRING
    CF_MESSAGE_MAX:STRING
    CF_PATH_MAX:STRING
    CF_MAX_DEPTH:STRING
    CF_STACK_BUDGET:STRING)
set(_cf_profile_root "${CMAKE_CURRENT_LIST_DIR}/..")

# Read the option values and names from a profile file.
function(cf_profile_parse file prefix)
  # cmake-lint: disable=C0103
  # Profile keys use the caller's output prefix.
  file(READ "${file}" contents)
  string(APPEND contents "\n")
  set(names "")
  set(number 0)
  string(CONCAT profile_pattern "^set\\(([A-Za-z0-9_]+) ([^ \t;\"()]+) CACHE "
                "(BOOL|STRING) \"profile\"\\)$")
  while(NOT contents STREQUAL "")
    string(REGEX MATCH "^([^\n]*)\n" line "${contents}")
    string(LENGTH "${line}" line_length)
    set(line "${CMAKE_MATCH_1}")
    string(SUBSTRING "${contents}" ${line_length} -1 contents)
    math(EXPR number "${number} + 1")
    if(line STREQUAL "" OR line MATCHES "^#")
      continue()
    endif()
    if(NOT line MATCHES "${profile_pattern}")
      message(FATAL_ERROR "${file}:${number}: expected a profile set(...) line")
    endif()
    set(name "${CMAKE_MATCH_1}")
    set(value "${CMAKE_MATCH_2}")
    set(type "${CMAKE_MATCH_3}")
    if(NOT "${name}:BOOL" IN_LIST CF_PROFILE_OPTIONS
       AND NOT "${name}:STRING" IN_LIST CF_PROFILE_OPTIONS)
      message(
        FATAL_ERROR
          "${file}:${number}: ${name} is not an option a profile may set")
    endif()
    if(NOT "${name}:${type}" IN_LIST CF_PROFILE_OPTIONS)
      message(FATAL_ERROR "${file}:${number}: ${name} has the wrong cache type")
    endif()
    if(type STREQUAL "BOOL" AND NOT value MATCHES "^(ON|OFF)$")
      message(FATAL_ERROR "${file}:${number}: ${name} must be ON or OFF")
    endif()
    list(APPEND names "${name}")
    set("${prefix}_${name}"
        "${value}"
        PARENT_SCOPE)
  endwhile()
  set("${prefix}_NAMES"
      "${names}"
      PARENT_SCOPE)
endfunction()

# Options whose individual code savings are measured for the README.
set(CF_SIZE_PARTS
    CF_WITH_INI
    CF_WITH_JSON
    CF_WITH_TOML
    CF_WITH_PROPERTIES
    CF_WITH_CBOR
    CF_WITH_TLV
    CF_WITH_YAML
    CF_WITH_DIR
    CF_WITH_DOTENV
    CF_WITH_EMIT
    CF_WITH_IMAGE
    CF_WITH_REPORT
    CF_WITH_REAL)

# Read literal declarations, including help strings that span lines or
# contain parentheses and semicolons. Only declaration heads are lists.
function(cf_option_defaults)
  set(root "${_cf_profile_root}")
  file(
    GLOB
    files
    "${root}/CMakeLists.txt"
    "${root}/cmake/*.cmake"
    "${root}/lib/*/CMakeLists.txt"
    "${root}/lib/*/*.cmake")
  set(names "")
  set(space "[ \t\n]+")
  set(padding "[ \t\n]*")
  set(word "(\"[^\"]*\"|[^ \t\n)]+)")
  foreach(file IN LISTS files)
    file(READ "${file}" contents)
    string(REGEX MATCHALL
                 "(^|\n)[ \t]*(option|set)\\(${padding}CF_[A-Za-z0-9_]+" heads
                 "${contents}")
    list(REMOVE_DUPLICATES heads)
    foreach(head IN LISTS heads)
      string(REGEX MATCH "CF_[A-Za-z0-9_]+$" name "${head}")
      if(head MATCHES "option\\(")
        string(CONCAT pattern "(^|\n)[ \t]*option\\(${padding}${name}${space}"
                      "\"([^\"]*)\"(${space}" "${word})?${padding}\\)")
        if(NOT contents MATCHES "${pattern}")
          continue()
        endif()
        set(help "${CMAKE_MATCH_2}")
        set(value "${CMAKE_MATCH_4}")
        if(value STREQUAL "")
          set(value OFF)
        endif()
      else()
        string(
          CONCAT
            pattern
            "(^|\n)[ \t]*set\\(${padding}${name}${space}${word}${space}CACHE"
            "${space}(STRING|BOOL|FILEPATH|PATH)${space}\"([^\"]*)\"")
        if(NOT contents MATCHES "${pattern}")
          continue()
        endif()
        set(value "${CMAKE_MATCH_2}")
        set(help "${CMAKE_MATCH_4}")
      endif()
      string(REGEX REPLACE "^\"(.*)\"$" "\\1" value "${value}")
      list(APPEND names "${name}")
      # cmake-lint: disable=C0103
      set("_cf_default_${name}"
          "${value}"
          PARENT_SCOPE)
      # cmake-lint: disable=C0103
      set("_cf_help_${name}"
          "${help}"
          PARENT_SCOPE)
    endforeach()
  endforeach()
  list(REMOVE_DUPLICATES names)
  list(SORT names)
  set(_cf_option_names
      "${names}"
      PARENT_SCOPE)
  set(_cf_option_root
      "${root}"
      PARENT_SCOPE)
endfunction()

# Return the help text declared for a build option.
function(cf_option_help name out)
  if(NOT "${_cf_option_root}" STREQUAL "${_cf_profile_root}")
    cf_option_defaults()
  endif()
  # cmake-lint: disable=C0103
  set("${out}"
      "${_cf_help_${name}}"
      PARENT_SCOPE)
endfunction()

# Select a build profile and apply its defaults to the cache.
function(cf_profile_load)
  # cmake-lint: disable=C0103
  # Cache and output names come from the profile option list.
  string(CONCAT profile_help
                "Build profile: full, default, minimal, misra, embedded-json, "
                "embedded-cbor or server")
  set(CF_PROFILE
      default
      CACHE STRING "${profile_help}")
  set_property(CACHE CF_PROFILE PROPERTY STRINGS ${CF_PROFILES})
  # Save the spelling before CACHE FILEPATH resolves it against the shell.
  set(requested "${CF_PROFILE_FILE}")
  set(CF_PROFILE_FILE
      ""
      CACHE FILEPATH
            "A build profile of your own, in the form of cmake/profiles/*.cmake"
  )
  if(NOT requested STREQUAL "")
    message(
      STATUS
        "profile: CF_PROFILE_FILE is set; CF_PROFILE '${CF_PROFILE}' is not "
        "used")
    get_filename_component(
      file
      "${requested}"
      ABSOLUTE
      BASE_DIR
      "${CMAKE_SOURCE_DIR}")
    get_filename_component(stem "${file}" NAME)
    string(REGEX REPLACE "\\.cmake$" "" stem "${stem}")
    if(NOT stem MATCHES "^[A-Za-z0-9._+-]+$")
      message(
        FATAL_ERROR
          "CF_PROFILE_FILE: the file name '${stem}' may hold only letters, "
          "digits and . _ + -")
    endif()
    # Keep the source-relative resolution for later configures too.
    get_property(
      cached
      CACHE CF_PROFILE_FILE
      PROPERTY VALUE)
    if(NOT cached STREQUAL "" AND NOT cached STREQUAL file)
      set_property(CACHE CF_PROFILE_FILE PROPERTY VALUE "${file}")
    endif()
    set(name "file:${stem}")
  else()
    if(NOT CF_PROFILE IN_LIST CF_PROFILES)
      string(REPLACE ";" ", " available "${CF_PROFILES}")
      message(
        FATAL_ERROR
          "CF_PROFILE '${CF_PROFILE}' is not a profile; there are: ${available}"
      )
    endif()
    set(name "${CF_PROFILE}")
    get_filename_component(
      file "${_cf_profile_root}/cmake/profiles/${name}.cmake" ABSOLUTE)
  endif()

  file(SHA256 "${file}" hash)
  set(applied "${name}|${hash}")
  if(DEFINED CF_PROFILE_APPLIED AND NOT CF_PROFILE_APPLIED STREQUAL applied)
    string(REGEX REPLACE "\\|.*$" "" old "${CF_PROFILE_APPLIED}")
    string(
      CONCAT
        reason
        "CF_PROFILE is '${name}', but this build directory was configured with "
        "'${old}', or that profile's file has changed: the options it set are "
        "kept in CMakeCache.txt, so the new profile would only half apply. "
        "Use another build directory, or delete CMakeCache.txt here.")
    message(FATAL_ERROR "${reason}")
  endif()

  cf_profile_parse("${file}" _cf_pv)
  foreach(entry IN LISTS CF_PROFILE_OPTIONS)
    string(REPLACE ":" ";" pair "${entry}")
    list(GET pair 0 option)
    list(GET pair 1 type)
    if(option IN_LIST _cf_pv_NAMES)
      set("_cf_pv_${option}"
          "${_cf_pv_${option}}"
          PARENT_SCOPE)
      set("${option}"
          "${_cf_pv_${option}}"
          CACHE ${type} "set by profile ${name}")
    endif()
  endforeach()
  set(CF_PROFILE_APPLIED
      "${applied}"
      CACHE INTERNAL "Profile and file hash")
  set(CF_BUILD_PROFILE_NAME
      "${name}"
      PARENT_SCOPE)
  set(_cf_profile_file
      "${file}"
      PARENT_SCOPE)
endfunction()

# Report the selected profile, option overrides and estimated code size.
function(cf_profile_report)
  file(RELATIVE_PATH path "${CMAKE_SOURCE_DIR}" "${_cf_profile_file}")
  if(path MATCHES "^\\.\\.(/|$)" OR IS_ABSOLUTE "${path}")
    set(path "${_cf_profile_file}")
  endif()
  message(STATUS "profile: ${CF_BUILD_PROFILE_NAME} (${path})")
  set(differs OFF)
  foreach(entry IN LISTS CF_PROFILE_OPTIONS)
    string(REGEX REPLACE ":.*$" "" name "${entry}")
    if(DEFINED _cf_pv_${name} AND NOT "${${name}}" STREQUAL "${_cf_pv_${name}}")
      message(
        STATUS
          "profile: ${name} is ${${name}}, the profile says ${_cf_pv_${name}}")
      set(differs ON)
    endif()
  endforeach()
  if(NOT differs)
    message(STATUS "profile: no option differs from it")
  endif()

  # Packaged sources may omit the README used for this optional size estimate.
  set(readme "")
  if(EXISTS "${_cf_profile_root}/README.md")
    file(READ "${_cf_profile_root}/README.md" readme)
  endif()
  string(REGEX MATCHALL
               "[|] `CF_WITH_[A-Z_]+` [|] [^|\n]+ [|] [0-9]+\\.[0-9]+ KB [|]"
               rows "${readme}")
  if(NOT rows OR NOT readme MATCHES "from ([0-9]+) KB to")
    message(
      STATUS
        "profile: library code size not estimated (the README's table was not "
        "found)")
    return()
  endif()
  math(EXPR size "10 * ${CMAKE_MATCH_1}")
  foreach(row IN LISTS rows)
    if(row MATCHES "`(CF_WITH_[A-Z_]+)`.* ([0-9]+)\\.([0-9]) KB")
      if(NOT ${CMAKE_MATCH_1})
        math(EXPR size "${size} - 10 * ${CMAKE_MATCH_2} - ${CMAKE_MATCH_3}")
      endif()
    endif()
  endforeach()
  math(EXPR size "(${size} + 5) / 10")
  cf_profile_parse("${_cf_profile_root}/cmake/profiles/default.cmake" default)
  set(extra "")
  foreach(name IN LISTS default_NAMES)
    if(name MATCHES "^CF_WITH_"
       AND ${name}
       AND NOT default_${name})
      list(APPEND extra "${name}")
    endif()
  endforeach()
  if(extra)
    string(REPLACE ";" ", " extra "${extra}")
    message(
      STATUS "profile: library code about ${size} KB, not counting ${extra}")
  else()
    message(
      STATUS
        "profile: library code about ${size} KB (README table; parts overlap, "
        "so a build with several off is a little larger)")
  endif()
endfunction()
