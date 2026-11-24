cmake_minimum_required(VERSION 3.10)

# The libraries' own sources run through cppcheck in the analyze build:
# both targets take their C_CPPCHECK from CF_CPPCHECK_LIBRARIES, and no
# target opts out with an empty C_CPPCHECK.
file(READ "${SRC}/CMakeLists.txt" text)
string(REGEX REPLACE "[ \n]+" " " flat "${text}")
set(failed OFF)

set(wanted "set_target_properties(configforge configforge_serialize ")
string(APPEND wanted "PROPERTIES C_CPPCHECK \"\${CF_CPPCHECK_LIBRARIES}\")")
string(FIND "${flat}" "${wanted}" found)
if(found EQUAL -1)
  message(STATUS "VIOLATION: the libraries do not both take C_CPPCHECK from "
                 "CF_CPPCHECK_LIBRARIES")
  set(failed ON)
endif()
if("${flat}" MATCHES "C_CPPCHECK \"\"")
  message(STATUS "VIOLATION: a target sets an empty C_CPPCHECK")
  set(failed ON)
endif()

if(failed)
  message(FATAL_ERROR "gate.cppcheck: the libraries are not both checked")
endif()
message(STATUS "gate.cppcheck: both libraries are checked")
