# Every public header must compile standalone, as C89, with nothing
# included before it. A header that only works because some other
# header happened to come first is a header that breaks for the first
# user who includes it alone.
file(GLOB headers "${SRC}/include/configforge/*.h")
set(bad 0)
foreach(h ${headers})
  get_filename_component(name "${h}" NAME)
  set(probe "${BIN}/probe_${name}.c")
  file(WRITE "${probe}" "#include <configforge/${name}>\nint main(void){return 0;}\n")
  execute_process(
    COMMAND ${CC} -std=c89 -pedantic -Werror -I${SRC}/include
            -c -o "${BIN}/probe_${name}.o" "${probe}"
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
  if(NOT rc EQUAL 0)
    message("VIOLATION: ${name} does not compile standalone as C89")
    message("${out}")
    set(bad 1)
  endif()
endforeach()
if(bad)
  message(FATAL_ERROR "header gate failed")
endif()
message("header gate: clean")

# The C++ wrapper claims C++03. A claim the build does not check is a
# claim that decays, so the header is compiled standalone as C++98 too.
if(CXX AND EXISTS "${SRC}/include/configforge/configforge.hpp")
  file(WRITE "${BIN}/probe_hpp.cpp"
       "#include <configforge/configforge.hpp>\nint main(){return 0;}\n")
  execute_process(
    COMMAND ${CXX} -std=c++98 -pedantic -Werror -Wall -Wextra
            -I${SRC}/include -c -o "${BIN}/probe_hpp.o" "${BIN}/probe_hpp.cpp"
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
  if(NOT rc EQUAL 0)
    message("VIOLATION: configforge.hpp does not compile standalone as C++98")
    message("${out}")
    message(FATAL_ERROR "header gate failed")
  endif()
  message("header gate: C++98 clean")
endif()
