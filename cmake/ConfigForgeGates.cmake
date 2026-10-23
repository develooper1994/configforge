# Architectural gates.
#
# These are the checks that keep the specification's claims true. A
# claim that is not checked by the build is a claim that decays.

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

  # 1. The header version and the project version agree.
  add_test(
    NAME gate.version
    COMMAND
      ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR}
      -DPROJECT_VER=${PROJECT_VERSION} -P
      ${CMAKE_SOURCE_DIR}/cmake/gates/GateVersion.cmake)

  # 2. The core may not reach into sources, platform or serialize.
  add_test(NAME gate.layering
           COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR}/src -P
                   ${CMAKE_SOURCE_DIR}/cmake/gates/GateLayering.cmake)

  # 2. Feature detection macros may appear only in the portability layer.
  add_test(NAME gate.detection
           COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR} -P
                   ${CMAKE_SOURCE_DIR}/cmake/gates/GateDetection.cmake)

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
      -DSANITIZE=${CF_SANITIZE} -DO_INI=$<BOOL:${CF_WITH_INI}>
      -DO_JSON=$<BOOL:${CF_WITH_JSON}> -DO_REAL=$<BOOL:${CF_WITH_REAL}>
      -DO_TOML=$<BOOL:${CF_WITH_TOML}> -DO_XML=$<BOOL:${CF_WITH_XML}>
      -DO_SQLITE=$<BOOL:${CF_WITH_SQLITE}> -P
      ${CMAKE_SOURCE_DIR}/cmake/gates/GateAmalgamation.cmake)

  # 5. No locale-sensitive C library function may be referenced, and
  #    every exported symbol must carry the cf_ prefix.
  if(UNIX)
    add_test(
      NAME gate.install
      COMMAND
        ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR} -DBIN=${CMAKE_BINARY_DIR}
        -DCC=${CMAKE_C_COMPILER} -DWITH_CXX=${CF_WITH_CXX}
        -DSANITIZE=${CF_SANITIZE} -DEXPECT_REAL=$<BOOL:${CF_WITH_REAL}>
        -DEXPECT_JSON=$<BOOL:${CF_WITH_JSON}> -P
        ${CMAKE_SOURCE_DIR}/cmake/gates/GateInstall.cmake)

    add_test(NAME gate.formats
             COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR} -P
                     ${CMAKE_SOURCE_DIR}/cmake/gates/GateFormats.cmake)

    add_test(
      NAME gate.interface
      COMMAND
        ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR} -DBIN=${CMAKE_BINARY_DIR}
        -DCC=${CMAKE_C_COMPILER} -P
        ${CMAKE_SOURCE_DIR}/cmake/gates/GateInterface.cmake)

    add_test(
      NAME gate.symbols
      COMMAND
        ${CMAKE_COMMAND} -DLIB=$<TARGET_FILE:${tgt}> -DSANITIZE=${CF_SANITIZE}
        -P ${CMAKE_SOURCE_DIR}/cmake/gates/GateSymbols.cmake)
  endif()
endfunction()
