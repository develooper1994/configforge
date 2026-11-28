# The tests of the C++ wrapper (D-100): a test belongs to the lowest
# component that holds every configforge/ header it includes, and a C++
# test belongs to cxx.
cf_component_tests(cxx)
if(NOT CF_WITH_CXX)
  return()
endif()

# Change notification through the C++03 wrapper, built as C++98 like
# every other wrapper consumer, in a directory of its own for the same
# reason as the C test.
add_executable(test_wrapper_watch test_wrapper_watch.cpp)
target_link_libraries(test_wrapper_watch PRIVATE ${CF_TEST_LIB})
# C++98 on purpose: the floor, kept for good (D-72).
set_target_properties(
  test_wrapper_watch
  PROPERTIES CXX_STANDARD 98
             CXX_STANDARD_REQUIRED ON
             CXX_EXTENSIONS OFF)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
  target_compile_options(test_wrapper_watch PRIVATE -pedantic -Werror -Wall
                                                    -Wextra)
elseif(MSVC)
  # The C set is GCC's C flags; MSVC's is language-neutral and applies.
  configforge_apply_warnings(test_wrapper_watch)
endif()
configforge_apply_analysis(test_wrapper_watch)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/watch_cxx
     ${CMAKE_CURRENT_BINARY_DIR}/cf_wcreds) # test_wrapper_access's
add_test(NAME unit.test_wrapper_watch COMMAND test_wrapper_watch)
set_tests_properties(
  unit.test_wrapper_watch PROPERTIES WORKING_DIRECTORY
                                     ${CMAKE_CURRENT_BINARY_DIR}/watch_cxx)

# Changing and keeping, and reading without repeating a default,
# through the wrapper, built the same way.
foreach(t test_wrapper_write test_wrapper_access)
  add_executable(${t} ${t}.cpp)
  target_link_libraries(${t} PRIVATE ${CF_TEST_LIB})
  set_target_properties(
    ${t}
    PROPERTIES CXX_STANDARD 98
               CXX_STANDARD_REQUIRED ON
               CXX_EXTENSIONS OFF)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(${t} PRIVATE -pedantic -Werror -Wall -Wextra)
  elseif(MSVC)
    configforge_apply_warnings(${t})
  endif()
  configforge_apply_analysis(${t})
  add_test(NAME unit.${t} COMMAND ${t})
endforeach()
add_test(NAME unit.test_wrapper_write.rsp COMMAND test_wrapper_write --rsp
                                                  @cf_wrsp_args.txt)
add_test(NAME unit.test_wrapper_write.rsp_absent
         COMMAND test_wrapper_write --rsp @cf_wrsp_absent.txt)

# A callback that throws is caught before it reaches C: cf::Live takes
# callables under C++11, and an exception through cf_live's frames is
# undefined behaviour (K-9i).
if("cxx_std_11" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
  add_executable(test_wrapper_callback test_wrapper_callback.cpp)
  target_link_libraries(test_wrapper_callback PRIVATE ${CF_TEST_LIB})
  set_target_properties(
    test_wrapper_callback
    PROPERTIES CXX_STANDARD 11
               CXX_STANDARD_REQUIRED ON
               CXX_EXTENSIONS OFF)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(test_wrapper_callback PRIVATE -pedantic -Werror
                                                         -Wall -Wextra)
  elseif(MSVC)
    configforge_apply_warnings(test_wrapper_callback)
  endif()
  configforge_apply_analysis(test_wrapper_callback)
  add_test(NAME unit.test_wrapper_callback COMMAND test_wrapper_callback)
endif()

# The same reads under C++17. The header is C++03 but is included by
# programs on every later standard, and there cf_i64 can be long: the
# wrapper compiled as C++98 for years and not at all as C++11 on
# LP64, because nothing built it any other way.
add_executable(test_wrapper_access_cxx17 test_wrapper_access.cpp)
target_link_libraries(test_wrapper_access_cxx17 PRIVATE ${CF_TEST_LIB})
set_target_properties(test_wrapper_access_cxx17 PROPERTIES CXX_STANDARD 17
                                                           CXX_EXTENSIONS OFF)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
  target_compile_options(test_wrapper_access_cxx17 PRIVATE -pedantic -Werror
                                                           -Wall -Wextra)
elseif(MSVC)
  configforge_apply_warnings(test_wrapper_access_cxx17)
endif()
configforge_apply_analysis(test_wrapper_access_cxx17)
add_test(NAME unit.test_wrapper_access_cxx17 COMMAND test_wrapper_access_cxx17)
# The same source as test_wrapper_access, so the same files in the same
# directory: run side by side, one removed the file the other had just
# written, and "fileWith reads one file through it" failed now and then.
set_tests_properties(unit.test_wrapper_access unit.test_wrapper_access_cxx17
                     PROPERTIES RESOURCE_LOCK wrapper_access_files)

# The wrapper's C++17 and C++20 parts, each where the header itself
# turns it on: a test of the C++20 part built where the library has no
# <format> would test nothing, so it is asked of the header, not the
# compiler.
include(CheckCXXSourceCompiles)

# Whether configforge.hpp, compiled as C++<std>, turns on CF_CPP<std>.
function(cf_wrapper_part std var)
  set(CMAKE_CXX_STANDARD ${std})
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_REQUIRED_INCLUDES ${CF_STAGED_DIR})
  set(probe "#include <configforge/configforge.hpp>\n")
  string(APPEND probe "#if !CF_CPP${std}\n#error off\n#endif\n")
  string(APPEND probe "int main(){return 0;}")
  check_cxx_source_compiles("${probe}" ${var})
endfunction()
set(cf_modern)
if("cxx_std_17" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
  cf_wrapper_part(17 CF_WRAPPER_CXX17)
  if(CF_WRAPPER_CXX17)
    list(APPEND cf_modern "test_wrapper_cxx17:17")
  endif()
endif()
if("cxx_std_20" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
  cf_wrapper_part(20 CF_WRAPPER_CXX20)
  if(CF_WRAPPER_CXX20)
    list(APPEND cf_modern "test_wrapper_cxx20:20")
  endif()
endif()
foreach(entry ${cf_modern})
  string(REPLACE ":" ";" parts "${entry}")
  list(GET parts 0 t)
  list(GET parts 1 std)
  add_executable(${t} ${t}.cpp)
  target_link_libraries(${t} PRIVATE ${CF_TEST_LIB})
  set_target_properties(
    ${t}
    PROPERTIES CXX_STANDARD ${std}
               CXX_STANDARD_REQUIRED ON
               CXX_EXTENSIONS OFF)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(${t} PRIVATE -pedantic -Werror -Wall -Wextra)
  elseif(MSVC)
    configforge_apply_warnings(${t})
  endif()
  configforge_apply_analysis(${t})
  add_test(NAME unit.${t} COMMAND ${t})
endforeach()

# D-75: what the compiler checks of a key. test_wrapper_access compiled
# as C++11 and C++14 as well, since the check is written for each (a
# recursion, then a loop); and each case that must not compile, under
# every standard from C++11 the compiler has, refused by name.
include(ConfigForgeMustNotCompile)
set(cf_refused "1:not_a_canonical_key" "2:a_default_outside_its_own_range"
               "3:a_range_whose_low_end_is_above_its_high_end")
foreach(std 11 14 17 20)
  if("cxx_std_${std}" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
    if(std LESS 17)
      add_library(test_wrapper_access_cxx${std} OBJECT test_wrapper_access.cpp)
      target_link_libraries(test_wrapper_access_cxx${std}
                            PRIVATE ${CF_TEST_LIB})
      set_target_properties(
        test_wrapper_access_cxx${std}
        PROPERTIES CXX_STANDARD ${std}
                   CXX_STANDARD_REQUIRED ON
                   CXX_EXTENSIONS OFF)
      if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(test_wrapper_access_cxx${std}
                               PRIVATE -pedantic -Werror -Wall -Wextra)
      elseif(MSVC)
        configforge_apply_warnings(test_wrapper_access_cxx${std})
      endif()
    endif()
    foreach(entry ${cf_refused})
      string(REPLACE ":" ";" parts "${entry}")
      list(GET parts 0 n)
      list(GET parts 1 why)
      cf_must_not_compile(
        unit.refused.key${n}.cxx${std}
        SOURCE
        test_wrapper_access.cpp
        LANG
        CXX
        STD
        ${std}
        DEFINE
        CF_TEST_REFUSED=${n}
        EXPECT
        ${why})
    endforeach()
  endif()
endforeach()
if(TARGET test_wrapper_cxx17)
  cf_must_not_compile(
    unit.refused.fields.cxx17
    SOURCE
    test_wrapper_cxx17.cpp
    LANG
    CXX
    STD
    17
    DEFINE
    CF_TEST_REFUSED=1
    EXPECT
    not_a_canonical_key)
endif()
if(TARGET test_wrapper_cxx20)
  cf_must_not_compile(
    unit.refused.constinit.cxx20
    SOURCE
    test_wrapper_cxx20.cpp
    LANG
    CXX
    STD
    20
    DEFINE
    CF_TEST_REFUSED=1
    EXPECT
    a_default_outside_its_own_range)
endif()
