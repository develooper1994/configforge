# The marker-secret gate, Doctor's half. test_marker.c drives a marker
# through every way the library accepts or refuses a secret and looks for
# it in every text the library writes; this does the same for what
# Doctor prints. cf_fx_marker_a.json and _b.json are full reports of one
# configuration that holds a secret in as many states as it can at once
# (an accepted one, a reference quoted in a refusal, refused out of a
# set, too long, out of range, losing to another source, under a
# deprecated name, in a list, a number), the text MRKA7731XQ in one and
# MRKB7731XQ in the other, the digits 77317731 and 77327732. Every
# Doctor command, in every format it has and with every option that
# changes what it prints, runs on them, and nothing it says on stdout or
# stderr may hold a marker, its first six characters, or the digits, in
# any case.
#
# A command Doctor gains is one more entry in templates below, a format
# one more in formats. A leak that is known and not yet fixed goes in
# KNOWN_LEAK, by the command line, so that the suite passes and the leak
# stays written down; the list is empty when there is none.
#
# What is not run: --baseline and --severity only choose which findings
# print, and the import-* adapters and check/gen read other documents,
# not reports; formats reads nothing.

cmake_minimum_required(VERSION 3.10)

set(work "${CMAKE_CURRENT_BINARY_DIR}")

# The command lines that are known to leak: "doctor <arguments>" with
# the arguments joined by spaces, as a failure prints them.
set(KNOWN_LEAK)

set(needles
    MRKA7731XQ
    MRKB7731XQ
    MRKA77
    MRKB77
    77317731
    77327732)

set(a cf_fx_marker_a.json)
set(b cf_fx_marker_b.json)

# Every sensitive key of the pair, for the commands that take a key.
set(keys
    db.pass
    db.password
    mk.allowed
    mk.big
    mk.list.0
    mk.list.1
    mk.list.count
    mk.lose
    mk.maxlen
    mk.num
    mk.ok
    mk.ref)

# What differs between a and b, and what a refuses: the names a command
# must print, or its silence proves nothing.
set(changed
    db.pass
    db.password
    mk.list.0
    mk.list.1
    mk.lose
    mk.num
    mk.ok)
set(refused mk.allowed mk.big mk.maxlen mk.ref)

# The formats of the commands that have them.
set(formats text markdown sarif)

# <arguments>#<names it must print>. @X@ and @Y@ are the reports, taken
# as a-b and b-a (and a-a, b-b where a template has no @Y@, they are
# dropped as the same line), @K@ a key (@Q@, a few) and @F@ a format. A template with
# @K@ runs for every key; one with @F@ for every format; the names
# after the # may hold @K@ too, and "-" means none is required.
set(templates
    "dump|@X@#@keys@"
    "explain|@X@|@K@#@K@"
    "lint|@X@|--format|@F@#@refused@"
    "lint|@X@|--base|@Y@|--only-changed|--format|@F@#-"
    "diff|@X@|@Y@|--format|@F@#@changed@"
    "history|@X@|@Y@|--format|@F@#@changed@"
    "history|@X@|@Y@|--key|@Q@|--format|@F@#-"
    "history|@X@|--format|@F@#-"
    "blame|@Q@|@X@|--format|@F@#@Q@"
    "blame|@Q@|@X@|@Y@|--format|@F@#@Q@")

# Run Doctor in the fixture directory; leaves rc, and all (stdout and
# stderr together), set.
function(doctor_run)
  execute_process(
    COMMAND ${DOCTOR} ${ARGN}
    WORKING_DIRECTORY "${work}"
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err)
  set(rc
      "${rc}"
      PARENT_SCOPE)
  set(all
      "${out}${err}"
      PARENT_SCOPE)
endfunction()

# The first needle in the text, in any case, and the eighty characters
# around it; empty when there is none.
function(find_leak text result)
  string(TOLOWER "${text}" low)
  foreach(needle IN LISTS needles)
    string(TOLOWER "${needle}" want)
    string(FIND "${low}" "${want}" at)
    if(NOT at EQUAL -1)
      math(EXPR from "${at} - 40")
      if(from LESS 0)
        set(from 0)
      endif()
      string(SUBSTRING "${text}" ${from} 80 around)
      string(REGEX REPLACE "[\r\n]" " " around "${around}")
      set(${result}
          "\"${needle}\" in: ...${around}..."
          PARENT_SCOPE)
      return()
    endif()
  endforeach()
  set(${result}
      ""
      PARENT_SCOPE)
endfunction()

set(ran 0)
set(known 0)
list(LENGTH templates ntemplates)

foreach(template IN LISTS templates)
  string(REPLACE "#" ";" halves "${template}")
  list(GET halves 0 pattern)
  list(GET halves 1 wanted)
  # @Q@ is a key too, from a few that are enough for a command whose
  # cost is a process per key and format.
  set(use_keys ${keys})
  if(pattern MATCHES "@Q@")
    set(use_keys mk.ok mk.ref mk.lose db.password)
    string(REPLACE "@Q@" "@K@" pattern "${pattern}")
    string(REPLACE "@Q@" "@K@" wanted "${wanted}")
  endif()
  set(lines)
  foreach(pair "${a};${b}" "${b};${a}")
    list(GET pair 0 x)
    list(GET pair 1 y)
    set(now "${pattern}")
    string(REPLACE "@X@" "${x}" now "${now}")
    string(REPLACE "@Y@" "${y}" now "${now}")
    # One line per key, then per format, each with its own wanted names.
    if(now MATCHES "@K@")
      set(each ${use_keys})
    else()
      set(each "")
    endif()
    foreach(key IN LISTS each)
      string(REPLACE "@K@" "${key}" line "${now}")
      string(REPLACE "@K@" "${key}" want "${wanted}")
      list(APPEND lines "${line}#${want}")
    endforeach()
    if(NOT now MATCHES "@K@")
      list(APPEND lines "${now}#${wanted}")
    endif()
  endforeach()

  set(expanded)
  foreach(entry IN LISTS lines)
    string(REPLACE "#" ";" halves "${entry}")
    list(GET halves 0 line)
    list(GET halves 1 want)
    if(line MATCHES "@F@")
      foreach(format IN LISTS formats)
        string(REPLACE "@F@" "${format}" one "${line}")
        list(APPEND expanded "${one}#${want}")
      endforeach()
    else()
      list(APPEND expanded "${line}#${want}")
    endif()
  endforeach()
  list(REMOVE_DUPLICATES expanded)

  foreach(entry IN LISTS expanded)
    string(REPLACE "#" ";" halves "${entry}")
    list(GET halves 0 line)
    list(GET halves 1 want)
    string(REPLACE "|" ";" argv "${line}")
    string(REPLACE "|" " " shown "${line}")
    # A finding is not what is under test, so none may fail the run.
    list(GET argv 0 command)
    if(NOT command STREQUAL "dump" AND NOT command STREQUAL "explain")
      list(APPEND argv --fail-on never)
      set(shown "${shown} --fail-on never")
    endif()
    doctor_run(${argv})
    math(EXPR ran "${ran} + 1")
    if(NOT rc EQUAL 0)
      message("${all}")
      message(FATAL_ERROR "doctor ${shown}: exit ${rc}")
    endif()
    find_leak("${all}" leak)
    if(NOT leak STREQUAL "")
      if("doctor ${shown}" IN_LIST KNOWN_LEAK)
        message("KNOWN LEAK: doctor ${shown}: ${leak}")
        math(EXPR known "${known} + 1")
      else()
        message(FATAL_ERROR "doctor ${shown} printed a secret: ${leak}")
      endif()
    endif()
    # The test must not pass because the command printed nothing.
    string(REPLACE "@keys@" "${keys}" want "${want}")
    string(REPLACE "@changed@" "${changed}" want "${want}")
    string(REPLACE "@refused@" "${refused}" want "${want}")
    string(REPLACE " " ";" want "${want}")
    if(NOT want STREQUAL "-")
      foreach(name IN LISTS want)
        string(FIND "${all}" "${name}" at)
        if(at EQUAL -1)
          message("${all}")
          message(FATAL_ERROR "doctor ${shown} did not name ${name}")
        endif()
      endforeach()
    endif()
  endforeach()
endforeach()

# That the commands run say what the secrets did, by name and by digest.
doctor_run(
  diff
  ${a}
  ${b}
  --fail-on
  never)
foreach(name IN LISTS changed)
  if(NOT all MATCHES "CFD014[^\n]*${name}[^\n]*by digest")
    message("${all}")
    message(FATAL_ERROR "doctor diff did not say ${name} differs by digest")
  endif()
endforeach()
if(ran LESS 100)
  message(FATAL_ERROR "doctor marker: only ${ran} runs; the lists are empty?")
endif()

message("doctor marker: clean, ${ran} runs (${known} known leaks)")
