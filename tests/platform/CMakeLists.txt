# A build with no operating system and no C library (K-11b, D-220): the
# preset none-bare has CF_PLATFORM=none, CF_NO_STDIO and CF_NO_STDLIB, and
# this is its one test. Anywhere else it would fail by design.
if(CF_PLATFORM STREQUAL "none"
   AND CF_NO_STDIO
   AND CF_NO_STDLIB)
  cf_unit_test_executable(freestanding configforge)
  add_test(NAME platform.freestanding COMMAND freestanding)
endif()
