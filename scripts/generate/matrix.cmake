cmake_minimum_required(VERSION 3.10)

# Read the target rows and check their jobs against the CI workflow.
# One pass over the rows, each check beside the field it reads.
# cmake-lint: disable=C0103,R0912,R0915
function(cf_targets_read file workflow out)
  # cmake-lint: disable=C0103
  # Exported field names include the output prefix and row number.
  file(STRINGS "${workflow}" workflow_lines)
  set(jobs "")
  set(in_jobs OFF)
  foreach(line IN LISTS workflow_lines)
    if(line MATCHES "^jobs:")
      set(in_jobs ON)
    elseif(in_jobs AND line MATCHES "^[^ #\t]")
      break()
    elseif(in_jobs AND line MATCHES "^  ([A-Za-z0-9_-]+):")
      list(APPEND jobs "${CMAKE_MATCH_1}")
    endif()
  endforeach()

  file(READ "${file}" contents)
  string(REPLACE "\r" "" contents "${contents}")
  string(APPEND contents "\n")
  set(number 0)
  set(count 0)
  while(NOT contents STREQUAL "")
    string(REGEX MATCH "^([^\n]*)\n" line "${contents}")
    string(LENGTH "${line}" line_length)
    set(line "${CMAKE_MATCH_1}")
    string(SUBSTRING "${contents}" ${line_length} -1 contents)
    math(EXPR number "${number} + 1")
    if(line MATCHES "^[ \t]*(#|$)")
      continue()
    endif()
    set(location "targets.txt:${number}")
    if(NOT line MATCHES
       "^([^|]+)[|]([^|]*)[|]([^|]*)[|]([^|]*)[|]([^|]*)[|]([^|]*)$")
      message(FATAL_ERROR "${location}: expected six fields and a target name")
    endif()
    set(target "${CMAKE_MATCH_1}")
    set(how "${CMAKE_MATCH_2}")
    set(where "${CMAKE_MATCH_3}")
    set(date "${CMAKE_MATCH_4}")
    set(commit "${CMAKE_MATCH_5}")
    set(note "${CMAKE_MATCH_6}")
    if(NOT how MATCHES "^(hardware|runner|qemu|built|none)$")
      message(
        FATAL_ERROR
          "${location}: how '${how}' is not one of hardware, runner, qemu, "
          "built, none")
    endif()
    string(MD5 key "${target}")
    if(DEFINED seen_${key})
      message(FATAL_ERROR "${location}: target '${target}' is there twice")
    endif()
    set(seen_${key} ON)
    if(how STREQUAL "hardware")
      string(CONCAT commit_pattern
                    "^[0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f]"
                    "[0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f]+$")
      if(where STREQUAL ""
         OR NOT date MATCHES "^[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]$"
         OR NOT commit MATCHES "${commit_pattern}")
        message(
          FATAL_ERROR
            "${location}: a hardware line needs a board, a date YYYY-MM-DD and "
            "a commit")
      endif()
    elseif(how STREQUAL "none")
      if(NOT where STREQUAL ""
         OR NOT date STREQUAL ""
         OR NOT commit STREQUAL "")
        message(
          FATAL_ERROR
            "${location}: a 'none' line names no job, board, date or commit")
      endif()
    elseif(how STREQUAL "runner" AND where STREQUAL "")
      if(note STREQUAL "")
        message(
          FATAL_ERROR "${location}: a runner line with no job needs a note")
      endif()
    elseif(NOT where IN_LIST jobs)
      message(FATAL_ERROR "${location}: job '${where}' is not in ci.yml")
    endif()
    math(EXPR count "${count} + 1")
    if(how STREQUAL "runner" AND where STREQUAL "")
      set(how "local")
    endif()
    foreach(
      field
      target
      how
      where
      date
      commit
      note)
      set("${out}_${count}_${field}"
          "${${field}}"
          PARENT_SCOPE)
    endforeach()
  endwhile()
  set("${out}"
      "${count}"
      PARENT_SCOPE)
endfunction()

# Replace the named generated blocks in the README.
function(
  cf_matrix_replace
  readme
  names
  prefix
  out)
  set(active "")
  set(seen "")
  set(result "")
  while(NOT readme STREQUAL "")
    string(REGEX MATCH "^[^\n]*\n?" line "${readme}")
    string(LENGTH "${line}" line_length)
    string(SUBSTRING "${readme}" ${line_length} -1 readme)
    if(line MATCHES "^<!-- matrix:(begin|end) ([^ \n]+) -->\n?$")
      set(kind "${CMAKE_MATCH_1}")
      set(name "${CMAKE_MATCH_2}")
      if(NOT name IN_LIST names)
        message(FATAL_ERROR "README.md: marker '${name}' is unknown")
      endif()
      if(kind STREQUAL "begin")
        if(name IN_LIST seen)
          message(FATAL_ERROR "README.md: marker '${name}' is duplicated")
        endif()
        if(NOT active STREQUAL "")
          message(FATAL_ERROR "README.md: marker '${active}' is unpaired")
        endif()
        set(active "${name}")
        list(APPEND seen "${name}")
        string(APPEND result "${line}${${prefix}_${name}}")
      else()
        if(NOT name STREQUAL active)
          if(name IN_LIST seen)
            message(FATAL_ERROR "README.md: marker '${name}' is duplicated")
          endif()
          message(FATAL_ERROR "README.md: marker '${name}' is unpaired")
        endif()
        set(active "")
        string(APPEND result "${line}")
      endif()
    elseif(active STREQUAL "")
      string(APPEND result "${line}")
    endif()
  endwhile()
  if(NOT active STREQUAL "")
    message(FATAL_ERROR "README.md: marker '${active}' is unpaired")
  endif()
  foreach(name IN LISTS names)
    if(NOT name IN_LIST seen)
      message(FATAL_ERROR "README.md: marker '${name}' is missing")
    endif()
  endforeach()
  # cmake-lint: disable=C0103
  set("${out}"
      "${result}"
      PARENT_SCOPE)
endfunction()

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/ConfigForgeProfile.cmake")

# Sizes are decimal kilobytes, rounded half up to a tenth.
function(cf_matrix_kb bytes out)
  if("${bytes}" STREQUAL "" OR "${bytes}" STREQUAL "-")
    set(value "-")
  else()
    if(bytes LESS 0)
      math(EXPR magnitude "0 - ${bytes}")
      set(sign "-")
    else()
      set(magnitude "${bytes}")
      set(sign "")
    endif()
    math(EXPR tenths "(${magnitude} + 50) / 100")
    math(EXPR whole "${tenths} / 10")
    math(EXPR decimal "${tenths} % 10")
    set(value "${sign}${whole}.${decimal} KB")
  endif()
  # cmake-lint: disable=C0103
  set("${out}"
      "${value}"
      PARENT_SCOPE)
endfunction()

# Write the profile table from the checked-in profiles and sizes.
function(cf_matrix_profiles root)
  string(
    CONCAT
      profiles
      "| Profile | Library code | What it is for |\n| ------- | -----------: | "
      "-------------- |\n")
  set(header "| Option |")
  set(rule "| ------ |")
  foreach(profile IN LISTS CF_PROFILES)
    set(file "${root}/cmake/profiles/${profile}.cmake")
    cf_profile_parse("${file}" "profile_${profile}")
    file(STRINGS "${file}" comments REGEX "^# ")
    foreach(comment IN LISTS comments)
      if(NOT comment MATCHES "^# cmake-format:")
        string(SUBSTRING "${comment}" 2 -1 description)
        break()
      endif()
    endforeach()
    cf_matrix_kb("${size_profile_${profile}}" kb)
    string(APPEND profiles "| `${profile}` | ${kb} | ${description} |\n")
    string(APPEND header " ${profile} |")
    string(REGEX REPLACE "." "-" dashes "${profile}")
    string(APPEND rule " ${dashes} |")
  endforeach()
  set(features "${header}\n${rule}\n")
  foreach(entry IN LISTS CF_PROFILE_OPTIONS)
    string(REGEX REPLACE ":.*" "" name "${entry}")
    string(APPEND features "| `${name}` |")
    foreach(profile IN LISTS CF_PROFILES)
      set(value "${profile_${profile}_${name}}")
      if(value MATCHES "^(ON|OFF)$")
        string(TOLOWER "${value}" value)
      endif()
      string(APPEND features " ${value} |")
    endforeach()
    string(APPEND features "\n")
  endforeach()
  set(profiles
      "${profiles}"
      PARENT_SCOPE)
  set(features
      "${features}"
      PARENT_SCOPE)
endfunction()

# List the build options in a wrapped paragraph.
function(cf_matrix_options out)
  set(names "${_cf_option_names}")
  list(
    REMOVE_ITEM
    names
    CF_KEY_MAX
    CF_MESSAGE_MAX
    CF_PATH_MAX
    CF_MAX_DEPTH
    CF_STACK_BUDGET)
  list(SORT names)
  set(line "Options:")
  set(result "")
  list(LENGTH names remaining)
  foreach(name IN LISTS names)
    set(word "`${name}`")
    math(EXPR remaining "${remaining} - 1")
    if(remaining EQUAL 0)
      string(APPEND word ".")
    endif()
    string(LENGTH "${line} ${word}" line_length)
    if(line_length GREATER 72)
      string(APPEND result "${line}\n")
      set(line "${word}")
    else()
      string(APPEND line " ${word}")
    endif()
  endforeach()
  # cmake-lint: disable=C0103
  set("${out}"
      "${result}${line}\nThe limits are in the table below.\n"
      PARENT_SCOPE)
endfunction()

# Write the optional parts table in order of measured savings.
function(cf_matrix_parts out)
  if(arch)
    set(title "Code it saves (${arch}, `-Os`)")
  else()
    set(title "Code it saves")
  endif()
  string(CONCAT parts
                "| Option | What goes | ${title} |\n| ------ | --------- | "
                "------------------------------: |\n")
  set(pending "${size_parts}")
  list(SORT pending)
  # The first of equal savings is already first alphabetically. Unknowns go
  # last.
  while(pending)
    list(GET pending 0 best)
    foreach(name IN LISTS pending)
      if(NOT size_off_${name} STREQUAL "-"
         AND (size_off_${best} STREQUAL "-" OR size_off_${name} GREATER
                                               size_off_${best}))
        set(best "${name}")
      endif()
    endforeach()
    list(REMOVE_ITEM pending "${best}")
    cf_option_help("${best}" help)
    cf_matrix_kb("${size_off_${best}}" kb)
    string(APPEND parts "| `${best}` | ${help} | ${kb} |\n")
  endwhile()
  if(arch)
    math(EXPR before "(${size_base_default} + 500) / 1000")
    math(EXPR after "(${size_all_parts} + 500) / 1000")
    string(APPEND parts
           "\nLeaving out TOML, EMIT, REAL, REPORT, IMAGE, INI, DOTENV and DIR "
           "together takes the library from ${before} KB to ${after} KB.\n")
  else()
    string(APPEND parts "\nCI's `sizes` job measures these figures.\n")
  endif()
  # cmake-lint: disable=C0103
  set("${out}"
      "${parts}"
      PARENT_SCOPE)
endfunction()

# Write the target support table from the checked target rows.
function(cf_matrix_targets root out)
  cf_targets_read("${root}/docs/targets.txt" "${root}/.github/workflows/ci.yml"
                  rows)
  string(CONCAT header "| Target | Shown by | Where | Note |\n| ------ | "
                "-------- | ----- | ---- " "|\n")
  set(table "${header}")
  set(local_rows "")
  set(i 0)
  while(i LESS rows)
    math(EXPR i "${i} + 1")
    set(how "${rows_${i}_how}")
    set(where "${rows_${i}_where}")
    set(note "${rows_${i}_note}")
    set(location "-")
    if(how STREQUAL "hardware")
      set(shown "hardware (${where}, ${rows_${i}_date}, ${rows_${i}_commit})")
    elseif(how STREQUAL "none")
      set(shown "not tried")
    elseif(how STREQUAL "local")
      string(APPEND local_rows "| ${rows_${i}_target} | ${note} |\n")
      continue()
    else()
      set(location "job `${where}`")
      if(how STREQUAL "runner")
        set(shown "CI runner")
      elseif(how STREQUAL "built")
        set(shown "built only")
      else()
        set(shown "qemu")
      endif()
    endif()
    if(note STREQUAL "")
      set(note "-")
    endif()
    string(APPEND table
           "| ${rows_${i}_target} | ${shown} | ${location} | ${note} |\n")
  endwhile()
  if(local_rows)
    string(APPEND table
           "\n### Run locally\n\n| Target | Note |\n| ------ | ---- |\n"
           "${local_rows}")
  endif()
  # cmake-lint: disable=C0103
  set("${out}"
      "${table}"
      PARENT_SCOPE)
endfunction()

# Generate the README blocks from the repository inputs.
function(cf_matrix_tables root prefix)
  # cmake-lint: disable=C0103
  # Size keys combine the report variant and target name.
  cf_option_defaults()
  set(arch "")
  set(size_parts "")
  if(EXISTS "${root}/docs/sizes.txt")
    file(STRINGS "${root}/docs/sizes.txt" lines)
    foreach(line IN LISTS lines)
      if(line MATCHES "^tool[|]([^|]+)[|]")
        set(arch "${CMAKE_MATCH_1}")
      elseif(line MATCHES "^(base|off|all|profile)[|]([^|]+)[|](-?[0-9]+|-)$")
        set("size_${CMAKE_MATCH_1}_${CMAKE_MATCH_2}" "${CMAKE_MATCH_3}")
        if(CMAKE_MATCH_1 STREQUAL "off")
          list(APPEND size_parts "${CMAKE_MATCH_2}")
        endif()
      endif()
    endforeach()
  else()
    set(size_parts "${CF_SIZE_PARTS}")
    foreach(name IN LISTS size_parts)
      set(size_off_${name} "-")
    endforeach()
  endif()
  cf_matrix_profiles("${root}")
  cf_matrix_options(options)
  cf_matrix_parts(parts)
  cf_matrix_targets("${root}" targets)
  foreach(
    name
    profiles
    features
    options
    parts
    targets)
    set("${prefix}_${name}"
        "${${name}}"
        PARENT_SCOPE)
  endforeach()
endfunction()

if(NOT MATRIX_LIB)
  get_filename_component(root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
  if(NOT DEFINED OUT)
    set(OUT "${root}/README.md")
  endif()
  cf_matrix_tables("${root}" block)
  file(READ "${root}/README.md" readme)
  cf_matrix_replace("${readme}" "targets;profiles;features;options;parts" block
                    generated)
  if(EXISTS "${OUT}")
    file(READ "${OUT}" previous)
    if(previous STREQUAL generated)
      return()
    endif()
  endif()
  include("${root}/cmake/ConfigForgeVersion.cmake")
  configforge_write_lf("${OUT}" "${generated}")
endif()
