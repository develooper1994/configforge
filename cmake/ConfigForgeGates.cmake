# Architectural gates.
#
# These are the checks that keep the specification's claims true. A
# claim that is not checked by the build is a claim that decays.

# configforge_add_consumer_gates(): the C23 proof (tests/consumer), apart from
# configforge_add_gates, which is as long as a function here may be.
function(configforge_add_consumer_gates)
  if(NOT CF_ENABLE_GATES)
    return()
  endif()
  add_test(
    NAME consumer.nodiscard
    COMMAND
      ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR} -DBIN=${CMAKE_BINARY_DIR}
      -DCC=${CMAKE_C_COMPILER} -DCC_ID=${CMAKE_C_COMPILER_ID} -P
      ${CMAKE_SOURCE_DIR}/cmake/gates/ConsumerNodiscard.cmake)
endfunction()

# Register the architectural gates as tests. See the README.
#
# The gate scripts live in cmake/gates because they are *run* by ctest,
# while the modules beside this file are *included* by CMakeLists. Two
# different kinds of file sharing one directory is how a directory stops
# telling you anything.
#
# ~~~
# configforge_add_gates(<target>)
# ~~~
function(configforge_add_gates tgt)
  if(NOT CF_ENABLE_GATES)
    return()
  endif()

  # Every option and limit, for the gates that compile the library a
  # second way and have to compile the same one: see GateAmalgamation.
  string(REPLACE ";" "|" features "${CF_FEATURES}")
  set(options
      "-DFEATURES=${features}"
      "-DWERROR=$<BOOL:${CF_WERROR}>"
      "-DSINGLE=$<BOOL:${CF_SINGLE_THREADED}>"
      "-DPORTABLE=$<BOOL:${CF_FORCE_PORTABLE}>"
      "-DPLATFORM=${CF_PLATFORM}"
      "-DNO_STDIO=$<BOOL:${CF_NO_STDIO}>"
      "-DNO_STDLIB=$<BOOL:${CF_NO_STDLIB}>")
  foreach(f ${CF_FEATURES})
    list(APPEND options "-DO_${f}=$<BOOL:${CF_WITH_${f}}>")
  endforeach()
  foreach(lim CF_KEY_MAX CF_MESSAGE_MAX CF_PATH_MAX CF_MAX_DEPTH)
    list(APPEND options "-DL_${lim}=${${lim}}")
  endforeach()
  if(CF_WITH_YAML)
    list(
      APPEND
      options
      "-DYAML_INC=${CF_YAML_INCLUDE_DIR}"
      "-DYAML_LIB=${CF_YAML_LIBRARY}"
      "-DYAML_FY=${_CF_YAML_LIBFYAML}"
      "-DYAML_LY=${_CF_YAML_LIBYAML}"
      "-DYAML_FY_MIN=${_CF_FYAML_MIN}")
  endif()
  if(CF_WITH_XML)
    list(APPEND options "-DXML_INC=${CF_EXPAT_INCLUDE_DIR}"
         "-DXML_LIB=${CF_EXPAT_LIBRARY}")
  endif()
  if(CF_WITH_SQLITE)
    list(APPEND options "-DSQLITE_INC=${CF_SQLITE_INCLUDE_DIR}"
         "-DSQLITE_LIB=${CF_SQLITE_LIBRARY}")
  endif()
  # macOS's user defaults link CoreFoundation. One argument, the linker's
  # own spelling: "-framework CoreFoundation" is two, and a list passed
  # through a -D of its own arrives split.
  set(prefs_lib "")
  if(APPLE AND CF_WITH_PREFERENCES)
    set(prefs_lib "-Wl,-framework,CoreFoundation")
    list(APPEND options "-DPREFS_LIB=${prefs_lib}")
  endif()

  # 1. The header version and the project version agree.
  add_test(
    NAME gate.version
    COMMAND
      ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR}
      -DPROJECT_VER=${PROJECT_VERSION} -P
      ${CMAKE_SOURCE_DIR}/cmake/gates/GateVersion.cmake)

  # 2. The core may not reach into sources, platform or serialize.
  add_test(
    NAME gate.layering
    COMMAND
      ${CMAKE_COMMAND} -DLIB=${CMAKE_SOURCE_DIR}/lib
      -DLAYERS=${CMAKE_BINARY_DIR}/layers.cmake -P
      ${CMAKE_SOURCE_DIR}/cmake/gates/GateLayering.cmake)

  # 2. Feature detection macros may appear only in the portability layer:
  #    gate.semgrep (cf-detect-compiler, cf-detect-platform; ConfigForgeFormat).

  # The dependency page names everything the build looks for, and the
  # generated SBOM is a valid document about this build. Neither needs
  # a compiler, so both run everywhere.
  # Every tracked text file is text: no NUL byte from a lost backslash,
  # no carriage return from a Windows writer.
  add_test(NAME gate.text
           COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR} -P
                   ${CMAKE_SOURCE_DIR}/cmake/gates/GateText.cmake)
  # Every download is listed with how it is held, and every workflow's
  # top-level token is read-only (scripts/check/fetches.py, permissions.py).
  # Every public function whose result reports a failure is CF_NODISCARD
  # (scripts/check/nodiscard.py).
  # Every public options struct has size first and an _init function, and
  # every CF_MAX_* macro says where its limit went (scripts/check/options.py).
  # The rest read only the tree: each is cmake/gates/Gate<Name>.cmake.
  foreach(
    gate
    Fetches
    Nodiscard
    Options
    Permissions
    Index
    Dependencies)
    string(TOLOWER "${gate}" lower)
    add_test(NAME gate.${lower}
             COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR} -P
                     ${CMAKE_SOURCE_DIR}/cmake/gates/Gate${gate}.cmake)
  endforeach()
  # The API reference is the headers' comments, regenerated and compared.
  add_test(
    NAME gate.apiref
    COMMAND
      ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR} -DBIN=${CMAKE_BINARY_DIR} -P
      ${CMAKE_SOURCE_DIR}/cmake/gates/GateApiref.cmake)
  add_test(
    NAME gate.codemap
    COMMAND
      ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR} -DBIN=${CMAKE_BINARY_DIR} -P
      ${CMAKE_SOURCE_DIR}/cmake/gates/GateCodemap.cmake)
  add_test(
    NAME gate.limits
    COMMAND
      ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR} -DBIN=${CMAKE_BINARY_DIR} -P
      ${CMAKE_SOURCE_DIR}/cmake/gates/GateLimits.cmake)
  # The symbol list is the public interface: against the headers
  # everywhere, and against the built library where nm can read it. A DLL
  # is read through its import library, the linker file: nm reads no
  # export table, and an import library names exactly what is exported.
  add_test(
    NAME gate.abi
    COMMAND
      ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR}
      -DLIB=$<TARGET_LINKER_FILE:${tgt}> -DWITH_REAL=${CF_WITH_REAL} -P
      ${CMAKE_SOURCE_DIR}/cmake/gates/GateAbi.cmake)
  # The serialization library's own list (D-90): its promised functions,
  # every one of them libconfigforge's too, and what its library defines.
  add_test(
    NAME gate.abi.serialize
    COMMAND
      ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR} -DPART=serialize
      -DLIB=$<TARGET_LINKER_FILE:configforge_serialize>
      -DWITH_REAL=${CF_WITH_REAL} -P
      ${CMAKE_SOURCE_DIR}/cmake/gates/GateAbi.cmake)
  add_test(
    NAME gate.sbom
    COMMAND
      ${CMAKE_COMMAND} -DSBOM=${CMAKE_BINARY_DIR}/configforge.spdx.json
      -DPROJECT_VER=${PROJECT_VERSION} -P
      ${CMAKE_SOURCE_DIR}/cmake/gates/GateSbom.cmake)

  add_test(
    NAME gate.matrix
    COMMAND
      ${CMAKE_COMMAND} "-DSRC=${CMAKE_SOURCE_DIR}" "-DBIN=${CMAKE_BINARY_DIR}"
      -P ${CMAKE_SOURCE_DIR}/cmake/gates/GateMatrix.cmake)

  add_test(NAME gate.profiles
           COMMAND ${CMAKE_COMMAND} "-DSRC=${CMAKE_SOURCE_DIR}" -P
                   ${CMAKE_SOURCE_DIR}/cmake/gates/GateProfiles.cmake)

  # The libraries' own sources run through cppcheck in the analyze build.
  add_test(NAME gate.cppcheck
           COMMAND ${CMAKE_COMMAND} "-DSRC=${CMAKE_SOURCE_DIR}" -P
                   ${CMAKE_SOURCE_DIR}/cmake/gates/GateCppcheck.cmake)

  # The rest drive the compiler with a GCC-style command line, which
  # MSVC rejects at the first flag - and it has no C89 mode to ask for
  # in any case (see ConfigForgeCompiler.cmake). They are about the
  # sources rather than the compiler, so the GCC and Clang jobs answer
  # them for every platform; on MSVC the library and every test are
  # still built at /W4 /WX, which is the check this compiler can give.
  # Registering them anyway would make the Windows suite fail on a
  # command line rather than on anything in the code.
  if(MSVC)
    return()
  endif()

  add_test(
    NAME gate.cversion
    COMMAND
      ${CMAKE_COMMAND} "-DSRC=${CMAKE_SOURCE_DIR}"
      "-DLIST=${CMAKE_BINARY_DIR}/cf_cversion.txt"
      "-DCOMPDB=${CMAKE_BINARY_DIR}/compile_commands.json"
      "-DC89_ONLY=$<BOOL:${CF_C89_ONLY}>" -P
      ${CMAKE_SOURCE_DIR}/cmake/gates/GateCVersion.cmake)

  # 3. Every public header compiles standalone as C89.
  add_test(
    NAME gate.headers
    COMMAND
      ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR} -DBIN=${CMAKE_BINARY_DIR}
      -DCC=${CMAKE_C_COMPILER} -DCXX=${CMAKE_CXX_COMPILER} -P
      ${CMAKE_SOURCE_DIR}/cmake/gates/GateHeaders.cmake)

  # 4. The amalgamation builds and runs.
  add_test(
    NAME gate.amalgamation
    COMMAND
      ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR} -DBIN=${CMAKE_BINARY_DIR}
      -DCC=${CMAKE_C_COMPILER} -DLIB=$<TARGET_FILE:${tgt}>
      -DSANITIZE=${CF_INSTRUMENTED} ${options} -P
      ${CMAKE_SOURCE_DIR}/cmake/gates/GateAmalgamation.cmake)

  # 5. No locale-sensitive C library function may be referenced, and
  #    every exported symbol must carry the cf_ prefix.
  if(UNIX)
    add_test(
      NAME gate.install
      COMMAND
        ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR} -DBIN=${CMAKE_BINARY_DIR}
        -DCC=${CMAKE_C_COMPILER} -DWITH_CXX=${CF_WITH_CXX}
        -DSANITIZE=${CF_INSTRUMENTED} -DEXPECT_REAL=$<BOOL:${CF_WITH_REAL}>
        -DEXPECT_JSON=$<BOOL:${CF_WITH_JSON}> -DYAML_LIB=${CF_YAML_LIBRARY}
        -DXML_LIB=${CF_EXPAT_LIBRARY} -DSQLITE_LIB=${CF_SQLITE_LIBRARY}
        -DPREFS_LIB=${prefs_lib} -DWERROR=$<BOOL:${CF_WERROR}> -P
        ${CMAKE_SOURCE_DIR}/cmake/gates/GateInstall.cmake)

    add_test(NAME gate.formats
             COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR} -P
                     ${CMAKE_SOURCE_DIR}/cmake/gates/GateFormats.cmake)

    # docs/Performance.md's static block is what scripts/bench/page.py writes
    # (P-1b, D-111). Unix only, like gate.formats, and skipped when there is no
    # Python.
    add_test(NAME gate.perfpage
             COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR} -P
                     ${CMAKE_SOURCE_DIR}/cmake/gates/GatePerfpage.cmake)

    add_test(
      NAME gate.interface
      COMMAND
        ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR} -DBIN=${CMAKE_BINARY_DIR}
        -DCC=${CMAKE_C_COMPILER} -P
        ${CMAKE_SOURCE_DIR}/cmake/gates/GateInterface.cmake)

    add_test(
      NAME gate.symbols
      COMMAND
        ${CMAKE_COMMAND} -DLIB=$<TARGET_FILE:${tgt}>
        -DSANITIZE=${CF_INSTRUMENTED} -DCOMPILER=${CMAKE_C_COMPILER_ID} -P
        ${CMAKE_SOURCE_DIR}/cmake/gates/GateSymbols.cmake)
  endif()

  # cppcheck's MISRA C:2012 addon over the sources the misra profile builds,
  # against docs/compliance/deviations.txt (N-19, D-112). Only there: the
  # profile's own compile database is what is analysed. The compiler's
  # macros go through -D joined with '|', as the features do.
  if(CF_BUILD_PROFILE_NAME STREQUAL "misra")
    cf_cppcheck_compiler_defines(misra_defines)
    string(REPLACE ";" "|" misra_defines "${misra_defines}")
    add_test(
      NAME gate.misra
      COMMAND
        ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR} -DBIN=${CMAKE_BINARY_DIR}
        "-DDEFINES=${misra_defines}" -P
        ${CMAKE_SOURCE_DIR}/cmake/gates/GateMisra.cmake)
    set_tests_properties(gate.misra PROPERTIES TIMEOUT 900)
  endif()
endfunction()
