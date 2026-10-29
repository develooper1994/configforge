# Feature selection.
#
# Every format beyond the built-in set is a compile-time option, and so
# is every part of the library a program can do without. Something
# compiled out must fail loudly when it is used, never silently: its
# functions stay, so a program still links, and each one says which
# option it needs. See Design.md, Build-Time Feature Selection.

option(CF_WITH_INI "INI subset codec (built in, no dependency)" ON)
option(CF_WITH_JSON "JSON codec (built in; Report Replay and Doctor read it)"
       ON)
option(CF_WITH_TOML "TOML subset codec (built in, no dependency)" ON)
option(CF_WITH_PROPERTIES "Java .properties reader (built in, no dependency)"
       ON)
option(CF_WITH_XML "XML codec (requires expat)" OFF)
option(CF_WITH_YAML "YAML codec (requires libyaml)" OFF)
option(CF_WITH_SQLITE "SQLite store source (requires sqlite3)" OFF)
option(CF_WITH_DIR "Drop-in directory source (conf.d)" ON)
option(CF_WITH_DOTENV ".env file source" ON)
option(CF_WITH_EMIT "Writing a configuration out (cf_emit, the encoders)" ON)
option(CF_WITH_IMAGE "Binary snapshot image, writer and reader" ON)
option(CF_WITH_REPORT
       "Provenance report, Report Replay, JSON Schema (Doctor needs it)" ON)
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

# Limits and the stack budget (D-37). Every limit is a fixed buffer, so
# every limit is stack; which way to trade is the builder's decision, and
# the build measures the result against CF_STACK_BUDGET rather than
# asserting a number of its own. cf_build_options.h says what each costs.
set(CF_KEY_MAX
    256
    CACHE STRING "Longest key in bytes, terminator included (32-65535)")
set(CF_MESSAGE_MAX
    256
    CACHE STRING "Longest error message in bytes (64-65535)")
set(CF_PATH_MAX
    512
    CACHE STRING "Longest file path in bytes (64-65535)")
set(CF_MAX_DEPTH
    32
    CACHE STRING "Deepest nesting a document may have (4-1024)")
set(CF_STACK_BUDGET
    2048
    CACHE
      STRING
      "Largest stack frame a library function may have, in bytes; 0 = unchecked"
)

foreach(
  lim
  CF_KEY_MAX:32:65535
  CF_MESSAGE_MAX:64:65535
  CF_PATH_MAX:64:65535
  CF_MAX_DEPTH:4:1024
  CF_STACK_BUDGET:0:1048576)
  string(REPLACE ":" ";" lim "${lim}")
  list(GET lim 0 name)
  list(GET lim 1 lo)
  list(GET lim 2 hi)
  if(NOT "${${name}}" MATCHES "^[0-9]+$"
     OR ${name} LESS ${lo}
     OR ${name} GREATER ${hi})
    message(
      FATAL_ERROR "${name}=${${name}}: must be a number from ${lo} to ${hi}")
  endif()
endforeach()

# Whether the limits are the ones this project's own code - tests,
# Doctor, examples - is written and checked against.
if(CF_KEY_MAX EQUAL 256
   AND CF_MESSAGE_MAX EQUAL 256
   AND CF_PATH_MAX EQUAL 512
   AND CF_MAX_DEPTH EQUAL 32)
  set(CF_DEFAULT_LIMITS ON)
else()
  set(CF_DEFAULT_LIMITS OFF)
endif()

option(CF_SINGLE_THREADED "Compile out all synchronisation" OFF)
option(CF_FORCE_PORTABLE "Disable every opportunistic upgrade" OFF)

# Every CF_WITH_* option, once. Everything that turns the options into
# something - definitions, the generated header, the SBOM - reads this
# list, so a new option is added here and nowhere else.
set(CF_FEATURES
    INI
    JSON
    TOML
    PROPERTIES
    XML
    YAML
    SQLITE
    REAL
    DIR
    DOTENV
    EMIT
    IMAGE
    REPORT)

option(CF_BUILD_TESTING "Build the test suite" ON)
option(CF_BUILD_EXAMPLES "Build the examples" ON)
option(CF_RUST
       "Run the Rust binding's examples (needs cargo; downloads nothing)" OFF)
option(CF_ENABLE_GATES "Run conformance and symbol gates in CI" ON)

# The definitions as a list of -D flags, for the places that compile by
# hand rather than through a target: the differential amalgamation check
# has to build both halves with the same options, or it is comparing two
# different libraries rather than two builds of one.
set(_CF_OPTION_DEFS "")
foreach(f ${CF_FEATURES})
  if(CF_WITH_${f})
    list(APPEND _CF_OPTION_DEFS "-DCF_WITH_${f}=1")
  else()
    list(APPEND _CF_OPTION_DEFS "-DCF_WITH_${f}=0")
  endif()
endforeach()
foreach(lim CF_KEY_MAX CF_MESSAGE_MAX CF_PATH_MAX CF_MAX_DEPTH)
  list(APPEND _CF_OPTION_DEFS "-D${lim}=${${lim}}")
endforeach()
if(CF_FORCE_PORTABLE)
  list(APPEND _CF_OPTION_DEFS "-DCF_FORCE_PORTABLE=1")
endif()
if(CF_SINGLE_THREADED)
  list(APPEND _CF_OPTION_DEFS "-DCF_SINGLE_THREADED=1")
endif()
string(REPLACE ";" "|" _CF_OPTION_DEFS "${_CF_OPTION_DEFS}")

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
  foreach(f ${CF_FEATURES})
    if(CF_WITH_${f})
      set(CF_OPT_${f} 1)
    else()
      set(CF_OPT_${f} 0)
    endif()
  endforeach()

  # Whether this build is a shared library is a fact about it, like the
  # formats: a consumer of the installed DLL needs dllimport, and gets it
  # from the header rather than from remembering a -D.
  if(BUILD_SHARED_LIBS)
    set(CF_OPT_SHARED 1)
  else()
    set(CF_OPT_SHARED 0)
  endif()

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
  foreach(f ${CF_FEATURES})
    if(CF_WITH_${f})
      target_compile_definitions(${tgt} PUBLIC CF_WITH_${f}=1)
    else()
      target_compile_definitions(${tgt} PUBLIC CF_WITH_${f}=0)
    endif()
  endforeach()
  foreach(lim CF_KEY_MAX CF_MESSAGE_MAX CF_PATH_MAX CF_MAX_DEPTH)
    target_compile_definitions(${tgt} PUBLIC ${lim}=${${lim}})
  endforeach()

  if(CF_SINGLE_THREADED)
    target_compile_definitions(${tgt} PUBLIC CF_SINGLE_THREADED=1)
  endif()
  if(CF_FORCE_PORTABLE)
    target_compile_definitions(${tgt} PUBLIC CF_FORCE_PORTABLE=1)
  endif()
endfunction()
