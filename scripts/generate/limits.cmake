# The blocks of docs/Limits.md that are read from the build and the sources.
#
#   cmake -DWRITE=docs/Limits.md -P scripts/generate/limits.cmake
#                                  replaces the blocks of that page in place
#   cmake -DOUT=<file> -P scripts/generate/limits.cmake
#                                  writes the blocks alone, between their markers
#
# A limit is stated in the code and again on a page, and a page nothing
# checks goes the way two statements of the limits already did. So the
# numbers of sections 2, 3, 4, 5, 7 and 8 are read here, each from the
# file that holds it, and gate.limits regenerates the blocks and fails
# when the page differs. A constant that is renamed, removed or reworded
# breaks the generator by name; it never changes the page quietly.
#
# Written in CMake for the reason the other scripts are: it runs wherever
# the build does.

cmake_minimum_required(VERSION 3.10)

get_filename_component(REPO "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
if(NOT DEFINED WRITE AND NOT DEFINED OUT)
  message(
    FATAL_ERROR
      "limits.cmake: give -DWRITE=<page> or -DOUT=<file> (see the top of the script)"
  )
endif()

set(MATRIX_LIB ON)
include("${REPO}/scripts/generate/matrix.cmake")

function(cf_limits_read file out)
  file(READ "${REPO}/${file}" text)
  string(REPLACE "\r" "" text "${text}")
  set("${out}"
      "${text}"
      PARENT_SCOPE)
endfunction()

# The raw text of "#define NAME <raw>", a trailing comment cut off.
function(cf_limits_define file name out)
  cf_limits_read("${file}" text)
  if(NOT text MATCHES "(^|\n)[ \t]*#[ \t]*define[ \t]+${name}[ \t]+([^\n]*)")
    message(FATAL_ERROR "${name}: no #define in ${file}")
  endif()
  set(raw "${CMAKE_MATCH_2}")
  string(REGEX REPLACE "[ \t]*/\\*.*$" "" raw "${raw}")
  string(STRIP "${raw}" raw)
  set("${out}"
      "${raw}"
      PARENT_SCOPE)
endfunction()

# The value of a raw text: a suffix after digits is dropped, (size_t) is
# dropped, a name already read is replaced by its value, 0x... is hex, and
# math(EXPR) does the rest.
function(cf_limits_eval raw out)
  string(REPLACE "(size_t)" "" raw "${raw}")
  foreach(name IN LISTS CF_LIMITS_NAMES)
    string(REPLACE "${name}" "${LIM_${name}}" raw "${raw}")
  endforeach()
  string(REGEX REPLACE "(0[xX][0-9A-Fa-f]+)[uU][lL]?" "\\1" raw "${raw}")
  string(REGEX REPLACE "([0-9])[uU][lL]?([^A-Za-z0-9_]|$)" "\\1\\2" raw
                       "${raw}")
  math(EXPR value "${raw}")
  set("${out}"
      "${value}"
      PARENT_SCOPE)
endfunction()

# One fixed row: checks the raw text, then keeps LIM_<name>, LIM_<name>_FILE.
function(cf_limits_row name file expected)
  cf_limits_define("${file}" "${name}" raw)
  if(NOT raw STREQUAL "${expected}")
    message(
      FATAL_ERROR
        "${name} is now '${raw}', not '${expected}': change the generator's row"
    )
  endif()
  cf_limits_eval("${raw}" value)
  set("LIM_${name}"
      "${value}"
      PARENT_SCOPE)
  set("LIM_${name}_FILE"
      "${file}"
      PARENT_SCOPE)
  set(CF_LIMITS_NAMES ${CF_LIMITS_NAMES} "${name}")
  set(CF_LIMITS_NAMES
      "${CF_LIMITS_NAMES}"
      PARENT_SCOPE)
endfunction()

# 1114112 -> "1.06 MiB", 68157440 -> "65 MiB", 17180000000 -> "16.0 GiB".
function(cf_limits_size bytes out)
  math(EXPR mib "${bytes} / 1048576")
  math(EXPR rest "${bytes} % 1048576")
  if(bytes GREATER_EQUAL 1073741824)
    math(EXPR tenths "${bytes} * 10 / 1073741824")
    math(EXPR whole "${tenths} / 10")
    math(EXPR frac "${tenths} % 10")
    set("${out}"
        "${whole}.${frac} GiB"
        PARENT_SCOPE)
  elseif(bytes GREATER_EQUAL 1048576 AND rest EQUAL 0)
    set("${out}"
        "${mib} MiB"
        PARENT_SCOPE)
  elseif(bytes GREATER_EQUAL 1048576)
    math(EXPR hundredths "${bytes} * 100 / 1048576")
    math(EXPR whole "${hundredths} / 100")
    math(EXPR frac "${hundredths} % 100")
    if(frac LESS 10)
      set(frac "0${frac}")
    endif()
    set("${out}"
        "${whole}.${frac} MiB"
        PARENT_SCOPE)
  else()
    set("${out}"
        "${bytes} bytes"
        PARENT_SCOPE)
  endif()
endfunction()

# A value as the page prints it: the number, and its size when it is a
# whole number of MiB.
function(cf_limits_show value out)
  math(EXPR rest "${value} % 1048576")
  if(value GREATER_EQUAL 1048576 AND rest EQUAL 0)
    cf_limits_size("${value}" size)
    set("${out}"
        "${value} (${size})"
        PARENT_SCOPE)
  else()
    set("${out}"
        "${value}"
        PARENT_SCOPE)
  endif()
endfunction()

# ---- Section 2: the build limits, three sources that are one fact ----

function(cf_limits_section2 out)
  set(options "cmake/ConfigForgeOptions.cmake")
  set(buildh "lib/port/include/configforge/cf_build_options.h")
  set(porth "lib/port/include/configforge/cf_port.h")
  cf_limits_read("${options}" optionstext)
  cf_limits_read("${buildh}" buildtext)
  cf_limits_read("${porth}" porttext)
  set(bounds_CF_KEY_MAX
      "longest key, terminator included, so one byte less of text; held by every error, the flattener and the decoders that name the path they are in"
  )
  set(bounds_CF_MESSAGE_MAX
      "longest error message; every `cf_error` holds one, and a builder call keeps one on its frame"
  )
  set(bounds_CF_PATH_MAX
      "longest file path; the directory source and the platform layer hold two or three at once"
  )
  set(bounds_CF_MAX_DEPTH
      "deepest nesting a document may have, the document counting as the first level (every reader and writer; see the depth rule below)"
  )
  set(bounds_CF_STACK_BUDGET
      "bytes a library function's stack frame may take; `0` measures without checking"
  )
  set(table
      "| Option | Default | Range | What it bounds |\n| ------ | ------: | ----- | -------------- |\n"
  )
  foreach(
    name
    CF_KEY_MAX
    CF_MESSAGE_MAX
    CF_PATH_MAX
    CF_MAX_DEPTH
    CF_STACK_BUDGET)
    if(NOT optionstext MATCHES "set\\(${name}[ \n\t]+([0-9]+)[ \n\t]+CACHE")
      message(FATAL_ERROR "${name}: no default in ${options}")
    endif()
    set(default "${CMAKE_MATCH_1}")
    if(NOT optionstext MATCHES "${name}:([0-9]+):([0-9]+)")
      message(FATAL_ERROR "${name}: no range in ${options}")
    endif()
    set(low "${CMAKE_MATCH_1}")
    set(high "${CMAKE_MATCH_2}")
    if(NOT name STREQUAL "CF_STACK_BUDGET")
      if(NOT buildtext MATCHES
         "#ifndef ${name}\n#define ${name} ([0-9]+)\n#endif")
        message(FATAL_ERROR "${name}: no default in ${buildh}")
      endif()
      set(header_default "${CMAKE_MATCH_1}")
      if(NOT porttext MATCHES
         "#if ${name} < ([0-9]+) \\|\\| ${name} > ([0-9]+)")
        message(FATAL_ERROR "${name}: no range check in ${porth}")
      endif()
      set(port_low "${CMAKE_MATCH_1}")
      set(port_high "${CMAKE_MATCH_2}")
      if(NOT header_default EQUAL default)
        message(
          FATAL_ERROR
            "${name} is ${default} in ${options} and ${header_default} in ${buildh} (its range is ${port_low} to ${port_high} in ${porth}): the CMake default, the header default and the range are one fact"
        )
      endif()
      if(NOT port_low EQUAL low OR NOT port_high EQUAL high)
        message(
          FATAL_ERROR
            "${name} ranges ${low} to ${high} in ${options} and ${port_low} to ${port_high} in ${porth}: the CMake default, the header default and the range are one fact"
        )
      endif()
    endif()
    string(
      APPEND table
      "| `${name}` | ${default} | ${low} to ${high} | ${bounds_${name}} |\n")
  endforeach()
  set(text
      "These are the options of \"Limits and the stack budget\" in the [README](../README.md#limits-and-the-stack-budget) (D-37). Each is a fixed buffer, so each is stack. The default, the range and the check are read from `${options}`, `${buildh}` and `${porth}`, and the build fails if the three disagree.\n\n${table}\nThe first three size `cf_error`'s buffers and so belong to a build's ABI (D-45): `cf_build_limit()` says which numbers a library was built with.\n"
  )
  set("${out}"
      "${text}"
      PARENT_SCOPE)
endfunction()

# ---- Sections 3, 4 and 5: constants read from the headers ----

function(cf_limits_fixed_rows)
  set(base "lib/base/include/configforge")
  set(config "lib/config/include/configforge")
  set(serialize "lib/serialize/src")
  set(rows
      "CF_SOURCE_MAX|${base}/cf_error.h|64"
      "CF_MAX_SOURCES|${config}/cf_explain.h|32"
      "CF_MAX_OPEN_PREFIXES|${config}/cf_define.h|8"
      "CF_EXPAND_MAX|${config}/cf_define.h|1048576ul"
      "CF_EXPAND_TOTAL_MAX|${config}/cf_define.h|(16ul * CF_EXPAND_MAX)"
      "CF_DIR_MAX_FILES|lib/config/src/bridge/dir.c|256"
      "CF_SECRETS_MAX_FILES|lib/config/src/bridge/secrets.c|256"
      "YP_KEY_MAX|${serialize}/yamlparse.c|1024"
      "CF_XML_INPUT_MAX|${serialize}/xml.c|0x7FFFFFFFu"
      "CF_DECODE_FACTOR|${config}/cf_flatten.h|64u"
      "CF_DECODE_FLOOR|${config}/cf_flatten.h|((size_t)1024u * 1024u)"
      "CF_DECODE_KEY_COST|${config}/cf_flatten.h|64u"
      "YAML_ALIAS_FACTOR|${serialize}/yaml.c|64u"
      "YAML_ALIAS_FLOOR|${serialize}/yaml.c|4096u"
      "YAML_ALIAS_BYTES_FACTOR|${serialize}/yaml.c|64u"
      "YAML_ALIAS_BYTES_FLOOR|${serialize}/yaml.c|((size_t)1024u * 1024u)")
  foreach(row IN LISTS rows)
    string(REPLACE "|" ";" fields "${row}")
    list(GET fields 0 name)
    list(GET fields 1 file)
    list(GET fields 2 expected)
    cf_limits_row("${name}" "${file}" "${expected}")
  endforeach()
  foreach(name IN LISTS CF_LIMITS_NAMES)
    set("LIM_${name}"
        "${LIM_${name}}"
        PARENT_SCOPE)
    set("LIM_${name}_FILE"
        "${LIM_${name}_FILE}"
        PARENT_SCOPE)
  endforeach()
  set(CF_LIMITS_NAMES
      "${CF_LIMITS_NAMES}"
      PARENT_SCOPE)
endfunction()

function(cf_limits_section3 out)
  set(bounds_CF_SOURCE_MAX
      "a source's name in an error; part of the ABI (D-45)")
  set(bounds_CF_MAX_SOURCES "the sources one `explain()` lists")
  set(bounds_CF_MAX_OPEN_PREFIXES "open key prefixes a definition set holds")
  set(bounds_CF_EXPAND_MAX
      "the bytes one value's `\${...}` may expand to (D-77)")
  set(bounds_CF_EXPAND_TOTAL_MAX "the expansion of a whole configuration")
  set(bounds_CF_DIR_MAX_FILES "files in a drop-in directory")
  set(bounds_CF_SECRETS_MAX_FILES "files in a secrets directory")
  set(bounds_YP_KEY_MAX "an implicit YAML key (spec 7.4.2)")
  set(bounds_CF_XML_INPUT_MAX "the bytes of one XML document (2 GiB less one)")
  set(held_CF_DIR_MAX_FILES "`unit.test_dirlimits`")
  set(held_CF_SECRETS_MAX_FILES "`unit.test_dirlimits`")
  set(table
      "| Name | Value | Read from | What it bounds | Held by |\n| ---- | ----: | --------- | -------------- | ------- |\n"
  )
  foreach(
    name
    CF_SOURCE_MAX
    CF_MAX_SOURCES
    CF_MAX_OPEN_PREFIXES
    CF_EXPAND_MAX
    CF_EXPAND_TOTAL_MAX
    CF_DIR_MAX_FILES
    CF_SECRETS_MAX_FILES
    YP_KEY_MAX
    CF_XML_INPUT_MAX)
    cf_limits_show("${LIM_${name}}" shown)
    set(held "-")
    if(DEFINED held_${name})
      set(held "${held_${name}}")
    endif()
    string(
      APPEND
      table
      "| `${name}` | ${shown} | `${LIM_${name}_FILE}` | ${bounds_${name}} | ${held} |\n"
    )
  endforeach()
  set(text
      "These are not build options: each is a constant in the file named, read from it. `CF_PLATFORM_NAME_MAX` is `CF_PATH_MAX` (`cf_platform.h`).\n\n${table}"
  )
  set("${out}"
      "${text}"
      PARENT_SCOPE)
endfunction()

function(cf_limits_section4 out)
  set(factor "${LIM_CF_DECODE_FACTOR}")
  set(floor "${LIM_CF_DECODE_FLOOR}")
  cf_limits_size("${floor}" floor_size)
  set(constants
      "| Name | Value | Read from | Held by |\n| ---- | ----: | --------- | ------- |\n"
  )
  foreach(name CF_DECODE_FACTOR CF_DECODE_FLOOR CF_DECODE_KEY_COST)
    cf_limits_show("${LIM_${name}}" shown)
    string(
      APPEND
      constants
      "| `${name}` | ${shown} | `${LIM_${name}_FILE}` | `unit.test_budget`, `unit.test_limits` |\n"
    )
  endforeach()
  set(computed
      "| A document of | May make at most | Arithmetic |\n| ------------- | ---------------: | ---------- |\n"
  )
  foreach(n 1024 1048576 268435456)
    math(EXPR budget "${factor} * ${n} + ${floor}")
    cf_limits_size("${n}" n_size)
    if(n LESS 1048576)
      set(n_size "1 KiB")
    endif()
    cf_limits_size("${budget}" budget_size)
    string(
      APPEND
      computed
      "| ${n_size} | ${budget_size} | ${factor} x ${n} + ${floor} = ${budget} bytes |\n"
    )
  endforeach()
  set(yaml
      "| Name | Value | Read from | What it bounds |\n| ---- | ----: | --------- | -------------- |\n"
  )
  set(bounds_YAML_ALIAS_FACTOR "events an alias may replay per event read")
  set(bounds_YAML_ALIAS_FLOOR "events, the floor of that rule")
  set(bounds_YAML_ALIAS_BYTES_FACTOR
      "bytes of scalar text an alias may replay per byte of the document")
  set(bounds_YAML_ALIAS_BYTES_FLOOR "bytes, the floor of that rule")
  foreach(name YAML_ALIAS_FACTOR YAML_ALIAS_FLOOR YAML_ALIAS_BYTES_FACTOR
               YAML_ALIAS_BYTES_FLOOR)
    cf_limits_show("${LIM_${name}}" shown)
    string(
      APPEND yaml
      "| `${name}` | ${shown} | `${LIM_${name}_FILE}` | ${bounds_${name}} |\n")
  endforeach()
  cf_limits_read("cmake/ConfigForgeOptions.cmake" optionstext)
  if(NOT optionstext MATCHES "set\\(CF_MAX_DEPTH[ \n\t]+([0-9]+)[ \n\t]+CACHE")
    message(
      FATAL_ERROR "CF_MAX_DEPTH: no default in cmake/ConfigForgeOptions.cmake")
  endif()
  set(depth "${CMAKE_MATCH_1}")
  math(EXPR inside "${depth} - 1")
  set(text
      "One document may make at most `CF_DECODE_FACTOR` times its own size, and `CF_DECODE_FLOOR` more, of keys and values, each key charged `CF_DECODE_KEY_COST` for the records that hold it (D-82). Past that the document is refused by name, whatever its format. It is not a build option: it sizes no buffer.\n\n${constants}\nSo a document of n bytes may make at most ${factor} x n + ${floor_size} (\`cf_decode_budget(n)\`):\n\n${computed}\nThe budget is the flattener's. A codec used alone, through \`cf_codec_decode\` and a sink of the program's, is not under it (D-82).\n\nYAML's aliases are bounded on their own, in \`yaml.c\`, so that a document of aliases of aliases is refused in linear time (D-63):\n\n${yaml}\n**The depth rule.** The document is level one, so \`CF_MAX_DEPTH\` ${depth} allows ${inside} levels of tables and arrays inside it, in every reader and writer; a build with more or less is another number, and \`cf_build_limit(\"depth\")\` says which. Held by `unit.test_serialize` and `unit.test_cbor`.\n"
  )
  set("${out}"
      "${text}"
      PARENT_SCOPE)
endfunction()

function(cf_limits_section5 out)
  set(text
      "The one limit a reader of its own states in a constant:\n\n| Format | What the reader refuses | Read from |\n| ------ | ----------------------- | --------- |\n| XML | a document of more than ${LIM_CF_XML_INPUT_MAX} bytes (`CF_XML_INPUT_MAX`; the call to expat takes an `int`) | `${LIM_CF_XML_INPUT_MAX_FILE}` |\n"
  )
  set("${out}"
      "${text}"
      PARENT_SCOPE)
endfunction()

# ---- Section 7: the platforms, from docs/targets.txt ----

function(cf_limits_section7 out)
  cf_targets_read("${REPO}/docs/targets.txt" "${REPO}/.github/workflows/ci.yml"
                  rows)
  set(heading_hardware "Tested on hardware")
  set(heading_runner "Tested on CI runners")
  set(heading_qemu "Tested under qemu")
  set(heading_built "Built, not run")
  set(heading_none "Not tried")
  set(text
      "Read from `docs/targets.txt` (A-12); how a target is shown is the heading it is under.\n"
  )
  foreach(
    how
    hardware
    runner
    qemu
    built
    none)
    set(table "")
    set(i 0)
    while(i LESS rows)
      math(EXPR i "${i} + 1")
      if(NOT rows_${i}_how STREQUAL how)
        continue()
      endif()
      set(note "${rows_${i}_note}")
      if(how STREQUAL "hardware")
        set(note "${rows_${i}_where}, ${rows_${i}_date}, ${rows_${i}_commit}")
      elseif(how STREQUAL "runner")
        if(rows_${i}_where STREQUAL "")
          set(note "tested by `verify`, not CI: ${note}")
        else()
          set(note "job `${rows_${i}_where}`")
        endif()
      elseif(how STREQUAL "qemu" OR how STREQUAL "built")
        set(note "job `${rows_${i}_where}`")
      endif()
      if(note STREQUAL "")
        set(note "-")
      endif()
      string(APPEND table "| ${rows_${i}_target} | ${note} |\n")
    endwhile()
    if(NOT table STREQUAL "")
      string(
        APPEND
        text
        "\n### ${heading_${how}}\n\n| Target | Note |\n| ------ | ---- |\n${table}"
      )
    endif()
  endforeach()
  set("${out}"
      "${text}"
      PARENT_SCOPE)
endfunction()

# ---- Section 8: C versions, from the cf_source_needs_c calls ----

function(cf_limits_section8 out)
  set(files "CMakeLists.txt")
  file(
    GLOB libs
    RELATIVE "${REPO}"
    "${REPO}/lib/*/CMakeLists.txt")
  list(SORT libs)
  list(APPEND files ${libs})
  set(seen "")
  set(table "")
  foreach(file IN LISTS files)
    cf_limits_read("${file}" text)
    string(REGEX REPLACE "(^|\n)[ \t]*#[^\n]*" "\\1" text "${text}")
    string(REGEX MATCHALL "cf_source_needs_c\\(" opened "${text}")
    list(LENGTH opened opened_count)
    string(
      REGEX
        MATCHALL
        "cf_source_needs_c\\([ \n\t]*[^ \n\t]+[ \n\t]+[0-9]+[ \n\t]+REASON[ \n\t]+\"[^\"]*\"[ \n\t]*\\)"
        calls
        "${text}")
    list(LENGTH calls call_count)
    if(NOT opened_count EQUAL call_count)
      message(
        FATAL_ERROR "${file}: a cf_source_needs_c call this script cannot read")
    endif()
    get_filename_component(dir "${file}" DIRECTORY)
    foreach(call IN LISTS calls)
      string(
        REGEX
        REPLACE
          "^cf_source_needs_c\\([ \n\t]*([^ \n\t]+)[ \n\t]+([0-9]+)[ \n\t]+REASON[ \n\t]+\"([^\"]*)\"[ \n\t]*\\)$"
          "\\1|\\2|\\3"
          fields
          "${call}")
      string(REPLACE "|" ";" parts "${fields}")
      list(GET parts 0 source)
      list(GET parts 1 standard)
      list(GET parts 2 reason)
      if(NOT dir STREQUAL "" AND NOT source MATCHES "^lib/")
        set(source "${dir}/${source}")
      endif()
      # the condition: the nearest enclosing if() that names a CF_WITH_ option
      string(FIND "${text}" "${call}" at)
      string(SUBSTRING "${text}" 0 ${at} before)
      string(
        REGEX MATCHALL
              "(^|[^A-Za-z0-9_:])(if[ \t]*\\([^)]*\\)|endif[ \t]*\\([^)]*\\))"
              tokens "${before}")
      set(stack "")
      foreach(token IN LISTS tokens)
        if(token MATCHES "endif[ \t]*\\(")
          list(LENGTH stack depth)
          if(depth GREATER 0)
            math(EXPR last "${depth} - 1")
            list(REMOVE_AT stack ${last})
          endif()
        else()
          string(REGEX REPLACE "^[^i]*if[ \t]*\\(" "" condition "${token}")
          list(APPEND stack "${condition}")
        endif()
      endforeach()
      set(option "-")
      list(REVERSE stack)
      foreach(condition IN LISTS stack)
        if(condition MATCHES "(CF_WITH_[A-Z0-9_]+)")
          set(option "`${CMAKE_MATCH_1}`")
          break()
        endif()
      endforeach()
      set(key "${source}|${standard}|${reason}")
      if(NOT key IN_LIST seen)
        list(APPEND seen "${key}")
        string(APPEND table
               "| `${source}` | C${standard} | ${reason} | ${option} |\n")
      endif()
    endforeach()
  endforeach()
  if(table STREQUAL "")
    message(
      FATAL_ERROR
        "no cf_source_needs_c call in CMakeLists.txt: A-11's helper is not used"
    )
  endif()
  set(text
      "Every source file is C89 (D-72, D-106) except the files below, each of which says why in a `cf_source_needs_c` call (scanned from `CMakeLists.txt` and `lib/*/CMakeLists.txt`, not from a build, so the page does not change with the options). Where a library is built alone, `lib/<name>/CMakeLists.txt` repeats the call for the same file.\n\n| File | Standard | Why | Only with |\n| ---- | -------- | --- | --------- |\n${table}"
  )
  set("${out}"
      "${text}"
      PARENT_SCOPE)
endfunction()

# ---- Assembly ----

function(cf_limits_replace page names out)
  set(result "${page}")
  foreach(n IN LISTS names)
    set(begin "<!-- limits:${n}:begin -->\n")
    set(end "<!-- limits:${n}:end -->")
    string(FIND "${result}" "${begin}" b)
    string(FIND "${result}" "${end}" e)
    if(b LESS 0 OR e LESS 0)
      message(FATAL_ERROR "the page has no limits:${n} marker pair")
    endif()
    string(LENGTH "${begin}" blen)
    math(EXPR from "${b} + ${blen}")
    string(SUBSTRING "${result}" 0 ${from} head)
    string(SUBSTRING "${result}" ${e} -1 tail)
    set(result "${head}\n${block_${n}}\n${tail}")
  endforeach()
  set("${out}"
      "${result}"
      PARENT_SCOPE)
endfunction()

set(CF_LIMITS_NAMES "")
cf_limits_fixed_rows()
cf_limits_section2(block_2)
cf_limits_section3(block_3)
cf_limits_section4(block_4)
cf_limits_section5(block_5)
cf_limits_section7(block_7)
cf_limits_section8(block_8)

set(numbers
    2
    3
    4
    5
    7
    8)
if(DEFINED WRITE)
  file(READ "${WRITE}" page)
  string(REPLACE "\r" "" page "${page}")
  cf_limits_replace("${page}" "${numbers}" result)
else()
  set(result "")
  foreach(n IN LISTS numbers)
    string(
      APPEND result
      "<!-- limits:${n}:begin -->\n\n${block_${n}}\n<!-- limits:${n}:end -->\n")
  endforeach()
endif()
set(target "${OUT}")
if(DEFINED WRITE)
  set(target "${WRITE}")
endif()
file(WRITE "${target}" "${result}")
