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

if(CF_FUZZ)
  add_subdirectory(fuzz)
endif()

# The Python binding loads the library with ctypes, which needs a shared
# build: run where there is one (verify's EXTRA=shared, CI's shared job)
# and a Python 3 to run it.
if(BUILD_SHARED_LIBS AND CF_WITH_INI)
  find_package(
    Python3
    COMPONENTS Interpreter
    QUIET)
  if(Python3_Interpreter_FOUND)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/python)
    add_test(
      NAME python.binding
      COMMAND
        ${CMAKE_COMMAND} -E env CONFIGFORGE_LIB=$<TARGET_FILE:configforge>
        ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/python/test_binding.py
        ${CMAKE_SOURCE_DIR}/bindings/python/src
        ${CMAKE_CURRENT_BINARY_DIR}/python)
  endif()
endif()
