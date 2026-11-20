# configforge-gen: the contract's program. Every test runs where
# doctor_fixtures writes the contract fixtures (tests/doctor), after it.

# Register one configforge-gen invocation as a test.
#
# ~~~
# gen_case(<name> <expected-exit-code> <arg>...)
# ~~~
function(gen_case name expect_fail)
  add_test(
    NAME gen.${name}
    COMMAND
      ${CMAKE_COMMAND} -DDOCTOR=$<TARGET_FILE:configforge_gen> -DARGS=${ARGN}
      -DEXPECT_FAIL=${expect_fail} -P ${CMAKE_SOURCE_DIR}/tests/doctor/run.cmake
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/tests/doctor)
  set_tests_properties(gen.${name} PROPERTIES FIXTURES_REQUIRED DOCTOR_REPORTS)
endfunction()

# Files against a JSON Schema contract (contract.c): a file that holds to
# it, one with a misspelt key and one with a value out of range, and a
# schema that is not there, which is unusable input. The files are
# fixtures.c's.
set(contract ${CMAKE_SOURCE_DIR}/tests/doctor/contract.schema.json)
gen_case(check_ok 0 check|--schema|${contract}|cf_fx_contract_ok.json)
gen_case(check_typo 1 check|--schema|${contract}|cf_fx_contract_typo.json)
gen_case(check_range 1 check|--schema|${contract}|cf_fx_contract_range.json)
gen_case(
  check_layers 1
  check|--schema|${contract}|cf_fx_contract_ok.json|cf_fx_contract_range.json)
gen_case(check_no_schema 2
         check|--schema|cf_fx_none.json|cf_fx_contract_ok.json)
gen_case(check_alone 2 check|cf_fx_contract_ok.json)

# A contract drafted from a sample file (`gen schema --from`, D-96), each
# case in the script's own directory; a format this build left out skips
# the cases that read it.
add_test(
  NAME gen.schema
  COMMAND
    ${CMAKE_COMMAND} -DGEN=$<TARGET_FILE:configforge_gen>
    -DWORK=${CMAKE_CURRENT_BINARY_DIR}/schema_work -DTOML=${CF_WITH_TOML}
    -DINI=${CF_WITH_INI} -P ${CMAKE_CURRENT_SOURCE_DIR}/schema.cmake)

# An option it does not know is the usage.
gen_case(usage 2 --nonsense)

# `--lang` finds its language in the table (D-95): C, C++, Rust and Python are
# there, a name that is not is unusable input, and a name the language cannot
# hold is the usage.
gen_case(lang_c 0 --schema|${contract}|--lang|c)
gen_case(lang_cpp 0 --schema|${contract}|--lang|cpp)
gen_case(lang_rust 0 --schema|${contract}|--lang|rust)
gen_case(lang_python 0 --schema|${contract}|--lang|python)
add_test(
  NAME gen.lang_none
  COMMAND
    ${CMAKE_COMMAND} -DDOCTOR=$<TARGET_FILE:configforge_gen>
    -DARGS=--schema|${contract}|--lang|nope -DEXPECT_FAIL=2
    "-DEXPECT_ERR=gen: no language 'nope'; there are: c, cpp, rust, python" -P
    ${CMAKE_SOURCE_DIR}/tests/doctor/run.cmake
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/tests/doctor)
set_tests_properties(gen.lang_none PROPERTIES FIXTURES_REQUIRED DOCTOR_REPORTS)
gen_case(lang_name 2 --schema|${contract}|--lang|cpp|--name|1x)
gen_case(lang_name_rust 2 --schema|${contract}|--lang|rust|--name|_x)
gen_case(lang_name_python 2 --schema|${contract}|--lang|python|--name|class)

# A generator outside the tree (`--lang <name>` runs configforge-gen-<name>,
# D-110): fake_gen.c is the program, built into ext/ with the copies the cases
# need - one named for a language the table has, which must never run, and
# configforge-gen itself, for finding a generator beside the program.
# external.cmake holds the cases. Not built where a program cannot run
# another (the front door's own test).
if(NOT CF_PLATFORM STREQUAL "none"
   AND NOT CMAKE_SYSTEM_NAME STREQUAL "WASI"
   AND NOT CMAKE_CROSSCOMPILING)
  set(gen_ext ${CMAKE_CURRENT_BINARY_DIR}/ext)
  # $<0:> keeps multi-config generators (Visual Studio, Xcode) from adding
  # a Debug/ or Release/ directory: the cases find the program in ext/.
  add_executable(gen_fake fake_gen.c)
  set_target_properties(
    gen_fake
    PROPERTIES C_STANDARD 90
               C_STANDARD_REQUIRED ON
               C_EXTENSIONS OFF
               OUTPUT_NAME configforge-gen-fake
               RUNTIME_OUTPUT_DIRECTORY ${gen_ext}$<0:>)
  configforge_apply_warnings(gen_fake)
  configforge_apply_analysis(gen_fake)
  # The copies are made by a target of their own, so that a rebuilt
  # configforge-gen is copied again and not only when gen_fake is relinked.
  add_custom_target(
    gen_ext ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:gen_fake>
            ${gen_ext}/configforge-gen-c${CMAKE_EXECUTABLE_SUFFIX}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:configforge_gen>
            ${gen_ext}/configforge-gen${CMAKE_EXECUTABLE_SUFFIX}
    VERBATIM)
  add_dependencies(gen_ext gen_fake configforge_gen)
  if(WIN32 AND BUILD_SHARED_LIBS)
    add_custom_command(
      TARGET gen_ext
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:configforge>
              ${gen_ext}
      VERBATIM)
  endif()
endif()

# The golden file has a real (`ratio` and its range), which a build without
# reals cannot read from the schema.
if(NOT CMAKE_CROSSCOMPILING
   AND CF_WITH_REAL
   AND TARGET gen_fake)
  add_test(
    NAME gen.external
    COMMAND
      ${CMAKE_COMMAND} -DGEN=$<TARGET_FILE:configforge_gen> -DEXT=${gen_ext}
      -DEXE=${CMAKE_EXECUTABLE_SUFFIX} -DSCHEMA=${contract}
      -DGOLDEN=${CMAKE_CURRENT_SOURCE_DIR}/golden_contract.json
      -DWORK=${CMAKE_CURRENT_BINARY_DIR}/external_work -P
      ${CMAKE_CURRENT_SOURCE_DIR}/external.cmake)
endif()

# The version it reports is the one cf_version.h says, with the commit
# after it when the build knew one.
add_test(NAME gen.version COMMAND configforge_gen --version)
set_tests_properties(
  gen.version
  PROPERTIES
    PASS_REGULAR_EXPRESSION
    "^ConfigForge gen ${PROJECT_VERSION}( \\([0-9a-f]+(-dirty)?\\))?\n$")

# The contract as Rust (`--lang rust`, D-95) against the golden file checked
# in: the crate compiles and runs that file (bindings/rust/tests/
# gen_contract.rs, under rust.cargo_test), so what is checked here is that
# the program still writes it.
if(NOT CMAKE_CROSSCOMPILING)
  add_test(
    NAME gen.rust
    COMMAND
      ${CMAKE_COMMAND} -DGEN=$<TARGET_FILE:configforge_gen>
      -DSCHEMA=${contract} -DLANG=rust
      -DGOLDEN=${CMAKE_CURRENT_SOURCE_DIR}/contract.rs
      -DOUT=${CMAKE_CURRENT_BINARY_DIR}/gen_contract.rs -P
      ${CMAKE_CURRENT_SOURCE_DIR}/golden.cmake)
endif()

# The contract as Python (`--lang python`, D-95): the module imported by its
# path and a configuration read through its dataclass. ctypes needs a shared
# library, as python.binding does.
if(BUILD_SHARED_LIBS AND NOT CMAKE_CROSSCOMPILING)
  find_package(
    Python3
    COMPONENTS Interpreter
    QUIET)
  if(Python3_Interpreter_FOUND)
    add_test(
      NAME gen.python
      COMMAND
        ${CMAKE_COMMAND} -E env CONFIGFORGE_LIB=$<TARGET_FILE:configforge>
        ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_gen_python.py
        ${CMAKE_SOURCE_DIR}/bindings/python/src $<TARGET_FILE:configforge_gen>
        ${contract} ${CMAKE_CURRENT_BINARY_DIR}/python)
  endif()
endif()

# The contract as C (`configforge gen --schema`), generated by this
# build's program and compiled with the project's warnings, against the
# schema it came from. Where the program runs on the build machine.
if(NOT CMAKE_CROSSCOMPILING)
  set(gen_c ${CMAKE_CURRENT_BINARY_DIR}/gen_contract.c)
  add_custom_command(
    OUTPUT ${gen_c}
    COMMAND configforge_gen --schema ${contract} --name gen_contract --out
            ${gen_c}
    DEPENDS configforge_gen ${contract}
    VERBATIM)
  add_executable(gen_c test_gen.c ${gen_c})
  target_compile_definitions(gen_c PRIVATE CF_CONTRACT_SCHEMA="${contract}")
  target_link_libraries(gen_c PRIVATE configforge)
  set_target_properties(
    gen_c
    PROPERTIES C_STANDARD 90
               C_STANDARD_REQUIRED ON
               C_EXTENSIONS OFF)
  configforge_apply_warnings(gen_c)
  configforge_apply_analysis(gen_c)
  add_test(NAME gen.c COMMAND gen_c)

  # The contract as a C++ header (`--lang cpp`, D-95): its definitions, a
  # Key for each key and a struct read through a Binding, compiled as C++98
  # like every wrapper consumer.
  if(CF_WITH_CXX)
    set(gen_hpp ${CMAKE_CURRENT_BINARY_DIR}/gen_contract.hpp)
    add_custom_command(
      OUTPUT ${gen_hpp}
      COMMAND configforge_gen --schema ${contract} --lang cpp --out ${gen_hpp}
              --name gen_contract
      DEPENDS configforge_gen ${contract}
      VERBATIM)
    add_executable(gen_hpp_test test_gen_cpp.cpp ${gen_hpp})
    target_include_directories(gen_hpp_test PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    target_compile_definitions(gen_hpp_test
                               PRIVATE CF_CONTRACT_SCHEMA="${contract}")
    target_link_libraries(gen_hpp_test PRIVATE configforge)
    set_target_properties(
      gen_hpp_test
      PROPERTIES CXX_STANDARD 98
                 CXX_STANDARD_REQUIRED ON
                 CXX_EXTENSIONS OFF)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
      target_compile_options(gen_hpp_test PRIVATE -pedantic -Werror -Wall
                                                  -Wextra)
    elseif(MSVC)
      configforge_apply_warnings(gen_hpp_test)
    endif()
    configforge_apply_analysis(gen_hpp_test)
    add_test(NAME gen.cpp COMMAND gen_hpp_test)
  endif()

  # The same contract as C++, and the contract of a schema the library reads
  # and refuses only at create (a text default longer than its maxLength),
  # each by the build's program. The generated file checks itself where the
  # language can (D-75): the good one must compile with its checks, the
  # refused one must not.
  set(gen_cpp ${CMAKE_CURRENT_BINARY_DIR}/gen_contract.cpp)
  add_custom_command(
    OUTPUT ${gen_cpp}
    COMMAND configforge_gen --schema ${contract} --name gen_contract --out
            ${gen_cpp}
    DEPENDS configforge_gen ${contract}
    VERBATIM)
  set(refused ${CMAKE_CURRENT_SOURCE_DIR}/contract_refused.schema.json)
  set(gen_refused_c ${CMAKE_CURRENT_BINARY_DIR}/gen_refused.c)
  set(gen_refused_cpp ${CMAKE_CURRENT_BINARY_DIR}/gen_refused.cpp)
  add_custom_command(
    OUTPUT ${gen_refused_c}
    COMMAND configforge_gen --schema ${refused} --name gen_refused --out
            ${gen_refused_c}
    DEPENDS configforge_gen ${refused}
    VERBATIM)
  add_custom_command(
    OUTPUT ${gen_refused_cpp}
    COMMAND configforge_gen --schema ${refused} --name gen_refused --out
            ${gen_refused_cpp}
    DEPENDS configforge_gen ${refused}
    VERBATIM)
  include(ConfigForgeMustNotCompile)
  set(gen_refusal "a text default longer than its max_len")

  if("c_std_11" IN_LIST CMAKE_C_COMPILE_FEATURES AND NOT CF_FORCE_PORTABLE)
    add_library(gen_c11 OBJECT ${gen_c})
    set_target_properties(
      gen_c11
      PROPERTIES C_STANDARD 11
                 C_STANDARD_REQUIRED ON
                 C_EXTENSIONS OFF)
    target_link_libraries(gen_c11 PRIVATE configforge)
    configforge_apply_warnings(gen_c11)
    cf_must_not_compile(
      gen.refused.c11
      SOURCE
      ${gen_refused_c}
      LANG
      C
      STD
      11
      DEFINE
      CF_GEN_REFUSED=1
      EXPECT
      "${gen_refusal}")
  endif()

  if(CF_WITH_CXX AND "cxx_std_11" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
    add_library(gen_cxx11 OBJECT ${gen_cpp})
    set_target_properties(
      gen_cxx11
      PROPERTIES CXX_STANDARD 11
                 CXX_STANDARD_REQUIRED ON
                 CXX_EXTENSIONS OFF)
    target_link_libraries(gen_cxx11 PRIVATE configforge)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
      target_compile_options(gen_cxx11 PRIVATE -pedantic -Werror -Wall -Wextra)
    elseif(MSVC)
      configforge_apply_warnings(gen_cxx11)
    endif()
    cf_must_not_compile(
      gen.refused.cxx11
      SOURCE
      ${gen_refused_cpp}
      LANG
      CXX
      STD
      11
      DEFINE
      CF_GEN_REFUSED=1
      EXPECT
      "${gen_refusal}")
  endif()
endif()
