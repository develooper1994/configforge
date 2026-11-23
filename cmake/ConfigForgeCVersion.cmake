# Record the standard a source needs above the library's C89 baseline.
option(CF_C89_ONLY "Refuse every source file that needs more than C89 (D-106)"
       OFF)

# Give one source the C standard it needs and record why.
function(cf_source_needs_c file standard)
  cmake_parse_arguments(
    PARSE_ARGV
    2
    need
    ""
    "REASON"
    "")
  if(NOT standard MATCHES "^(99|11)$")
    message(
      FATAL_ERROR "cf_source_needs_c: standard '${standard}' is not 99 or 11")
  endif()
  get_filename_component(abs "${file}" ABSOLUTE)
  if(NOT EXISTS "${abs}")
    message(FATAL_ERROR "cf_source_needs_c: ${file} does not exist")
  endif()
  if(NOT DEFINED need_REASON OR need_REASON STREQUAL "")
    message(FATAL_ERROR "cf_source_needs_c: REASON is required")
  endif()
  if(need_REASON MATCHES "[|\r\n]")
    message(
      FATAL_ERROR "cf_source_needs_c: a REASON may not contain '|' or a newline"
    )
  endif()
  set(root "${CMAKE_SOURCE_DIR}")
  if(DEFINED CF_C_ROOT)
    set(root "${CF_C_ROOT}")
  endif()
  file(RELATIVE_PATH file "${root}" "${abs}")

  if(CF_C89_ONLY)
    message(
      FATAL_ERROR
        "${file} needs C${standard} (${need_REASON}), which profile "
        "${CF_BUILD_PROFILE_NAME} does not allow (CF_C89_ONLY=ON). "
        "Turn off the option that needs it, or choose another profile.")
  endif()
  set(flag "")
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set(flag "-std=c${standard}")
  elseif(CMAKE_C_COMPILER_ID STREQUAL "MSVC" AND standard STREQUAL "11")
    if(MSVC_VERSION LESS 1928)
      message(
        FATAL_ERROR
          "${file} needs C11 and MSVC 19.28 (Visual Studio 2019 16.8) is the "
          "first with /std:c11")
    endif()
    set(flag "/std:c11")
  endif()

  if(flag)
    set_property(
      SOURCE "${abs}"
      APPEND
      PROPERTY COMPILE_OPTIONS "${flag}")
    message(WARNING "${file} is compiled as C${standard} (${flag}), not C89: "
                    "${need_REASON}. Every other file stays C89.")
  else()
    message(
      WARNING
        "${file} needs C${standard}, and ${CMAKE_C_COMPILER_ID} is given no "
        "flag for it here: ${need_REASON}.")
  endif()
  set_property(GLOBAL APPEND_STRING
               PROPERTY CF_C_ABOVE_89 "${file}|c${standard}|${need_REASON}\n")
endfunction()

# Write the C-version declarations and the SBOM summary.
function(cf_cversion_write)
  get_property(entries GLOBAL PROPERTY CF_C_ABOVE_89)
  configforge_write_lf("${CMAKE_BINARY_DIR}/cf_cversion.txt" "${entries}")
  set(summary "none")
  string(REGEX MATCHALL "[^\n|]+\\|c(99|11)\\|[^\n]*" lines "${entries}")
  if(lines)
    string(REGEX REPLACE "\\|c(99|11)\\|[^\n]*\n" "=c\\1 " summary "${entries}")
    string(STRIP "${summary}" summary)
  endif()
  set(CF_C_ABOVE_89_SBOM
      "c-above-89=${summary}"
      PARENT_SCOPE)
endfunction()
