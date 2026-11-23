cmake_minimum_required(VERSION 3.10)
include("${SRC}/scripts/generate/matrix.cmake")
set(data "${CMAKE_CURRENT_LIST_DIR}/data/tables")
set(_cf_profile_root "${data}")
set(CF_PROFILES small large)
set(CF_PROFILE_OPTIONS CF_Z:BOOL CF_A:STRING CF_KEY_MAX:STRING)
set(CF_SIZE_PARTS CF_Z)
cf_matrix_tables("${data}" block)
foreach(
  name
  profiles
  features
  options
  parts
  targets)
  file(READ "${data}/${name}.md" expected)
  if(NOT block_${name} STREQUAL expected)
    message(FATAL_ERROR "${name}: expected\n${expected}got\n${block_${name}}")
  endif()
endforeach()
