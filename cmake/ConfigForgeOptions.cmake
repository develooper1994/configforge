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
option(CF_WITH_WATCH "File watching and reload" OFF)
option(CF_WITH_CXX "C++03 wrapper header" ON)

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
function(configforge_apply_options tgt)
  foreach(
    opt
    CF_WITH_INI
    CF_WITH_JSON
    CF_WITH_TOML
    CF_WITH_XML
    CF_WITH_SQLITE
    CF_WITH_REAL
    CF_WITH_WATCH)
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
