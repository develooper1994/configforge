# The API reference, generated from the public headers.
#
#   cmake -P scripts/apiref.cmake                 write docs/API.md
#   cmake -DOUT=<file> -P scripts/apiref.cmake    write it somewhere else
#
# The header comments are the API documentation (CONTRIBUTING.md says so), and
# a reference kept by hand beside them is a second copy that drifts. So
# this one is extracted: every CF_API declaration, with the comment
# written directly above it, grouped under the section banners of the
# header it is in. gate.apiref regenerates it and fails when the result
# differs from docs/API.md, so the file cannot fall behind the headers.
#
# Written in CMake for the reason the other scripts are: it runs wherever
# the build does. No Doxygen: the comments are prose, not markup, and
# should stay that way.

cmake_minimum_required(VERSION 3.10)

get_filename_component(REPO "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
if(NOT DEFINED OUT)
  set(OUT "${REPO}/docs/API.md")
endif()

# The facade first, as a reader meets it; the rest by name.
file(
  GLOB names
  RELATIVE "${REPO}/include/configforge"
  "${REPO}/include/configforge/*.h")
list(SORT names)
list(REMOVE_ITEM names "configforge.h")
set(names "configforge.h;${names}")

# A comment's text without its delimiters and the leading " * " or
# indentation of each line.
function(clean_comment raw out)
  string(REGEX REPLACE "^[ \t]*/\\*+[ \t]?" "" t "${raw}")
  string(REGEX REPLACE "[ \t]*\\*+/[ \t]*$" "" t "${t}")
  string(REPLACE "<NL>" "\n" t "${t}")
  if(t MATCHES "\n[ \t]*\\*")
    string(REGEX REPLACE "\n[ \t]*\\* ?" "\n" t "${t}")
  else()
    string(REPLACE "\n   " "\n" t "${t}")
  endif()
  string(STRIP "${t}" t)
  set(${out}
      "${t}"
      PARENT_SCOPE)
endfunction()

# CMake reads semicolons and brackets as list syntax, so they are put
# away before a file becomes a list of lines and put back on output.
function(restore text out)
  string(REPLACE "<SEMI>" ";" t "${text}")
  string(REPLACE "<LB>" "[" t "${t}")
  string(REPLACE "<RB>" "]" t "${t}")
  set(${out}
      "${t}"
      PARENT_SCOPE)
endfunction()

# Prose for Markdown: restored, and with "<" escaped, which a renderer
# would otherwise take for the start of a tag and hide - the comments
# say "<sensitive>" in exactly the place it matters.
function(prose text out)
  restore("${text}" t)
  string(REPLACE "<" "&lt;" t "${t}")
  set(${out}
      "${t}"
      PARENT_SCOPE)
endfunction()

set(doc "")
string(
  APPEND
  doc
  "# API reference\n\n"
  "Generated from the public headers by `scripts/apiref.cmake`, and\n"
  "checked against them by `gate.apiref`: edit the comment in the\n"
  "header, then run the script. Every function is listed with the\n"
  "comment written above it, under the section of the header it is\n"
  "in; the types are in the headers themselves.\n\n"
  "Semantic versioning covers all of it (D-56) except the internal\n"
  "tier, which the library's own layers use and 1.x may change:\n"
  "`cf_flatten.h`, `cf_keyspace.h`, `cf_numeric.h`, `cf_platform.h`,\n"
  "the definition set's `cf_defset_*` and `cf_def_*` in `cf_define.h`,\n"
  "and `cf_port.h`'s `cf_ref_*`, `cf_fmt*` and `cf_cmp_*`. The flags a\n"
  "definition takes are bits 0-15 and a source's bits 16 and up; an\n"
  "unknown bit is refused by name.\n\n")

# The error codes, from the one list every other place takes them from,
# so the table cannot name a code the library does not have.
file(STRINGS "${REPO}/include/configforge/cf_code.def" codes
     REGEX "^CF_CODE_(LAST_)?X\\(CF_")
string(
  APPEND
  doc
  "Every error carries one of these codes, stable from 1.0;\n"
  "`cf_code_name` and `cf_code_description` give the last two columns.\n\n"
  "| Code | Name | Meaning |\n"
  "| ---- | ---- | ------- |\n")
foreach(l IN LISTS codes)
  if(l MATCHES "\\((CF_[A-Z_]+), *\"([a-z]+)\", *\"([^\"]*)\"\\)")
    string(APPEND doc "| `${CMAKE_MATCH_1}` | `${CMAKE_MATCH_2}` | "
           "${CMAKE_MATCH_3} |\n")
  endif()
endforeach()
string(APPEND doc "\n")
set(toc "")
set(sections "")

set(total 0)
foreach(h ${names})
  file(READ "${REPO}/include/configforge/${h}" text)
  string(REPLACE ";" "<SEMI>" text "${text}")
  string(REPLACE "[" "<LB>" text "${text}")
  string(REPLACE "]" "<RB>" text "${text}")
  string(REPLACE "\n" ";" lines "${text}")

  set(section "")
  set(intro "")
  set(comment "")
  set(in_comment FALSE)
  set(raw "")
  set(decl "")
  set(in_decl FALSE)
  set(group "")
  set(group_comment "")
  set(body "")
  set(count 0)

  foreach(l IN LISTS lines)
    if(in_comment)
      string(APPEND raw "<NL>${l}")
      if(l MATCHES "\\*/")
        set(in_comment FALSE)
        clean_comment("${raw}" c)
        if(raw MATCHES "^/\\* -+ \\*<NL> \\* ([^<]+)<NL>")
          # A section banner: its second line is the title.
          set(title "${CMAKE_MATCH_1}")
          string(STRIP "${title}" title)
          string(REGEX REPLACE "^-+ \\*\n[^\n]*\n?" "" c "${c}")
          string(REGEX REPLACE "\n?-+[ *]*$" "" c "${c}")
          string(STRIP "${c}" c)
          prose("${c}" c)
          string(APPEND body "\n### ${title}\n\n")
          if(NOT c STREQUAL "")
            string(APPEND body "${c}\n\n")
          endif()
          set(comment "")
        elseif(intro STREQUAL "" AND body STREQUAL "")
          prose("${c}" intro)
          set(comment "")
        else()
          set(comment "${c}")
        endif()
      endif()
      continue()
    endif()
    if(in_decl)
      string(STRIP "${l}" s)
      string(APPEND decl " ${s}")
      if(l MATCHES "<SEMI>")
        set(in_decl FALSE)
        list(APPEND group "${decl}")
      endif()
      continue()
    endif()
    if(l MATCHES "^[ \t]*/\\*")
      # A new comment ends any group the previous one introduced.
      if(group)
        string(REPLACE ";" "\n" g "${group}")
        restore("${g}" g)
        string(APPEND body "```c\n${g}\n```\n\n")
        if(NOT group_comment STREQUAL "")
          prose("${group_comment}" gc)
          string(APPEND body "${gc}\n\n")
        endif()
        set(group "")
      endif()
      set(raw "${l}")
      if(l MATCHES "\\*/")
        clean_comment("${raw}" c)
        set(comment "${c}")
      else()
        set(in_comment TRUE)
      endif()
      continue()
    endif()
    if(l MATCHES "^(CF_NODISCARD[ \t]+)?CF_(DOCTOR_)?API[ \t]")
      if(NOT group)
        set(group_comment "${comment}")
        set(comment "")
      endif()
      math(EXPR count "${count} + 1")
      string(REGEX REPLACE "[ \t]+" " " decl "${l}")
      if(l MATCHES "<SEMI>")
        list(APPEND group "${decl}")
      else()
        set(in_decl TRUE)
      endif()
      continue()
    endif()
    # Anything else - a blank line, a type, a macro - ends a group, and a
    # comment that was not followed by a declaration belongs to nothing
    # listed here.
    string(STRIP "${l}" s)
    if(s MATCHES "^#if|^#endif|^#else")
      continue() # a declaration under a feature test still belongs
    endif()
    if(group)
      string(REPLACE ";" "\n" g "${group}")
      restore("${g}" g)
      string(APPEND body "```c\n${g}\n```\n\n")
      if(NOT group_comment STREQUAL "")
        prose("${group_comment}" gc)
        string(APPEND body "${gc}\n\n")
      endif()
      set(group "")
    endif()
    if(NOT s STREQUAL "")
      set(comment "")
    endif()
  endforeach()

  if(count EQUAL 0)
    continue()
  endif()
  math(EXPR total "${total} + ${count}")
  string(REGEX REPLACE "[^a-z0-9_-]" "" anchor "${h}")
  string(APPEND toc "- [`${h}`](#${anchor}) - ${count}\n")
  string(APPEND sections "## ${h}\n\n")
  if(NOT intro STREQUAL "")
    string(APPEND sections "${intro}\n\n")
  endif()
  string(APPEND sections "${body}")
endforeach()

string(APPEND doc "${toc}\n${sections}")
restore("${doc}" doc)
string(REGEX REPLACE "\n\n\n+" "\n\n" doc "${doc}")
# LF on every system: file(WRITE) writes CRLF on Windows, which the text
# gate refuses and git rewrites on every commit.
include("${REPO}/cmake/ConfigForgeVersion.cmake")
configforge_write_lf("${OUT}" "${doc}")
message("apiref: ${total} functions from ${OUT}")
