# Feature selection.
#
# Every format beyond the built-in set is a compile-time option, and a
# format that is compiled out must fail loudly when a source needs it,
# never silently. See the Build-Time Feature Selection section.

option(CF_WITH_INI "INI subset codec (built in, no dependency)" ON)
option(CF_WITH_JSON "JSON codec (built in; required by reports)" ON)
option(CF_WITH_TOML "TOML codec" OFF)
option(CF_WITH_XML "XML codec (requires expat)" OFF)
option(CF_WITH_SQLITE "SQLite store source (requires sqlite3)" OFF)
set(CF_PLATFORM
    "auto"
    CACHE STRING "posix, win32, none, or auto")
set_property(
  CACHE CF_PLATFORM
  PROPERTY STRINGS
           auto
           posix
           win32
           none)

option(CF_WITH_REAL "CF_REAL value type and its conversions" ON)
option(CF_WITH_CXX "C++03 wrapper header" ON)
# The kqueue change handle on Linux, through libkqueue: how the macOS and
# BSD implementation is compiled and run by a machine that is neither.
# A test build. Nothing needs it on a BSD, where kqueue is the default.
option(CF_WAKE_KQUEUE "Use kqueue for the change handle (libkqueue on Linux)"
       OFF)

option(CF_SINGLE_THREADED "Compile out all synchronisation" OFF)
option(CF_FORCE_PORTABLE "Disable every opportunistic upgrade" OFF)

option(CF_BUILD_TESTING "Build the test suite" ON)
option(CF_BUILD_EXAMPLES "Build the examples" ON)
option(CF_ENABLE_GATES "Run conformance and symbol gates in CI" ON)

# Turn the CF_WITH_* options into compile definitions on a target.
#
# ~~~
# configforge_apply_options(<target>)
# ~~~
# The same definitions as a list of -D flags, for the places that
# compile by hand rather than through a target: the differential
# amalgamation check has to build both halves with the same options, or
# it is comparing two different libraries rather than two builds of one.
set(_CF_OPTION_DEFS "")
foreach(
  opt
  CF_WITH_INI
  CF_WITH_JSON
  CF_WITH_TOML
  CF_WITH_XML
  CF_WITH_SQLITE
  CF_WITH_REAL)
  if(${opt})
    list(APPEND _CF_OPTION_DEFS "-D${opt}=1")
  else()
    list(APPEND _CF_OPTION_DEFS "-D${opt}=0")
  endif()
endforeach()
if(CF_FORCE_PORTABLE)
  list(APPEND _CF_OPTION_DEFS "-DCF_FORCE_PORTABLE=1")
endif()
if(CF_SINGLE_THREADED)
  list(APPEND _CF_OPTION_DEFS "-DCF_SINGLE_THREADED=1")
endif()
string(REPLACE ";" "|" _CF_OPTION_DEFS "${_CF_OPTION_DEFS}")

# Turn the CF_WITH_* options into compile definitions on a target.
#
# ~~~
# configforge_apply_options(<target>)
# ~~~
# Generate the header that records which features this build has, and
# put it ahead of the source tree's copy. The source copy holds defaults
# behind #ifndef guards for someone compiling the sources themselves;
# this one states facts about a library that is already compiled, so a
# consumer cannot disagree with it by accident.
#
# ~~~
# configforge_generate_options(<target>)
# ~~~
function(configforge_generate_options tgt)
  foreach(
    name
    REAL
    INI
    JSON
    TOML
    XML
    SQLITE)
    if(CF_WITH_${name})
      set(CF_OPT_${name} 1)
    else()
      set(CF_OPT_${name} 0)
    endif()
  endforeach()

  set(gen "${CMAKE_BINARY_DIR}/generated/configforge")
  configure_file("${CMAKE_SOURCE_DIR}/cmake/cf_build_options.h.in"
                 "${gen}/cf_build_options.h" @ONLY)

  # Ahead of the source include directory, so the generated file wins
  # inside this build; consumers get the installed copy, which is this
  # one.
  target_include_directories(
    ${tgt} BEFORE PUBLIC $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated>)
endfunction()

# Turn the CF_WITH_* options into compile definitions on a target.
#
# ~~~
# configforge_apply_options(<target>)
# ~~~
function(configforge_apply_options tgt)
  foreach(
    opt
    CF_WITH_INI
    CF_WITH_JSON
    CF_WITH_TOML
    CF_WITH_XML
    CF_WITH_SQLITE
    CF_WITH_REAL)
    if(${opt})
      target_compile_definitions(${tgt} PUBLIC ${opt}=1)
    else()
      target_compile_definitions(${tgt} PUBLIC ${opt}=0)
    endif()
  endforeach()

  if(CF_SINGLE_THREADED)
    target_compile_definitions(${tgt} PUBLIC CF_SINGLE_THREADED=1)
  endif()
  if(CF_FORCE_PORTABLE)
    target_compile_definitions(${tgt} PUBLIC CF_FORCE_PORTABLE=1)
  endif()
endfunction()
