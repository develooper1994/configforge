# Examples are built as part of the build so a broken example is a
# broken build. The first page of the documentation is an example and
# must not be allowed to rot.
# clang-tidy is for the product; see tests/CMakeLists.txt.
set(CMAKE_C_CLANG_TIDY "")

set(CF_EXAMPLE_EXPECT ${CMAKE_CURRENT_SOURCE_DIR}/expect.cmake)

# Register a run of one example: a clean exit, and every line its
# comment promises (expect.cmake). Both languages use it, and the same
# lines - the C++ programs are the C ones through the wrapper, so they
# must say the same thing.
#
# ~~~
# example_expect(<target> <working-dir> <regex>...)
# ~~~
function(example_expect name dir)
  string(REPLACE ";" "|||" wanted "${ARGN}")
  add_test(NAME example.${name}
           COMMAND ${CMAKE_COMMAND} -DEXE=$<TARGET_FILE:${name}> -DDIR=${dir}
                   "-DEXPECT=${wanted}" -P ${CF_EXAMPLE_EXPECT})
endfunction()

# One directory a language, so a reader looking for the C++ wrapper in
# use does not have to pick it out from among the C programs.
add_subdirectory(c)
if(CF_WITH_CXX)
  add_subdirectory(cpp)
endif()
