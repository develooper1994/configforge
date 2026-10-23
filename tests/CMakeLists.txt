# clang-tidy is pointed at the product - the library and Doctor - and not
# at the programs that exercise it. Here its findings were a printf whose
# result went unchecked and a test asserting on a pointer it had just
# checked, forty of them, and a check whose advice nobody will take
# teaches people to run it with the output ignored. The tests still get
# the compiler's warnings, cppcheck, -fanalyzer and the sanitisers.
set(CMAKE_C_CLANG_TIDY "")

add_subdirectory(unit)

if(CF_BUILD_TOOLS
   AND CF_WITH_JSON
   AND CF_WITH_REPORT)
  add_subdirectory(doctor)
endif()
