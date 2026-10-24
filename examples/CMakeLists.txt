# Examples are built as part of the build so a broken example is a
# broken build. The first page of the documentation is an example and
# must not be allowed to rot.
# clang-tidy is for the product; see tests/CMakeLists.txt.
set(CMAKE_C_CLANG_TIDY "")

# One directory a language, so a reader looking for the C++ wrapper in
# use does not have to pick it out from among the C programs.
add_subdirectory(c)
if(CF_WITH_CXX)
  add_subdirectory(cpp)
endif()
