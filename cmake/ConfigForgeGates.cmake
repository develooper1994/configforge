# Architectural gates.
#
# These are the checks that keep the specification's claims true. A
# claim that is not checked by the build is a claim that decays.

function(configforge_add_gates tgt)
  if(NOT CF_ENABLE_GATES)
    return()
  endif()

  # 1. The header version and the project version agree.
  add_test(NAME gate.version
           COMMAND ${CMAKE_COMMAND}
                   -DSRC=${CMAKE_SOURCE_DIR}
                   -DPROJECT_VER=${PROJECT_VERSION}
                   -P ${CMAKE_SOURCE_DIR}/cmake/GateVersion.cmake)

  # 2. The core may not reach into sources, platform or serialize.
  add_test(NAME gate.layering
           COMMAND ${CMAKE_COMMAND}
                   -DSRC=${CMAKE_SOURCE_DIR}/src
                   -P ${CMAKE_SOURCE_DIR}/cmake/GateLayering.cmake)

  # 2. Feature detection macros may appear only in the portability layer.
  add_test(NAME gate.detection
           COMMAND ${CMAKE_COMMAND}
                   -DSRC=${CMAKE_SOURCE_DIR}
                   -P ${CMAKE_SOURCE_DIR}/cmake/GateDetection.cmake)

  # 3. Every public header compiles standalone as C89.
  add_test(NAME gate.headers
           COMMAND ${CMAKE_COMMAND}
                   -DSRC=${CMAKE_SOURCE_DIR}
                   -DBIN=${CMAKE_BINARY_DIR}
                   -DCC=${CMAKE_C_COMPILER}
                   -DCXX=${CMAKE_CXX_COMPILER}
                   -P ${CMAKE_SOURCE_DIR}/cmake/GateHeaders.cmake)

  # 4. The amalgamation builds and runs.
  add_test(NAME gate.amalgamation
           COMMAND ${CMAKE_COMMAND}
                   -DSRC=${CMAKE_SOURCE_DIR}
                   -DBIN=${CMAKE_BINARY_DIR}
                   -DCC=${CMAKE_C_COMPILER}
                   -P ${CMAKE_SOURCE_DIR}/cmake/GateAmalgamation.cmake)

  # 5. No locale-sensitive C library function may be referenced, and
  #    every exported symbol must carry the cf_ prefix.
  if(UNIX)
    add_test(NAME gate.symbols
             COMMAND ${CMAKE_COMMAND}
                     -DLIB=$<TARGET_FILE:${tgt}>
                     -P ${CMAKE_SOURCE_DIR}/cmake/GateSymbols.cmake)
  endif()
endfunction()
