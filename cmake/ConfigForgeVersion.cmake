# The version, read from the one place it is written, and the commit a
# build comes from.
#
# Included before project(), which is why it is included by path rather
# than through CMAKE_MODULE_PATH: project(VERSION) takes what
# configforge_read_version finds in cf_version.h, so no number is
# written here or in CMakeLists.txt.

# Taken now: inside a function CMAKE_CURRENT_LIST_DIR is the caller's.
set(CF_VERSION_MODULE_DIR "${CMAKE_CURRENT_LIST_DIR}")

# The X.Y.Z in include/configforge/cf_version.h, into `out`.
function(configforge_read_version out)
  set(header "${CF_VERSION_MODULE_DIR}/../include/configforge/cf_version.h")
  file(STRINGS "${header}" lines
       REGEX "^#define CF_VERSION_(MAJOR|MINOR|PATCH)[ \t]")
  foreach(part MAJOR MINOR PATCH)
    set(v_${part} "")
    foreach(l IN LISTS lines)
      if(l MATCHES "^#define CF_VERSION_${part}[ \t]+([0-9]+)")
        set(v_${part} "${CMAKE_MATCH_1}")
      endif()
    endforeach()
    if(v_${part} STREQUAL "")
      message(FATAL_ERROR "cf_version.h has no CF_VERSION_${part}")
    endif()
  endforeach()
  set(${out}
      "${v_MAJOR}.${v_MINOR}.${v_PATCH}"
      PARENT_SCOPE)
endfunction()

# Write `text` to `path` with LF line ends on every platform: file(WRITE)
# writes CRLF on Windows, which turned every file a version bump touched
# into CRLF there. configure_file is the one writer that takes a newline
# style on CMake 3.10; it ends the file with one newline of its own.
function(configforge_write_lf path text)
  string(REGEX REPLACE "\r\n" "\n" CF_LF_TEXT "${text}")
  string(REGEX REPLACE "\n$" "" CF_LF_TEXT "${CF_LF_TEXT}")
  file(WRITE "${path}.in" "@CF_LF_TEXT@")
  configure_file("${path}.in" "${path}" @ONLY NEWLINE_STYLE UNIX)
  file(REMOVE "${path}.in")
endfunction()

# Write cf_build_id.h for `tgt` on every build, not once at configure
# time, so that the commit follows a checkout or a new commit without a
# reconfigure. The script rewrites the header only when the id changed,
# so an unchanged id rebuilds nothing.
function(configforge_build_id tgt)
  find_package(Git QUIET)
  set(out "${CMAKE_BINARY_DIR}/generated/cf_build_id.h")
  set(cmd
      ${CMAKE_COMMAND}
      -DSRC=${CMAKE_SOURCE_DIR}
      -DOUT=${out}
      "-DGIT=${GIT_EXECUTABLE}"
      -P
      ${CMAKE_SOURCE_DIR}/scripts/buildid.cmake)
  # Once now, so the header exists for the first compile and for an IDE.
  execute_process(COMMAND ${cmd})
  add_custom_target(
    ${tgt}_build_id
    COMMAND ${cmd}
    BYPRODUCTS ${out}
    COMMENT "build id"
    VERBATIM)
  add_dependencies(${tgt} ${tgt}_build_id)
  target_compile_definitions(${tgt} PRIVATE CF_HAVE_BUILD_ID_H=1)
endfunction()
