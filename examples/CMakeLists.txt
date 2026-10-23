# Examples are built as part of the build so a broken example is a
# broken build. The first page of the documentation is an example and
# must not be allowed to rot.
# clang-tidy is for the product; see tests/CMakeLists.txt.
set(CMAKE_C_CLANG_TIDY "")

foreach(ex tier0 tier1 explain_demo)
  add_executable(${ex} ${ex}.c)
  target_link_libraries(${ex} PRIVATE configforge)
  set_target_properties(
    ${ex}
    PROPERTIES C_STANDARD 90
               C_STANDARD_REQUIRED ON
               C_EXTENSIONS OFF)
  configforge_apply_warnings(${ex})
  configforge_apply_analysis(${ex})
  add_test(NAME example.${ex} COMMAND ${ex})
endforeach()

if(CF_WITH_CXX)
  # Built as C++98 on purpose: the wrapper claims C++03 and a claim the
  # build does not check is a claim that decays.
  add_executable(tier1_cpp tier1.cpp)
  target_link_libraries(tier1_cpp PRIVATE configforge)
  set_target_properties(
    tier1_cpp
    PROPERTIES CXX_STANDARD 98
               CXX_STANDARD_REQUIRED ON
               CXX_EXTENSIONS OFF)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(tier1_cpp PRIVATE -pedantic -Werror -Wall -Wextra)
  endif()
  configforge_apply_analysis(tier1_cpp)
  add_test(NAME example.tier1_cpp COMMAND tier1_cpp)
endif()

add_executable(settings settings.c)
target_link_libraries(settings PRIVATE configforge)
set_target_properties(
  settings
  PROPERTIES C_STANDARD 90
             C_STANDARD_REQUIRED ON
             C_EXTENSIONS OFF)
configforge_apply_warnings(settings)
configforge_apply_analysis(settings)

# The whole lifecycle, driven from a script so that every line of the
# README's output is produced by the code rather than transcribed from
# it. Run in its own directory because it writes a file.
add_test(NAME example.settings
         COMMAND ${CMAKE_COMMAND} -DSETTINGS=$<TARGET_FILE:settings> -P
                 ${CMAKE_CURRENT_SOURCE_DIR}/settings_test.cmake)

# The failure paths are part of the documentation too, so they are run
# and their exit status is asserted rather than described.
add_test(NAME example.tier1.range COMMAND tier1 --server.port=99999)
add_test(NAME example.tier1.typo COMMAND tier1 --server.pot=1)
add_test(NAME example.tier1.enum COMMAND tier1 --log.level=verbose)
set_tests_properties(example.tier1.range example.tier1.typo example.tier1.enum
                     PROPERTIES WILL_FAIL TRUE)
