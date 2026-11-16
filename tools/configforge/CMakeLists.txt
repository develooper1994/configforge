# The front door: `configforge <command>` runs `configforge-<command>`
# (D-99). It links no library: a program that only finds and starts
# others needs none, and so needs no DLL beside it on Windows.
if(CF_PLATFORM STREQUAL "none" OR CMAKE_SYSTEM_NAME STREQUAL "WASI")
  message(STATUS "configforge: a front door runs other programs, "
                 "which this platform cannot; not built")
  return()
endif()

if(WIN32)
  set(front_run run_win32.c)
else()
  set(front_run run_posix.c)
endif()
add_executable(configforge_front main.c ${front_run})
set_target_properties(
  configforge_front
  PROPERTIES OUTPUT_NAME configforge
             C_STANDARD 90
             C_STANDARD_REQUIRED ON
             C_EXTENSIONS OFF
             RUNTIME_OUTPUT_DIRECTORY ${CF_TOOLS_DIR})
# The library's headers and definitions, without the library: the
# platform is decided once, where the library decides it.
target_include_directories(
  configforge_front
  PRIVATE $<TARGET_PROPERTY:configforge,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(
  configforge_front
  PRIVATE $<TARGET_PROPERTY:configforge,INTERFACE_COMPILE_DEFINITIONS>)
if(WIN32)
  target_link_libraries(configforge_front PRIVATE shell32)
endif()
configforge_apply_warnings(configforge_front)
configforge_apply_hardening(configforge_front)
configforge_apply_analysis(configforge_front)

# The same clang-tidy as Doctor's, less cert-err33-c: a diagnostic that
# could not be printed has nowhere better to be reported.
if(CMAKE_C_CLANG_TIDY)
  set_target_properties(
    configforge_front
    PROPERTIES C_CLANG_TIDY "${CMAKE_C_CLANG_TIDY};--checks=-cert-err33-c")
endif()
