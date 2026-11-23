# Generate the source and document line index. Set OUT to redirect it.
cmake_minimum_required(VERSION 3.10)

get_filename_component(repo "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
if(NOT DEFINED OUT)
  set(OUT "${repo}/docs/Codemap.md")
endif()

# Keep empty lines and protect CMake list syntax so offsets match the files.
function(read_lines path out)
  file(READ "${repo}/${path}" contents)
  string(REPLACE "\r\n" "\n" contents "${contents}")
  string(REGEX REPLACE "\n$" "" contents "${contents}")
  string(REPLACE ";" "<SEMICOLON>" contents "${contents}")
  string(REPLACE "[" "<LBRACKET>" contents "${contents}")
  string(REPLACE "]" "<RBRACKET>" contents "${contents}")
  string(REPLACE "\\" "<BACKSLASH>" contents "${contents}")
  string(REPLACE "\n" ";" contents "${contents}")
  set(${out}
      "${contents}"
      PARENT_SCOPE)
endfunction()

# Map C declarations and definitions to their source line ranges.
function(map_code path out)
  # Keep the parser's line and declaration state transitions together.
  # cmake-lint: disable=R0912,R0915
  string(CONCAT tagged_typedef "^ typedef[ \t]+"
                "(struct|enum)[ \t]+([A-Za-z_][A-Za-z_0-9]*)[ \t]*[{]")
  string(CONCAT pointer_typedef
                "\\([ \t]*\\*[ \t]*([A-Za-z_][A-Za-z_0-9]*)[ \t]*\\)")
  string(CONCAT plain_typedef
                "([A-Za-z_][A-Za-z_0-9]*)[ \t]*(<LBRACKET>.*)?<SEMICOLON>$")
  string(CONCAT function_start
                "^([A-Za-z_][^(]*[^A-Za-z_0-9(])?([A-Za-z_][A-Za-z_0-9]*)\\(")
  read_lines("${path}" lines)
  set(entries "")
  set(number 0)
  set(name "")
  set(state "")
  set(in_comment FALSE)
  foreach(line IN LISTS lines)
    math(EXPR number "${number} + 1")
    # Strip block comments while preserving the column of any remaining code.
    if(in_comment)
      if(NOT line MATCHES "\\*/")
        continue()
      endif()
      string(REGEX REPLACE "^[^*]*(\\*+[^*/][^*]*)*\\*+/" " " line "${line}")
      set(in_comment FALSE)
    endif()
    string(REGEX REPLACE "/\\*[^*]*(\\*+[^*/][^*]*)*\\*+/" " " line "${line}")
    if(line MATCHES "/\\*")
      string(REGEX REPLACE "/\\*.*$" "" line "${line}")
      set(in_comment TRUE)
    endif()
    string(REGEX REPLACE "[ \t]+$" "" line "${line}")

    if(state STREQUAL "function" OR state STREQUAL "type")
      if((state STREQUAL "function" AND line STREQUAL "}")
         OR (state STREQUAL "type" AND line MATCHES "^}.*<SEMICOLON>$"))
        string(APPEND entries "- ${name} ${start}-${number}\n")
        set(state "")
      endif()
    elseif(state STREQUAL "signature")
      if(line MATCHES "<SEMICOLON>")
        set(state "")
      elseif(line MATCHES "[{]$")
        set(state "function")
      endif()
    elseif(state STREQUAL "typedef" OR line MATCHES "^typedef[ \t]")
      if(NOT state STREQUAL "typedef")
        set(start "${number}")
        set(declaration "")
      endif()
      string(APPEND declaration " ${line}")
      if(declaration MATCHES "${tagged_typedef}")
        set(name "${CMAKE_MATCH_1} ${CMAKE_MATCH_2}")
        set(state "type")
      elseif(line MATCHES "<SEMICOLON>$")
        if(declaration MATCHES "${pointer_typedef}")
          string(APPEND entries "- typedef ${CMAKE_MATCH_1} ${start}\n")
        elseif(declaration MATCHES "${plain_typedef}")
          string(APPEND entries "- typedef ${CMAKE_MATCH_1} ${start}\n")
        endif()
        set(state "")
      else()
        set(state "typedef")
      endif()
    elseif(line MATCHES "^(struct|enum)[ \t]+([A-Za-z_][A-Za-z_0-9]*)[ \t]*[{]")
      set(name "${CMAKE_MATCH_1} ${CMAKE_MATCH_2}")
      set(start "${number}")
      set(state "type")
    elseif(path MATCHES "/include/configforge/[^/]+[.]h$"
           AND line MATCHES "^#[ \t]*define[ \t]+([A-Za-z_][A-Za-z_0-9]*)")
      string(APPEND entries "- #define ${CMAKE_MATCH_1} ${number}\n")
    elseif(line MATCHES "${function_start}")
      set(name "${CMAKE_MATCH_2}")
      set(start "${number}")
      if(line MATCHES "[{]$")
        set(state "function")
      elseif(NOT line MATCHES "<SEMICOLON>")
        set(state "signature")
      endif()
    endif()
  endforeach()
  set(${out}
      "${entries}"
      PARENT_SCOPE)
endfunction()

# Map document headings and verify steps to their line ranges.
function(map_document path out)
  # Keep heading detection and range tracking in the same pass.
  # cmake-lint: disable=R0912
  read_lines("${path}" lines)
  set(entries "")
  set(number 0)
  set(children "")
  set(in_fence FALSE)
  foreach(line IN LISTS lines)
    math(EXPR number "${number} + 1")
    if(path STREQUAL "scripts/verify.cmake")
      if(NOT line MATCHES "^[ \t]*(if|elseif)\\(x STREQUAL \"([^\"]+)\"\\)$")
        continue()
      endif()
      set(level 2)
      set(heading "${CMAKE_MATCH_2}")
    elseif(line MATCHES "^[ 	]*```")
      if(in_fence)
        set(in_fence FALSE)
      else()
        set(in_fence TRUE)
      endif()
      continue()
    elseif(NOT in_fence AND line MATCHES "^(#|##|###)[ \t]+(.+)$")
      string(LENGTH "${CMAKE_MATCH_1}" level)
      set(heading "${CMAKE_MATCH_2}")
    else()
      continue()
    endif()
    math(EXPR end "${number} - 1")
    if(DEFINED start3)
      if(DEFINED start2)
        string(APPEND children "- ${heading3} ${start3}-${end}\n")
      else()
        string(APPEND entries "- ${heading3} ${start3}-${end}\n")
      endif()
      unset(start3)
    endif()
    if(level LESS 3 AND DEFINED start2)
      string(APPEND entries "- ${heading2} ${start2}-${end}\n${children}")
      unset(start2)
      set(children "")
    endif()
    if(level GREATER 1)
      set(start${level} "${number}")
      set(heading${level} "${heading}")
    endif()
  endforeach()
  if(DEFINED start2)
    string(APPEND entries "- ${heading2} ${start2}-${number}\n${children}")
  endif()
  if(DEFINED start3)
    string(APPEND entries "- ${heading3} ${start3}-${number}\n")
  endif()
  string(REPLACE "<SEMICOLON>" ";" entries "${entries}")
  string(REPLACE "<LBRACKET>" "[" entries "${entries}")
  string(REPLACE "<RBRACKET>" "]" entries "${entries}")
  string(REPLACE "<BACKSLASH>" "\\" entries "${entries}")
  set(${out}
      "${entries}"
      PARENT_SCOPE)
endfunction()

file(
  GLOB components
  LIST_DIRECTORIES TRUE
  "${repo}/lib/*")
set(code "")
foreach(component IN LISTS components)
  file(
    GLOB_RECURSE sources
    RELATIVE "${repo}"
    "${component}/src/*.c" "${component}/src/*.h")
  file(
    GLOB headers
    RELATIVE "${repo}"
    "${component}/include/configforge/*.h")
  list(APPEND code ${sources} ${headers})
endforeach()
file(
  GLOB_RECURSE tools
  RELATIVE "${repo}"
  "${repo}/tools/*.c" "${repo}/tools/*.h")
list(
  FILTER
  tools
  EXCLUDE
  REGEX
  "(^|/)(tests|generated)(/|$)")
list(APPEND code ${tools})
list(SORT code)

file(
  GLOB documents
  RELATIVE "${repo}"
  "${repo}/docs/plan/*.md")
list(
  APPEND
  documents
  docs/Roadmap.md
  docs/Plan.md
  docs/Maintaining.md
  docs/Architecture.md
  scripts/verify.cmake)
list(SORT documents)

set(page "")
foreach(path IN LISTS code documents)
  if(path IN_LIST code)
    map_code("${path}" entries)
  else()
    map_document("${path}" entries)
  endif()
  set(section "`${path}`\n${entries}")
  string(LENGTH "${section}" bytes)
  math(EXPR sort_key "1000000000 + ${bytes}")
  list(APPEND shares "${sort_key}:${path}")
  get_filename_component(directory "${path}" DIRECTORY)
  list(APPEND directories "${directory}")
  math(EXPR "bytes_${directory}" "0${bytes_${directory}} + ${bytes}")
  string(APPEND page "${section}")
endforeach()

string(LENGTH "${page}" bytes)
if(bytes GREATER 65536)
  list(SORT shares ORDER DESCENDING)
  set(largest "")
  foreach(index RANGE 0 4 1)
    list(GET shares ${index} share)
    string(REGEX MATCH "^([0-9]+):(.+)$" share "${share}")
    math(EXPR size "${CMAKE_MATCH_1} - 1000000000")
    string(APPEND largest "\n  ${CMAKE_MATCH_2}: ${size} bytes")
  endforeach()
  list(REMOVE_DUPLICATES directories)
  set(directory_shares "")
  foreach(directory IN LISTS directories)
    string(APPEND directory_shares
           "\n  ${directory}: ${bytes_${directory}} bytes")
  endforeach()
  message(
    FATAL_ERROR "Codemap: ${bytes} bytes exceeds 65536 bytes.\n"
                "Largest files:${largest}\n" "Directories:${directory_shares}")
endif()

include("${repo}/cmake/ConfigForgeVersion.cmake")
configforge_write_lf("${OUT}" "${page}")
