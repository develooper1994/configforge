# WebAssembly, for a WASI runtime: the library in an edge runtime, a
# plugin host, or a browser with a WASI shim (tier 2, D-55).
#
#   cmake -S . -B build-wasm --toolchain cmake/toolchains/wasm32-wasi.cmake
#
# clang's own wasm32 target and wasi-libc's sysroot - in Ubuntu,
# clang, lld, wasi-libc and libclang-rt-18-dev-wasm32. Single-threaded
# (WASI preview 1 has no threads) and without a change handle (it has no
# file notification either): cf_changed still works, by polling. Tests
# run under Node's WASI, through scripts/wasi-run.js.
set(CMAKE_SYSTEM_NAME WASI)
set(CMAKE_SYSTEM_PROCESSOR wasm32)

# wasi-sdk's layout, or a distribution's multiarch one (Ubuntu's
# wasi-libc: /usr/include/wasm32-wasi), which clang finds under /usr.
if(NOT DEFINED WASI_SYSROOT)
  if(EXISTS /usr/share/wasi-sysroot/include)
    set(WASI_SYSROOT /usr/share/wasi-sysroot)
  else()
    set(WASI_SYSROOT /usr)
  endif()
endif()
set(CMAKE_C_COMPILER clang)
set(CMAKE_C_COMPILER_TARGET wasm32-wasi)
set(CMAKE_SYSROOT "${WASI_SYSROOT}")
# WASI preview 1 has no signals and no process clock; wasi-libc emulates
# both, and the tests use them (raise in the abort tests, clock in the
# complexity checks). The library itself needs neither.
set(CMAKE_C_FLAGS_INIT
    "-D_WASI_EMULATED_SIGNAL -D_WASI_EMULATED_PROCESS_CLOCKS")
set(CMAKE_EXE_LINKER_FLAGS_INIT
    "-fuse-ld=lld -lwasi-emulated-signal -lwasi-emulated-process-clocks")
set(CMAKE_EXECUTABLE_SUFFIX ".wasm")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# What WASI has not: threads, shared libraries, C++ exceptions worth the
# name. The wrapper, Doctor and the examples stay on the host.
set(CF_SINGLE_THREADED
    ON
    CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS
    OFF
    CACHE BOOL "" FORCE)
set(CF_WITH_CXX
    OFF
    CACHE BOOL "" FORCE)
set(CF_BUILD_TOOLS
    OFF
    CACHE BOOL "" FORCE)
set(CF_BUILD_EXAMPLES
    OFF
    CACHE BOOL "" FORCE)

find_program(NODE_EXE node)
if(NODE_EXE)
  set(CMAKE_CROSSCOMPILING_EMULATOR
      "${NODE_EXE};${CMAKE_CURRENT_LIST_DIR}/../../scripts/wasi-run.js")
endif()
